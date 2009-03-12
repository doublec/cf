#include <cassert>
#include <iostream>
#include <sstream>
#include <map>
#include <vector>
#include <deque>
#include <iterator>
#include <string>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>

using namespace std;
using namespace boost;

// XY is the object that contains the state of the running
// system. For example, the stack (X), the queue (Y) and
// the environment.
class XY;

// Base class for all objects in the XY system. Anything
// stored on the stack, in the queue, in the the environment
// must be derived from this.
//
// Memory allocation and freeing is handled via the shared_ptr
// class. This means all allocation via 'new' must be assigned
// to a shared_ptr.
class XYObject : public enable_shared_from_this<XYObject>
{
  public:
    // Ensure virtual destructors for base classes
    virtual ~XYObject() { }

    // Call when the object has been removed from the XY
    // queue and some action needs to be taken. For
    // literal objects (numbers, strings, etc) this 
    // involves pushing the object on the stack.
    virtual void eval1(XY* xy) = 0;

    // Convert the object into a string repesentation for
    // printing.
    virtual string toString() = 0;
};

// All numbers are represented by this object, or a class
// derived from it.
// TODO: Do something better than signed integers for numbers.
class XYNumber : public XYObject
{
  public:
    // Numbers are signed integers for now.
    int mValue;

  public:
    XYNumber(int v = 0);
    virtual string toString();
    virtual void eval1(XY* xy);
};

// A symbol is an unquoted string.
class XYSymbol : public XYObject
{
  public:
    string mValue;

  public:
    XYSymbol(string v);
    virtual string toString();
    virtual void eval1(XY* xy);
};

// A list of objects. Can include other nested
// lists. All items in the list are derived from
// XYObject. 
class XYList : public XYObject
{
  public:
    typedef vector< shared_ptr<XYObject> > List;
    typedef List::iterator    iterator;
    List mList;

  public:
    XYList();
    template <class InputIterator> XYList(InputIterator first, InputIterator last);
    virtual string toString();
    virtual void eval1(XY* xy);
};

// A primitive is the implementation of a core function.
// Primitives execute immediately when taken off the queue
// and do not need to have their value looked up.
class XYPrimitive : public XYObject
{
  public:
    string mName;

  public:
    XYPrimitive(string name);
    virtual string toString();
};

// [X^lhs^rhs] Y] -> [X^lhs+rhs Y]
class XYAddition : public XYPrimitive
{
  public:
    XYAddition();
    virtual void eval1(XY* xy);
};

// [X^value^name Y] -> [X Y] 
class XYSet : public XYPrimitive
{
  public:
    XYSet();
    virtual void eval1(XY* xy);
};

// [X^name Y] [X^value Y]
class XYGet : public XYPrimitive
{
  public:
    XYGet();
    virtual void eval1(XY* xy);
};

// [X^{O1..On} Y] [X O1^..^On^Y]
class XYUnquote : public XYPrimitive
{
  public:
    XYUnquote();
    virtual void eval1(XY* xy);
};

// The environment maps names to objects
typedef map<string, shared_ptr<XYObject> > XYEnv;

// The state of the runtime interpreter.
// Holds the environment, stack and queue
// and provides methods to step through or run
// the interpreter.
class XY {
  public:
    // Environment holding mappings of names
    // to objects.
    XYEnv mEnv;

    // The Stack
    vector<shared_ptr<XYObject> > mX;

    // The Queue
    deque<shared_ptr<XYObject> > mY;

  public:
    // Print a representation of the state of the
    // interpter.
    void print();

    // Remove one item from the queue and evaluate it.
    void eval1();

