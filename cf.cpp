#include <cassert>
#include <iostream>
#include <sstream>
#include <map>
#include <vector>
#include <deque>
#include <iterator>
#include <string>

class XY;
class XYObject;

using namespace std;

class XYObject
{
  public:
    virtual void eval1(XY* xy) = 0;
    virtual string toString() = 0;
};

class XYNumber : public XYObject
{
  public:
    int mValue;

  public:
    XYNumber(int v = 0);
    virtual string toString();
    virtual void eval1(XY* xy);
};

class XYSymbol : public XYObject
{
  public:
    string mValue;

  public:
    XYSymbol(string v);
    virtual string toString();
    virtual void eval1(XY* xy);
};

class XYList : public XYObject
{
  public:
    typedef vector<XYObject*> type;
    type mList;

  public:
    XYList();
    template <class InputIterator> XYList(InputIterator first, InputIterator last);
    virtual string toString();
    virtual void eval1(XY* xy);
};

class XYPrimitive : public XYObject
{
  public:
    string mName;

  public:
    XYPrimitive(string name);
    virtual string toString();
};

class XYAddition : public XYPrimitive
{
  public:
    XYAddition();
    virtual void eval1(XY* xy);
};

class XYSet : public XYPrimitive
{
  public:
    XYSet();
    virtual void eval1(XY* xy);
};

class XYGet : public XYPrimitive
{
  public:
    XYGet();
    virtual void eval1(XY* xy);
};

class XYUnquote : public XYPrimitive
{
  public:
    XYUnquote();
    virtual void eval1(XY* xy);
};

typedef map<string, XYObject*> XYEnv;

class XY {
  public:
    XYEnv mEnv;
    vector<XYObject*> mX;
    deque<XYObject*> mY;

  public:
    void print();
    XY* eval1();
    XY* eval();
};

// XY
void XY::print() {
  for(int i=0; i < mX.size(); ++i) {
    cout << mX[i]->toString() << " ";
  }

  cout << "-> ";

  for(int i=0; i < mY.size(); ++i) {
    cout << mY[i]->toString() << " ";
  }

  cout << endl;
}

XY* XY::eval1() {
  assert(mY.size() > 0);

  XYObject* o = mY.front();
  assert(o);

  mY.pop_front();
  o->eval1(this);
  return this;
}

XY* XY::eval() {
  while (mY.size() > 0) {
    eval1();
  }
  return this;
}

// XYNumber
XYNumber::XYNumber(int v) : mValue(v) { }

string XYNumber::toString() {
  ostringstream s;
  s << mValue;
  return s.str();
}

void XYNumber::eval1(XY* xy) {
  xy->mX.push_back(this);
}

// XYSymbol
XYSymbol::XYSymbol(string v) : mValue(v) { }

string XYSymbol::toString() {
  ostringstream s;
  s << mValue;
  return s.str();
}

void XYSymbol::eval1(XY* xy) {
  xy->mX.push_back(this);
}

// XYList
XYList::XYList() { }

template <class InputIterator>
XYList::XYList(InputIterator first, InputIterator last) {
  mList.assign(first, last);
}

string XYList::toString() {
  ostringstream s;
  s << "[ ";
  for(type::iterator it = mList.begin(); it != mList.end(); ++it) {
    s << (*it)->toString() << " ";
  }
  s << "]";
  return s.str();
}

void XYList::eval1(XY* xy) {
  xy->mX.push_back(this);
}


// XYPrimitive
XYPrimitive::XYPrimitive(string n) : mName(n) { }

string XYPrimitive::toString() {
  ostringstream s;
  s << mName;
  return s.str();
}

// XYAddition
XYAddition::XYAddition() : XYPrimitive("+") { }

void XYAddition::eval1(XY* xy) {
  assert(xy->mX.size() >= 2);
  XYNumber* rhs = dynamic_cast<XYNumber*>(xy->mX.back());
  assert(rhs);
  xy->mX.pop_back();

  XYNumber* lhs = dynamic_cast<XYNumber*>(xy->mX.back());
  assert(lhs);
  xy->mX.pop_back();

  xy->mX.push_back(new XYNumber(lhs->mValue + rhs->mValue));
}

// XYSet
XYSet::XYSet() : XYPrimitive("set") { }

void XYSet::eval1(XY* xy) {
  assert(xy->mX.size() >= 2);
  XYSymbol* name = dynamic_cast<XYSymbol*>(xy->mX.back());
  assert(name);
  xy->mX.pop_back();

  XYObject* value = xy->mX.back();
  xy->mX.pop_back();

  xy->mEnv[name->mValue] = value;
}

// XYGet
XYGet::XYGet() : XYPrimitive(";") { }

void XYGet::eval1(XY* xy) {
  assert(xy->mX.size() >= 1);
  XYSymbol* name = dynamic_cast<XYSymbol*>(xy->mX.back());
  assert(name);
  xy->mX.pop_back();

  XYEnv::iterator it = xy->mEnv.find(name->mValue);
  assert(it != xy->mEnv.end());

  xy->mX.push_back((*it).second);
}

// XYUnquote
XYUnquote::XYUnquote() : XYPrimitive("!") { }

void XYUnquote::eval1(XY* xy) {
  assert(xy->mX.size() >= 1);
  XYObject* o = xy->mX.back();
  xy->mX.pop_back();
  XYList* list = dynamic_cast<XYList*>(o);

  if (list) {
    xy->mY.insert(xy->mY.begin(), list->mList.begin(), list->mList.end());
  }
  else {
    xy->mY.push_front(o);
  }
}

