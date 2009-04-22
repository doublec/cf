// Copyright (C) 2009 Chris Double. All Rights Reserved.
// See the license at the end of this file
#include <cassert>
#include <cmath>
#include <iostream>
#include <sstream>
#include <fstream>
#include <iterator>
#include <algorithm>
#include <functional>
#include <boost/lexical_cast.hpp>
#include <boost/lambda/lambda.hpp>
#include <boost/lambda/bind.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/gregorian/gregorian.hpp>
#include "cf.h"

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

// Macro to implement double dispatch operations in class
#define DD_IMPL(class, name)						\
  shared_ptr<XYObject> class::name(XYObject* rhs) { return rhs->name(this); } \
  shared_ptr<XYObject> class::name(XYFloat* lhs) { return dd_##name(lhs, this); } \
  shared_ptr<XYObject> class::name(XYInteger* lhs) { return dd_##name(lhs, this); } \
  shared_ptr<XYObject> class::name(XYSequence* lhs) { return dd_##name(lhs, this); }

#define DD_IMPL2(name, op) \
static shared_ptr<XYObject> dd_##name(XYFloat* lhs, XYFloat* rhs) { \
  return msp(new XYFloat(lhs->mValue op rhs->mValue)); \
} \
\
static shared_ptr<XYObject> dd_##name(XYFloat* lhs, XYInteger* rhs) { \
  return msp(new XYFloat(lhs->mValue op rhs->as_float()->mValue)); \
} \
\
static shared_ptr<XYObject> dd_##name(XYFloat* lhs, XYSequence* rhs) { \
  shared_ptr<XYList> list(new XYList()); \
  for (XYSequence::iterator it = rhs->begin(); it != rhs->end(); ++it) \
    list->mList.push_back(lhs->name((*it).get())); \
  return list; \
}\
\
static shared_ptr<XYObject> dd_##name(XYInteger* lhs, XYFloat* rhs) { \
  return msp(new XYFloat(lhs->as_float()->mValue op rhs->mValue)); \
} \
\
static shared_ptr<XYObject> dd_##name(XYInteger* lhs, XYInteger* rhs) { \
  return msp(new XYInteger(lhs->mValue op rhs->mValue)); \
} \
\
static shared_ptr<XYObject> dd_##name(XYInteger* lhs, XYSequence* rhs) { \
  shared_ptr<XYList> list(new XYList()); \
  for (XYSequence::iterator it = rhs->begin(); it != rhs->end(); ++it) \
    list->mList.push_back(lhs->name((*it).get())); \
  return list; \
} \
\
static shared_ptr<XYObject> dd_##name(XYSequence* lhs, XYObject* rhs) { \
  shared_ptr<XYList> list(new XYList()); \
  for (XYSequence::iterator it = lhs->begin(); it != lhs->end(); ++it) \
    list->mList.push_back((*it)->name(rhs)); \
  return list; \
} \
\
static shared_ptr<XYObject> dd_##name(XYSequence* lhs, XYSequence* rhs) { \
  assert(lhs->size() == rhs->size()); \
  shared_ptr<XYList> list(new XYList()); \
  for (XYSequence::iterator lit = lhs->begin(), \
 	                          rit = rhs->begin(); \
       lit != lhs->end() && rit != rhs->end(); \
       ++lit, ++rit) \
    list->mList.push_back((*lit)->name((*rit).get()));\
  return list; \
}

DD_IMPL2(add, +)
DD_IMPL2(subtract, -)
DD_IMPL2(multiply, *)

static shared_ptr<XYObject> dd_divide(XYFloat* lhs, XYFloat* rhs) {
  return msp(new XYFloat(lhs->mValue / rhs->mValue));
}

static shared_ptr<XYObject> dd_divide(XYFloat* lhs, XYInteger* rhs) {
  return msp(new XYFloat(lhs->mValue / rhs->as_float()->mValue));
}

static shared_ptr<XYObject> dd_divide(XYFloat* lhs, XYSequence* rhs) { 
  shared_ptr<XYList> list(new XYList());
  for (XYSequence::iterator it = rhs->begin(); it != rhs->end(); ++it)
    list->mList.push_back(lhs->divide((*it).get()));
  return list;
}

static shared_ptr<XYObject> dd_divide(XYInteger* lhs, XYFloat* rhs) {
  return msp(new XYFloat(lhs->as_float()->mValue / rhs->mValue));
}

static shared_ptr<XYObject> dd_divide(XYInteger* lhs, XYInteger* rhs) {
  return msp(new XYFloat(lhs->as_float()->mValue / rhs->as_float()->mValue));
}

static shared_ptr<XYObject> dd_divide(XYInteger* lhs, XYSequence* rhs) {
  shared_ptr<XYList> list(new XYList());
  for (XYSequence::iterator it = rhs->begin(); it != rhs->end(); ++it)
    list->mList.push_back(lhs->divide((*it).get()));
  return list;
}

static shared_ptr<XYObject> dd_divide(XYSequence* lhs, XYObject* rhs) {
  shared_ptr<XYList> list(new XYList());
  for (XYSequence::iterator it = lhs->begin(); it != lhs->end(); ++it)
    list->mList.push_back((*it)->divide(rhs));
  return list;
}


static shared_ptr<XYObject> dd_divide(XYSequence* lhs, XYSequence* rhs) {
  assert(lhs->size() == rhs->size());
  shared_ptr<XYList> list(new XYList());
  for (XYSequence::iterator lit = lhs->begin(),
 	                          rit = rhs->begin();
       lit != lhs->end() && rit != rhs->end();
       ++lit, ++rit)
    list->mList.push_back((*lit)->divide((*rit).get()));
  return list; 
}