    // Evaluate all items in the queue.
    void eval();
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

void XY::eval1() {
  assert(mY.size() > 0);

  shared_ptr<XYObject> o = mY.front();
  assert(o);

  mY.pop_front();
  o->eval1(this);
}

void XY::eval() {
  while (mY.size() > 0) {
    eval1();
  }
}

// XYNumber
XYNumber::XYNumber(int v) : mValue(v) { }

string XYNumber::toString() {
  ostringstream s;
  s << mValue;
  return s.str();
}

void XYNumber::eval1(XY* xy) {
  xy->mX.push_back(shared_from_this());
}

// XYSymbol
XYSymbol::XYSymbol(string v) : mValue(v) { }

string XYSymbol::toString() {
  ostringstream s;
  s << mValue;
  return s.str();
}

void XYSymbol::eval1(XY* xy) {
  xy->mX.push_back(shared_from_this());
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
  for(iterator it = mList.begin(); it != mList.end(); ++it) {
    s << (*it)->toString() << " ";
  }
  s << "]";
  return s.str();
}

void XYList::eval1(XY* xy) {
  xy->mX.push_back(shared_from_this());
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
  shared_ptr<XYNumber> rhs = dynamic_pointer_cast<XYNumber>(xy->mX.back());
  assert(rhs);
  xy->mX.pop_back();

  shared_ptr<XYNumber> lhs = dynamic_pointer_cast<XYNumber>(xy->mX.back());
  assert(lhs);
  xy->mX.pop_back();

  xy->mX.push_back(shared_ptr<XYNumber>(new XYNumber(lhs->mValue + rhs->mValue)));
}

// XYSet
XYSet::XYSet() : XYPrimitive("set") { }

void XYSet::eval1(XY* xy) {
  assert(xy->mX.size() >= 2);
  shared_ptr<XYSymbol> name = dynamic_pointer_cast<XYSymbol>(xy->mX.back());
  assert(name);
  xy->mX.pop_back();

  shared_ptr<XYObject> value = xy->mX.back();
  xy->mX.pop_back();

  xy->mEnv[name->mValue] = value;
}

// XYGet
XYGet::XYGet() : XYPrimitive(";") { }

void XYGet::eval1(XY* xy) {
  assert(xy->mX.size() >= 1);
  shared_ptr<XYSymbol> name = dynamic_pointer_cast<XYSymbol>(xy->mX.back());
  assert(name);
  xy->mX.pop_back();

  XYEnv::iterator it = xy->mEnv.find(name->mValue);
  assert(it != xy->mEnv.end());

  shared_ptr<XYObject> value = (*it).second;
  xy->mX.push_back(value);
}

// XYUnquote
XYUnquote::XYUnquote() : XYPrimitive("!") { }

void XYUnquote::eval1(XY* xy) {
  assert(xy->mX.size() >= 1);
  shared_ptr<XYObject> o = xy->mX.back();
  xy->mX.pop_back();
  shared_ptr<XYList> list = dynamic_pointer_cast<XYList>(o);

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
            *out++ = shared_ptr<XYUnquote>(new XYUnquote());
          }
          else
            *out++ = shared_ptr<XYSymbol>(new XYSymbol(string(1, *first++)));
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
          *out++ = shared_ptr<XYNumber>(new XYNumber(number));
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
          *out++ = shared_ptr<XYSymbol>(new XYSymbol(result));
          state = XYSTATE_INIT;
        }
        else {
          result.push_back(ch);
        }
      }
      break;

      case XYSTATE_LIST_START:
      {
        shared_ptr<XYList> list(new XYList());
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
     *out++ = shared_ptr<XYNumber>(new XYNumber(number));
    }
    break;

    case XYSTATE_SYMBOL_REST:
    {
      *out++ = shared_ptr<XYSymbol>(new XYSymbol(result));
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

template <class T> shared_ptr<T> msp(T* o)
{
  return shared_ptr<T>(o);
}

void testXY() {
  shared_ptr<XY> xy(new XY());
  shared_ptr<XYObject> program2[] = {
    msp(new XYNumber(100)),
    msp(new XYNumber(200)),
    msp(new XYAddition())
  };

  shared_ptr<XYObject> program1[] = {
    msp(new XYNumber(1)),
    msp(new XYNumber(2)),
    msp(new XYNumber(3)),
    msp(new XYAddition()),
    msp(new XYAddition()),
    msp( new XYNumber(42)),
    msp(new XYSymbol("a")),
    msp(new XYSet()),
    msp(new XYSymbol("a")),
    msp(new XYGet()),
    msp(new XYAddition()),
    msp(new XYList(program2, program2+sizeof(program2)/sizeof(shared_ptr<XYObject>))),
    msp(new XYUnquote()),
    msp(new XYUnquote())
  };

  xy->mY.insert(xy->mY.begin(), program1, program1 + (sizeof(program1)/sizeof(shared_ptr<XYObject>)));
  string s("1 2 3 45 + a;! [1 2 3] !");
  parse(s.begin(), s.end(), back_inserter(xy->mY));
  while(xy->mY.size() > 0) {
    xy->print();
    xy->eval1();
  }

  xy->print();
}

void runTests() {
  testXY();
}

int main() {
  runTests();

  return 0;
}

