#include <cassert>
#include <iostream>
#include <sstream>
#include <fstream>
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
#include <boost/xpressive/xpressive.hpp>
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

// The null value
class XYNull : public XYObject
{
  public:
    XYNull();
    virtual string toString() const;
    virtual void eval1(XY* xy);
};

// All numbers are represented by this object, or a class
// derived from it.
class XYNumber : public XYObject
{
  public:
    // Numbers are libgmp floats for now.
    mpf_class mValue;

  public:
    XYNumber(int v = 0);
    XYNumber(string v);
    XYNumber(mpf_class const& v);
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

// A string
class XYString : public XYObject
{
  public:
    string mValue;

  public:
    XYString(string v);
    virtual string toString() const;
    virtual void eval1(XY* xy);
};

// A shuffle symbol describes pattern to rearrange the stack.
class XYShuffle : public XYObject
{
  public:
    string mBefore;
    string mAfter;

  public:
    XYShuffle(string v);
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

// XYNull
XYNull::XYNull() {}

string XYNull::toString() const {
  return "null";
}

void XYNull::eval1(XY* xy) {
  xy->mX.push_back(shared_from_this());
}

// XYNumber
XYNumber::XYNumber(int v) : mValue(v) { }
XYNumber::XYNumber(string v) : mValue(v) { }
XYNumber::XYNumber(mpf_class const& v) : mValue(v) { }

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

// XYString
XYString::XYString(string v) : mValue(v) { }

string XYString::toString() const {
  ostringstream s;
  s << '\"' << mValue << '\"';
  return s.str();
}

void XYString::eval1(XY* xy) {
  xy->mX.push_back(shared_from_this());
}

// XYShuffle
XYShuffle::XYShuffle(string v) { 
  vector<string> result;
  split(result, v, is_any_of("-"));
  assert(result.size() == 2);
  mBefore = result[0];
  mAfter  = result[1];
}

string XYShuffle::toString() const {
  ostringstream out;
  out << mBefore << "-" << mAfter;
  return out.str();
}

void XYShuffle::eval1(XY* xy) {
  map<char, shared_ptr<XYObject> > env;
  for(string::reverse_iterator it = mBefore.rbegin(); it != mBefore.rend(); ++it) {
    env[*it] = xy->mX.back();
    xy->mX.pop_back();
  }

  for(string::iterator it = mAfter.begin(); it != mAfter.end(); ++it) {
    assert(env.find(*it) != env.end());
    xy->mX.push_back(env[*it]);
  }
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

  shared_ptr<XYNumber> result(new XYNumber(mpf_class(lhs->mValue)));
  mpf_pow_ui(result->mValue.get_mpf_t(), lhs->mValue.get_mpf_t(), rhs->mValue.get_ui());
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

    // Append to stack
    list = dynamic_pointer_cast<XYList>(list->mList[0]);
    assert(list);
    xy->mX.insert(xy->mX.end(), list->mList.begin(), list->mList.end());
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

// ` dip [X^b^{a0..an} Y] [X a0..an^b^Y]
static void primitive_dip(XY* xy) {
  assert(xy->mX.size() >= 2);
  shared_ptr<XYList> list = dynamic_pointer_cast<XYList>(xy->mX.back());
  assert(list);
  xy->mX.pop_back();

  shared_ptr<XYObject> o = xy->mX.back();
  assert(o);
  xy->mX.pop_back();

  xy->mY.push_front(o);
  xy->mY.insert(xy->mY.begin(), list->mList.begin(), list->mList.end());
}

// | reverse [X^{a0..an} Y] [X^{an..a0} Y]
static void primitive_reverse(XY* xy) {
  assert(xy->mX.size() >= 1);
  shared_ptr<XYList> list = dynamic_pointer_cast<XYList>(xy->mX.back());
  assert(list);
  xy->mX.pop_back();

  shared_ptr<XYList> reversed = msp(new XYList(list->mList.rbegin(), list->mList.rend()));
  xy->mX.push_back(reversed);
}

// \ quote [X^o Y] [X^{o} Y]
static void primitive_quote(XY* xy) {
  assert(xy->mY.size() >= 1);
  shared_ptr<XYObject> o = xy->mY.front();
  assert(o);
  xy->mY.pop_front();

  shared_ptr<XYList> list = msp(new XYList());
  list->mList.push_back(o);
  xy->mX.push_back(list);
}

// , join [X^a^b Y] [X^{...} Y]
static void primitive_join(XY* xy) {
  assert(xy->mX.size() >= 2);
  shared_ptr<XYObject> rhs = xy->mX.back();
  assert(rhs);
  xy->mX.pop_back();

  shared_ptr<XYObject> lhs = xy->mX.back();
  assert(lhs);
  xy->mX.pop_back();

  shared_ptr<XYList> list_lhs = dynamic_pointer_cast<XYList>(lhs);
  shared_ptr<XYList> list_rhs = dynamic_pointer_cast<XYList>(rhs);

  if (list_lhs && list_rhs) {
    // Two lists are concatenated
    shared_ptr<XYList> list(new XYList(list_lhs->mList.begin(), list_lhs->mList.end()));
    list->mList.insert(list->mList.end(), list_rhs->mList.begin(), list_rhs->mList.end()); 
    xy->mX.push_back(list);
  }
  else if(list_lhs) {
    // If rhs is not a list, then it is added to the end of the list
    shared_ptr<XYList> list(new XYList(list_lhs->mList.begin(), list_lhs->mList.end()));
    list->mList.push_back(rhs);
    xy->mX.push_back(list);
  }
  else if(list_rhs) {
    // If lhs is not a list, it is added to the front of the list
    shared_ptr<XYList> list(new XYList());
    list->mList.push_back(lhs);
    list->mList.insert(list->mList.end(), list_rhs->mList.begin(), list_rhs->mList.end());
    xy->mX.push_back(list);
  }
  else {
    // If neither are lists, a list is made containing the two items
    shared_ptr<XYList> list(new XYList());
    list->mList.push_back(lhs);
    list->mList.push_back(rhs);
    xy->mX.push_back(list);
  }
}

// $ stack - Expects a program on the X stack. That program
// has stack effect ( stack queue -- stack queue ). $ will
// call this program with X and Y on the stack, and replace
// X and Y with the results.
static void primitive_stack(XY* xy) {
  assert(xy->mX.size() >= 1);
  shared_ptr<XYList> list  = dynamic_pointer_cast<XYList>(xy->mX.back());
  assert(list);
  xy->mX.pop_back();

  shared_ptr<XYList> stack(new XYList(xy->mX.begin(), xy->mX.end()));
  shared_ptr<XYList> queue(new XYList(xy->mY.begin(), xy->mY.end()));

  xy->mX.push_back(stack);
  xy->mX.push_back(queue);
  xy->mY.push_front(msp(new XYSymbol("$$")));
  xy->mY.insert(xy->mY.begin(), list->mList.begin(), list->mList.end()); 
}

// $$ stackqueue - Helper word for '$'. Given a stack and queue on the
// stack, replaces the existing stack and queue with them.
static void primitive_stackqueue(XY* xy) {
  assert(xy->mX.size() >= 2);

  shared_ptr<XYList> queue = dynamic_pointer_cast<XYList>(xy->mX.back());
  assert(queue);
  xy->mX.pop_back();

  shared_ptr<XYList> stack = dynamic_pointer_cast<XYList>(xy->mX.back());
  assert(stack);
  xy->mX.pop_back();

  xy->mX.assign(stack->mList.begin(), stack->mList.end());
  xy->mY.assign(queue->mList.begin(), queue->mList.end());
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
  mP["'"]   = msp(new XYPrimitive("'", primitive_unquote));
  mP[")"]   = msp(new XYPrimitive(")", primitive_pattern_ss));
  mP["("]   = msp(new XYPrimitive("(", primitive_pattern_sq));
  mP["`"]   = msp(new XYPrimitive("`", primitive_dip));
  mP["|"]   = msp(new XYPrimitive("|", primitive_reverse));
  mP["\\"]  = msp(new XYPrimitive("\\", primitive_quote));
  mP[","]   = msp(new XYPrimitive(",", primitive_join));
  mP["$"]   = msp(new XYPrimitive("$", primitive_stack));
  mP["$$"]  = msp(new XYPrimitive("$$", primitive_stackqueue));
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
    XYList::iterator pit = pattern_list->mList.begin(),
                     oit = object_list->mList.begin(); 
    for(;
        pit != pattern_list->mList.end() && oit != object_list->mList.end(); 
        ++pit, ++oit) {
      match(out, (*oit), (*pit), object_list, oit);
    }
    // If there are more pattern items than there are list items,
    // set the pattern value to null.
    while(pit != pattern_list->mList.end()) {
      shared_ptr<XYSymbol> s = dynamic_pointer_cast<XYSymbol>(*pit);
      if (s) {
        *out++ = make_pair(s->mValue, msp(new XYList()));
      }
      ++pit;
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

// Returns true if the string is a shuffle pattern
bool is_shuffle_pattern(string s) {
  // A string is a shuffle pattern if it is of the form:
  //   abcd-dbca
  // No letters may be duplicated on the lhs.
  // The rhs must not contain letters that are not in the lhs.
  // The lhs may be empty but the rhs may not be.
  vector<string> result;
  split(result, s, is_any_of("-"));
  if (result.size() != 2)
    return false;

  string before = result[0];
  string after = result[1];
 
  sort(before.begin(), before.end());
  string::iterator bsend = before.end();
  string::iterator bnend = unique(before.begin(), before.end());
  if(bsend != bnend)
    return false; // Duplicates on the lhs of the pattern are invalid
  sort(after.begin(), after.end());
  string::iterator anend = unique(after.begin(), after.end());
  vector<char> diff;
  set_difference(after.begin(), anend, before.begin(), bnend, back_inserter(diff));
  return diff.size() == 0;
}

// Return regex for tokenizing numbers
boost::xpressive::sregex re_number() {
  using namespace boost::xpressive;
  return optional('-') >> +_d >> optional('.' >> *_d);
}

// Return regex for tokenizing specials
boost::xpressive::sregex re_special() {
  using namespace boost::xpressive;
  return as_xpr('\\') | '[' | ']' | '{' | '}' | '(' | ')' | ';' | '!' | ',' | '`' | '\'' | '|';
}

// Return regex for non-specials
boost::xpressive::sregex re_non_special() {
  using namespace boost::xpressive;
  using boost::xpressive::set;
  return ~(set[(set= '\\','[',']','{','}','(',')',';','!',',','`','\'','|') | _s]);
}

// Return regex for symbols
boost::xpressive::sregex re_symbol() {
  using namespace boost::xpressive;
  return !range('0', '9') >> +(re_non_special());
}

// Return regex for strings
boost::xpressive::sregex re_string() {
  using namespace boost::xpressive;
  return as_xpr('\"') >> *(~(as_xpr('\"'))) >> '\"';
}

// Return regex for comments
boost::xpressive::sregex re_comment() {
  using namespace boost::xpressive;
  return as_xpr('*') >> '*' >> -*_ >> '*' >> '*';
}

 

// Given a string, store a sequence of XY tokens using the 'out' iterator
// to put them in a container.
template <class InputIterator, class OutputIterator>
void tokenize(InputIterator first, InputIterator last, OutputIterator out)
{
  using namespace boost::xpressive;

  sregex xy = re_comment() | re_special() | re_string() | re_symbol() | re_number();
  sregex_token_iterator begin(first, last, xy), end;
  copy(begin, end, out);
}

// Parse a sequence of tokens storing the result using the
// given output iterator.
template <class InputIterator, class OutputIterator>
InputIterator parse(InputIterator first, InputIterator last, OutputIterator out) {
  using namespace boost::xpressive;

  while (first != last) {
    string token = *first++;
    smatch what;
    if (regex_match(token, what, re_comment())) {
      // Ignore comments
    }
    else if (regex_match(token, what, re_string())) {
      *out++ = msp(new XYString(token.substr(1, token.size()-2)));
    }
    else if(regex_match(token, re_number())) {
      *out++ = msp(new XYNumber(token));
    }
    else if(token == "[") {
      shared_ptr<XYList> list(new XYList());
      first = parse(first, last, back_inserter(list->mList));
      *out++ = list;
    }
    else if( token == "]") {
      return first;
    }
    else if(is_shuffle_pattern(token)) {
      *out++ = msp(new XYShuffle(token));
    }
    else {
      *out++ = msp(new XYSymbol(token));
    }
  }

  return first;
}
#if 0
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
        else if (is_numeric_digit(ch) || ch == '.')
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
        if (is_symbol_break(ch) && ch != '-') {
          if (is_shuffle_pattern(result))
            *out++ = shared_ptr<XYShuffle>(new XYShuffle(result));
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
        shared_ptr<XYList> list(new X00vYList());
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
      if (is_shuffle_pattern(result))
        *out++ = shared_ptr<XYShuffle>(new XYShuffle(result));
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
      assert(1 == 0);
      break;
  }

  return last;
}
#endif
// Parse a string into XY objects, storing the result in the
// container pointer to by the output iterator.
template <class OutputIterator>
void parse(string s, OutputIterator out) {
  vector<string> tokens;
  tokenize(s.begin(), s.end(), back_inserter(tokens));
  parse(tokens.begin(), tokens.end(), out);
}

#if !defined(TEST)
void eval_file(shared_ptr<XY> xy, char* filename) {
  cout << "Loading " << filename << endl;
  ifstream file(filename);
  ostringstream out;
  while (file.good()) {
    string line;
    getline(file, line);
    out << line << endl;
  }
  file.close();

  parse(out.str(), back_inserter(xy->mY));

  xy->eval();
}

template <class InputIterator>
void eval_files(shared_ptr<XY> xy, InputIterator first, InputIterator last) {
  for_each(first, last, bind(eval_file, xy, _1));
}

int main(int argc, char* argv[]) {
  shared_ptr<XY> xy(new XY());

  if (argc > 1) {
    // Load all files given on the command line in order
    eval_files(xy, argv + 1, argv + argc);
  }

  while (1) {
    string input;
    xy->print();
    cout << "ok ";
    getline(cin, input);
    parse(input, back_inserter(xy->mY));
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

    shared_ptr<XYList> n1(new XYList(xy->mX.begin(), xy->mX.end()));

    BOOST_CHECK(n1->toString() == "[ 2 1 ]");
  }
  {
    // Pattern 2 - Stack to Stack
    shared_ptr<XY> xy(new XY());
    parse("1 2 [[a b] a b [ c [ b ] ]])", back_inserter(xy->mY));

    while(xy->mY.size() > 0) {
      xy->eval1();
    }

    shared_ptr<XYList> n1(new XYList(xy->mX.begin(), xy->mX.end()));

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
  {
    // Pattern 5 - Stack to Queue - with list to short
    shared_ptr<XY> xy(new XY());
    parse("[1] [[[a A]] a A] (", back_inserter(xy->mY));
    xy->eval();
    shared_ptr<XYList> n1(new XYList(xy->mX.begin(), xy->mX.end()));
    BOOST_CHECK(n1->toString() == "[ 1 [ ] ]");
  }

  {
    // Shuffle pattern test 1
    BOOST_CHECK(is_shuffle_pattern("ab-ab"));
    BOOST_CHECK(!is_shuffle_pattern("ab-cd"));
    BOOST_CHECK(!is_shuffle_pattern("-cd"));
    BOOST_CHECK(is_shuffle_pattern("ab-"));
    BOOST_CHECK(is_shuffle_pattern("b-b"));
    BOOST_CHECK(!is_shuffle_pattern("abcd"));
    BOOST_CHECK(!is_shuffle_pattern("ab1-2cd"));
    BOOST_CHECK(!is_shuffle_pattern("aba-aba"));
    BOOST_CHECK(is_shuffle_pattern("a-aa"));
  }
  {
    // Shuffle pattern test 2
    shared_ptr<XY> xy(new XY());
    parse("1 2 a-", back_inserter(xy->mY));

    while(xy->mY.size() > 0) {
      xy->eval1();
    }

    shared_ptr<XYList> n1(new XYList(xy->mX.begin(), xy->mX.end()));

    BOOST_CHECK(n1->toString() == "[ 1 ]");
  }
  {
    // Shuffle pattern test 3
    shared_ptr<XY> xy(new XY());
    parse("1 2 a-aa", back_inserter(xy->mY));

    while(xy->mY.size() > 0) {
      xy->eval1();
    }

    shared_ptr<XYList> n1(new XYList(xy->mX.begin(), xy->mX.end()));

    BOOST_CHECK(n1->toString() == "[ 1 2 2 ]");
  }
  {
    // Shuffle pattern test 4
    shared_ptr<XY> xy(new XY());
    parse("1 2 ab-aba", back_inserter(xy->mY));

    while(xy->mY.size() > 0) {
      xy->eval1();
    }

    shared_ptr<XYList> n1(new XYList(xy->mX.begin(), xy->mX.end()));

    BOOST_CHECK(n1->toString() == "[ 1 2 1 ]");
  }
  {
    // Shuffle pattern test 5
    shared_ptr<XY> xy(new XY());
    parse("1 2 foo-bar", back_inserter(xy->mY));

    while(xy->mY.size() > 0) {
      xy->eval1();
    }

    shared_ptr<XYList> n1(new XYList(xy->mX.begin(), xy->mX.end()));

    BOOST_CHECK(n1->toString() == "[ 1 2 foo-bar ]");
  }
  {
    // Dip test 1
    shared_ptr<XY> xy(new XY());
    parse("1 2 hello [ + ] ` 4", back_inserter(xy->mY));
    xy->eval();
    shared_ptr<XYList> n1(new XYList(xy->mX.begin(), xy->mX.end()));
    BOOST_CHECK(n1->toString() == "[ 3 hello 4 ]");
  }
  {
    // Join test 1
    shared_ptr<XY> xy(new XY());
    parse("1 2, [ 1 2 ] 2, 2 [1 2], [1 2] [3 4],", back_inserter(xy->mY));
    xy->eval();
    shared_ptr<XYList> n1(new XYList(xy->mX.begin(), xy->mX.end()));
    BOOST_CHECK(n1->toString() == "[ [ 1 2 ] [ 1 2 2 ] [ 2 1 2 ] [ 1 2 3 4 ] ]");
  }
  {
    // stackqueue test 1
    shared_ptr<XY> xy(new XY());
    parse("1 2 [4 5] [ 6 7 ] $$ 9 10 11", back_inserter(xy->mY));
    xy->eval();
    shared_ptr<XYList> n1(new XYList(xy->mX.begin(), xy->mX.end()));
    BOOST_CHECK(n1->toString() == "[ 4 5 6 7 ]");
  }
  {
    // stack test 1
    shared_ptr<XY> xy(new XY());
    parse("1 2 [ [3,]`12, ] $ 9 10 11", back_inserter(xy->mY));
    xy->eval();
    shared_ptr<XYList> n1(new XYList(xy->mX.begin(), xy->mX.end()));
    BOOST_CHECK(n1->toString() == "[ 1 2 3 9 10 11 12 ]");
  }
}

int test_main(int argc, char* argv[]) {
  testParse();

  return 0;
}
#endif