static shared_ptr<XYObject> dd_power(XYFloat* lhs, XYFloat* rhs) {
  return msp(new XYFloat(pow(static_cast<double>(lhs->mValue.get_d()), 
			     static_cast<double>(rhs->mValue.get_d()))));
}

static shared_ptr<XYObject> dd_power(XYFloat* lhs, XYInteger* rhs) {
  shared_ptr<XYFloat> result(new XYFloat(lhs->mValue));
  mpf_pow_ui(result->mValue.get_mpf_t(), lhs->mValue.get_mpf_t(), rhs->as_uint());
  return result;
}

static shared_ptr<XYObject> dd_power(XYFloat* lhs, XYSequence* rhs) {
  shared_ptr<XYList> list(new XYList());
  for (XYSequence::iterator it = rhs->begin(); it != rhs->end(); ++it)
    list->mList.push_back(lhs->power((*it).get()));
  return list;
}

static shared_ptr<XYObject> dd_power(XYInteger* lhs, XYFloat* rhs) {
  return msp(new XYFloat(pow(static_cast<double>(lhs->mValue.get_d()), 
			     static_cast<double>(rhs->mValue.get_d()))));
}

static shared_ptr<XYObject> dd_power(XYInteger* lhs, XYInteger* rhs) {
  shared_ptr<XYInteger> result(new XYInteger(lhs->mValue));
  mpz_pow_ui(result->mValue.get_mpz_t(), lhs->mValue.get_mpz_t(), rhs->as_uint());
  return result;
}

static shared_ptr<XYObject> dd_power(XYInteger* lhs, XYSequence* rhs) {
  shared_ptr<XYList> list(new XYList());
  for (XYSequence::iterator it = rhs->begin(); it != rhs->end(); ++it)
    list->mList.push_back(lhs->power((*it).get()));
  return list;
}

static shared_ptr<XYObject> dd_power(XYSequence* lhs, XYObject* rhs) {
  shared_ptr<XYList> list(new XYList());
  for (XYSequence::iterator it = lhs->begin(); it != lhs->end(); ++it)
    list->mList.push_back((*it)->power(rhs));
  return list;
}


static shared_ptr<XYObject> dd_power(XYSequence* lhs, XYSequence* rhs) {
  assert(lhs->size() == rhs->size());
  shared_ptr<XYList> list(new XYList());
  for (XYSequence::iterator lit = lhs->begin(),
 	                          rit = rhs->begin();
       lit != lhs->end() && rit != rhs->end();
       ++lit, ++rit)
    list->mList.push_back((*lit)->power((*rit).get()));
  return list;
}

// XYObject
shared_ptr<XYObject> XYObject::add(XYObject* rhs) {
  assert(1==0);
}

shared_ptr<XYObject> XYObject::add(XYFloat* rhs) {
  assert(1==0);
}

shared_ptr<XYObject> XYObject::add(XYInteger* rhs) {
  assert(1==0);
}

shared_ptr<XYObject> XYObject::add(XYSequence* rhs) {
  assert(1==0);
}

shared_ptr<XYObject> XYObject::subtract(XYObject* rhs) {
  assert(1==0);
}

shared_ptr<XYObject> XYObject::subtract(XYFloat* rhs) {
  assert(1==0);
}

shared_ptr<XYObject> XYObject::subtract(XYInteger* rhs) {
  assert(1==0);
}

shared_ptr<XYObject> XYObject::subtract(XYSequence* rhs) {
  assert(1==0);
}

shared_ptr<XYObject> XYObject::multiply(XYObject* rhs) {
  assert(1==0);
}

shared_ptr<XYObject> XYObject::multiply(XYFloat* rhs) {
  assert(1==0);
}

shared_ptr<XYObject> XYObject::multiply(XYInteger* rhs) {
  assert(1==0);
}

shared_ptr<XYObject> XYObject::multiply(XYSequence* rhs) {
  assert(1==0);
}

shared_ptr<XYObject> XYObject::divide(XYObject* rhs) {
  assert(1==0);
}

shared_ptr<XYObject> XYObject::divide(XYFloat* rhs) {
  assert(1==0);
}

shared_ptr<XYObject> XYObject::divide(XYInteger* rhs) {
  assert(1==0);
}

shared_ptr<XYObject> XYObject::divide(XYSequence* rhs) {
  assert(1==0);
}

shared_ptr<XYObject> XYObject::power(XYObject* rhs) {
  assert(1==0);
}

shared_ptr<XYObject> XYObject::power(XYFloat* rhs) {
  assert(1==0);
}

shared_ptr<XYObject> XYObject::power(XYInteger* rhs) {
  assert(1==0);
}

shared_ptr<XYObject> XYObject::power(XYSequence* rhs) {
  assert(1==0);
}

// XYNumber
XYNumber::XYNumber(Type type) : mType(type) { }

// XYFloat
DD_IMPL(XYFloat, add)
DD_IMPL(XYFloat, subtract)
DD_IMPL(XYFloat, multiply)
DD_IMPL(XYFloat, divide)
DD_IMPL(XYFloat, power)

XYFloat::XYFloat(long v) : XYNumber(FLOAT), mValue(v) { }
XYFloat::XYFloat(double v) : XYNumber(FLOAT), mValue(v) { }
XYFloat::XYFloat(string v) : XYNumber(FLOAT), mValue(v) { }
XYFloat::XYFloat(mpf_class const& v) : XYNumber(FLOAT), mValue(v) { }

