#include <cassert>
#include <iostream>
#include <sstream>
#include <map>
#include <vector>
#include <deque>
#include <iterator>
#include <algorithm>
#include <string>
#include <functional>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/lambda/lambda.hpp>
#include <boost/lambda/bind.hpp>
#include <boost/algorithm/string.hpp>
#include <gmpxx.h>

// If defined, compiles as a test applicatation that tests
// that things are working.
#if defined(TEST)
#include <boost/test/minimal.hpp>
#endif

using namespace std;
using namespace boost;
using namespace boost::lambda;

// XY is the object that contains the state of the running
// system. For example, the stack (X), the queue (Y) and
// the environment.
class XY;

// Helper function for creating a shared pointer of a type.
// Replace: shared_ptr<T> x = new shared_ptr<T>(new T());
// With:    shared_ptr<T> x = msp(new T());   
template <class T> shared_ptr<T> msp(T* o)
{
  return shared_ptr<T>(o);
}

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
    void (*mFunc)(XY*);

  public:
    XYPrimitive(string name, void (*func)(XY*));
    virtual string toString() const;
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

    // Mapping of primitives to their primtive
    // object. These are symbols that are executed
    // implicitly and don't need their value looked up
    // by the user.
    XYEnv mP;

    // The Stack
    XYStack mX;

    // The Queue
    XYQueue mY;

  public:
    // Constructor installs any primitives into the
    // environment.
    XY();

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

    // Given a mapping of names to values in 'env', replaces symbols in 'object'
    // that have the name with the given value. Store the newly created list
    // in 'out'.
    template <class OutputIterator>
    void replacePattern(XYEnv const& env, shared_ptr<XYObject> object, OutputIterator out);
};

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
  XYEnv::iterator it = xy->mP.find(mValue);
  if (it != xy->mP.end())
    // Primitive symbol, execute immediately
    (*it).second->eval1(xy);
  else
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
XYPrimitive::XYPrimitive(string n, void (*func)(XY*)) : mName(n), mFunc(func) { }

string XYPrimitive::toString() const {
  return mName;
}

void XYPrimitive::eval1(XY* xy) {
  mFunc(xy);
}

// Primitive Implementations

// + [X^lhs^rhs] Y] -> [X^lhs+rhs Y]
static void primitive_addition(XY* xy) {
  assert(xy->mX.size() >= 2);
  shared_ptr<XYNumber> rhs = dynamic_pointer_cast<XYNumber>(xy->mX.back());
  assert(rhs);
  xy->mX.pop_back();

  shared_ptr<XYNumber> lhs = dynamic_pointer_cast<XYNumber>(xy->mX.back());
  assert(lhs);
  xy->mX.pop_back();

  xy->mX.push_back(shared_ptr<XYNumber>(new XYNumber(lhs->mValue + rhs->mValue)));
}

// + [X^lhs^rhs] Y] -> [X^lhs-rhs Y]
static void primitive_subtraction(XY* xy) {
  assert(xy->mX.size() >= 2);
  shared_ptr<XYNumber> rhs = dynamic_pointer_cast<XYNumber>(xy->mX.back());
  assert(rhs);
  xy->mX.pop_back();

  shared_ptr<XYNumber> lhs = dynamic_pointer_cast<XYNumber>(xy->mX.back());
  assert(lhs);
  xy->mX.pop_back();

  xy->mX.push_back(shared_ptr<XYNumber>(new XYNumber(lhs->mValue - rhs->mValue)));
}

// + [X^lhs^rhs] Y] -> [X^lhs*rhs Y]
static void primitive_multiplication(XY* xy) {
  assert(xy->mX.size() >= 2);
  shared_ptr<XYNumber> rhs = dynamic_pointer_cast<XYNumber>(xy->mX.back());
  assert(rhs);
  xy->mX.pop_back();

  shared_ptr<XYNumber> lhs = dynamic_pointer_cast<XYNumber>(xy->mX.back());
  assert(lhs);
  xy->mX.pop_back();

  xy->mX.push_back(shared_ptr<XYNumber>(new XYNumber(lhs->mValue * rhs->mValue)));
}