enum XYState {
  XYSTATE_INIT,
  XYSTATE_STRING_START,
  XYSTATE_STRING_MIDDLE,
  XYSTATE_STRING_END,
  XYSTATE_NUMBER_START,
  XYSTATE_NUMBER_REST,
  XYSTATE_SYMBOL_START,
  XYSTATE_SYMBOL_REST,
  XYSTATE_LIST_START
};

// Returns true if the character is whitespace
bool is_whitespace(char ch) {
  return ch == '\r' || ch == '\t' || ch == '\n' || ch == ' ';
}

// Returns true if the character is one that indicates a symbol has ended
// in the token stream.
bool is_symbol_break(char ch) {
  switch(ch) {
    case '[':
    case ']':
    case '{':
    case '}':
    case '(':
    case ')':
    case ';':
    case '!':
    case '\'':
    case ',':
    case '`':
      return true;

    default:
      return is_whitespace(ch);
  }
}

// Returns true if the character is a numeric digit
bool is_numeric_digit(char ch) {
  return (ch >= '0' && ch <='9');
}

template <class InputIterator, class OutputIterator>
InputIterator parse(InputIterator first, InputIterator last, OutputIterator out) {
  XYState state = XYSTATE_INIT;
  string result;

  while (first != last) {
    cout << state << ": " << *first << endl;
    switch (state) {
      case XYSTATE_INIT: {
        result = "";
        char ch = *first;
        if (is_whitespace(ch))
          *first++;
        else if (ch == '[')
          state = XYSTATE_LIST_START;
        else if (ch == ']')
          return ++first;
        else if (is_symbol_break(ch)) {
          if (ch == '!') {
            ++first;
            *out++ = new XYUnquote();
          }
          else
            *out++ = new XYSymbol(string(1, *first++));
        }
        else if (is_numeric_digit(ch))
          state = XYSTATE_NUMBER_START;
        else if (ch == '\"') 
          state = XYSTATE_STRING_START;
        else
          state = XYSTATE_SYMBOL_START;
      }
      break;

      case XYSTATE_NUMBER_START:
      {
       char ch = *first++;
       result.push_back(ch);
       state = XYSTATE_NUMBER_REST;
      }
      break;

      case XYSTATE_NUMBER_REST:
      {
        char ch = *first;
        if (is_numeric_digit(ch)) {
          result.push_back(ch);
          ++first;
        }
        else {
          int number = 0;
          istringstream s(result);
          s >> number;
          cout << "Outing " << number << " from " << result << endl;
          *out++ = new XYNumber(number);
          state = XYSTATE_INIT;
        }
      }
      break;

      case XYSTATE_SYMBOL_START:
      {
        char ch = *first++;
        result.push_back(ch);
        state = XYSTATE_SYMBOL_REST;
      }
      break;

      case XYSTATE_SYMBOL_REST:
      {
        char ch = *first++;
        if (is_symbol_break(ch)) {
          cout << "Symbol(" << result << ")" << endl;
          *out++ = new XYSymbol(result);
          state = XYSTATE_INIT;
        }
        else {
          result.push_back(ch);
        }
      }
      break;

      case XYSTATE_LIST_START:
      {
        XYList* list = new XYList();
        cout << "Recursing for list" << endl;
        first = parse(++first, last, back_inserter(list->mList));
        cout << "Recursion for list ended" << endl;
        *out++ = list;
        state = XYSTATE_INIT;
      }
      break;

      default:
        assert(1 == 0);
        break;
    }
  }

  switch (state) {
    case XYSTATE_NUMBER_REST: {
     int number = 0;
     istringstream s(result);
     s >> number;
     cout << "Outing " << number << " from " << result << endl;
     *out++ = new XYNumber(number);
    }
    break;

    case XYSTATE_SYMBOL_REST:
    {
      *out++ = new XYSymbol(result);
    }
    break;

    case XYSTATE_INIT:
    {
      // Ignore
    }
    break;

    default:
      cout << "State: " << state << " result " << result << endl;
      assert(1 == 0);
      break;
  }

  return last;
}

void testXY() {
  XY* xy = new XY();
  XYObject* program2[] = {
    new XYNumber(100),
    new XYNumber(200),
    new XYAddition()
  };

  XYObject* program1[] = {
    new XYNumber(1),
    new XYNumber(2),
    new XYNumber(3),
    new XYAddition(),
    new XYAddition(),
    new XYNumber(42),
    new XYSymbol("a"),
    new XYSet(),
    new XYSymbol("a"),
    new XYGet(),
    new XYAddition(),
    new XYList(program2, program2+sizeof(program2)/sizeof(XYObject*)),
    new XYUnquote(),
    new XYUnquote()
  };

  xy->mY.insert(xy->mY.begin(), program1, program1 + (sizeof(program1)/sizeof(XYObject*)));
  string s("1 2 3 45 + a;! [1 2 3] !");
  parse(s.begin(), s.end(), back_inserter(xy->mY));
  while(xy->mY.size() > 0) {
    xy->print();
    xy->eval1();
  }

  xy->print();
  delete xy;
}

void runTests() {
  testXY();
}

int main() {
  runTests();

  return 0;
}

