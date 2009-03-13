#include <cassert>
#include <iostream>
#include <sstream>
#include <map>
#include <vector>
#include <deque>
#include <iterator>
#include <algorithm>
#include <string>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/lambda/lambda.hpp>
#include <boost/lambda/bind.hpp>
#include <boost/algorithm/string.hpp>
#include <gmpxx.h>

using namespace std;
using namespace boost;
using namespace boost::lambda;

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
    virtual string toString() const = 0;
};

// This lovely piece of code is to allow XY objects to be called within
// Boost lambda expressions, even though they are actually shared_ptr
// objects instead of XYObjects.
namespace boost {
  namespace lambda {
    template <class R> struct function_adaptor< R (XYObject::*)() const >
    {
      template <class T> struct sig { typedef R type; };
      template <class RET>
       static string apply(R (XYObject::*func)() const, shared_ptr<XYObject>const & o) {
          return (o.get()->*func)();
        }
    };
  }
}

// All numbers are represented by this object, or a class
// derived from it.
class XYNumber : public XYObject
{
  public:
    // Numbers are signed integers for now.
    mpz_class mValue;

  public:
    XYNumber(int v = 0);
    XYNumber(string v);
    XYNumber(mpz_class const& v);
    virtual string toString() const;
    virtual void eval1(XY* xy);
};

// A symbol is an unquoted string.
class XYSymbol : public XYObject
{
  public:
    string mValue;

  public:
    XYSymbol(string v);
    virtual string toString() const;
    virtual void eval1(XY* xy);
};

// A list of objects. Can include other nested
// lists. All items in the list are derived from
// XYObject. 
class XYList : public XYObject
{
  public:
    typedef vector< shared_ptr<XYObject> > List;
    typedef List::iterator iterator;
    typedef List::const_iterator const_iterator;

    List mList;

  public:
    XYList();
    template <class InputIterator> XYList(InputIterator first, InputIterator last);
    virtual string toString() const;
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
    virtual string toString() const;
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

// [X^{pattern} Y] [X^result Y]
class XYPatternStackToStack : public XYPrimitive
{
  public:
    XYPatternStackToStack();
    virtual void eval1(XY* xy);
};


// The environment maps names to objects
typedef map<string, shared_ptr<XYObject> > XYEnv;
typedef vector<shared_ptr<XYObject> > XYStack;
typedef deque<shared_ptr<XYObject> > XYQueue;

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
    XYStack mX;

    // The Queue
    XYQueue mY;

  public:
    // Print a representation of the state of the
    // interpter.
    void print();

    // Remove one item from the queue and evaluate it.
    void eval1();

    // Evaluate all items in the queue.
    void eval();

    // Perform a recursive match of pattern values to items
    // in the given stack.
    template <class OutputIterator>
    void match(OutputIterator out, 
               shared_ptr<XYObject> object,
               shared_ptr<XYObject> pattern,
               shared_ptr<XYList> sequence,
               XYList::iterator it);

    // Given a pattern list of symbols (which can contain
    // nested lists of symbols), store in the environment
    // a mapping of symbol name to value from the stack.
    // This operation destructures within lists on the stack.
    template <class OutputIterator>
    void getPatternValues(shared_ptr<XYObject> symbols, OutputIterator out);
};