// + [X^lhs^rhs] Y] -> [X^lhs/rhs Y]
static void primitive_division(XY* xy) {
  assert(xy->mX.size() >= 2);
  shared_ptr<XYNumber> rhs = dynamic_pointer_cast<XYNumber>(xy->mX.back());
  assert(rhs);
  xy->mX.pop_back();

  shared_ptr<XYNumber> lhs = dynamic_pointer_cast<XYNumber>(xy->mX.back());
  assert(lhs);
  xy->mX.pop_back();

  xy->mX.push_back(shared_ptr<XYNumber>(new XYNumber(lhs->mValue / rhs->mValue)));
}

// + [X^lhs^rhs] Y] -> [X^lhs**rhs Y]
static void primitive_power(XY* xy) {
  assert(xy->mX.size() >= 2);
  shared_ptr<XYNumber> rhs = dynamic_pointer_cast<XYNumber>(xy->mX.back());
  assert(rhs);
  xy->mX.pop_back();

  shared_ptr<XYNumber> lhs = dynamic_pointer_cast<XYNumber>(xy->mX.back());
  assert(lhs);
  xy->mX.pop_back();

  shared_ptr<XYNumber> result(new XYNumber(mpz_class(lhs->mValue)));
  mpz_pow_ui(result->mValue.get_mpz_t(), lhs->mValue.get_mpz_t(), rhs->mValue.get_ui());
  xy->mX.push_back(result);
}

// set [X^value^name Y] -> [X Y] 
static void primitive_set(XY* xy) {
  assert(xy->mX.size() >= 2);
  shared_ptr<XYSymbol> name = dynamic_pointer_cast<XYSymbol>(xy->mX.back());
  assert(name);
  xy->mX.pop_back();

  shared_ptr<XYObject> value = xy->mX.back();
  xy->mX.pop_back();

  xy->mEnv[name->mValue] = value;
}

// get [X^name Y] [X^value Y]
static void primitive_get(XY* xy) {
  assert(xy->mX.size() >= 1);
  shared_ptr<XYSymbol> name = dynamic_pointer_cast<XYSymbol>(xy->mX.back());
  assert(name);
  xy->mX.pop_back();

  XYEnv::iterator it = xy->mEnv.find(name->mValue);
  assert(it != xy->mEnv.end());

  shared_ptr<XYObject> value = (*it).second;
  xy->mX.push_back(value);
}

