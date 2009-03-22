// Copyright (C) 2009 Chris Double. All Rights Reserved.
// See the license at the end of this file
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
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/gregorian/gregorian.hpp>
#include <gmpxx.h>

// If defined, compiles as a test applicatation that tests
// that things are working.
#if defined(TEST)
#include <boost/test/minimal.hpp>
#endif

using namespace std;
using namespace boost;
using namespace boost::lambda;

// Given an input string, unescape any special characters
string unescape(string s) {
  string r1 = replace_all_copy(s, "\\\"", "\"");
  string r2 = replace_all_copy(r1, "\\n", "\n");
  return r2;
}
 
// Given an input string, escape any special characters
string escape(string s) {
  string r1 = replace_all_copy(s, "\"", "\\\"");
  string r2 = replace_all_copy(r1, "\n", "\\n");
  return r2;
}
 
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
    // printing. if 'parse' is true then the output
    // should be able to be read back in by the cf parser.
    virtual string toString(bool parse) const = 0;

    // Return a negative number if this object is less than
    // the rhs object. Return 0 if they are equal. Returns
    // a positive number if it is greater.
    virtual int compare(shared_ptr<XYObject> rhs) const = 0;
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
    template <class R, class D> struct function_adaptor< R (XYObject::*)(D) const >
    {
      template <class T> struct sig { typedef R type; };
      template <class RET>
       static string apply(R (XYObject::*func)(D) const, shared_ptr<XYObject>const & o,D d) {
          return (o.get()->*func)(d);
        }
    };
  }
}

// Forward declare other number types to allow defining
// conversion functions in XYNumber.
class XYInteger;
class XYFloat;

// All number objects are derived from this class.
class XYNumber : public XYObject
{
  public:
    // Keep track of the type of the object to allow
    // switching in the math methods.
    enum Type {
      FLOAT,
      INTEGER
    } mType ;

  public:
    XYNumber(Type type);

    // Returns true if the number is zero
    virtual bool is_zero() const = 0;
    virtual unsigned int as_uint() const = 0;
    virtual shared_ptr<XYInteger> as_integer() = 0;
    virtual shared_ptr<XYFloat> as_float() = 0;

    // Math Operators
    virtual shared_ptr<XYNumber> add(shared_ptr<XYNumber> rhs) = 0;
    virtual shared_ptr<XYNumber> subtract(shared_ptr<XYNumber> rhs) = 0;
    virtual shared_ptr<XYNumber> multiply(shared_ptr<XYNumber> rhs) = 0;
    virtual shared_ptr<XYNumber> divide(shared_ptr<XYNumber> rhs) = 0;
    virtual shared_ptr<XYNumber> power(shared_ptr<XYNumber> rhs) = 0;
};

// Floating point numbers
class XYFloat : public XYNumber
{
  public:
    mpf_class mValue;

  public:
    XYFloat(long v = 0);
    XYFloat(string v);
    XYFloat(mpf_class const& v);
    virtual string toString(bool parse) const;
    virtual void eval1(XY* xy);
    virtual int compare(shared_ptr<XYObject> rhs) const;
    virtual bool is_zero() const;
    virtual unsigned int as_uint() const;
    virtual shared_ptr<XYInteger> as_integer();
    virtual shared_ptr<XYFloat> as_float();
    virtual shared_ptr<XYNumber> add(shared_ptr<XYNumber> rhs);
    virtual shared_ptr<XYNumber> subtract(shared_ptr<XYNumber> rhs);
    virtual shared_ptr<XYNumber> multiply(shared_ptr<XYNumber> rhs);
    virtual shared_ptr<XYNumber> divide(shared_ptr<XYNumber> rhs);
    virtual shared_ptr<XYNumber> power(shared_ptr<XYNumber> rhs);
};

// Integer numbers
class XYInteger : public XYNumber
{
  public:
    mpz_class mValue;

  public:
    XYInteger(long v = 0);
    XYInteger(string v);
    XYInteger(mpz_class const& v);
    virtual string toString(bool parse) const;
    virtual void eval1(XY* xy);
    virtual int compare(shared_ptr<XYObject> rhs) const;
    virtual bool is_zero() const;
    virtual unsigned int as_uint() const;
    virtual shared_ptr<XYInteger> as_integer();
    virtual shared_ptr<XYFloat> as_float();
    virtual shared_ptr<XYNumber> add(shared_ptr<XYNumber> rhs);
    virtual shared_ptr<XYNumber> subtract(shared_ptr<XYNumber> rhs);
    virtual shared_ptr<XYNumber> multiply(shared_ptr<XYNumber> rhs);
    virtual shared_ptr<XYNumber> divide(shared_ptr<XYNumber> rhs);
    virtual shared_ptr<XYNumber> power(shared_ptr<XYNumber> rhs);
};