// XY
void XY::print() {
  for_each(mX.begin(), mX.end(), cout << bind(&XYObject::toString, _1) << " ");
  cout << " -> ";
  for_each(mY.begin(), mY.end(), cout << bind(&XYObject::toString, _1) << " ");
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

template <class OutputIterator>
void XY::match(OutputIterator out, 
               shared_ptr<XYObject> object,
               shared_ptr<XYObject> pattern,
               shared_ptr<XYList> sequence,
               XYList::iterator it) {
  shared_ptr<XYList> object_list = dynamic_pointer_cast<XYList>(object);
  shared_ptr<XYList> pattern_list = dynamic_pointer_cast<XYList>(pattern);
  shared_ptr<XYSymbol> pattern_symbol = dynamic_pointer_cast<XYSymbol>(pattern);
  if (object_list && pattern_list) {
    for(XYList::iterator pit = pattern_list->mList.begin(),
                         oit = object_list->mList.begin(); 
        pit != pattern_list->mList.end(); 
        ++pit, ++oit) {
      match(out, (*oit), (*pit), object_list, oit);
    }
  }
  else if(pattern_symbol) {
    string uppercase = pattern_symbol->mValue;
    to_upper(uppercase);
    if (uppercase == pattern_symbol->mValue) {
      *out++ = make_pair(pattern_symbol->mValue, new XYList(it, sequence->mList.end()));
    }
    else
      *out++ = make_pair(pattern_symbol->mValue, object);
  }
}

template <class OutputIterator>
void XY::getPatternValues(shared_ptr<XYObject> pattern, OutputIterator out)
{
  shared_ptr<XYList> list = dynamic_pointer_cast<XYList>(pattern);
  if (list) {
    cout << mX.size() << " " << list->mList.size() << endl;
    assert(mX.size() >= list->mList.size());
    shared_ptr<XYList> stack(new XYList(mX.end() - list->mList.size(), mX.end()));
    match(out, stack, pattern, stack, stack->mList.begin());
    mX.resize(stack->mList.size());
  }
  else {
    shared_ptr<XYObject> o = mX.back();
    mX.pop_back();
    shared_ptr<XYList> list(new XYList());
    match(out, o, pattern, list, list->mList.end());
  }
}
                   
// XYNumber
XYNumber::XYNumber(int v) : mValue(v) { }
XYNumber::XYNumber(string v) : mValue(v) { }
XYNumber::XYNumber(mpz_class const& v) : mValue(v) { }

string XYNumber::toString() const {
  return lexical_cast<string>(mValue);
}

void XYNumber::eval1(XY* xy) {
  xy->mX.push_back(shared_from_this());
}

// XYSymbol
XYSymbol::XYSymbol(string v) : mValue(v) { }

string XYSymbol::toString() const {
  return mValue;
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

string XYList::toString() const {
  ostringstream s;
  s << "[ ";
  for_each(mList.begin(), mList.end(), s << bind(&XYObject::toString, _1) << " ");
  s << "]";
  return s.str();
}

void XYList::eval1(XY* xy) {
  xy->mX.push_back(shared_from_this());
}


// XYPrimitive
XYPrimitive::XYPrimitive(string n) : mName(n) { }

string XYPrimitive::toString() const {
  return mName;
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

// XYPatternStackToStack
XYPatternStackToStack::XYPatternStackToStack() : XYPrimitive(")") { }

void XYPatternStackToStack::eval1(XY* xy) {
  assert(xy->mX.size() >= 1);

  shared_ptr<XYList> pattern = dynamic_pointer_cast<XYList>(xy->mX.back());
  assert(pattern);
  xy->mX.pop_back();
  assert(pattern->mList.size() > 0);

  XYEnv env;
  xy->getPatternValues(*(pattern->mList.begin()), inserter(env, env.begin()));
  for(XYEnv::iterator it = env.begin(); it != env.end(); ++it) {
    string key = (*it).first;
    shared_ptr<XYObject> value = (*it).second;
    cout << "Key: " << key << " value: " << value->toString() << endl;
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
        else if (ch == ']') {
          cout << "List ended" << endl;
          return ++first;
        }
        else if (is_symbol_break(ch)) {
          if (ch == '!') {
            ++first;
            *out++ = shared_ptr<XYUnquote>(new XYUnquote());
          }
          else if (ch == ')' ) {
            ++first;
            *out++ = shared_ptr<XYPatternStackToStack>(new XYPatternStackToStack());
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
          *out++ = shared_ptr<XYNumber>(new XYNumber(result));
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
        char ch = *first;
        if (is_symbol_break(ch)) {
          cout << "Symbol(" << result << ")" << endl;
          if (result == ")")
            *out++ = shared_ptr<XYPatternStackToStack>(new XYPatternStackToStack());
          else
            *out++ = shared_ptr<XYSymbol>(new XYSymbol(result));
          state = XYSTATE_INIT;
        }
        else {
          first++;
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
     *out++ = shared_ptr<XYNumber>(new XYNumber(result));
    }
    break;

    case XYSTATE_SYMBOL_REST:
    {
      if (result == ")") {
        cout << "here" << endl;
        *out++ = shared_ptr<XYPatternStackToStack>(new XYPatternStackToStack());
      }
      else
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

  shared_ptr<XY> xy(new XY());
  string s("1 [ 2 3 ] [ [a [b c]] c b a]) 1");
  parse(s.begin(), s.end(), back_inserter(xy->mY));
  while(xy->mY.size() > 0) {
    xy->print();
    xy->eval1();
  }

  xy->print();

}

int main() {
  runTests();

  return 0;
}