// ! [X^{O1..On} Y] [X O1^..^On^Y]
static void primitive_unquote(XY* xy) {
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

// ) [X^{pattern} Y] [X^result Y]
static void primitive_pattern_ss(XY* xy) {
  assert(xy->mX.size() >= 1);

  // Get the pattern from the stack
  shared_ptr<XYList> pattern = dynamic_pointer_cast<XYList>(xy->mX.back());
  assert(pattern);
  xy->mX.pop_back();
  assert(pattern->mList.size() > 0);

  // Populate env with a mapping between the pattern variables to the
  // values on the stack.
  XYEnv env;
  xy->getPatternValues(*(pattern->mList.begin()), inserter(env, env.begin()));
  // Process pattern body using these mappings.
  if (pattern->mList.size() > 1) {
    XYList::iterator start = pattern->mList.begin();
    XYList::iterator end   = pattern->mList.end();
    shared_ptr<XYList> list(new XYList());
    xy->replacePattern(env, msp(new XYList(++start, end)), back_inserter(list->mList));
    assert(list->mList.size() > 0);
    xy->mX.push_back(list->mList[0]);
  }
}

// ( [X^{pattern} Y] [X result^Y]
static void primitive_pattern_sq(XY* xy) {
  assert(xy->mX.size() >= 1);

  // Get the pattern from the stack
  shared_ptr<XYList> pattern = dynamic_pointer_cast<XYList>(xy->mX.back());
  assert(pattern);
  xy->mX.pop_back();
  assert(pattern->mList.size() > 0);

  // Populate env with a mapping between the pattern variables to the
  // values on the stack.
  XYEnv env;
  xy->getPatternValues(*(pattern->mList.begin()), inserter(env, env.begin()));
  // Process pattern body using these mappings.
  if (pattern->mList.size() > 1) {
    XYList::iterator start = pattern->mList.begin();
    XYList::iterator end   = pattern->mList.end();
    shared_ptr<XYList> list(new XYList());
    xy->replacePattern(env, msp(new XYList(++start, end)), back_inserter(list->mList));
    assert(list->mList.size() > 0);

    // Prepend to queue
    list = dynamic_pointer_cast<XYList>(list->mList[0]);
    assert(list);
    xy->mY.insert(xy->mY.begin(), list->mList.begin(), list->mList.end());
  }
}

// XY
XY::XY() {
  mP["+"]   = msp(new XYPrimitive("+", primitive_addition));
  mP["-"]   = msp(new XYPrimitive("-", primitive_subtraction));
  mP["*"]   = msp(new XYPrimitive("*", primitive_multiplication));
  mP["%"]   = msp(new XYPrimitive("%", primitive_division));
  mP["^"]   = msp(new XYPrimitive("^", primitive_power));
  mP["set"] = msp(new XYPrimitive("set", primitive_set));
  mP[";"]   = msp(new XYPrimitive(";", primitive_get));
  mP["!"]   = msp(new XYPrimitive("!", primitive_unquote));
  mP[")"]   = msp(new XYPrimitive(")", primitive_pattern_ss));
  mP["("]   = msp(new XYPrimitive("(", primitive_pattern_sq));
}

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
void XY::getPatternValues(shared_ptr<XYObject> pattern, OutputIterator out) {
  shared_ptr<XYList> list = dynamic_pointer_cast<XYList>(pattern);
  if (list) {
    assert(mX.size() >= list->mList.size());
    shared_ptr<XYList> stack(new XYList(mX.end() - list->mList.size(), mX.end()));
    match(out, stack, pattern, stack, stack->mList.begin());
    mX.resize(mX.size() - list->mList.size());
  }
  else {
    shared_ptr<XYObject> o = mX.back();
    mX.pop_back();
    shared_ptr<XYList> list(new XYList());
    match(out, o, pattern, list, list->mList.end());
  }
}
 
template <class OutputIterator>
void XY::replacePattern(XYEnv const& env, shared_ptr<XYObject> object, OutputIterator out) {
  shared_ptr<XYList>   list   = dynamic_pointer_cast<XYList>(object);
  shared_ptr<XYSymbol> symbol = dynamic_pointer_cast<XYSymbol>(object); 
  if (list) {
    // Recurse through the list replacing variables as needed
    shared_ptr<XYList> new_list(new XYList());
    for(XYList::iterator it = list->mList.begin(); it != list->mList.end(); ++it) 
      replacePattern(env, *it, back_inserter(new_list->mList));
    *out++ = new_list;
  }
  else if (symbol) {
    XYEnv::const_iterator it = env.find(symbol->mValue);
    if (it != env.end())
      *out++ = (*it).second;
    else
      *out++ = object;
  }
  else
    *out++ = object;
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

// Parse a sequence of characters storing the result using the
// given output iterator.
template <class InputIterator, class OutputIterator>
InputIterator parse(InputIterator first, InputIterator last, OutputIterator out) {
  XYState state = XYSTATE_INIT;
  string result;

  while (first != last) {
    switch (state) {
      case XYSTATE_INIT: {
        result = "";
        char ch = *first;
        if (is_whitespace(ch))
          *first++;
        else if (ch == '[')
          state = XYSTATE_LIST_START;
        else if (ch == ']') {
          return ++first;
        }
        else if (ch == '-') {
          // Leading sign for number or a symbol
          state = XYSTATE_NUMBER_START;
        }
        else if (is_symbol_break(ch)) {
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
        else if(is_symbol_break(ch)) {
          if (result == "-") {
            // '-' was a symbol, not a sign
            *out++ = msp(new XYSymbol("-"));
            state = XYSTATE_INIT;
          }
          else {
            *out++ = shared_ptr<XYNumber>(new XYNumber(result));
            state = XYSTATE_INIT;
          }
        }
        else {
          // Actually a symbol which is prefixed by a number
          state = XYSTATE_SYMBOL_REST;
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
        first = parse(++first, last, back_inserter(list->mList));
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
    case XYSTATE_NUMBER_REST: 
    {
      if (result == "-") 
        *out++ = msp(new XYSymbol("-"));
      else
        *out++ = shared_ptr<XYNumber>(new XYNumber(result));
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
      assert(1 == 0);
      break;
  }

  return last;
}

// Parse a string into XY objects, storing the result in the
// container pointer to by the output iterator.
template <class OutputIterator>
void parse(string s, OutputIterator out) {
  parse(s.begin(), s.end(), out);
}

#if !defined(TEST)
int main() {
  shared_ptr<XY> xy(new XY());
  while (1) {
    string input;
    xy->print();
    cout << "ok ";
    getline(cin, input);
    parse(input.begin(), input.end(), back_inserter(xy->mY));
    xy->eval(); 
  }


  return 0;
}
#else
void testParse() {
  {
    // Simple number parsing
    XYStack x;
    parse("1 20 300 -400", back_inserter(x));
    BOOST_CHECK(x.size() == 4);
    shared_ptr<XYNumber> n1(dynamic_pointer_cast<XYNumber>(x[0]));
    shared_ptr<XYNumber> n2(dynamic_pointer_cast<XYNumber>(x[1]));
    shared_ptr<XYNumber> n3(dynamic_pointer_cast<XYNumber>(x[2]));
    shared_ptr<XYNumber> n4(dynamic_pointer_cast<XYNumber>(x[3]));

    BOOST_CHECK(n1 && n1->mValue == 1);
    BOOST_CHECK(n2 && n2->mValue == 20);
    BOOST_CHECK(n3 && n3->mValue == 300);
    BOOST_CHECK(n4 && n4->mValue == -400);
  }

  {
    // Simple Symbol parsing
    XYStack x;
    parse("a abc 2ab ab2 ab34cd", back_inserter(x));
    BOOST_CHECK(x.size() == 5);
    shared_ptr<XYSymbol> s1(dynamic_pointer_cast<XYSymbol>(x[0]));
    shared_ptr<XYSymbol> s2(dynamic_pointer_cast<XYSymbol>(x[1]));
    shared_ptr<XYSymbol> s3(dynamic_pointer_cast<XYSymbol>(x[2]));
    shared_ptr<XYSymbol> s4(dynamic_pointer_cast<XYSymbol>(x[3]));
    shared_ptr<XYSymbol> s5(dynamic_pointer_cast<XYSymbol>(x[4]));

    BOOST_CHECK(s1 && s1->mValue == "a");
    BOOST_CHECK(s2 && s2->mValue == "abc");
    BOOST_CHECK(s3 && s3->mValue == "2ab");
    BOOST_CHECK(s4 && s4->mValue == "ab2");
    BOOST_CHECK(s5 && s5->mValue == "ab34cd");
  }

  {
    // Simple List parsing
    XYStack x;
    parse("[ 1 2 [ 3 4 ] [ 5 6 [ 7 ] ] ]", back_inserter(x));
    BOOST_CHECK(x.size() == 1);

    shared_ptr<XYList> l1(dynamic_pointer_cast<XYList>(x[0]));
    BOOST_CHECK(l1 && l1->mList.size() == 4);
    
    shared_ptr<XYList> l2(dynamic_pointer_cast<XYList>(l1->mList[2]));
    BOOST_CHECK(l2 && l2->mList.size() == 2);

    shared_ptr<XYList> l3(dynamic_pointer_cast<XYList>(l1->mList[3]));
    BOOST_CHECK(l3 && l3->mList.size() == 3);

    shared_ptr<XYList> l4(dynamic_pointer_cast<XYList>(l3->mList[2]));
    BOOST_CHECK(l4 && l4->mList.size() == 1);
  }

  {
    // Simple List parsing 2
    XYStack x;
    parse("[1 2[3 4] [5 6[7]]]", back_inserter(x));
    BOOST_CHECK(x.size() == 1);

    shared_ptr<XYList> l1(dynamic_pointer_cast<XYList>(x[0]));
    BOOST_CHECK(l1 && l1->mList.size() == 4);
    
    shared_ptr<XYList> l2(dynamic_pointer_cast<XYList>(l1->mList[2]));
    BOOST_CHECK(l2 && l2->mList.size() == 2);

    shared_ptr<XYList> l3(dynamic_pointer_cast<XYList>(l1->mList[3]));
    BOOST_CHECK(l3 && l3->mList.size() == 3);

    shared_ptr<XYList> l4(dynamic_pointer_cast<XYList>(l3->mList[2]));
    BOOST_CHECK(l4 && l4->mList.size() == 1);
  }

  {
    // Addition
    shared_ptr<XY> xy(new XY());
    parse("1 2 +", back_inserter(xy->mY));
    BOOST_CHECK(xy->mY.size() == 3);

    while(xy->mY.size() > 0) {
      xy->eval1();
    }
    shared_ptr<XYNumber> n1(dynamic_pointer_cast<XYNumber>(xy->mX[0]));

    BOOST_CHECK(n1 && n1->mValue == 3);
  }

  {
    // Set/Get
    shared_ptr<XY> xy(new XY());
    parse("[5 +] add5 set", back_inserter(xy->mY));
    BOOST_CHECK(xy->mY.size() == 3);

    while(xy->mY.size() > 0) {
      xy->eval1();
    }

    XYEnv::iterator it = xy->mEnv.find("add5");
    BOOST_CHECK(it != xy->mEnv.end());
    shared_ptr<XYList> o1(dynamic_pointer_cast<XYList>((*it).second));
    BOOST_CHECK(o1 && o1->mList.size() == 2);

    parse("2 add5;!", back_inserter(xy->mY));
    while(xy->mY.size() > 0) {
      xy->eval1();
    }
    
    BOOST_CHECK(xy->mX.size() == 1);
    shared_ptr<XYNumber> o2(dynamic_pointer_cast<XYNumber>(xy->mX.back()));
    BOOST_CHECK(o2 && o2->mValue == 7);
  }

  {
    // Pattern deconstruction
    shared_ptr<XY> xy(new XY());
    parse("1 2 3 [[a b c] c b a]", back_inserter(xy->mX));
    BOOST_CHECK(xy->mX.size() == 4);

    shared_ptr<XYList> pattern = dynamic_pointer_cast<XYList>(xy->mX.back());
    BOOST_CHECK(pattern && pattern->mList.size() == 4);
    xy->mX.pop_back();

    XYEnv env;
    xy->getPatternValues(*(pattern->mList.begin()), inserter(env, env.begin()));
    BOOST_CHECK(env.size() == 3);
    BOOST_CHECK(env["a"]->toString() == "1");
    BOOST_CHECK(env["b"]->toString() == "2");
    BOOST_CHECK(env["c"]->toString() == "3");
  }

  {
    // Pattern deconstruction 2
    shared_ptr<XY> xy(new XY());
    parse("1 [2 [3]] [[a [b [c]]] c b a]", back_inserter(xy->mX));
    BOOST_CHECK(xy->mX.size() == 3);

    shared_ptr<XYList> pattern = dynamic_pointer_cast<XYList>(xy->mX.back());
    BOOST_CHECK(pattern && pattern->mList.size() == 4);
    xy->mX.pop_back();

    XYEnv env;
    xy->getPatternValues(*(pattern->mList.begin()), inserter(env, env.begin()));
    BOOST_CHECK(env.size() == 3);
    BOOST_CHECK(env["a"]->toString() == "1");
    BOOST_CHECK(env["b"]->toString() == "2");
    BOOST_CHECK(env["c"]->toString() == "3");
  }
  {
    // Pattern deconstruction 2
    shared_ptr<XY> xy(new XY());
    parse("foo [a a]", back_inserter(xy->mX));
    BOOST_CHECK(xy->mX.size() == 2);

    shared_ptr<XYList> pattern = dynamic_pointer_cast<XYList>(xy->mX.back());
    BOOST_CHECK(pattern && pattern->mList.size() == 2);
    xy->mX.pop_back();

    XYEnv env;
    xy->getPatternValues(*(pattern->mList.begin()), inserter(env, env.begin()));
    BOOST_CHECK(env.size() == 1);
    BOOST_CHECK(env["a"]->toString() == "foo");
  }
  {
    // Pattern replace 1
    XYEnv env;
    env["a"] = msp(new XYNumber(1));
    env["b"] = msp(new XYNumber(2));

    shared_ptr<XYList> list(new XYList());
    list->mList.push_back(msp(new XYNumber(42)));
    list->mList.push_back(msp(new XYSymbol("b")));
    list->mList.push_back(msp(new XYSymbol("a")));

    shared_ptr<XYList> out(new XYList());
    shared_ptr<XY> xy(new XY());
    xy->replacePattern(env, list, back_inserter(out->mList));
    BOOST_CHECK(out->mList.size() > 0);
    shared_ptr<XYList> result(dynamic_pointer_cast<XYList>(out->mList[0]));
    BOOST_CHECK(result);
    BOOST_CHECK(result->mList.size() == 3);
    BOOST_CHECK(result->mList[0]->toString() == "42");
    BOOST_CHECK(result->mList[1]->toString() == "2");
    BOOST_CHECK(result->mList[2]->toString() == "1");
  }
  {
    // Pattern replace 2
    XYEnv env;
    env["a"] = msp(new XYNumber(1));
    env["b"] = msp(new XYNumber(2));

    shared_ptr<XYList> list(new XYList());
    parse("[a [ b a ] a c]", back_inserter(list->mList));

    shared_ptr<XYList> out(new XYList());
    shared_ptr<XY> xy(new XY());
    xy->replacePattern(env, list, back_inserter(out->mList));
    BOOST_CHECK(out->mList.size() > 0);
    shared_ptr<XYList> result(dynamic_pointer_cast<XYList>(out->mList[0]));
    BOOST_CHECK(result);
    BOOST_CHECK(result->mList.size() == 1);
    BOOST_CHECK(result->toString() == "[ [ 1 [ 2 1 ] 1 c ] ]");
  }
  {
    // Pattern 1 - Stack to Stack
    shared_ptr<XY> xy(new XY());
    parse("1 2 [[a b] b a])", back_inserter(xy->mY));

    while(xy->mY.size() > 0) {
      xy->eval1();
    }

    BOOST_CHECK(xy->mX.size() == 1);
    shared_ptr<XYList> n1(dynamic_pointer_cast<XYList>(xy->mX.back()));

    BOOST_CHECK(n1->toString() == "[ 2 1 ]");
  }
  {
    // Pattern 2 - Stack to Stack
    shared_ptr<XY> xy(new XY());
    parse("1 2 [[a b] a b [ c [ b ] ]])", back_inserter(xy->mY));

    while(xy->mY.size() > 0) {
      xy->eval1();
    }

    BOOST_CHECK(xy->mX.size() == 1);
    shared_ptr<XYList> n1(dynamic_pointer_cast<XYList>(xy->mX.back()));

    BOOST_CHECK(n1->toString() == "[ 1 2 [ c [ 2 ] ] ]");
  }
  {
    // Pattern 3 - Stack to Queue
    shared_ptr<XY> xy(new XY());
    parse("[ [a a a +] ] double set 2 double;!( 0", back_inserter(xy->mY));

    while(xy->mY.size() > 0) {
      xy->eval1();
    }

    shared_ptr<XYList> n1(new XYList(xy->mX.begin(), xy->mX.end()));

    BOOST_CHECK(n1->toString() == "[ 4 0 ]");
  }

  {
    // Pattern 4 - Stack to Queue - with list deconstruction
    shared_ptr<XY> xy(new XY());
    parse("[1 2 3] [[[a A]] a A] (", back_inserter(xy->mY));

    while(xy->mY.size() > 0) {
      xy->eval1();
    }

    shared_ptr<XYList> n1(new XYList(xy->mX.begin(), xy->mX.end()));

    BOOST_CHECK(n1->toString() == "[ 1 [ 2 3 ] ]");
  }




}

int test_main(int argc, char* argv[]) {
  testParse();

  return 0;
}
#endif