// A symbol is an unquoted string.
class XYSymbol : public XYObject
{
  public:
    string mValue;

  public:
    XYSymbol(string v);
    virtual string toString(bool parse) const;
    virtual void eval1(XY* xy);
    virtual int compare(shared_ptr<XYObject> rhs) const;
};

// A string
class XYString : public XYObject
{
  public:
    string mValue;

  public:
    XYString(string v);
    virtual string toString(bool parse) const;
    virtual void eval1(XY* xy);
    virtual int compare(shared_ptr<XYObject> rhs) const;
};

// A shuffle symbol describes pattern to rearrange the stack.
class XYShuffle : public XYObject
{
  public:
    string mBefore;
    string mAfter;

  public:
    XYShuffle(string v);
    virtual string toString(bool parse) const;
    virtual void eval1(XY* xy);
    virtual int compare(shared_ptr<XYObject> rhs) const;
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
    virtual string toString(bool parse) const;
    virtual void eval1(XY* xy);
    virtual int compare(shared_ptr<XYObject> rhs) const;
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
    virtual string toString(bool parse) const;
    virtual void eval1(XY* xy);
    virtual int compare(shared_ptr<XYObject> rhs) const;
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
XYNumber::XYNumber(Type type) : mType(type) { }

// XYFloat
XYFloat::XYFloat(long v) : XYNumber(FLOAT), mValue(v) { }
XYFloat::XYFloat(string v) : XYNumber(FLOAT), mValue(v) { }
XYFloat::XYFloat(mpf_class const& v) : XYNumber(FLOAT), mValue(v) { }

string XYFloat::toString(bool) const {
  return lexical_cast<string>(mValue);
}

void XYFloat::eval1(XY* xy) {
  xy->mX.push_back(shared_from_this());
}

int XYFloat::compare(shared_ptr<XYObject> rhs) const {
  shared_ptr<XYFloat> o = dynamic_pointer_cast<XYFloat>(rhs);
  if (!o) {
    shared_ptr<XYInteger> i = dynamic_pointer_cast<XYInteger>(rhs);
    if (i)
      return cmp(mValue, i->mValue);
    else
      return toString(true).compare(rhs->toString(true));
  }

  return cmp(mValue, o->mValue);
}

bool XYFloat::is_zero() const {
  return mValue == 0;
}

unsigned int XYFloat::as_uint() const {
  return mValue.get_ui();
}

shared_ptr<XYInteger> XYFloat::as_integer() {
  return msp(new XYInteger(mValue));
}

shared_ptr<XYFloat> XYFloat::as_float() {
  return dynamic_pointer_cast<XYFloat>(shared_from_this());
}

shared_ptr<XYNumber> XYFloat::add(shared_ptr<XYNumber> rhs) {
  return msp(new XYFloat(mValue + rhs->as_float()->mValue));
}

shared_ptr<XYNumber> XYFloat::subtract(shared_ptr<XYNumber> rhs) {
  return msp(new XYFloat(mValue - rhs->as_float()->mValue));
}

shared_ptr<XYNumber> XYFloat::multiply(shared_ptr<XYNumber> rhs) {
  return msp(new XYFloat(mValue * rhs->as_float()->mValue));
}

shared_ptr<XYNumber> XYFloat::divide(shared_ptr<XYNumber> rhs) {
  return msp(new XYFloat(mValue / rhs->as_float()->mValue));
}

shared_ptr<XYNumber> XYFloat::power(shared_ptr<XYNumber> rhs) {
  shared_ptr<XYFloat> result(new XYFloat(mValue));
  mpf_pow_ui(result->mValue.get_mpf_t(), mValue.get_mpf_t(), rhs->as_uint());
  return result;
}

// XYInteger
XYInteger::XYInteger(long v) : XYNumber(INTEGER), mValue(v) { }
XYInteger::XYInteger(string v) : XYNumber(INTEGER), mValue(v) { }
XYInteger::XYInteger(mpz_class const& v) : XYNumber(INTEGER), mValue(v) { }

string XYInteger::toString(bool) const {
  return lexical_cast<string>(mValue);
}

void XYInteger::eval1(XY* xy) {
  xy->mX.push_back(shared_from_this());
}

int XYInteger::compare(shared_ptr<XYObject> rhs) const {
  shared_ptr<XYInteger> o = dynamic_pointer_cast<XYInteger>(rhs);
  if (!o) {
    shared_ptr<XYFloat> f = dynamic_pointer_cast<XYFloat>(rhs);
    if (f)
      return cmp(mValue, f->mValue);
    else
      return toString(true).compare(rhs->toString(true));
  }

  return cmp(mValue, o->mValue);
}

bool XYInteger::is_zero() const {
  return mValue == 0;
}

unsigned int XYInteger::as_uint() const {
  return mValue.get_ui();
}

shared_ptr<XYInteger> XYInteger::as_integer() {
  return dynamic_pointer_cast<XYInteger>(shared_from_this());
}

shared_ptr<XYFloat> XYInteger::as_float() {
  return msp(new XYFloat(mValue));
}

shared_ptr<XYNumber> XYInteger::add(shared_ptr<XYNumber> rhs) {
  switch(rhs->mType) {
    case INTEGER:
      return msp(new XYInteger(mValue + rhs->as_integer()->mValue));
    case FLOAT:
      return msp(new XYFloat(as_float()->mValue + rhs->as_float()->mValue));
    default:
      assert(1==0);
  }
}

shared_ptr<XYNumber> XYInteger::subtract(shared_ptr<XYNumber> rhs) {
  switch(rhs->mType) {
    case INTEGER:
      return msp(new XYInteger(mValue - rhs->as_integer()->mValue));
    case FLOAT:
      return msp(new XYFloat(as_float()->mValue - rhs->as_float()->mValue));
    default:
      assert(1==0);
  }
}

shared_ptr<XYNumber> XYInteger::multiply(shared_ptr<XYNumber> rhs) {
  switch(rhs->mType) {
    case INTEGER:
      return msp(new XYInteger(mValue * rhs->as_integer()->mValue));
    case FLOAT:
      return msp(new XYFloat(as_float()->mValue * rhs->as_float()->mValue));
    default:
      assert(1==0);
  }
}

shared_ptr<XYNumber> XYInteger::divide(shared_ptr<XYNumber> rhs) {
  switch(rhs->mType) {
    case INTEGER:
      return msp(new XYFloat(as_float()->mValue / rhs->as_float()->mValue));
    case FLOAT:
      return msp(new XYFloat(as_float()->mValue / rhs->as_float()->mValue));
    default:
      assert(1==0);
  }
}

shared_ptr<XYNumber> XYInteger::power(shared_ptr<XYNumber> rhs) {
  shared_ptr<XYInteger> result(new XYInteger(mValue));
  mpz_pow_ui(result->mValue.get_mpz_t(), mValue.get_mpz_t(), rhs->as_uint());
  return result;
}

// XYSymbol
XYSymbol::XYSymbol(string v) : mValue(v) { }

string XYSymbol::toString(bool) const {
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

int XYSymbol::compare(shared_ptr<XYObject> rhs) const {
  shared_ptr<XYSymbol> o = dynamic_pointer_cast<XYSymbol>(rhs);
  if (!o)
    return toString(true).compare(rhs->toString(true));

  return mValue.compare(o->mValue);
}

// XYString
XYString::XYString(string v) : mValue(v) { }

string XYString::toString(bool parse) const {
  if (parse) {
    ostringstream s;
    s << '\"' << escape(mValue) << '\"';
    return s.str();
  }
  
  return mValue;
}

void XYString::eval1(XY* xy) {
  xy->mX.push_back(shared_from_this());
}

int XYString::compare(shared_ptr<XYObject> rhs) const {
  shared_ptr<XYString> o = dynamic_pointer_cast<XYString>(rhs);
  if (!o)
    return toString(true).compare(rhs->toString(true));

  return mValue.compare(o->mValue);
}

// XYShuffle
XYShuffle::XYShuffle(string v) { 
  vector<string> result;
  split(result, v, is_any_of("-"));
  assert(result.size() == 2);
  mBefore = result[0];
  mAfter  = result[1];
}

string XYShuffle::toString(bool) const {
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

int XYShuffle::compare(shared_ptr<XYObject> rhs) const {
  shared_ptr<XYShuffle> o = dynamic_pointer_cast<XYShuffle>(rhs);
  if (!o)
    return toString(true).compare(rhs->toString(true));

  return (mBefore + mAfter).compare(o->mBefore + o->mAfter);
}


// XYList
XYList::XYList() { }

template <class InputIterator>
XYList::XYList(InputIterator first, InputIterator last) {
  mList.assign(first, last);
}

string XYList::toString(bool parse) const {
  ostringstream s;
  s << "[ ";
  for_each(mList.begin(), mList.end(), s << bind(&XYObject::toString, _1, parse) << " ");
  s << "]";
  return s.str();
}

void XYList::eval1(XY* xy) {
  xy->mX.push_back(shared_from_this());
}

int XYList::compare(shared_ptr<XYObject> rhs) const {
  shared_ptr<XYList> o = dynamic_pointer_cast<XYList>(rhs);
  if (!o)
    return toString(true).compare(rhs->toString(true));

  const_iterator lit = mList.begin(),
                 rit = o->mList.begin();

  for(;
      lit != mList.end();
      ++lit, ++rit) {
    int c = (*lit)->compare(*rit);
    if (c != 0)
      return c;
  }

  if(lit != mList.end())
    return -1;

  if(rit != o->mList.end())
    return 1;
  return 0;
}

// XYPrimitive
XYPrimitive::XYPrimitive(string n, void (*func)(XY*)) : mName(n), mFunc(func) { }

string XYPrimitive::toString(bool) const {
  return mName;
}

void XYPrimitive::eval1(XY* xy) {
  mFunc(xy);
}

int XYPrimitive::compare(shared_ptr<XYObject> rhs) const {
  shared_ptr<XYPrimitive> o = dynamic_pointer_cast<XYPrimitive>(rhs);
  if (!o)
    return toString(true).compare(rhs->toString(true));

  return mName.compare(o->mName);
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

  xy->mX.push_back(lhs->add(rhs));
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

  xy->mX.push_back(lhs->subtract(rhs));
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

  xy->mX.push_back(lhs->multiply(rhs));
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

  xy->mX.push_back(lhs->divide(rhs));
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

  xy->mX.push_back(lhs->power(rhs));
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

// = equals [X^a^b Y] [X^? Y] 
static void primitive_equals(XY* xy) {
  assert(xy->mX.size() >= 2);

  shared_ptr<XYObject> rhs = xy->mX.back();
  assert(rhs);
  xy->mX.pop_back();

  shared_ptr<XYObject> lhs = xy->mX.back();
  assert(lhs);
  xy->mX.pop_back();

  if (lhs->compare(rhs) == 0)
    xy->mX.push_back(msp(new XYInteger(1)));
  else
    xy->mX.push_back(msp(new XYInteger(0)));
}

// <  [X^a^b Y] [X^? Y] 
static void primitive_lessThan(XY* xy) {
  assert(xy->mX.size() >= 2);

  shared_ptr<XYObject> rhs = xy->mX.back();
  assert(rhs);
  xy->mX.pop_back();

  shared_ptr<XYObject> lhs = xy->mX.back();
  assert(lhs);
  xy->mX.pop_back();

  if (lhs->compare(rhs) < 0)
    xy->mX.push_back(msp(new XYInteger(1)));
  else
    xy->mX.push_back(msp(new XYInteger(0)));
}

// >  [X^a^b Y] [X^? Y] 
static void primitive_greaterThan(XY* xy) {
  assert(xy->mX.size() >= 2);

  shared_ptr<XYObject> rhs = xy->mX.back();
  assert(rhs);
  xy->mX.pop_back();

  shared_ptr<XYObject> lhs = xy->mX.back();
  assert(lhs);
  xy->mX.pop_back();

  if (lhs->compare(rhs) > 0)
    xy->mX.push_back(msp(new XYInteger(1)));
  else
    xy->mX.push_back(msp(new XYInteger(0)));
}

// <=  [X^a^b Y] [X^? Y] 
static void primitive_lessThanEqual(XY* xy) {
  assert(xy->mX.size() >= 2);

  shared_ptr<XYObject> rhs = xy->mX.back();
  assert(rhs);
  xy->mX.pop_back();

  shared_ptr<XYObject> lhs = xy->mX.back();
  assert(lhs);
  xy->mX.pop_back();

  if (lhs->compare(rhs) <= 0)
    xy->mX.push_back(msp(new XYInteger(1)));
  else
    xy->mX.push_back(msp(new XYInteger(0)));
}

// >=  [X^a^b Y] [X^? Y] 
static void primitive_greaterThanEqual(XY* xy) {
  assert(xy->mX.size() >= 2);

  shared_ptr<XYObject> rhs = xy->mX.back();
  assert(rhs);
  xy->mX.pop_back();

  shared_ptr<XYObject> lhs = xy->mX.back();
  assert(lhs);
  xy->mX.pop_back();

  if (lhs->compare(rhs) >= 0)
    xy->mX.push_back(msp(new XYInteger(1)));
  else
    xy->mX.push_back(msp(new XYInteger(0)));
}


// not not [X^a Y] [X^? Y] 
static void primitive_not(XY* xy) {
  assert(xy->mX.size() >= 1);

  shared_ptr<XYObject> o = xy->mX.back();
  assert(o);
  xy->mX.pop_back();

  shared_ptr<XYNumber> n = dynamic_pointer_cast<XYNumber>(o);
  if (n && n->is_zero()) {
    xy->mX.push_back(msp(new XYInteger(1)));
  }
  else {
    shared_ptr<XYList> l = dynamic_pointer_cast<XYList>(o);
    if(l && l->mList.size() == 0)
      xy->mX.push_back(msp(new XYInteger(1)));
    else
      xy->mX.push_back(msp(new XYInteger(0)));
  }
}

// nth nth [X^n^{...} Y] [X^o Y] 
static void primitive_nth(XY* xy) {
  assert(xy->mX.size() >= 2);

  shared_ptr<XYList> list = dynamic_pointer_cast<XYList>(xy->mX.back());
  assert(list);
  xy->mX.pop_back();

  shared_ptr<XYNumber> n = dynamic_pointer_cast<XYNumber>(xy->mX.back());
  assert(n);
  xy->mX.pop_back();

  if (n->as_uint() >= list->mList.size()) 
    xy->mX.push_back(msp(new XYInteger(list->mList.size())));
  else
    xy->mX.push_back(list->mList[n->as_uint()]);
}

// print [X^n Y] [X Y] 
static void primitive_print(XY* xy) {
  assert(xy->mX.size() >= 1);

  shared_ptr<XYObject> o(xy->mX.back());
  assert(o);
  xy->mX.pop_back();

  cout << o->toString(true);
}

// . printnl [X^n Y] [X Y] 
static void primitive_printnl(XY* xy) {
  assert(xy->mX.size() >= 1);

  shared_ptr<XYObject> o(xy->mX.back());
  assert(o);
  xy->mX.pop_back();

  cout << o->toString(true) << endl;
}


// write [X^n Y] [X Y] 
static void primitive_write(XY* xy) {
  assert(xy->mX.size() >= 1);

  shared_ptr<XYObject> o(xy->mX.back());
  assert(o);
  xy->mX.pop_back();

  cout << o->toString(false);
}

// count [X^{...} Y] [X^n Y] 
// Returns the length of any list. If the item at the top of the
// stack is an atom, returns 1.
static void primitive_count(XY* xy) {
  assert(xy->mX.size() >= 1);

  shared_ptr<XYObject> o(xy->mX.back());
  assert(o);
  xy->mX.pop_back();

  shared_ptr<XYList> list(dynamic_pointer_cast<XYList>(o));
  if (list)
    xy->mX.push_back(msp(new XYInteger(list->mList.size())));
  else {
    shared_ptr<XYString> s(dynamic_pointer_cast<XYString>(o));
    if (s)
      xy->mX.push_back(msp(new XYInteger(s->mValue.size())));
    else
      xy->mX.push_back(msp(new XYInteger(1)));
  }
}

// Forward declare tokenize function for tokenize primitive
template <class InputIterator, class OutputIterator>
void tokenize(InputIterator first, InputIterator last, OutputIterator out);

// tokenize [X^s Y] [X^{tokens} Y] 
// Given a string, returns a list of cf tokens
static void primitive_tokenize(XY* xy) {
  assert(xy->mX.size() >= 1);

  shared_ptr<XYString> s(dynamic_pointer_cast<XYString>(xy->mX.back()));
  assert(s);
  xy->mX.pop_back();

  vector<string> tokens;
  tokenize(s->mValue.begin(), s->mValue.end(), back_inserter(tokens));

  shared_ptr<XYList> result(new XYList());
  for(vector<string>::iterator it=tokens.begin(); it != tokens.end(); ++it)
    result->mList.push_back(msp(new XYString(*it)));

  xy->mX.push_back(result);
}

// Forward declare parse function for parse primitive
template <class InputIterator, class OutputIterator>
InputIterator parse(InputIterator first, InputIterator last, OutputIterator out);

// parse [X^{tokens} Y] [X^{...} Y] 
// Given a list of tokens, parses it and returns the program
static void primitive_parse(XY* xy) {
  assert(xy->mX.size() >= 1);

  shared_ptr<XYList> tokens(dynamic_pointer_cast<XYList>(xy->mX.back()));
  assert(tokens);
  xy->mX.pop_back();

  vector<string> strings;
  for(XYList::iterator it = tokens->mList.begin(); it!=tokens->mList.end(); ++it) {
    shared_ptr<XYString> s = dynamic_pointer_cast<XYString>(*it);
    assert(s);
    strings.push_back(s->mValue);
  }

  shared_ptr<XYList> result(new XYList());
  parse(strings.begin(), strings.end(), back_inserter(result->mList));
  xy->mX.push_back(result);
}

// getline [X Y] [X^".." Y] 
// Get a line of input from the user
static void primitive_getline(XY* xy) {
  assert(cin.good());

  string line;
  getline(cin, line);
  assert(cin.good());

  xy->mX.push_back(msp(new XYString(line)));
}

// millis [X Y] [X^m Y]
// Runs the number of milliseconds on the stack since
// 1 Janary 1970.
static void primitive_millis(XY* xy) {
  using namespace boost::posix_time;
  using namespace boost::gregorian;

  ptime e(microsec_clock::universal_time());
  ptime s(date(1970,1,1));

  time_duration d(e - s);

  xy->mX.push_back(msp(new XYInteger(d.total_milliseconds())));
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
  mP["="]   = msp(new XYPrimitive("=", primitive_equals));
  mP["<"]   = msp(new XYPrimitive("<", primitive_lessThan));
  mP["<="]  = msp(new XYPrimitive("<=", primitive_lessThanEqual));
  mP[">"]   = msp(new XYPrimitive(">", primitive_greaterThan));
  mP[">="]  = msp(new XYPrimitive(">=", primitive_greaterThanEqual));
  mP["not"] = msp(new XYPrimitive("not", primitive_not));
  mP["nth"] = msp(new XYPrimitive("nth", primitive_nth));
  mP["."]   = msp(new XYPrimitive(".", primitive_printnl));
  mP["print"] = msp(new XYPrimitive("print", primitive_print));
  mP["write"] = msp(new XYPrimitive("write", primitive_write));
  mP["count"] = msp(new XYPrimitive("count", primitive_count));
  mP["tokenize"] = msp(new XYPrimitive("tokenize", primitive_tokenize));
  mP["parse"] = msp(new XYPrimitive("parse", primitive_parse));
  mP["getline"] = msp(new XYPrimitive("getline", primitive_getline));
  mP["millis"] = msp(new XYPrimitive("millis", primitive_millis));

}

void XY::print() {
  for_each(mX.begin(), mX.end(), cout << bind(&XYObject::toString, _1, true) << " ");
  cout << " -> ";
  for_each(mY.begin(), mY.end(), cout << bind(&XYObject::toString, _1, true) << " ");
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
  else if(pattern_list && !object_list) {
    // If the pattern is a list, but the object is not, 
    // pretend the object is a one element list. This enables:
    // 42 [ [[a A]] a A ] -> 42 []
    shared_ptr<XYList> list(new XYList());
    list->mList.push_back(object);
    match(out, list, pattern, sequence, it);
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

  string before = trim_copy(result[0]);
  string after = trim_copy(result[1]);

  if(before.size() == 0 && after.size() == 0)
    return false;
 
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

// Return regex for tokenizing integers
boost::xpressive::sregex re_integer() {
  using namespace boost::xpressive;
  return optional('-') >> +_d;
}

// Return regex for tokenizing floats
boost::xpressive::sregex re_float() {
  using namespace boost::xpressive;
  return optional('-') >> +_d >> '.' >> *_d;
}

// Return regex for tokenizing numbers
boost::xpressive::sregex re_number() {
  using namespace boost::xpressive;
  return re_float() | re_integer();
}

// Return regex for tokenizing specials
boost::xpressive::sregex re_special() {
  using namespace boost::xpressive;
  using boost::xpressive::set;
  return (set= '\\' , '[' , ']' , '{' , '}' , '(' , ')' , ';' , '!' , ',' , '`' , '\'' , '|');
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

// Return regex for a character in a string
boost::xpressive::sregex re_stringchar() {
  using namespace boost::xpressive;
  using boost::xpressive::set;
  return ~(set= '\"','\\') | ('\\' >> _);
}

// Return regex for strings
boost::xpressive::sregex re_string() {
  using namespace boost::xpressive;
  return as_xpr('\"') >> *(re_stringchar()) >> '\"';
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

  // inline string regular expression. Without this I get a 'pure virtual function'
  // called inside the boost regular expression code when compiled with -O2 and -O3.
  sregex xy = re_comment() | (as_xpr('\"') >> *(re_stringchar()) >> '\"') | re_special() | re_symbol() | re_number();
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
      *out++ = msp(new XYString(unescape(token.substr(1, token.size()-2))));
    }
    else if(regex_match(token, re_float()))
      *out++ = msp(new XYFloat(token));
    else if(regex_match(token, re_integer())) {
      *out++ = msp(new XYInteger(token));
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

  while (cin.good()) {
    string input;
    xy->print();
    cout << "ok ";
    getline(cin, input);
    if (cin.good()) {
      parse(input, back_inserter(xy->mY));
      xy->eval(); 
    }
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
    shared_ptr<XYInteger> n1(dynamic_pointer_cast<XYInteger>(x[0]));
    shared_ptr<XYInteger> n2(dynamic_pointer_cast<XYInteger>(x[1]));
    shared_ptr<XYInteger> n3(dynamic_pointer_cast<XYInteger>(x[2]));
    shared_ptr<XYInteger> n4(dynamic_pointer_cast<XYInteger>(x[3]));

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
    shared_ptr<XYInteger> n1(dynamic_pointer_cast<XYInteger>(xy->mX[0]));

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
    shared_ptr<XYInteger> o2(dynamic_pointer_cast<XYInteger>(xy->mX.back()));
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
    BOOST_CHECK(env["a"]->toString(true) == "1");
    BOOST_CHECK(env["b"]->toString(true) == "2");
    BOOST_CHECK(env["c"]->toString(true) == "3");
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
    BOOST_CHECK(env["a"]->toString(true) == "1");
    BOOST_CHECK(env["b"]->toString(true) == "2");
    BOOST_CHECK(env["c"]->toString(true) == "3");
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
    BOOST_CHECK(env["a"]->toString(true) == "foo");
  }
  {
    // Pattern replace 1
    XYEnv env;
    env["a"] = msp(new XYInteger(1));
    env["b"] = msp(new XYInteger(2));

    shared_ptr<XYList> list(new XYList());
    list->mList.push_back(msp(new XYInteger(42)));
    list->mList.push_back(msp(new XYSymbol("b")));
    list->mList.push_back(msp(new XYSymbol("a")));

    shared_ptr<XYList> out(new XYList());
    shared_ptr<XY> xy(new XY());
    xy->replacePattern(env, list, back_inserter(out->mList));
    BOOST_CHECK(out->mList.size() > 0);
    shared_ptr<XYList> result(dynamic_pointer_cast<XYList>(out->mList[0]));
    BOOST_CHECK(result);
    BOOST_CHECK(result->mList.size() == 3);
    BOOST_CHECK(result->mList[0]->toString(true) == "42");
    BOOST_CHECK(result->mList[1]->toString(true) == "2");
    BOOST_CHECK(result->mList[2]->toString(true) == "1");
  }
  {
    // Pattern replace 2
    XYEnv env;
    env["a"] = msp(new XYInteger(1));
    env["b"] = msp(new XYInteger(2));

    shared_ptr<XYList> list(new XYList());
    parse("[a [ b a ] a c]", back_inserter(list->mList));

    shared_ptr<XYList> out(new XYList());
    shared_ptr<XY> xy(new XY());
    xy->replacePattern(env, list, back_inserter(out->mList));
    BOOST_CHECK(out->mList.size() > 0);
    shared_ptr<XYList> result(dynamic_pointer_cast<XYList>(out->mList[0]));
    BOOST_CHECK(result);
    BOOST_CHECK(result->mList.size() == 1);
    BOOST_CHECK(result->toString(true) == "[ [ 1 [ 2 1 ] 1 c ] ]");
  }
  {
    // Pattern 1 - Stack to Stack
    shared_ptr<XY> xy(new XY());
    parse("1 2 [[a b] b a])", back_inserter(xy->mY));

    while(xy->mY.size() > 0) {
      xy->eval1();
    }

    shared_ptr<XYList> n1(new XYList(xy->mX.begin(), xy->mX.end()));

    BOOST_CHECK(n1->toString(true) == "[ 2 1 ]");
  }
  {
    // Pattern 2 - Stack to Stack
    shared_ptr<XY> xy(new XY());
    parse("1 2 [[a b] a b [ c [ b ] ]])", back_inserter(xy->mY));

    while(xy->mY.size() > 0) {
      xy->eval1();
    }

    shared_ptr<XYList> n1(new XYList(xy->mX.begin(), xy->mX.end()));

    BOOST_CHECK(n1->toString(true) == "[ 1 2 [ c [ 2 ] ] ]");
  }
  {
    // Pattern 3 - Stack to Queue
    shared_ptr<XY> xy(new XY());
    parse("[ [a a a +] ] double set 2 double;!( 0", back_inserter(xy->mY));

    while(xy->mY.size() > 0) {
      xy->eval1();
    }

    shared_ptr<XYList> n1(new XYList(xy->mX.begin(), xy->mX.end()));

    BOOST_CHECK(n1->toString(true) == "[ 4 0 ]");
  }
  {
    // Pattern 4 - Stack to Queue - with list deconstruction
    shared_ptr<XY> xy(new XY());
    parse("[1 2 3] [[[a A]] a A] (", back_inserter(xy->mY));

    while(xy->mY.size() > 0) {
      xy->eval1();
    }

    shared_ptr<XYList> n1(new XYList(xy->mX.begin(), xy->mX.end()));

    BOOST_CHECK(n1->toString(true) == "[ 1 [ 2 3 ] ]");
  }
  {
    // Pattern 5 - Stack to Queue - with list to short
    shared_ptr<XY> xy(new XY());
    parse("[1] [[[a A]] a A] (", back_inserter(xy->mY));
    xy->eval();
    shared_ptr<XYList> n1(new XYList(xy->mX.begin(), xy->mX.end()));
    BOOST_CHECK(n1->toString(true) == "[ 1 [ ] ]");
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

    BOOST_CHECK(n1->toString(true) == "[ 1 ]");
  }
  {
    // Shuffle pattern test 3
    shared_ptr<XY> xy(new XY());
    parse("1 2 a-aa", back_inserter(xy->mY));

    while(xy->mY.size() > 0) {
      xy->eval1();
    }

    shared_ptr<XYList> n1(new XYList(xy->mX.begin(), xy->mX.end()));

    BOOST_CHECK(n1->toString(true) == "[ 1 2 2 ]");
  }
  {
    // Shuffle pattern test 4
    shared_ptr<XY> xy(new XY());
    parse("1 2 ab-aba", back_inserter(xy->mY));

    while(xy->mY.size() > 0) {
      xy->eval1();
    }

    shared_ptr<XYList> n1(new XYList(xy->mX.begin(), xy->mX.end()));

    BOOST_CHECK(n1->toString(true) == "[ 1 2 1 ]");
  }
  {
    // Shuffle pattern test 5
    shared_ptr<XY> xy(new XY());
    parse("1 2 foo-bar", back_inserter(xy->mY));

    while(xy->mY.size() > 0) {
      xy->eval1();
    }

    shared_ptr<XYList> n1(new XYList(xy->mX.begin(), xy->mX.end()));

    BOOST_CHECK(n1->toString(true) == "[ 1 2 foo-bar ]");
  }
  {
    // Dip test 1
    shared_ptr<XY> xy(new XY());
    parse("1 2 hello [ + ] ` 4", back_inserter(xy->mY));
    xy->eval();
    shared_ptr<XYList> n1(new XYList(xy->mX.begin(), xy->mX.end()));
    BOOST_CHECK(n1->toString(true) == "[ 3 hello 4 ]");
  }
  {
    // Join test 1
    shared_ptr<XY> xy(new XY());
    parse("1 2, [ 1 2 ] 2, 2 [1 2], [1 2] [3 4],", back_inserter(xy->mY));
    xy->eval();
    shared_ptr<XYList> n1(new XYList(xy->mX.begin(), xy->mX.end()));
    BOOST_CHECK(n1->toString(true) == "[ [ 1 2 ] [ 1 2 2 ] [ 2 1 2 ] [ 1 2 3 4 ] ]");
  }
  {
    // stackqueue test 1
    shared_ptr<XY> xy(new XY());
    parse("1 2 [4 5] [ 6 7 ] $$ 9 10 11", back_inserter(xy->mY));
    xy->eval();
    shared_ptr<XYList> n1(new XYList(xy->mX.begin(), xy->mX.end()));
    BOOST_CHECK(n1->toString(true) == "[ 4 5 6 7 ]");
  }
  {
    // stack test 1
    shared_ptr<XY> xy(new XY());
    parse("1 2 [ [3,]`12, ] $ 9 10 11", back_inserter(xy->mY));
    xy->eval();
    shared_ptr<XYList> n1(new XYList(xy->mX.begin(), xy->mX.end()));
    BOOST_CHECK(n1->toString(true) == "[ 1 2 3 9 10 11 12 ]");
  }
  {
    // count test 1
    shared_ptr<XY> xy(new XY());
    parse("[1 2 3] count [] count 1 count \"abc\" count \"\" count", back_inserter(xy->mY));
    xy->eval();
    shared_ptr<XYList> n1(new XYList(xy->mX.begin(), xy->mX.end()));
    BOOST_CHECK(n1->toString(true) == "[ 3 0 1 3 0 ]");
  }

}

int test_main(int argc, char* argv[]) {
  testParse();

  return 0;
}
#endif
// Copyright (C) 2009 Chris Double. All Rights Reserved.
// The original author of this code can be contacted at: chris.double@double.co.nz
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
// 
// 1. Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimer.
// 
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
// 
// THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
// INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
// FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
// DEVELOPERS AND CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
// OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
// OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
// ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