string XYFloat::toString(bool) const {
  return lexical_cast<string>(mValue);
}

void XYFloat::eval1(XY* xy) {
  xy->mX.push_back(shared_from_this());
}

int XYFloat::compare(shared_ptr<XYObject> rhs) {
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

shared_ptr<XYNumber> XYFloat::floor() {
  shared_ptr<XYFloat> result(new XYFloat(::floor(mValue)));
  return result;
}

// XYInteger
DD_IMPL(XYInteger, add)
DD_IMPL(XYInteger, subtract)
DD_IMPL(XYInteger, multiply)
DD_IMPL(XYInteger, divide)
DD_IMPL(XYInteger, power)
XYInteger::XYInteger(long v) : XYNumber(INTEGER), mValue(v) { }
XYInteger::XYInteger(string v) : XYNumber(INTEGER), mValue(v) { }
XYInteger::XYInteger(mpz_class const& v) : XYNumber(INTEGER), mValue(v) { }

string XYInteger::toString(bool) const {
  return lexical_cast<string>(mValue);
}

void XYInteger::eval1(XY* xy) {
  xy->mX.push_back(shared_from_this());
}

int XYInteger::compare(shared_ptr<XYObject> rhs) {
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

shared_ptr<XYNumber> XYInteger::floor() {
  return dynamic_pointer_cast<XYNumber>(shared_from_this());
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

int XYSymbol::compare(shared_ptr<XYObject> rhs) {
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

int XYString::compare(shared_ptr<XYObject> rhs) {
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
  assert(xy->mX.size() >= mBefore.size());
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

int XYShuffle::compare(shared_ptr<XYObject> rhs) {
  shared_ptr<XYShuffle> o = dynamic_pointer_cast<XYShuffle>(rhs);
  if (!o)
    return toString(true).compare(rhs->toString(true));

  return (mBefore + mAfter).compare(o->mBefore + o->mAfter);
}

// XYSequence
DD_IMPL(XYSequence, add)
DD_IMPL(XYSequence, subtract)
DD_IMPL(XYSequence, multiply)
DD_IMPL(XYSequence, divide)
DD_IMPL(XYSequence, power)
int XYSequence::compare(shared_ptr<XYObject> rhs) {
  shared_ptr<XYSequence> o = dynamic_pointer_cast<XYSequence>(rhs);
  if (!o)
    return toString(true).compare(rhs->toString(true));

  XYSequence::const_iterator lit = begin(),
                             rit = o->begin();

  for(;
      lit != end() && rit != o->end();
      ++lit, ++rit) {
    int c = (*lit)->compare(*rit);
    if (c != 0)
      return c;
  }

  if(lit != end())
    return -1;

  if(rit != o->end())
    return 1;

  return 0;
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

XYSequence::iterator XYList::begin()
{
  return mList.begin();
}

XYSequence::iterator XYList::end()
{
  return mList.end();
}

size_t XYList::size()
{
  return mList.size();
}

shared_ptr<XYObject> XYList::at(size_t n)
{
  return mList[n];
}

shared_ptr<XYObject> XYList::head()
{
  assert(mList.size() > 0);
  return mList[0];
}

shared_ptr<XYSequence> XYList::tail()
{
  if (mList.size() <= 1) 
    return msp(new XYList());

  iterator start = mList.begin();
  return msp(new XYSlice(shared_from_this(), ++start, mList.end()));
}

boost::shared_ptr<XYSequence> XYList::join(boost::shared_ptr<XYSequence> const& rhs)
{
  shared_ptr<XYSequence> self(dynamic_pointer_cast<XYSequence>(shared_from_this()));

  if (dynamic_cast<XYJoin const*>(rhs.get())) {
    // If the reference to rhs is unique then we can modify the object itself.
    if (rhs.unique()) {    
      shared_ptr<XYJoin> result(dynamic_pointer_cast<XYJoin>(rhs));
      result->mSequences.push_front(self);
      return result;
    }
    else {
      // Pointer is shared, we have to copy the data
      shared_ptr<XYJoin> join_rhs(dynamic_pointer_cast<XYJoin>(rhs));
      shared_ptr<XYJoin> result(new XYJoin());
      result->mSequences.push_back(self);
      result->mSequences.insert(result->mSequences.end(), 
		  	        join_rhs->mSequences.begin(), join_rhs->mSequences.end());
      return result;
    }
  }

  return msp(new XYJoin(self, rhs));
}

// XYSlice
XYSlice::XYSlice(shared_ptr<XYObject> original,
                 XYSequence::iterator begin,
		 XYSequence::iterator end)  :
  mOriginal(original),
  mBegin(begin),
  mEnd(end)
{ 
  while (dynamic_cast<XYSlice*>(mOriginal.get())) {
    // Find the original, non-slice sequence. Without this we can
    // corrupt the C stack due to too much recursion when destroying
    // the tree of slices.
    mOriginal = dynamic_cast<XYSlice*>(mOriginal.get())->mOriginal;
  }
}

string XYSlice::toString(bool parse) const {
  ostringstream s;
  s << "[ ";
  for_each(mBegin, mEnd, s << bind(&XYObject::toString, _1, parse) << " ");
  s << "]";
  return s.str();
}

void XYSlice::eval1(XY* xy) {
  xy->mX.push_back(shared_from_this());
}

XYSequence::iterator XYSlice::begin()
{
  return mBegin;
}

XYSequence::iterator XYSlice::end()
{
  return mEnd;
}

size_t XYSlice::size()
{
  return mEnd - mBegin;
}

shared_ptr<XYObject> XYSlice::at(size_t n)
{
  assert(mBegin + n < mEnd);
  return *(mBegin + n);
}

shared_ptr<XYObject> XYSlice::head()
{
  assert(mBegin != mEnd);
  return *mBegin;
}

shared_ptr<XYSequence> XYSlice::tail()
{
  if (size() <= 1)
    return msp(new XYList());

  XYSequence::iterator start = ++mBegin;
  assert(start != mEnd);
  return msp(new XYSlice(mOriginal, start, mEnd));
}

boost::shared_ptr<XYSequence> XYSlice::join(boost::shared_ptr<XYSequence> const& rhs)
{
  shared_ptr<XYSequence> self(dynamic_pointer_cast<XYSequence>(shared_from_this()));

  if (dynamic_cast<XYJoin const*>(rhs.get())) {
    // If the reference to rhs is unique then we can modify the object itself.
    if (rhs.unique()) {    
      shared_ptr<XYJoin> result(dynamic_pointer_cast<XYJoin>(rhs));
      result->mSequences.push_front(self);
      return result;
    }
    else {
      // Pointer is shared, we have to copy the data
      shared_ptr<XYJoin> join_rhs(dynamic_pointer_cast<XYJoin>(rhs));
      shared_ptr<XYJoin> result(new XYJoin());
      result->mSequences.push_back(self);
      result->mSequences.insert(result->mSequences.end(), 
		  	        join_rhs->mSequences.begin(), join_rhs->mSequences.end());
      return result;
    }
  }

  return msp(new XYJoin(self, rhs));
}

// XYJoin
XYJoin::XYJoin(shared_ptr<XYSequence> first,
               shared_ptr<XYSequence> second)
{ 
  mSequences.push_back(first);
  mSequences.push_back(second);
}

string XYJoin::toString(bool parse) const {
  ostringstream s;
  s << "[ ";
  for(const_iterator it = mSequences.begin(); it != mSequences.end(); ++it)
    for_each((*it)->begin(), (*it)->end(), s << bind(&XYObject::toString, _1, parse) << " ");
  s << "]";
  return s.str();
}

void XYJoin::eval1(XY* xy) {
  xy->mX.push_back(shared_from_this());
}

XYSequence::iterator XYJoin::begin()
{
  // TODO:
  // This is a workaround until generic iterators for the different
  // sequence classes are implemented. We force the lazy append so that
  // the first list has the result, and the second is empty.
  if (mSequences.size() == 1)
    return mSequences[0]->begin();

  shared_ptr<XYList> list(new XYList());
  for(iterator it = mSequences.begin(); it != mSequences.end(); ++it) 
    list->mList.insert(list->mList.end(), (*it)->begin(), (*it)->end());

  mSequences.clear();
  mSequences.push_back(list);
  
  return list->begin();
}

XYSequence::iterator XYJoin::end()
{
  // TODO:
  // This is a workaround until generic iterators for the different
  // sequence classes are implemented. We force the lazy append so that
  // the first list has the result, and the second is empty.
  if (mSequences.size() == 1)
    return mSequences[0]->end();

  shared_ptr<XYList> list(new XYList());
  for(iterator it = mSequences.begin(); it != mSequences.end(); ++it) 
    list->mList.insert(list->mList.end(), (*it)->begin(), (*it)->end());

  mSequences.clear();
  mSequences.push_back(list);
  
  return list->end();
}

size_t XYJoin::size()
{
  size_t s = 0;
  for(iterator it = mSequences.begin(); it != mSequences.end(); ++it) 
    s += (*it)->size();

  return s;
}

shared_ptr<XYObject> XYJoin::at(size_t n)
{
  assert(n < size());
  
  size_t s = 0;
  for(iterator it = mSequences.begin(); it != mSequences.end(); ++it) {
    size_t b = s;
    s += (*it)->size();
    if (n < s)
      return (*it)->at(n-b);
  }

  assert(1 == 0);
}

shared_ptr<XYObject> XYJoin::head()
{
  return at(0);
}

shared_ptr<XYSequence> XYJoin::tail()
{
  if (size() <= 1)
    return msp(new XYList());

  return msp(new XYSlice(shared_from_this(), begin() + 1, end()));
}

boost::shared_ptr<XYSequence> XYJoin::join(boost::shared_ptr<XYSequence> const& rhs)
{
  shared_ptr<XYJoin> self(dynamic_pointer_cast<XYJoin>(shared_from_this()));

  if (dynamic_cast<XYJoin const*>(rhs.get())) {
    // If the reference to rhs is unique then we can modify the object itself.
    if (rhs.unique()) {    
      shared_ptr<XYJoin> result(dynamic_pointer_cast<XYJoin>(rhs));
      result->mSequences.insert(result->mSequences.begin(), 
				mSequences.begin(), mSequences.end());
      return result;
    }
    else if (self.use_count() == 2) { // to account for the 1 reference we are holding
      shared_ptr<XYJoin> join_rhs(dynamic_pointer_cast<XYJoin>(rhs));
      mSequences.insert(mSequences.end(), 
			join_rhs->mSequences.begin(), join_rhs->mSequences.end());
      return self;
    }
    else {
      // Pointer is shared, we have to copy the data
      shared_ptr<XYJoin> join_rhs(dynamic_pointer_cast<XYJoin>(rhs));
      shared_ptr<XYJoin> result(new XYJoin());
      result->mSequences.insert(result->mSequences.end(), 
				mSequences.begin(), mSequences.end());
      result->mSequences.insert(result->mSequences.end(), 
				join_rhs->mSequences.begin(), join_rhs->mSequences.end());
      return result;
    }
  }

  // rhs is not a join
  if (self.use_count() == 2) { // to account for the 1 reference we are holding
    mSequences.push_back(rhs);
    return self;
  }
  
  // rhs is not a join and we can't modify ourselves
  shared_ptr<XYJoin> result(new XYJoin());
  result->mSequences.insert(result->mSequences.end(), 
			    mSequences.begin(), mSequences.end());
  result->mSequences.push_back(rhs);
  return result;
}

// XYPrimitive
XYPrimitive::XYPrimitive(string n, void (*func)(XY*)) : mName(n), mFunc(func) { }

string XYPrimitive::toString(bool) const {
  return mName;
}

void XYPrimitive::eval1(XY* xy) {
  mFunc(xy);
}

int XYPrimitive::compare(shared_ptr<XYObject> rhs) {
  shared_ptr<XYPrimitive> o = dynamic_pointer_cast<XYPrimitive>(rhs);
  if (!o)
    return toString(true).compare(rhs->toString(true));

  return mName.compare(o->mName);
}
 
// Primitive Implementations

// + [X^lhs^rhs] Y] -> [X^lhs+rhs Y]
static void primitive_addition(XY* xy) {
  assert(xy->mX.size() >= 2);
  shared_ptr<XYObject> rhs(xy->mX.back());
  assert(rhs);
  xy->mX.pop_back();

  shared_ptr<XYObject> lhs(xy->mX.back());
  assert(lhs);
  xy->mX.pop_back();

  xy->mX.push_back(lhs->add(rhs.get()));
}

// - [X^lhs^rhs] Y] -> [X^lhs-rhs Y]
static void primitive_subtraction(XY* xy) {
  assert(xy->mX.size() >= 2);
  shared_ptr<XYObject> rhs(xy->mX.back());
  assert(rhs);
  xy->mX.pop_back();

  shared_ptr<XYObject> lhs(xy->mX.back());
  assert(lhs);
  xy->mX.pop_back();

  xy->mX.push_back(lhs->subtract(rhs.get()));
}

// * [X^lhs^rhs] Y] -> [X^lhs*rhs Y]
static void primitive_multiplication(XY* xy) {
  assert(xy->mX.size() >= 2);
  shared_ptr<XYObject> rhs(xy->mX.back());
  assert(rhs);
  xy->mX.pop_back();

  shared_ptr<XYObject> lhs(xy->mX.back());
  assert(lhs);
  xy->mX.pop_back();

  xy->mX.push_back(lhs->multiply(rhs.get()));
}

// % [X^lhs^rhs] Y] -> [X^lhs/rhs Y]
static void primitive_division(XY* xy) {
  assert(xy->mX.size() >= 2);
  shared_ptr<XYObject> rhs(xy->mX.back());
  assert(rhs);
  xy->mX.pop_back();

  shared_ptr<XYObject> lhs(xy->mX.back());
  assert(lhs);
  xy->mX.pop_back();

  xy->mX.push_back(lhs->divide(rhs.get()));
}

// ^ [X^lhs^rhs] Y] -> [X^lhs**rhs Y]
static void primitive_power(XY* xy) {
  assert(xy->mX.size() >= 2);
  shared_ptr<XYObject> rhs(xy->mX.back());
  assert(rhs);
  xy->mX.pop_back();

  shared_ptr<XYObject> lhs(xy->mX.back());
  assert(lhs);
  xy->mX.pop_back();

  xy->mX.push_back(lhs->power(rhs.get()));
}

// _ floor [X^n] Y] -> [X^n Y]
static void primitive_floor(XY* xy) {
  assert(xy->mX.size() >= 1);
  shared_ptr<XYNumber> n = dynamic_pointer_cast<XYNumber>(xy->mX.back());
  assert(n);
  xy->mX.pop_back();

  xy->mX.push_back(n->floor());
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

// . [X^{O1..On} Y] [X O1^..^On^Y]
static void primitive_unquote(XY* xy) {
  assert(xy->mX.size() >= 1);
  shared_ptr<XYObject> o = xy->mX.back();
  xy->mX.pop_back();

  shared_ptr<XYSequence> list = dynamic_pointer_cast<XYSequence>(o);

  if (list) {
    xy->mY.insert(xy->mY.begin(), list->begin(), list->end());
  }
  else {
    shared_ptr<XYSymbol> symbol = dynamic_pointer_cast<XYSymbol>(o);
    if (symbol) {
      // If it's a symbol, get the value of the symbol and apply
      // unquote to that.
      XYEnv::iterator it = xy->mEnv.find(symbol->mValue);
      if (it != xy->mEnv.end()) {
	xy->mX.push_back((*it).second);
	primitive_unquote(xy);
      }
      else {
	// If the symbol doesn't exist in the environment, just
	// return the symbol itself.
	xy->mX.push_back(symbol);
      }
    }
    else 
      xy->mY.push_front(o);
  }
}

// ) [X^{pattern} Y] [X^result Y]
static void primitive_pattern_ss(XY* xy) {
  assert(xy->mX.size() >= 1);

  // Get the pattern from the stack
  shared_ptr<XYSequence> pattern = dynamic_pointer_cast<XYSequence>(xy->mX.back());
  assert(pattern);
  xy->mX.pop_back();
  assert(pattern->begin() != pattern->end());

  // Populate env with a mapping between the pattern variables to the
  // values on the stack.
  XYEnv env;
  xy->getPatternValues(*(pattern->begin()), inserter(env, env.begin()));
  // Process pattern body using these mappings.
  if (pattern->end() - pattern->begin() > 1) {
    XYSequence::iterator start = pattern->begin();
    XYSequence::iterator end   = pattern->end();
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
  shared_ptr<XYSequence> pattern = dynamic_pointer_cast<XYSequence>(xy->mX.back());
  assert(pattern);
  xy->mX.pop_back();
  assert(pattern->begin() != pattern->end());

  // Populate env with a mapping between the pattern variables to the
  // values on the stack.
  XYEnv env;
  xy->getPatternValues(*(pattern->begin()), inserter(env, env.begin()));
  // Process pattern body using these mappings.
  if (pattern->end() - pattern->begin() > 1) {
    XYSequence::iterator start = pattern->begin();
    XYSequence::iterator end   = pattern->end();
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
  shared_ptr<XYSequence> list = dynamic_pointer_cast<XYSequence>(xy->mX.back());
  assert(list);
  xy->mX.pop_back();

  shared_ptr<XYObject> o = xy->mX.back();
  assert(o);
  xy->mX.pop_back();

  xy->mY.push_front(o);
  xy->mY.insert(xy->mY.begin(), list->begin(), list->end());
}

// | reverse [X^{a0..an} Y] [X^{an..a0} Y]
static void primitive_reverse(XY* xy) {
  assert(xy->mX.size() >= 1);
  shared_ptr<XYSequence> list = dynamic_pointer_cast<XYSequence>(xy->mX.back());
  assert(list);
  xy->mX.pop_back();

  shared_ptr<XYList> reversed = msp(new XYList(list->begin(), list->end()));
  reverse(reversed->begin(), reversed->end());
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

  shared_ptr<XYSequence> list_lhs = dynamic_pointer_cast<XYSequence>(lhs);
  shared_ptr<XYSequence> list_rhs = dynamic_pointer_cast<XYSequence>(rhs);

  if (list_lhs && list_rhs) {
    // Reset original pointers to enable copy on write optimisations
    rhs.reset(static_cast<XYSequence*>(0));
    lhs.reset(static_cast<XYSequence*>(0));

    // Two lists are concatenated
    xy->mX.push_back(list_lhs->join(list_rhs));
  }
  else if(list_lhs) {
    // Reset original pointers to enable copy on write optimisations
    lhs.reset(static_cast<XYSequence*>(0));

    // If rhs is not a list, it is added to the end of the list.
    shared_ptr<XYList> list(new XYList());
    list->mList.push_back(rhs);
    xy->mX.push_back(list_lhs->join(list));
  }
  else if(list_rhs) {
    // Reset original pointers to enable copy on write optimisations
    rhs.reset(static_cast<XYSequence*>(0));

    // If lhs is not a list, it is added to the front of the list
    shared_ptr<XYList> list(new XYList());
    list->mList.push_back(lhs);
    xy->mX.push_back(list->join(list_rhs));
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
  shared_ptr<XYSequence> list  = dynamic_pointer_cast<XYSequence>(xy->mX.back());
  assert(list);
  xy->mX.pop_back();

  shared_ptr<XYList> stack(new XYList(xy->mX.begin(), xy->mX.end()));
  shared_ptr<XYList> queue(new XYList(xy->mY.begin(), xy->mY.end()));

  xy->mX.push_back(stack);
  xy->mX.push_back(queue);
  xy->mY.push_front(msp(new XYSymbol("$$")));
  xy->mY.insert(xy->mY.begin(), list->begin(), list->end()); 
}

// $$ stackqueue - Helper word for '$'. Given a stack and queue on the
// stack, replaces the existing stack and queue with them.
static void primitive_stackqueue(XY* xy) {
  assert(xy->mX.size() >= 2);

  shared_ptr<XYSequence> queue = dynamic_pointer_cast<XYSequence>(xy->mX.back());
  assert(queue);
  xy->mX.pop_back();

  shared_ptr<XYSequence> stack = dynamic_pointer_cast<XYSequence>(xy->mX.back());
  assert(stack);
  xy->mX.pop_back();

  xy->mX.assign(stack->begin(), stack->end());
  xy->mY.assign(queue->begin(), queue->end());
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
    shared_ptr<XYSequence> l = dynamic_pointer_cast<XYSequence>(o);
    if(l && l->begin() == l->end())
      xy->mX.push_back(msp(new XYInteger(1)));
    else
      xy->mX.push_back(msp(new XYInteger(0)));
  }
}

// @ nth [X^n^{...} Y] [X^o Y] 
static void primitive_nth(XY* xy) {
  assert(xy->mX.size() >= 2);

  shared_ptr<XYSequence> list = dynamic_pointer_cast<XYSequence>(xy->mX.back());
  assert(list);
  xy->mX.pop_back();

  shared_ptr<XYObject> index(xy->mX.back());
  assert(index);
  xy->mX.pop_back();

  shared_ptr<XYNumber> n = dynamic_pointer_cast<XYNumber>(index);
  shared_ptr<XYSequence> s = dynamic_pointer_cast<XYSequence>(index);
  assert(n || s);

  if (n) {
    // Index is a number, do a direct index into the list
    if (n->as_uint() >= list->size()) 
      xy->mX.push_back(msp(new XYInteger(list->size())));
    else
      xy->mX.push_back(list->at(n->as_uint()));    
  }
  else if (s) {
    // Index is a list. Use this as a path into the list.
    if (s->size() == 0) {
      // If the path is empty, return the entire list
      xy->mX.push_back(list);
    }    
    else {
      shared_ptr<XYObject> head = s->head();
      shared_ptr<XYSequence> tail = s->tail();

      // If head is a list, then it's a request to get multiple values
      shared_ptr<XYSequence> headlist = dynamic_pointer_cast<XYSequence>(head);
      if (headlist && headlist->size() == 0) {
	if (tail->size() == 0) {
	  xy->mX.push_back(list);
	}
	else {
	  XYSequence::List code;
	  XYSequence::List code2;
	  for (XYSequence::iterator it = list->begin(); it != list->end(); ++it) {
	    code.push_back(tail);
	    code.push_back(*it);
	    code.push_back(msp(new XYSymbol("@")));
	    code.push_back(msp(new XYSymbol("'")));
	    code.push_back(msp(new XYSymbol("'")));
	    code.push_back(msp(new XYSymbol("`")));	  
	  }	
	  for (int i=0; i < list->size() - 1; ++i) {
	    code2.push_back(msp(new XYSymbol(",")));
	  }
	  xy->mY.insert(xy->mY.begin(), code2.begin(), code2.end());
	  xy->mY.insert(xy->mY.begin(), code.begin(), code.end());	  
	}
      }
      else if (headlist) {
	XYSequence::List code;
	XYSequence::List code2;
	for (XYSequence::iterator it = headlist->begin(); it != headlist->end(); ++it) {
	  code.push_back(*it);
	  code.push_back(list);
	  code.push_back(msp(new XYSymbol("@")));			
	  code.push_back(msp(new XYSymbol("'")));
	  code.push_back(msp(new XYSymbol("'")));
	  code.push_back(msp(new XYSymbol("`")));
	}
	
	for (int i=0; i < headlist->size()-1; ++i) {
	  code2.push_back(msp(new XYSymbol(",")));
	}

	xy->mY.insert(xy->mY.begin(), code2.begin(), code2.end());
	xy->mY.insert(xy->mY.begin(), code.begin(), code.end());
      }
      else if (tail->size() == 0) {
	xy->mX.push_back(head);
	xy->mX.push_back(list);
	xy->mY.push_front(msp(new XYSymbol("@")));
      }
      else {
	xy->mX.push_back(head);
	xy->mX.push_back(list);
	xy->mY.push_front(msp(new XYSymbol("@")));
	xy->mY.push_front(msp(new XYShuffle("ab-ba")));
	xy->mY.push_front(tail);
	xy->mY.push_front(msp(new XYSymbol("@")));
      }
    }
  }
}

// print [X^n Y] [X Y] 
static void primitive_print(XY* xy) {
  assert(xy->mX.size() >= 1);

  shared_ptr<XYObject> o(xy->mX.back());
  assert(o);
  xy->mX.pop_back();

  cout << o->toString(true);
}

// println [X^n Y] [X Y] 
static void primitive_println(XY* xy) {
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

  shared_ptr<XYSequence> list(dynamic_pointer_cast<XYSequence>(o));
  if (list)
    xy->mX.push_back(msp(new XYInteger(list->size())));
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

// parse [X^{tokens} Y] [X^{...} Y] 
// Given a list of tokens, parses it and returns the program
static void primitive_parse(XY* xy) {
  assert(xy->mX.size() >= 1);

  shared_ptr<XYSequence> tokens(dynamic_pointer_cast<XYSequence>(xy->mX.back()));
  assert(tokens);
  xy->mX.pop_back();

  vector<string> strings;
  for(XYSequence::iterator it = tokens->begin(); it!=tokens->end(); ++it) {
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

// enum [X^n Y] -> [X^{0..n} Y]
static void primitive_enum(XY* xy) {
  assert(xy->mX.size() >= 1);
  shared_ptr<XYNumber> n = dynamic_pointer_cast<XYNumber>(xy->mX.back());
  assert(n);
  xy->mX.pop_back();

  int value = n->as_uint();
  shared_ptr<XYList> list = msp(new XYList());
  for(int i=0; i < value; ++i)
    list->mList.push_back(msp(new XYInteger(i)));
  xy->mX.push_back(list);
}

// clone [X^o Y] -> [X^o Y]
static void primitive_clone(XY* xy) {
  // TODO: Only works for sequences for now
  // Implemented to work around an XYJoin limitation
  assert(xy->mX.size() >= 1);
  shared_ptr<XYSequence> o = dynamic_pointer_cast<XYSequence>(xy->mX.back());
  assert(o);
  xy->mX.pop_back();

  xy->mX.push_back(msp(new XYList(o->begin(), o->end())));
}

// XYTimeLimit
XYTimeLimit::XYTimeLimit(unsigned int milliseconds) :
  mMilliseconds(milliseconds) {
}

void XYTimeLimit::start(XY* xy) {
  using namespace boost::posix_time;
  
  mStart = (microsec_clock::universal_time() - ptime(min_date_time)).total_milliseconds();
}

bool XYTimeLimit::check(XY* xy) {
  using namespace boost::posix_time;
  unsigned int now = (microsec_clock::universal_time() - ptime(min_date_time)).total_milliseconds();
  return (now - mStart >= mMilliseconds);
}

// XY
XY::XY() {
  mP["+"]   = msp(new XYPrimitive("+", primitive_addition));
  mP["-"]   = msp(new XYPrimitive("-", primitive_subtraction));
  mP["*"]   = msp(new XYPrimitive("*", primitive_multiplication));
  mP["%"]   = msp(new XYPrimitive("%", primitive_division));
  mP["^"]   = msp(new XYPrimitive("^", primitive_power));
  mP["_"]   = msp(new XYPrimitive("_", primitive_floor));
  mP["set"] = msp(new XYPrimitive("set", primitive_set));
  mP[";"]   = msp(new XYPrimitive(";", primitive_get));
  mP["."]   = msp(new XYPrimitive(".", primitive_unquote));
  mP[")"]   = msp(new XYPrimitive(")", primitive_pattern_ss));
  mP["("]   = msp(new XYPrimitive("(", primitive_pattern_sq));
  mP["`"]   = msp(new XYPrimitive("`", primitive_dip));
  mP["|"]   = msp(new XYPrimitive("|", primitive_reverse));
  mP["'"]   = msp(new XYPrimitive("'", primitive_quote));
  mP[","]   = msp(new XYPrimitive(",", primitive_join));
  mP["$"]   = msp(new XYPrimitive("$", primitive_stack));
  mP["$$"]  = msp(new XYPrimitive("$$", primitive_stackqueue));
  mP["="]   = msp(new XYPrimitive("=", primitive_equals));
  mP["<"]   = msp(new XYPrimitive("<", primitive_lessThan));
  mP["<="]  = msp(new XYPrimitive("<=", primitive_lessThanEqual));
  mP[">"]   = msp(new XYPrimitive(">", primitive_greaterThan));
  mP[">="]  = msp(new XYPrimitive(">=", primitive_greaterThanEqual));
  mP["not"] = msp(new XYPrimitive("not", primitive_not));
  mP["@"]   = msp(new XYPrimitive("@", primitive_nth));
  mP["println"] = msp(new XYPrimitive("print", primitive_println));
  mP["print"] = msp(new XYPrimitive("print", primitive_print));
  mP["write"] = msp(new XYPrimitive("write", primitive_write));
  mP["count"] = msp(new XYPrimitive("count", primitive_count));
  mP["tokenize"] = msp(new XYPrimitive("tokenize", primitive_tokenize));
  mP["parse"] = msp(new XYPrimitive("parse", primitive_parse));
  mP["getline"] = msp(new XYPrimitive("getline", primitive_getline));
  mP["millis"] = msp(new XYPrimitive("millis", primitive_millis));
  mP["enum"]   = msp(new XYPrimitive("+", primitive_enum));
  mP["clone"]   = msp(new XYPrimitive("clone", primitive_clone));
}

void XY::checkLimits() {
  for(XYLimits::iterator it = mLimits.begin(); it != mLimits.end(); ++it) {
    if ((*it)->check(this)) {
      // This limit was reached, stop executing and throw the limit.
      throw *it;
    }
  }
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
  for(XYLimits::iterator it = mLimits.begin(); it != mLimits.end(); ++it) {
    (*it)->start(this);
  }

  while (mY.size() > 0) {
    eval1();
    checkLimits();
  }
}

template <class OutputIterator>
void XY::match(OutputIterator out, 
               shared_ptr<XYObject> object,
               shared_ptr<XYObject> pattern,
               shared_ptr<XYSequence> sequence,
               XYSequence::iterator it) {
  shared_ptr<XYSequence> object_list = dynamic_pointer_cast<XYSequence>(object);
  shared_ptr<XYSequence> pattern_list = dynamic_pointer_cast<XYSequence>(pattern);
  shared_ptr<XYSymbol> pattern_symbol = dynamic_pointer_cast<XYSymbol>(pattern);
  if (object_list && pattern_list) {
    XYSequence::iterator pit = pattern_list->begin(),
                         oit = object_list->begin(); 
    for(;
        pit != pattern_list->end() && oit != object_list->end(); 
        ++pit, ++oit) {
      match(out, (*oit), (*pit), object_list, oit);
    }
    // If there are more pattern items than there are list items,
    // set the pattern value to null.
    while(pit != pattern_list->end()) {
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
      *out++ = make_pair(pattern_symbol->mValue, new XYSlice(sequence, it, sequence->end()));
    }
    else
      *out++ = make_pair(pattern_symbol->mValue, object);
  }
}

template <class OutputIterator>
void XY::getPatternValues(shared_ptr<XYObject> pattern, OutputIterator out) {
  shared_ptr<XYSequence> list = dynamic_pointer_cast<XYSequence>(pattern);
  if (list) {
    assert(mX.size() >= list->size());
    shared_ptr<XYList> stack(new XYList(mX.end() - list->size(), mX.end()));
    match(out, stack, pattern, stack, stack->begin());
    mX.resize(mX.size() - list->size());
  }
  else {
    shared_ptr<XYObject> o = mX.back();
    mX.pop_back();
    shared_ptr<XYList> list(new XYList());
    match(out, o, pattern, list, list->end());
  }
}
 
template <class OutputIterator>
void XY::replacePattern(XYEnv const& env, shared_ptr<XYObject> object, OutputIterator out) {
  shared_ptr<XYSequence> list   = dynamic_pointer_cast<XYSequence>(object);
  shared_ptr<XYSymbol>   symbol = dynamic_pointer_cast<XYSymbol>(object); 
  if (list) {
    // Recurse through the list replacing variables as needed
    shared_ptr<XYList> new_list(new XYList());
    for(XYSequence::iterator it = list->begin(); it != list->end(); ++it) 
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
  return (set= '\\' , '[' , ']' , '{' , '}' , '(' , ')' , ';' , '!' , '.' , ',' , '`' , '\'' , '|', '@');
}

// Return regex for non-specials
boost::xpressive::sregex re_non_special() {
  using namespace boost::xpressive;
  using boost::xpressive::set;
  return ~(set[(set= '\\','[',']','{','}','(',')',';','!','.',',','`','\'','|','@') | _s]);
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
