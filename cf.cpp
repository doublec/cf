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
#include <boost/algorithm/string.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/gregorian/gregorian.hpp>
#include <boost/bind.hpp>
#include "cf.h"

// If defined, compiles as a test applicatation that tests
// that things are working.
#if defined(TEST)
#include <boost/test/minimal.hpp>
#endif

using namespace std;
using namespace boost;

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
 
// Macro to implement double dispatch operations in class
#define DD_IMPL(class, name)						\
  intrusive_ptr<XYObject> class::name(XYObject* rhs) { return rhs->name(this); } \
  intrusive_ptr<XYObject> class::name(XYFloat* lhs) { return dd_##name(lhs, this); } \
  intrusive_ptr<XYObject> class::name(XYInteger* lhs) { return dd_##name(lhs, this); } \
  intrusive_ptr<XYObject> class::name(XYSequence* lhs) { return dd_##name(lhs, this); }

#define DD_IMPL2(name, op) \
static intrusive_ptr<XYObject> dd_##name(XYFloat* lhs, XYFloat* rhs) { \
  return msp(new XYFloat(lhs->mValue op rhs->mValue)); \
} \
\
static intrusive_ptr<XYObject> dd_##name(XYFloat* lhs, XYInteger* rhs) { \
  return msp(new XYFloat(lhs->mValue op rhs->as_float()->mValue)); \
} \
\
static intrusive_ptr<XYObject> dd_##name(XYFloat* lhs, XYSequence* rhs) { \
  intrusive_ptr<XYList> list(new XYList()); \
  size_t len = rhs->size();\
  for(int i=0; i < len; ++i)\
    list->mList.push_back(lhs->name(rhs->at(i).get()));	\
  return list; \
}\
\
static intrusive_ptr<XYObject> dd_##name(XYInteger* lhs, XYFloat* rhs) { \
  return msp(new XYFloat(lhs->as_float()->mValue op rhs->mValue)); \
} \
\
static intrusive_ptr<XYObject> dd_##name(XYInteger* lhs, XYInteger* rhs) { \
  return msp(new XYInteger(lhs->mValue op rhs->mValue)); \
} \
\
static intrusive_ptr<XYObject> dd_##name(XYInteger* lhs, XYSequence* rhs) { \
  intrusive_ptr<XYList> list(new XYList()); \
  size_t len = rhs->size();\
  for(int i=0; i < len; ++i)\
    list->mList.push_back(lhs->name(rhs->at(i).get()));	\
  return list; \
} \
\
static intrusive_ptr<XYObject> dd_##name(XYSequence* lhs, XYObject* rhs) { \
  intrusive_ptr<XYList> list(new XYList()); \
  size_t len = lhs->size();\
  for(int i=0; i < len; ++i)\
    list->mList.push_back(lhs->at(i)->name(rhs));	\
  return list; \
} \
\
static intrusive_ptr<XYObject> dd_##name(XYSequence* lhs, XYSequence* rhs) { \
  assert(lhs->size() == rhs->size()); \
  intrusive_ptr<XYList> list(new XYList()); \
  size_t lhs_len = lhs->size();\
  size_t rhs_len = rhs->size();\
  for(int li = 0, ri = 0; li < lhs_len && ri < rhs_len; ++li, ++ri)\
    list->mList.push_back(lhs->at(li)->name(rhs->at(ri).get()));    \
  return list; \
}

DD_IMPL2(add, +)
DD_IMPL2(subtract, -)
DD_IMPL2(multiply, *)

static intrusive_ptr<XYObject> dd_divide(XYFloat* lhs, XYFloat* rhs) {
  return msp(new XYFloat(lhs->mValue / rhs->mValue));
}

static intrusive_ptr<XYObject> dd_divide(XYFloat* lhs, XYInteger* rhs) {
  return msp(new XYFloat(lhs->mValue / rhs->as_float()->mValue));
}

static intrusive_ptr<XYObject> dd_divide(XYFloat* lhs, XYSequence* rhs) { 
  intrusive_ptr<XYList> list(new XYList());
  size_t len = rhs->size();
  for (int i=0; i < len; ++i)
    list->mList.push_back(lhs->divide(rhs->at(i).get()));
  return list;
}

static intrusive_ptr<XYObject> dd_divide(XYInteger* lhs, XYFloat* rhs) {
  return msp(new XYFloat(lhs->as_float()->mValue / rhs->mValue));
}

static intrusive_ptr<XYObject> dd_divide(XYInteger* lhs, XYInteger* rhs) {
  return msp(new XYFloat(lhs->as_float()->mValue / rhs->as_float()->mValue));
}

static intrusive_ptr<XYObject> dd_divide(XYInteger* lhs, XYSequence* rhs) {
  intrusive_ptr<XYList> list(new XYList());
  size_t len = rhs->size();
  for (int i=0; i < len; ++i)
    list->mList.push_back(lhs->divide(rhs->at(i).get()));
  return list;
}

static intrusive_ptr<XYObject> dd_divide(XYSequence* lhs, XYObject* rhs) {
  intrusive_ptr<XYList> list(new XYList());
  size_t len = lhs->size();
  for (int i=0; i < len; ++i)
    list->mList.push_back(lhs->at(i)->divide(rhs));
  return list;
}


static intrusive_ptr<XYObject> dd_divide(XYSequence* lhs, XYSequence* rhs) {
  assert(lhs->size() == rhs->size());
  intrusive_ptr<XYList> list(new XYList());
  size_t lhs_len = lhs->size();
  size_t rhs_len = rhs->size();
  for(int li=0, ri=0; li < lhs_len && ri < rhs_len; ++li, ++ri)
    list->mList.push_back(lhs->at(li)->divide(rhs->at(ri).get()));
  return list; 
}

static intrusive_ptr<XYObject> dd_power(XYFloat* lhs, XYFloat* rhs) {
  return msp(new XYFloat(pow(static_cast<double>(lhs->mValue.get_d()), 
			     static_cast<double>(rhs->mValue.get_d()))));
}

static intrusive_ptr<XYObject> dd_power(XYFloat* lhs, XYInteger* rhs) {
  intrusive_ptr<XYFloat> result(new XYFloat(lhs->mValue));
  mpf_pow_ui(result->mValue.get_mpf_t(), lhs->mValue.get_mpf_t(), rhs->as_uint());
  return result;
}

static intrusive_ptr<XYObject> dd_power(XYFloat* lhs, XYSequence* rhs) {
  intrusive_ptr<XYList> list(new XYList());
  size_t len = rhs->size();
  for (int i=0; i < len; ++i)
    list->mList.push_back(lhs->power(rhs->at(i).get()));
  return list;
}

static intrusive_ptr<XYObject> dd_power(XYInteger* lhs, XYFloat* rhs) {
  return msp(new XYFloat(pow(static_cast<double>(lhs->mValue.get_d()), 
			     static_cast<double>(rhs->mValue.get_d()))));
}

static intrusive_ptr<XYObject> dd_power(XYInteger* lhs, XYInteger* rhs) {
  intrusive_ptr<XYInteger> result(new XYInteger(lhs->mValue));
  mpz_pow_ui(result->mValue.get_mpz_t(), lhs->mValue.get_mpz_t(), rhs->as_uint());
  return result;
}

static intrusive_ptr<XYObject> dd_power(XYInteger* lhs, XYSequence* rhs) {
  intrusive_ptr<XYList> list(new XYList());
  size_t len = rhs->size();
  for (int i=0; i < len; ++i)
    list->mList.push_back(lhs->power(rhs->at(i).get()));
  return list;
}

static intrusive_ptr<XYObject> dd_power(XYSequence* lhs, XYObject* rhs) {
  intrusive_ptr<XYList> list(new XYList());
  size_t len = lhs->size();
  for (int i=0; i < len; ++i)
    list->mList.push_back(lhs->at(i)->power(rhs));
  return list;
}


static intrusive_ptr<XYObject> dd_power(XYSequence* lhs, XYSequence* rhs) {
  assert(lhs->size() == rhs->size());
  intrusive_ptr<XYList> list(new XYList());
  size_t lhs_len = lhs->size();
  size_t rhs_len = rhs->size();
  for (int li=0, ri=0; li < lhs_len && ri < rhs_len; ++li, ++ri)
    list->mList.push_back(lhs->at(li)->power(rhs->at(ri).get()));
  return list;
}

// XYReference
XYReference::XYReference() :
  mReferences(0) {
}

void XYReference::increment() {
  ++mReferences;
}

void XYReference::decrement() {
  --mReferences;
  if (mReferences == 0)
    delete this;
}

// XYObject
XYObject::XYObject() { }

intrusive_ptr<XYObject> XYObject::add(XYObject* rhs) {
  assert(1==0);
}

intrusive_ptr<XYObject> XYObject::add(XYFloat* rhs) {
  assert(1==0);
}

intrusive_ptr<XYObject> XYObject::add(XYInteger* rhs) {
  assert(1==0);
}

intrusive_ptr<XYObject> XYObject::add(XYSequence* rhs) {
  assert(1==0);
}

intrusive_ptr<XYObject> XYObject::subtract(XYObject* rhs) {
  assert(1==0);
}

intrusive_ptr<XYObject> XYObject::subtract(XYFloat* rhs) {
  assert(1==0);
}

intrusive_ptr<XYObject> XYObject::subtract(XYInteger* rhs) {
  assert(1==0);
}

intrusive_ptr<XYObject> XYObject::subtract(XYSequence* rhs) {
  assert(1==0);
}

intrusive_ptr<XYObject> XYObject::multiply(XYObject* rhs) {
  assert(1==0);
}

intrusive_ptr<XYObject> XYObject::multiply(XYFloat* rhs) {
  assert(1==0);
}

intrusive_ptr<XYObject> XYObject::multiply(XYInteger* rhs) {
  assert(1==0);
}

intrusive_ptr<XYObject> XYObject::multiply(XYSequence* rhs) {
  assert(1==0);
}

intrusive_ptr<XYObject> XYObject::divide(XYObject* rhs) {
  assert(1==0);
}

intrusive_ptr<XYObject> XYObject::divide(XYFloat* rhs) {
  assert(1==0);
}

intrusive_ptr<XYObject> XYObject::divide(XYInteger* rhs) {
  assert(1==0);
}

intrusive_ptr<XYObject> XYObject::divide(XYSequence* rhs) {
  assert(1==0);
}

intrusive_ptr<XYObject> XYObject::power(XYObject* rhs) {
  assert(1==0);
}

intrusive_ptr<XYObject> XYObject::power(XYFloat* rhs) {
  assert(1==0);
}

intrusive_ptr<XYObject> XYObject::power(XYInteger* rhs) {
  assert(1==0);
}

intrusive_ptr<XYObject> XYObject::power(XYSequence* rhs) {
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

void XYFloat::eval1(intrusive_ptr<XY> const& xy) {
  xy->mX.push_back(msp(this));
}

int XYFloat::compare(intrusive_ptr<XYObject> rhs) {
  intrusive_ptr<XYFloat> o = dynamic_pointer_cast<XYFloat>(rhs);
  if (!o) {
    intrusive_ptr<XYInteger> i = dynamic_pointer_cast<XYInteger>(rhs);
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

intrusive_ptr<XYInteger> XYFloat::as_integer() {
  return msp(new XYInteger(mValue));
}

intrusive_ptr<XYFloat> XYFloat::as_float() {
  return dynamic_pointer_cast<XYFloat>(msp(this));
}

intrusive_ptr<XYNumber> XYFloat::floor() {
  intrusive_ptr<XYFloat> result(new XYFloat(::floor(mValue)));
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

void XYInteger::eval1(intrusive_ptr<XY> const& xy) {
  xy->mX.push_back(msp(this));
}

int XYInteger::compare(intrusive_ptr<XYObject> rhs) {
  intrusive_ptr<XYInteger> o = dynamic_pointer_cast<XYInteger>(rhs);
  if (!o) {
    intrusive_ptr<XYFloat> f = dynamic_pointer_cast<XYFloat>(rhs);
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

intrusive_ptr<XYInteger> XYInteger::as_integer() {
  return dynamic_pointer_cast<XYInteger>(msp(this));
}

intrusive_ptr<XYFloat> XYInteger::as_float() {
  return msp(new XYFloat(mValue));
}

intrusive_ptr<XYNumber> XYInteger::floor() {
  return dynamic_pointer_cast<XYNumber>(msp(this));
}

// XYSymbol
XYSymbol::XYSymbol(string v) : mValue(v) { }

string XYSymbol::toString(bool) const {
  return mValue;
}

void XYSymbol::eval1(intrusive_ptr<XY> const& xy) {
  XYEnv::iterator it = xy->mP.find(mValue);
  if (it != xy->mP.end())
    // Primitive symbol, execute immediately
    (*it).second->eval1(xy);
  else
    xy->mX.push_back(msp(this));
}

int XYSymbol::compare(intrusive_ptr<XYObject> rhs) {
  intrusive_ptr<XYSymbol> o = dynamic_pointer_cast<XYSymbol>(rhs);
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

void XYString::eval1(intrusive_ptr<XY> const& xy) {
  xy->mX.push_back(msp(this));
}

int XYString::compare(intrusive_ptr<XYObject> rhs) {
  intrusive_ptr<XYString> o = dynamic_pointer_cast<XYString>(rhs);
  if (!o)
    return toString(true).compare(rhs->toString(true));

  return mValue.compare(o->mValue);
}

size_t XYString::size()
{
  return mValue.size();
}

void XYString::pushBackInto(List& list) {
  for(string::iterator it = mValue.begin(); it != mValue.end(); ++it)
    list.push_back(msp(new XYInteger(*it)));
}

intrusive_ptr<XYObject> XYString::at(size_t n)
{
  return msp(new XYInteger(mValue[n]));
}

void XYString::set_at(size_t n, intrusive_ptr<XYObject> const& v)
{
  assert(n < mValue.size());
  intrusive_ptr<XYInteger> c(dynamic_pointer_cast<XYInteger>(v));
  //  xy_assert(c, XYError::TYPE);
  assert(c);

  mValue[n] = c->as_uint(); 
}

intrusive_ptr<XYObject> XYString::head()
{
  assert(mValue.size() > 0);
  return msp(new XYInteger(mValue[0]));
}

intrusive_ptr<XYSequence> XYString::tail()
{
  if (mValue.size() <= 1) 
    return msp(new XYString(""));

  return msp(new XYString(mValue.substr(1)));
}

boost::intrusive_ptr<XYSequence> XYString::join(boost::intrusive_ptr<XYSequence> const& rhs)
{
  XYString const* rhs_string = dynamic_cast<XYString const*>(rhs.get());
  if (rhs_string) {
    if (this->mReferences == 2) {
      mValue += rhs_string->mValue;
      return dynamic_pointer_cast<XYSequence>(msp(this));
    }

    return msp(new XYString(mValue + rhs_string->mValue));
  }

  intrusive_ptr<XYSequence> self(dynamic_pointer_cast<XYSequence>(msp(this)));

  if (dynamic_cast<XYJoin const*>(rhs.get())) {
    // If the reference to rhs is unique then we can modify the object itself.
    if (rhs->mReferences == 1) {    
      intrusive_ptr<XYJoin> result(dynamic_pointer_cast<XYJoin>(rhs));
      result->mSequences.push_front(self);
      return result;
    }
    else {
      // Pointer is shared, we have to copy the data
      intrusive_ptr<XYJoin> join_rhs(dynamic_pointer_cast<XYJoin>(rhs));
      intrusive_ptr<XYJoin> result(new XYJoin());
      result->mSequences.push_back(self);
      result->mSequences.insert(result->mSequences.end(), 
		  	        join_rhs->mSequences.begin(), join_rhs->mSequences.end());
      return result;
    }
  }

  return msp(new XYJoin(self, rhs));
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

void XYShuffle::eval1(intrusive_ptr<XY> const& xy) {
  xy_assert(xy->mX.size() >= mBefore.size(), XYError::STACK_UNDERFLOW);
  map<char, intrusive_ptr<XYObject> > env;
  for(string::reverse_iterator it = mBefore.rbegin(); it != mBefore.rend(); ++it) {
    env[*it] = xy->mX.back();
    xy->mX.pop_back();
  }

  for(string::iterator it = mAfter.begin(); it != mAfter.end(); ++it) {
    assert(env.find(*it) != env.end());
    xy->mX.push_back(env[*it]);
  }
}

int XYShuffle::compare(intrusive_ptr<XYObject> rhs) {
  intrusive_ptr<XYShuffle> o = dynamic_pointer_cast<XYShuffle>(rhs);
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
int XYSequence::compare(intrusive_ptr<XYObject> rhs) {
  intrusive_ptr<XYSequence> o = dynamic_pointer_cast<XYSequence>(rhs);
  if (!o)
    return toString(true).compare(rhs->toString(true));

  size_t lhs_len = size();
  size_t rhs_len = o->size();
  int li = 0;
  int ri = 0;

  for(li=0, ri=0;li < lhs_len && ri < rhs_len; ++li, ++ri) {
    int c = at(li)->compare(o->at(ri));
    if (c != 0)
      return c;
  }

  if(li != lhs_len)
    return -1;

  if(ri != rhs_len)
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
  for (XYSequence::const_iterator it = mList.begin(); it != mList.end(); ++it)
    s << (*it)->toString(parse) << " ";
  s << "]";
  return s.str();
}

void XYList::eval1(intrusive_ptr<XY> const& xy) {
  xy->mX.push_back(msp(this));
}

size_t XYList::size()
{
  return mList.size();
}

void XYList::pushBackInto(List& list) {
  for(List::iterator it = mList.begin(); it != mList.end(); ++it)
    list.push_back(*it);
}

intrusive_ptr<XYObject> XYList::at(size_t n)
{
  return mList[n];
}

void XYList::set_at(size_t n, intrusive_ptr<XYObject> const& v)
{
  assert(n < mList.size());
  mList[n] = v; 
}

intrusive_ptr<XYObject> XYList::head()
{
  assert(mList.size() > 0);
  return mList[0];
}

intrusive_ptr<XYSequence> XYList::tail()
{
  if (mList.size() <= 1) 
    return msp(new XYList());

  return msp(new XYSlice(dynamic_pointer_cast<XYSequence>(msp(this)), 1, mList.size()));
}

boost::intrusive_ptr<XYSequence> XYList::join(boost::intrusive_ptr<XYSequence> const& rhs)
{
  intrusive_ptr<XYSequence> self(dynamic_pointer_cast<XYSequence>(msp(this)));

  if (dynamic_cast<XYJoin const*>(rhs.get())) {
    // If the reference to rhs is unique then we can modify the object itself.
    if (rhs->mReferences == 1) {    
      intrusive_ptr<XYJoin> result(dynamic_pointer_cast<XYJoin>(rhs));
      result->mSequences.push_front(self);
      return result;
    }
    else {
      // Pointer is shared, we have to copy the data
      intrusive_ptr<XYJoin> join_rhs(dynamic_pointer_cast<XYJoin>(rhs));
      intrusive_ptr<XYJoin> result(new XYJoin());
      result->mSequences.push_back(self);
      result->mSequences.insert(result->mSequences.end(), 
		  	        join_rhs->mSequences.begin(), join_rhs->mSequences.end());
      return result;
    }
  }

  return msp(new XYJoin(self, rhs));
}

// XYSlice
XYSlice::XYSlice(intrusive_ptr<XYSequence> original,
                 int begin,
		 int end)  :
  mOriginal(original),
  mBegin(begin),
  mEnd(end)
{
  intrusive_ptr<XYSlice> slice;
  while ((slice = dynamic_pointer_cast<XYSlice>(mOriginal))) {
    // Find the original, non-slice sequence. Without this we can
    // corrupt the C stack due to too much recursion when destroying
    // the tree of slices.
    mOriginal = slice->mOriginal;
    mBegin += slice->mBegin;
    mEnd += slice->mBegin;
  }
}

string XYSlice::toString(bool parse) const {
  ostringstream s;
  s << "[ ";
  for (int i=mBegin; i < mEnd; ++i) 
    s << mOriginal->at(i)->toString(parse) << " ";
  s << "]";
  return s.str();
}

void XYSlice::eval1(intrusive_ptr<XY> const& xy) {
  xy->mX.push_back(msp(this));
}

size_t XYSlice::size()
{
  return mEnd - mBegin;
}

void XYSlice::pushBackInto(List& list) {
  for (int i = mBegin; i != mEnd; ++i)
    list.push_back(mOriginal->at(i));
}

intrusive_ptr<XYObject> XYSlice::at(size_t n)
{
  assert(mBegin + n < mEnd);
  return mOriginal->at(mBegin + n);
}

void XYSlice::set_at(size_t n, intrusive_ptr<XYObject> const& v)
{
  assert(mBegin + n < mEnd);
  mOriginal->set_at(mBegin + n, v); 
}

intrusive_ptr<XYObject> XYSlice::head()
{
  assert(mBegin != mEnd);
  return mOriginal->at(mBegin);
}

intrusive_ptr<XYSequence> XYSlice::tail()
{
  if (size() <= 1)
    return msp(new XYList());

  return msp(new XYSlice(mOriginal, mBegin+1, mEnd));
}

boost::intrusive_ptr<XYSequence> XYSlice::join(boost::intrusive_ptr<XYSequence> const& rhs)
{
  intrusive_ptr<XYSequence> self(dynamic_pointer_cast<XYSequence>(msp(this)));

  if (dynamic_cast<XYJoin const*>(rhs.get())) {
    // If the reference to rhs is unique then we can modify the object itself.
    if (rhs->mReferences == 1) {    
      intrusive_ptr<XYJoin> result(dynamic_pointer_cast<XYJoin>(rhs));
      result->mSequences.push_front(self);
      return result;
    }
    else {
      // Pointer is shared, we have to copy the data
      intrusive_ptr<XYJoin> join_rhs(dynamic_pointer_cast<XYJoin>(rhs));
      intrusive_ptr<XYJoin> result(new XYJoin());
      result->mSequences.push_back(self);
      result->mSequences.insert(result->mSequences.end(), 
		  	        join_rhs->mSequences.begin(), join_rhs->mSequences.end());
      return result;
    }
  }

  return msp(new XYJoin(self, rhs));
}

// XYJoin
XYJoin::XYJoin(intrusive_ptr<XYSequence> first,
               intrusive_ptr<XYSequence> second)
{ 
  mSequences.push_back(first);
  mSequences.push_back(second);
}

string XYJoin::toString(bool parse) const {
  ostringstream s;
  s << "[ ";
  for(const_iterator it = mSequences.begin(); it != mSequences.end(); ++it) {
    for (int i=0; i < (*it)->size(); ++i)
      s << (*it)->at(i)->toString(parse) << " ";
  }
  s << "]";
  return s.str();
}

void XYJoin::eval1(intrusive_ptr<XY> const& xy) {
  xy->mX.push_back(msp(this));
}

size_t XYJoin::size()
{
  size_t s = 0;
  for(iterator it = mSequences.begin(); it != mSequences.end(); ++it) 
    s += (*it)->size();

  return s;
}

intrusive_ptr<XYObject> XYJoin::at(size_t n)
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

void XYJoin::set_at(size_t n, intrusive_ptr<XYObject> const& v)
{
  assert(n < size());
  size_t s = 0;
  for(iterator it = mSequences.begin(); it != mSequences.end(); ++it) {
    size_t b = s;
    s += (*it)->size();
    if (n < s)
      return (*it)->set_at(n-b, v);
  }

  assert(1 == 0);
}

void XYJoin::pushBackInto(List& list)
{  
  for(iterator it = mSequences.begin(); it != mSequences.end(); ++it) {
    (*it)->pushBackInto(list);
  }
}

intrusive_ptr<XYObject> XYJoin::head()
{
  return at(0);
}

intrusive_ptr<XYSequence> XYJoin::tail()
{
  if (size() <= 1)
    return msp(new XYList());

  return msp(new XYSlice(dynamic_pointer_cast<XYSequence>(msp(this)), 1, size()));
}

boost::intrusive_ptr<XYSequence> XYJoin::join(boost::intrusive_ptr<XYSequence> const& rhs)
{
  intrusive_ptr<XYJoin> self(dynamic_pointer_cast<XYJoin>(msp(this)));

  if (dynamic_cast<XYJoin const*>(rhs.get())) {
    // If the reference to rhs is unique then we can modify the object itself.
    if (rhs->mReferences == 1) {    
      intrusive_ptr<XYJoin> result(dynamic_pointer_cast<XYJoin>(rhs));
      result->mSequences.insert(result->mSequences.begin(), 
				mSequences.begin(), mSequences.end());
      return result;
    }
    else if (self->mReferences == 2) { // to account for the 1 reference we are holding
      intrusive_ptr<XYJoin> join_rhs(dynamic_pointer_cast<XYJoin>(rhs));
      mSequences.insert(mSequences.end(), 
			join_rhs->mSequences.begin(), join_rhs->mSequences.end());
      return self;
    }
    else {
      // Pointer is shared, we have to copy the data
      intrusive_ptr<XYJoin> join_rhs(dynamic_pointer_cast<XYJoin>(rhs));
      intrusive_ptr<XYJoin> result(new XYJoin());
      result->mSequences.insert(result->mSequences.end(), 
				mSequences.begin(), mSequences.end());
      result->mSequences.insert(result->mSequences.end(), 
				join_rhs->mSequences.begin(), join_rhs->mSequences.end());
      return result;
    }
  }

  // rhs is not a join
  if (self->mReferences == 2) { // to account for the 1 reference we are holding
    mSequences.push_back(rhs);
    return self;
  }
  
  // rhs is not a join and we can't modify ourselves
  intrusive_ptr<XYJoin> result(new XYJoin());
  result->mSequences.insert(result->mSequences.end(), 
			    mSequences.begin(), mSequences.end());
  result->mSequences.push_back(rhs);
  return result;
}

// XYPrimitive
XYPrimitive::XYPrimitive(string n, void (*func)(intrusive_ptr<XY> const&)) : mName(n), mFunc(func) { }

string XYPrimitive::toString(bool) const {
  return mName;
}

void XYPrimitive::eval1(intrusive_ptr<XY> const& xy) {
  mFunc(xy);
}

int XYPrimitive::compare(intrusive_ptr<XYObject> rhs) {
  intrusive_ptr<XYPrimitive> o = dynamic_pointer_cast<XYPrimitive>(rhs);
  if (!o)
    return toString(true).compare(rhs->toString(true));

  return mName.compare(o->mName);
}
 
// Primitive Implementations

// + [X^lhs^rhs] Y] -> [X^lhs+rhs Y]
static void primitive_addition(boost::intrusive_ptr<XY> const& xy) {
  xy_assert(xy->mX.size() >= 2, XYError::STACK_UNDERFLOW);
  intrusive_ptr<XYObject> rhs(xy->mX.back());
  assert(rhs);
  xy->mX.pop_back();

  intrusive_ptr<XYObject> lhs(xy->mX.back());
  assert(lhs);
  xy->mX.pop_back();

  xy->mX.push_back(lhs->add(rhs.get()));
}

// - [X^lhs^rhs] Y] -> [X^lhs-rhs Y]
static void primitive_subtraction(boost::intrusive_ptr<XY> const& xy) {
  xy_assert(xy->mX.size() >= 2, XYError::STACK_UNDERFLOW);
  intrusive_ptr<XYObject> rhs(xy->mX.back());
  assert(rhs);
  xy->mX.pop_back();

  intrusive_ptr<XYObject> lhs(xy->mX.back());
  assert(lhs);
  xy->mX.pop_back();

  xy->mX.push_back(lhs->subtract(rhs.get()));
}

// * [X^lhs^rhs] Y] -> [X^lhs*rhs Y]
static void primitive_multiplication(boost::intrusive_ptr<XY> const& xy) {
  xy_assert(xy->mX.size() >= 2, XYError::STACK_UNDERFLOW);
  intrusive_ptr<XYObject> rhs(xy->mX.back());
  assert(rhs);
  xy->mX.pop_back();

  intrusive_ptr<XYObject> lhs(xy->mX.back());
  assert(lhs);
  xy->mX.pop_back();

  xy->mX.push_back(lhs->multiply(rhs.get()));
}

// % [X^lhs^rhs] Y] -> [X^lhs/rhs Y]
static void primitive_division(boost::intrusive_ptr<XY> const& xy) {
  xy_assert(xy->mX.size() >= 2, XYError::STACK_UNDERFLOW);
  intrusive_ptr<XYObject> rhs(xy->mX.back());
  assert(rhs);
  xy->mX.pop_back();

  intrusive_ptr<XYObject> lhs(xy->mX.back());
  assert(lhs);
  xy->mX.pop_back();

  xy->mX.push_back(lhs->divide(rhs.get()));
}

// ^ [X^lhs^rhs] Y] -> [X^lhs**rhs Y]
static void primitive_power(boost::intrusive_ptr<XY> const& xy) {
  xy_assert(xy->mX.size() >= 2, XYError::STACK_UNDERFLOW);
  intrusive_ptr<XYObject> rhs(xy->mX.back());
  assert(rhs);
  xy->mX.pop_back();

  intrusive_ptr<XYObject> lhs(xy->mX.back());
  assert(lhs);
  xy->mX.pop_back();

  xy->mX.push_back(lhs->power(rhs.get()));
}

// _ floor [X^n] Y] -> [X^n Y]
static void primitive_floor(boost::intrusive_ptr<XY> const& xy) {
  xy_assert(xy->mX.size() >= 1, XYError::STACK_UNDERFLOW);
  intrusive_ptr<XYNumber> n = dynamic_pointer_cast<XYNumber>(xy->mX.back());
  xy_assert(n, XYError::TYPE);
  xy->mX.pop_back();

  xy->mX.push_back(n->floor());
}

// set [X^value^name Y] -> [X Y] 
static void primitive_set(boost::intrusive_ptr<XY> const& xy) {
  xy_assert(xy->mX.size() >= 2, XYError::STACK_UNDERFLOW);
  intrusive_ptr<XYSymbol> name = dynamic_pointer_cast<XYSymbol>(xy->mX.back());
  xy_assert(name, XYError::TYPE);
  xy->mX.pop_back();

  intrusive_ptr<XYObject> value = xy->mX.back();
  xy->mX.pop_back();

  xy->mEnv[name->mValue] = value;
}

// get [X^name Y] [X^value Y]
static void primitive_get(boost::intrusive_ptr<XY> const& xy) {
  xy_assert(xy->mX.size() >= 1, XYError::STACK_UNDERFLOW);
  intrusive_ptr<XYSymbol> name = dynamic_pointer_cast<XYSymbol>(xy->mX.back());
  xy_assert(name, XYError::TYPE);
  xy->mX.pop_back();

  XYEnv::iterator it = xy->mEnv.find(name->mValue);
  xy_assert(it != xy->mEnv.end(), XYError::SYMBOL_NOT_FOUND);

  intrusive_ptr<XYObject> value = (*it).second;
  xy->mX.push_back(value);
}

// . [X^{O1..On} Y] [X O1^..^On^Y]
static void primitive_unquote(boost::intrusive_ptr<XY> const& xy) {
  xy_assert(xy->mX.size() >= 1, XYError::STACK_UNDERFLOW);
  intrusive_ptr<XYObject> o = xy->mX.back();
  xy->mX.pop_back();

  intrusive_ptr<XYSequence> list = dynamic_pointer_cast<XYSequence>(o);

  if (list) {
    XYStack temp;
    list->pushBackInto(temp);

    xy->mY.insert(xy->mY.begin(), temp.begin(), temp.end());
  }
  else {
    intrusive_ptr<XYSymbol> symbol = dynamic_pointer_cast<XYSymbol>(o);
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
static void primitive_pattern_ss(boost::intrusive_ptr<XY> const& xy) {
  xy_assert(xy->mX.size() >= 1, XYError::STACK_UNDERFLOW);

  // Get the pattern from the stack
  intrusive_ptr<XYSequence> pattern = dynamic_pointer_cast<XYSequence>(xy->mX.back());
  xy_assert(pattern, XYError::TYPE);
  xy->mX.pop_back();
  assert(pattern->size() != 0);

  // Populate env with a mapping between the pattern variables to the
  // values on the stack.
  XYEnv env;
  xy->getPatternValues(pattern->at(0), inserter(env, env.begin()));
  // Process pattern body using these mappings.
  if (pattern->size() > 1) {
    int start = 0;
    int end   = pattern->size();
    intrusive_ptr<XYList> list(new XYList());    
    xy->replacePattern(env, msp(new XYSlice(pattern, ++start, end)), back_inserter(list->mList));
    assert(list->size() > 0);

    // Append to stack
    list = dynamic_pointer_cast<XYList>(list->mList[0]);
    xy_assert(list, XYError::TYPE);
    xy->mX.insert(xy->mX.end(), list->mList.begin(), list->mList.end());
  }
}

// ( [X^{pattern} Y] [X result^Y]
static void primitive_pattern_sq(boost::intrusive_ptr<XY> const& xy) {
  xy_assert(xy->mX.size() >= 1, XYError::STACK_UNDERFLOW);

  // Get the pattern from the stack
  intrusive_ptr<XYSequence> pattern = dynamic_pointer_cast<XYSequence>(xy->mX.back());
  xy_assert(pattern, XYError::TYPE);
  xy->mX.pop_back();
  assert(pattern->size() != 0);

  // Populate env with a mapping between the pattern variables to the
  // values on the stack.
  XYEnv env;
  xy->getPatternValues(pattern->at(0), inserter(env, env.begin()));
  // Process pattern body using these mappings.
  if (pattern->size() > 1) {
    int start = 0;
    int end   = pattern->size();
    intrusive_ptr<XYList> list(new XYList());
    xy->replacePattern(env, msp(new XYSlice(pattern, ++start, end)), back_inserter(list->mList));
    assert(list->size() > 0);

    // Prepend to queue
    list = dynamic_pointer_cast<XYList>(list->mList[0]);
    xy_assert(list, XYError::TYPE);
    xy->mY.insert(xy->mY.begin(), list->mList.begin(), list->mList.end());
  }
}

// ` dip [X^b^{a0..an} Y] [X a0..an^b^Y]
static void primitive_dip(boost::intrusive_ptr<XY> const& xy) {
  xy_assert(xy->mX.size() >= 2, XYError::STACK_UNDERFLOW);
  intrusive_ptr<XYSequence> list = dynamic_pointer_cast<XYSequence>(xy->mX.back());
  xy_assert(list, XYError::TYPE);
  xy->mX.pop_back();

  intrusive_ptr<XYObject> o = xy->mX.back();
  assert(o);
  xy->mX.pop_back();

  xy->mY.push_front(o);
  XYStack temp;
  list->pushBackInto(temp);
  xy->mY.insert(xy->mY.begin(), temp.begin(), temp.end());
}

// | reverse [X^{a0..an} Y] [X^{an..a0} Y]
static void primitive_reverse(boost::intrusive_ptr<XY> const& xy) {
  xy_assert(xy->mX.size() >= 1, XYError::STACK_UNDERFLOW);
  intrusive_ptr<XYSequence> list = dynamic_pointer_cast<XYSequence>(xy->mX.back());
  xy_assert(list, XYError::TYPE);
  xy->mX.pop_back();

  intrusive_ptr<XYList> reversed(new XYList());
  list->pushBackInto(reversed->mList);
  reverse(reversed->mList.begin(), reversed->mList.end());
  xy->mX.push_back(reversed);
}

// \ quote [X^o Y] [X^{o} Y]
static void primitive_quote(boost::intrusive_ptr<XY> const& xy) {
  assert(xy->mY.size() >= 1);
  intrusive_ptr<XYObject> o = xy->mY.front();
  assert(o);
  xy->mY.pop_front();

  intrusive_ptr<XYList> list = msp(new XYList());
  list->mList.push_back(o);
  xy->mX.push_back(list);
}

// , join [X^a^b Y] [X^{...} Y]
static void primitive_join(boost::intrusive_ptr<XY> const& xy) {
  xy_assert(xy->mX.size() >= 2, XYError::STACK_UNDERFLOW);
  intrusive_ptr<XYObject> rhs = xy->mX.back();
  assert(rhs);
  xy->mX.pop_back();

  intrusive_ptr<XYObject> lhs = xy->mX.back();
  assert(lhs);
  xy->mX.pop_back();

  intrusive_ptr<XYSequence> list_lhs = dynamic_pointer_cast<XYSequence>(lhs);
  intrusive_ptr<XYSequence> list_rhs = dynamic_pointer_cast<XYSequence>(rhs);

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
    intrusive_ptr<XYList> list(new XYList());
    list->mList.push_back(rhs);
    xy->mX.push_back(list_lhs->join(list));
  }
  else if(list_rhs) {
    // Reset original pointers to enable copy on write optimisations
    rhs.reset(static_cast<XYSequence*>(0));

    // If lhs is not a list, it is added to the front of the list
    intrusive_ptr<XYList> list(new XYList());
    list->mList.push_back(lhs);
    xy->mX.push_back(list->join(list_rhs));
  }
  else {
    // If neither are lists, a list is made containing the two items
    intrusive_ptr<XYList> list(new XYList());
    list->mList.push_back(lhs);
    list->mList.push_back(rhs);
    xy->mX.push_back(list);
  }
}

// $ stack - Expects a program on the X stack. That program
// has stack effect ( stack queue -- stack queue ). $ will
// call this program with X and Y on the stack, and replace
// X and Y with the results.
static void primitive_stack(boost::intrusive_ptr<XY> const& xy) {
  xy_assert(xy->mX.size() >= 1, XYError::STACK_UNDERFLOW);
  intrusive_ptr<XYSequence> list  = dynamic_pointer_cast<XYSequence>(xy->mX.back());
  xy_assert(list, XYError::TYPE);
  xy->mX.pop_back();

  intrusive_ptr<XYList> stack(new XYList(xy->mX.begin(), xy->mX.end()));
  intrusive_ptr<XYList> queue(new XYList(xy->mY.begin(), xy->mY.end()));

  xy->mX.push_back(stack);
  xy->mX.push_back(queue);
  xy->mY.push_front(msp(new XYSymbol("$$")));
  XYStack temp;
  list->pushBackInto(temp);
  xy->mY.insert(xy->mY.begin(), temp.begin(), temp.end()); 
}

// $$ stackqueue - Helper word for '$'. Given a stack and queue on the
// stack, replaces the existing stack and queue with them.
static void primitive_stackqueue(boost::intrusive_ptr<XY> const& xy) {
  xy_assert(xy->mX.size() >= 2, XYError::STACK_UNDERFLOW);

  intrusive_ptr<XYSequence> queue = dynamic_pointer_cast<XYSequence>(xy->mX.back());
  xy_assert(queue, XYError::TYPE);
  xy->mX.pop_back();

  intrusive_ptr<XYSequence> stack = dynamic_pointer_cast<XYSequence>(xy->mX.back());
  xy_assert(stack, XYError::TYPE);
  xy->mX.pop_back();

  XYStack stemp;
  stack->pushBackInto(stemp);

  XYQueue qtemp;
  for( int i=0; i < queue->size(); ++i)
    qtemp.push_back(queue->at(i));

  xy->mX.assign(stemp.begin(), stemp.end());
  xy->mY.assign(qtemp.begin(), qtemp.end());
}

// = equals [X^a^b Y] [X^? Y] 
static void primitive_equals(boost::intrusive_ptr<XY> const& xy) {
  xy_assert(xy->mX.size() >= 2, XYError::STACK_UNDERFLOW);

  intrusive_ptr<XYObject> rhs = xy->mX.back();
  assert(rhs);
  xy->mX.pop_back();

  intrusive_ptr<XYObject> lhs = xy->mX.back();
  assert(lhs);
  xy->mX.pop_back();

  if (lhs->compare(rhs) == 0)
    xy->mX.push_back(msp(new XYInteger(1)));
  else
    xy->mX.push_back(msp(new XYInteger(0)));
}

// <  [X^a^b Y] [X^? Y] 
static void primitive_lessThan(boost::intrusive_ptr<XY> const& xy) {
  xy_assert(xy->mX.size() >= 2, XYError::STACK_UNDERFLOW);

  intrusive_ptr<XYObject> rhs = xy->mX.back();
  assert(rhs);
  xy->mX.pop_back();

  intrusive_ptr<XYObject> lhs = xy->mX.back();
  assert(lhs);
  xy->mX.pop_back();

  if (lhs->compare(rhs) < 0)
    xy->mX.push_back(msp(new XYInteger(1)));
  else
    xy->mX.push_back(msp(new XYInteger(0)));
}

// >  [X^a^b Y] [X^? Y] 
static void primitive_greaterThan(boost::intrusive_ptr<XY> const& xy) {
  xy_assert(xy->mX.size() >= 2, XYError::STACK_UNDERFLOW);

  intrusive_ptr<XYObject> rhs = xy->mX.back();
  assert(rhs);
  xy->mX.pop_back();

  intrusive_ptr<XYObject> lhs = xy->mX.back();
  assert(lhs);
  xy->mX.pop_back();

  if (lhs->compare(rhs) > 0)
    xy->mX.push_back(msp(new XYInteger(1)));
  else
    xy->mX.push_back(msp(new XYInteger(0)));
}

// <=  [X^a^b Y] [X^? Y] 
static void primitive_lessThanEqual(boost::intrusive_ptr<XY> const& xy) {
  xy_assert(xy->mX.size() >= 2, XYError::STACK_UNDERFLOW);

  intrusive_ptr<XYObject> rhs = xy->mX.back();
  assert(rhs);
  xy->mX.pop_back();

  intrusive_ptr<XYObject> lhs = xy->mX.back();
  assert(lhs);
  xy->mX.pop_back();

  if (lhs->compare(rhs) <= 0)
    xy->mX.push_back(msp(new XYInteger(1)));
  else
    xy->mX.push_back(msp(new XYInteger(0)));
}

// >=  [X^a^b Y] [X^? Y] 
static void primitive_greaterThanEqual(boost::intrusive_ptr<XY> const& xy) {
  xy_assert(xy->mX.size() >= 2, XYError::STACK_UNDERFLOW);

  intrusive_ptr<XYObject> rhs = xy->mX.back();
  assert(rhs);
  xy->mX.pop_back();

  intrusive_ptr<XYObject> lhs = xy->mX.back();
  assert(lhs);
  xy->mX.pop_back();

  if (lhs->compare(rhs) >= 0)
    xy->mX.push_back(msp(new XYInteger(1)));
  else
    xy->mX.push_back(msp(new XYInteger(0)));
}


// not not [X^a Y] [X^? Y] 
static void primitive_not(boost::intrusive_ptr<XY> const& xy) {
  xy_assert(xy->mX.size() >= 1, XYError::STACK_UNDERFLOW);

  intrusive_ptr<XYObject> o = xy->mX.back();
  assert(o);
  xy->mX.pop_back();

  intrusive_ptr<XYNumber> n = dynamic_pointer_cast<XYNumber>(o);
  if (n && n->is_zero()) {
    xy->mX.push_back(msp(new XYInteger(1)));
  }
  else {
    intrusive_ptr<XYSequence> l = dynamic_pointer_cast<XYSequence>(o);
    if(l && l->size() == 0)
      xy->mX.push_back(msp(new XYInteger(1)));
    else
      xy->mX.push_back(msp(new XYInteger(0)));
  }
}

// @ nth [X^n^{...} Y] [X^o Y] 
static void primitive_nth(boost::intrusive_ptr<XY> const& xy) {
  xy_assert(xy->mX.size() >= 2, XYError::STACK_UNDERFLOW);

  intrusive_ptr<XYSequence> list = dynamic_pointer_cast<XYSequence>(xy->mX.back());
  xy_assert(list, XYError::TYPE);
  xy->mX.pop_back();

  intrusive_ptr<XYObject> index(xy->mX.back());
  assert(index);
  xy->mX.pop_back();

  intrusive_ptr<XYNumber> n = dynamic_pointer_cast<XYNumber>(index);
  intrusive_ptr<XYSequence> s = dynamic_pointer_cast<XYSequence>(index);
  xy_assert(n || s, XYError::TYPE);

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
      intrusive_ptr<XYObject> head = s->head();
      intrusive_ptr<XYSequence> tail = s->tail();

      // If head is a list, then it's a request to get multiple values
      intrusive_ptr<XYSequence> headlist = dynamic_pointer_cast<XYSequence>(head);
      if (headlist && headlist->size() == 0) {
	if (tail->size() == 0) {
	  xy->mX.push_back(list);
	}
	else {
	  XYSequence::List code;
	  XYSequence::List code2;
	  for (int i=0; i < list->size(); ++i) {
	    code.push_back(tail);
	    code.push_back(list->at(i));
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
	for (int i=0; i < headlist->size(); ++ i) {
	  code.push_back(headlist->at(i));
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

// ! set-nth [X^v^n^{...} Y] [X Y] 
static void primitive_set_nth(boost::intrusive_ptr<XY> const& xy) {
  xy_assert(xy->mX.size() >= 3, XYError::STACK_UNDERFLOW);

  intrusive_ptr<XYSequence> list = dynamic_pointer_cast<XYSequence>(xy->mX.back());
  xy_assert(list, XYError::TYPE);
  xy->mX.pop_back();

  intrusive_ptr<XYNumber> index(dynamic_pointer_cast<XYNumber>(xy->mX.back()));
  xy_assert(index, XYError::TYPE);
  xy->mX.pop_back();

  intrusive_ptr<XYObject> v(xy->mX.back());
  assert(v);
  xy->mX.pop_back();

  unsigned int n = index->as_uint();
  xy_assert(n < list->size(), XYError::RANGE);

  list->set_at(n, v);
}

// print [X^n Y] [X Y] 
static void primitive_print(boost::intrusive_ptr<XY> const& xy) {
  xy_assert(xy->mX.size() >= 1, XYError::STACK_UNDERFLOW);

  intrusive_ptr<XYObject> o(xy->mX.back());
  assert(o);
  xy->mX.pop_back();

  boost::asio::streambuf buffer;
  ostream stream(&buffer);
  stream << o->toString(true);

  boost::asio::write(xy->mOutputStream, buffer);
}

// println [X^n Y] [X Y] 
static void primitive_println(boost::intrusive_ptr<XY> const& xy) {
  xy_assert(xy->mX.size() >= 1, XYError::STACK_UNDERFLOW);

  intrusive_ptr<XYObject> o(xy->mX.back());
  assert(o);
  xy->mX.pop_back();

  boost::asio::streambuf buffer;
  ostream stream(&buffer);
  stream << o->toString(true) << endl;
  boost::asio::write(xy->mOutputStream, buffer);
}


// write [X^n Y] [X Y] 
static void primitive_write(boost::intrusive_ptr<XY> const& xy) {
  xy_assert(xy->mX.size() >= 1, XYError::STACK_UNDERFLOW);

  intrusive_ptr<XYObject> o(xy->mX.back());
  assert(o);
  xy->mX.pop_back();

  boost::asio::streambuf buffer;
  ostream stream(&buffer);
  stream << o->toString(false);
  boost::asio::write(xy->mOutputStream, buffer);
}

// count [X^{...} Y] [X^n Y] 
// Returns the length of any list. If the item at the top of the
// stack is an atom, returns 1.
static void primitive_count(boost::intrusive_ptr<XY> const& xy) {
  xy_assert(xy->mX.size() >= 1, XYError::STACK_UNDERFLOW);

  intrusive_ptr<XYObject> o(xy->mX.back());
  assert(o);
  xy->mX.pop_back();

  intrusive_ptr<XYSequence> list(dynamic_pointer_cast<XYSequence>(o));
  if (list)
    xy->mX.push_back(msp(new XYInteger(list->size())));
  else {
    intrusive_ptr<XYString> s(dynamic_pointer_cast<XYString>(o));
    if (s)
      xy->mX.push_back(msp(new XYInteger(s->mValue.size())));
    else
      xy->mX.push_back(msp(new XYInteger(1)));
  }
}

// split [X^string^seps Y] [X^{...} Y] 
// Splits a string into an array of strings, splitting
// on the specified characters.
static void primitive_split(boost::intrusive_ptr<XY> const& xy) {
  xy_assert(xy->mX.size() >= 2, XYError::STACK_UNDERFLOW);

  intrusive_ptr<XYString> seps(dynamic_pointer_cast<XYString>(xy->mX.back()));
  xy_assert(seps, XYError::TYPE);
  xy->mX.pop_back();

  intrusive_ptr<XYString> str(dynamic_pointer_cast<XYString>(xy->mX.back()));
  xy_assert(str, XYError::TYPE);
  xy->mX.pop_back();

  vector<string> result;
  split(result, str->mValue, is_any_of(seps->mValue));
 
  intrusive_ptr<XYList> list(new XYList());
  for (vector<string>::iterator it = result.begin(); it != result.end(); ++it)
    list->mList.push_back(msp(new XYString(*it)));
  xy->mX.push_back(list);
}

// sdrop [X^seq^n Y] [X^{...} Y] 
// drops n items from the beginning of the sequence
static void primitive_sdrop(boost::intrusive_ptr<XY> const& xy) {
  xy_assert(xy->mX.size() >= 2, XYError::STACK_UNDERFLOW);

  intrusive_ptr<XYNumber> n(dynamic_pointer_cast<XYNumber>(xy->mX.back()));
  xy_assert(n, XYError::TYPE);
  xy->mX.pop_back();
  
  intrusive_ptr<XYString>   str(dynamic_pointer_cast<XYString>(xy->mX.back()));
  intrusive_ptr<XYSequence> seq(dynamic_pointer_cast<XYSequence>(xy->mX.back()));
  xy_assert(str || seq, XYError::TYPE);
  xy->mX.pop_back();

  if (str) {
    if (n->as_uint() >= str->mValue.size())
      xy->mX.push_back(msp(new XYString("")));
    else
      xy->mX.push_back(msp(new XYString(str->mValue.substr(n->as_uint()))));
  }
  else {
    // TODO
    assert(1 == 0);
  }
}

// stake [X^seq^n Y] [X^{...} Y] 
// takes n items from the beginning of the sequence
static void primitive_stake(boost::intrusive_ptr<XY> const& xy) {
  xy_assert(xy->mX.size() >= 2, XYError::STACK_UNDERFLOW);

  intrusive_ptr<XYNumber> n(dynamic_pointer_cast<XYNumber>(xy->mX.back()));
  xy_assert(n, XYError::TYPE);
  xy->mX.pop_back();
  
  intrusive_ptr<XYString>   str(dynamic_pointer_cast<XYString>(xy->mX.back()));
  intrusive_ptr<XYSequence> seq(dynamic_pointer_cast<XYSequence>(xy->mX.back()));
  xy_assert(str || seq, XYError::TYPE);
  xy->mX.pop_back();

  if (str) {
    xy->mX.push_back(msp(new XYString(str->mValue.substr(0, n->as_uint()))));
  }
  else {
    // TODO
    assert(1 == 0);
  }
}

// Forward declare tokenize function for tokenize primitive
template <class InputIterator, class OutputIterator>
void tokenize(InputIterator first, InputIterator last, OutputIterator out);

// tokenize [X^s Y] [X^{tokens} Y] 
// Given a string, returns a list of cf tokens
static void primitive_tokenize(boost::intrusive_ptr<XY> const& xy) {
  xy_assert(xy->mX.size() >= 1, XYError::STACK_UNDERFLOW);

  intrusive_ptr<XYString> s(dynamic_pointer_cast<XYString>(xy->mX.back()));
  xy_assert(s, XYError::TYPE);
  xy->mX.pop_back();

  vector<string> tokens;
  tokenize(s->mValue.begin(), s->mValue.end(), back_inserter(tokens));

  intrusive_ptr<XYList> result(new XYList());
  for(vector<string>::iterator it=tokens.begin(); it != tokens.end(); ++it)
    result->mList.push_back(msp(new XYString(*it)));

  xy->mX.push_back(result);
}

// parse [X^{tokens} Y] [X^{...} Y] 
// Given a list of tokens, parses it and returns the program
static void primitive_parse(boost::intrusive_ptr<XY> const& xy) {
  xy_assert(xy->mX.size() >= 1, XYError::STACK_UNDERFLOW);

  intrusive_ptr<XYSequence> tokens(dynamic_pointer_cast<XYSequence>(xy->mX.back()));
  xy_assert(tokens, XYError::TYPE);
  xy->mX.pop_back();

  vector<string> strings;
  for (int i=0; i < tokens->size(); ++i) {
    intrusive_ptr<XYString> s = dynamic_pointer_cast<XYString>(tokens->at(i));
    xy_assert(s, XYError::TYPE);
    strings.push_back(s->mValue);
  }

  intrusive_ptr<XYList> result(new XYList());
  parse(strings.begin(), strings.end(), back_inserter(result->mList));
  xy->mX.push_back(result);
}

// Asynchronous handler for getline
static void getlineHandler(intrusive_ptr<XY> const& xy, boost::system::error_code const& err) {
  if (!err) {
    istream stream(&xy->mInputBuffer);
    string input;
    std::getline(stream, input);
    xy->mX.push_back(msp(new XYString(input)));
    xy->mService.post(bind(&XY::evalHandler, xy));
  }
}

// getline [X Y] [X^".." Y] 
// Get a line of input from the user
static void primitive_getline(boost::intrusive_ptr<XY> const& xy) {
  boost::asio::async_read_until(xy->mInputStream,
				xy->mInputBuffer,
				"\n",
				bind(getlineHandler, xy, boost::asio::placeholders::error));

  throw XYError(xy, XYError::WAITING_FOR_ASYNC_EVENT);
} 

// millis [X Y] [X^m Y]
// Runs the number of milliseconds on the stack since
// 1 Janary 1970.
static void primitive_millis(boost::intrusive_ptr<XY> const& xy) {
  using namespace boost::posix_time;
  using namespace boost::gregorian;

  ptime e(microsec_clock::universal_time());
  ptime s(date(1970,1,1));

  time_duration d(e - s);

  xy->mX.push_back(msp(new XYInteger(d.total_milliseconds())));
}

// enum [X^n Y] -> [X^{0..n} Y]
static void primitive_enum(boost::intrusive_ptr<XY> const& xy) {
  xy_assert(xy->mX.size() >= 1, XYError::STACK_UNDERFLOW);
  intrusive_ptr<XYNumber> n = dynamic_pointer_cast<XYNumber>(xy->mX.back());
  xy_assert(n, XYError::TYPE);
  xy->mX.pop_back();

  int value = n->as_uint();
  intrusive_ptr<XYList> list = msp(new XYList());
  for(int i=0; i < value; ++i)
    list->mList.push_back(msp(new XYInteger(i)));
  xy->mX.push_back(list);
}

// clone [X^o Y] -> [X^o Y]
static void primitive_clone(boost::intrusive_ptr<XY> const& xy) {
  // TODO: Only works for sequences for now
  // Implemented to work around an XYJoin limitation
  xy_assert(xy->mX.size() >= 1, XYError::STACK_UNDERFLOW);
  intrusive_ptr<XYSequence> o = dynamic_pointer_cast<XYSequence>(xy->mX.back());
  xy_assert(o, XYError::TYPE);
  xy->mX.pop_back();

  intrusive_ptr<XYList> r(new XYList());
  o->pushBackInto(r->mList);
  xy->mX.push_back(r);
}

// clone [X^o Y] -> [X^string Y]
static void primitive_to_string(boost::intrusive_ptr<XY> const& xy) {
  xy_assert(xy->mX.size() >= 1, XYError::STACK_UNDERFLOW);
  intrusive_ptr<XYObject> o(xy->mX.back());
  xy->mX.pop_back();

  xy->mX.push_back(msp(new XYString(o->toString(true))));
}

// foldl [X^seq^seed^quot Y] -> [X^seq Y]
// [1 2 3] 0 [+] foldl
// [2 3] 0 1 + [+] foldl
// [3] 1 2 + [+] foldl
// [] 3 3 + [+] foldl
// 6
static void primitive_foldl(boost::intrusive_ptr<XY> const& xy) {
  xy_assert(xy->mX.size() >= 3, XYError::STACK_UNDERFLOW);
  intrusive_ptr<XYSequence> quot(dynamic_pointer_cast<XYSequence>(xy->mX.back()));
  xy_assert(quot, XYError::TYPE);
  xy->mX.pop_back();

  intrusive_ptr<XYObject> seed(xy->mX.back());
  xy->mX.pop_back();

  intrusive_ptr<XYSequence> seq(dynamic_pointer_cast<XYSequence>(xy->mX.back()));
  xy_assert(seq, XYError::TYPE);
  xy->mX.pop_back();
			     
  if (seq->size() == 0) {
    xy->mX.push_back(seed);
  }
  else {
    intrusive_ptr<XYObject> head(seq->head());
    intrusive_ptr<XYSequence> tail(seq->tail());
  
    XYStack temp;
    temp.push_back(tail);
    temp.push_back(seed);
    temp.push_back(head);    
    temp.push_back(quot);
    temp.push_back(msp(new XYPrimitive(".", primitive_unquote)));
    temp.push_back(quot);
    temp.push_back(msp(new XYPrimitive("foldl", primitive_foldl)));
    
    xy->mY.insert(xy->mY.begin(), temp.begin(), temp.end());    
  }
}

// foldr [X^seq^seed^quot Y] -> [X^seq Y]
// [1 2 3] 0 [+] foldr
// 0 [2 3] 1 [+] foldr +
// 0 1 [3] 2 [+] foldr + +
// 0 1 2 [] 3 [+] foldr + + +
// 0 1 2 3  + + +
// 6
static void primitive_foldr(boost::intrusive_ptr<XY> const& xy) {
  xy_assert(xy->mX.size() >= 3, XYError::STACK_UNDERFLOW);
  intrusive_ptr<XYSequence> quot(dynamic_pointer_cast<XYSequence>(xy->mX.back()));
  xy_assert(quot, XYError::TYPE);
  xy->mX.pop_back();

  intrusive_ptr<XYObject> seed(xy->mX.back());
  xy->mX.pop_back();

  intrusive_ptr<XYSequence> seq(dynamic_pointer_cast<XYSequence>(xy->mX.back()));
  xy_assert(seq, XYError::TYPE);
  xy->mX.pop_back();
			     
  if (seq->size() == 0) {
    xy->mX.push_back(seed);
  }
  else {
    intrusive_ptr<XYObject> head(seq->head());
    intrusive_ptr<XYSequence> tail(seq->tail());
    
    XYStack temp;
    temp.push_back(seed);
    temp.push_back(tail);
    temp.push_back(head);    
    temp.push_back(quot);
    temp.push_back(msp(new XYPrimitive("foldr", primitive_foldr)));
    temp.push_back(quot);
    temp.push_back(msp(new XYPrimitive(".", primitive_unquote)));
    
    xy->mY.insert(xy->mY.begin(), temp.begin(), temp.end());    
  }
}

// if [X^bool^then^else Y] -> [X Y]
// 2 1 = [ ... ] [ ... ] if
static void primitive_if(boost::intrusive_ptr<XY> const& xy) {
  xy_assert(xy->mX.size() >= 3, XYError::STACK_UNDERFLOW);
  intrusive_ptr<XYSequence> else_quot(dynamic_pointer_cast<XYSequence>(xy->mX.back()));
  xy_assert(else_quot, XYError::TYPE);
  xy->mX.pop_back();

  intrusive_ptr<XYSequence> then_quot(dynamic_pointer_cast<XYSequence>(xy->mX.back()));
  xy_assert(then_quot, XYError::TYPE);
  xy->mX.pop_back();

  intrusive_ptr<XYObject> o(xy->mX.back());
  intrusive_ptr<XYNumber> num(dynamic_pointer_cast<XYNumber>(o));
  intrusive_ptr<XYSequence> seq(dynamic_pointer_cast<XYSequence>(o));
  xy_assert(o || num || seq, XYError::TYPE);
  xy->mX.pop_back();

  if (num && num->is_zero() ||
      seq && seq->size() == 0) {
    XYStack temp;
    else_quot->pushBackInto(temp);
    xy->mY.insert(xy->mY.begin(), temp.begin(), temp.end());    
  }
  else {
    XYStack temp;
    then_quot->pushBackInto(temp);
    xy->mY.insert(xy->mY.begin(), temp.begin(), temp.end());    
  } 
}

// ? find [X^seq^elt Y] -> [X^index Y]
static void primitive_find(boost::intrusive_ptr<XY> const& xy) {
  xy_assert(xy->mX.size() >= 2, XYError::STACK_UNDERFLOW);
  intrusive_ptr<XYObject> elt(xy->mX.back());
  xy->mX.pop_back();

  intrusive_ptr<XYSequence> seq(dynamic_pointer_cast<XYSequence>(xy->mX.back()));
  xy_assert(seq, XYError::TYPE);
  xy->mX.pop_back();

  int i=0;
  for (i=0; i < seq->size(); ++i) {
    if(seq->at(i)->compare(elt) == 0)
      break;
  }

  xy->mX.push_back(msp(new XYInteger(i)));
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

// XYError
XYError::XYError(intrusive_ptr<XY> const& xy, code c) :
  mXY(xy),
  mCode(c),
  mLine(-1),
  mFile(0)
{
}

XYError::XYError(intrusive_ptr<XY> const& xy, code c, char const* file, int line) :
  mXY(xy),
  mCode(c),
  mLine(line),
  mFile(file)
{
}

string XYError::message() {
  ostringstream str;
  switch(mCode) {
  case LIMIT_REACHED:
    str << "Time limit to run code has been exceeded";
    break;

  case STACK_UNDERFLOW:
    str << "Stack underflow";
    break;

  case SYMBOL_NOT_FOUND:
    str << "Symbol not found";
    break;

  case TYPE:
    str << "Type error";
    break;

  default:
    return "Unknown error";
  }

  if (mLine >= 0)
    str << " at line " << mLine;

  if (mFile)
    str << " in file " << mFile;

  str << ".";
  return str.str();
}

// XY
XY::XY(boost::asio::io_service& service) :
  XYReference(),
  mService(service),
  mInputStream(service, ::dup(STDIN_FILENO)),
  mOutputStream(service, ::dup(STDOUT_FILENO)),
  mRepl(true) {
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
  mP["!"]   = msp(new XYPrimitive("!", primitive_set_nth));
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
  mP["to-string"] = msp(new XYPrimitive("to-string", primitive_to_string));
  mP["split"] = msp(new XYPrimitive("split", primitive_split));
  mP["sdrop"] = msp(new XYPrimitive("sdrop", primitive_sdrop));
  mP["stake"] = msp(new XYPrimitive("stake", primitive_stake));
  mP["foldl"] = msp(new XYPrimitive("foldl", primitive_foldl));
  mP["foldr"] = msp(new XYPrimitive("foldr", primitive_foldr));
  mP["if"] = msp(new XYPrimitive("if", primitive_if));
  mP["?"] = msp(new XYPrimitive("?", primitive_find));
}

void XY::stdioHandler(boost::system::error_code const& err) {
  if (!err) {
    istream stream(&mInputBuffer);
    string input;
    std::getline(stream, input);
    parse(input, back_inserter(mY));

    // Start the limit counting here for stdio/repl based code
    for(XYLimits::iterator it = mLimits.begin(); it != mLimits.end(); ++it) {
      (*it)->start(this);
    }

    mService.post(bind(&XY::evalHandler, msp(this)));
  }
  else if (err != boost::asio::error::eof) {
    boost::asio::streambuf buffer;
    ostream stream(&buffer);
    stream << "Input error: " << err << endl;
    boost::asio::write(mOutputStream, buffer);
  }
  else {
    mService.stop();
  }
}

void XY::evalHandler() {
  try {
    eval1();
    if (mY.size() != 0)
      checkLimits();
    if (mY.size() == 0 && mRepl) {
      print();
      boost::asio::streambuf buffer;
      ostream stream(&buffer);
      stream << "ok ";
      boost::asio::write(mOutputStream, buffer);
      boost::asio::async_read_until(mInputStream,
				    mInputBuffer,
				    "\n",
				    bind(&XY::stdioHandler, msp(this), boost::asio::placeholders::error));
    }
    else if(mY.size() == 0 && !mRepl) {
      // We've completed. Inform waiting interpreters we're done.
      if (mWaiting.size() > 0) {
	for(vector<intrusive_ptr<XY> >::iterator it = mWaiting.begin(); it != mWaiting.end(); ++it ) {
	  (*it)->mService.post(bind(&XY::evalHandler, (*it)));
	}
	mWaiting.clear();
      }
    }
    else {
      mService.post(bind(&XY::evalHandler, msp(this)));
    }
  }
  catch(XYError& e) {
    if (e.mCode != XYError::WAITING_FOR_ASYNC_EVENT) {
      // When an error occurs, create a list to hold:
      // 1: The 'error' symbol
      // 2. The error message
      // 3. The stack at the time of the error
      // 4. The queue at the time of the error
      // Empty the stack and queue of the current interpreter
      // and leave the error result on the stack.
      boost::asio::streambuf buffer;
      ostream stream(&buffer);
      stream << "Error: " << e.message() << endl;
      boost::asio::write(mOutputStream, buffer);

      intrusive_ptr<XYList> stack(new XYList(mX.begin(), mX.end()));
      intrusive_ptr<XYList> queue(new XYList(mY.begin(), mY.end()));
      intrusive_ptr<XYList> error(new XYList());

      mX.clear();
      mY.clear();

      error->mList.push_back(msp(new XYSymbol("error")));
      error->mList.push_back(msp(new XYString(e.message())));
      error->mList.push_back(stack);
      error->mList.push_back(queue);
      mX.push_back(error);
      // We've completed. Inform waiting interpreters we're done.
      if (!mRepl && mWaiting.size() > 0) {
	for(vector<intrusive_ptr<XY> >::iterator it = mWaiting.begin(); it != mWaiting.end(); ++it ) {
	  (*it)->mService.post(bind(&XY::evalHandler, (*it)));
	}
	mWaiting.clear();
      }

      if (mRepl) {
	for(XYLimits::iterator it = mLimits.begin(); it != mLimits.end(); ++it) {
	  (*it)->start(this);
	}

	mService.post(bind(&XY::evalHandler, msp(this)));
      }
    }
  }
}

void XY::yield() {
  mService.post(bind(&XY::evalHandler, msp(this)));
  throw XYError(msp(this), XYError::WAITING_FOR_ASYNC_EVENT);
}

void XY::checkLimits() {
  for(XYLimits::iterator it = mLimits.begin(); it != mLimits.end(); ++it) {
    if ((*it)->check(this)) {
      // This limit was reached, stop executing and throw the error
      throw XYError(msp(this), XYError::LIMIT_REACHED);
    }
  }
}

void XY::print() {
  boost::asio::streambuf buffer;
  ostream stream(&buffer);

  for (XYStack::iterator it = mX.begin(); it != mX.end(); ++it)
    stream << (*it)->toString(true) << " ";
  stream << " -> ";
  for (XYQueue::iterator it = mY.begin(); it != mY.end(); ++it)
    stream << (*it)->toString(true) << " ";
  stream << endl;
  boost::asio::write(mOutputStream, buffer);
}

void XY::eval1() {
  if (mY.size() == 0)
    return;

  intrusive_ptr<XYObject> o = mY.front();
  assert(o);

  mY.pop_front();

  o->eval1(msp(this));
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
               intrusive_ptr<XYObject> object,
               intrusive_ptr<XYObject> pattern,
               intrusive_ptr<XYSequence> sequence,
               int i) {
  intrusive_ptr<XYSequence> object_list = dynamic_pointer_cast<XYSequence>(object);
  intrusive_ptr<XYSequence> pattern_list = dynamic_pointer_cast<XYSequence>(pattern);
  intrusive_ptr<XYSymbol> pattern_symbol = dynamic_pointer_cast<XYSymbol>(pattern);
  if (object_list && pattern_list) {
    int pi = 0;
    int oi = 0;
    for(pi=0, oi=0; pi < pattern_list->size() && oi < object_list->size(); ++pi, ++oi) {
      match(out, object_list->at(oi), pattern_list->at(pi), object_list, oi);
    }
    // If there are more pattern items than there are list items,
    // set the pattern value to null.
    while(pi < pattern_list->size()) {
      intrusive_ptr<XYSymbol> s = dynamic_pointer_cast<XYSymbol>(pattern_list->at(pi));
      if (s) {
        *out++ = make_pair(s->mValue, msp(new XYList()));
      }
      ++pi;
    }
  }
  else if(pattern_list && !object_list) {
    // If the pattern is a list, but the object is not, 
    // pretend the object is a one element list. This enables:
    // 42 [ [[a A]] a A ] -> 42 []
    intrusive_ptr<XYList> list(new XYList());
    list->mList.push_back(object);
    match(out, list, pattern, sequence, i);
  }
  else if(pattern_symbol) {
    string uppercase = pattern_symbol->mValue;
    to_upper(uppercase);
    if (uppercase == pattern_symbol->mValue) {
      *out++ = make_pair(pattern_symbol->mValue, new XYSlice(sequence, i, sequence->size()));
    }
    else
      *out++ = make_pair(pattern_symbol->mValue, object);
  }
}

template <class OutputIterator>
void XY::getPatternValues(intrusive_ptr<XYObject> pattern, OutputIterator out) {
  intrusive_ptr<XYSequence> list = dynamic_pointer_cast<XYSequence>(pattern);
  if (list) {
    assert(mX.size() >= list->size());
    intrusive_ptr<XYList> stack(new XYList(mX.end() - list->size(), mX.end()));
    match(out, stack, pattern, stack, 0);
    mX.resize(mX.size() - list->size());
  }
  else {
    intrusive_ptr<XYObject> o = mX.back();
    mX.pop_back();
    intrusive_ptr<XYList> list(new XYList());
    match(out, o, pattern, list, list->size());
  }
}
 
template <class OutputIterator>
void XY::replacePattern(XYEnv const& env, intrusive_ptr<XYObject> object, OutputIterator out) {
  intrusive_ptr<XYSequence> list   = dynamic_pointer_cast<XYSequence>(object);
  intrusive_ptr<XYSymbol>   symbol = dynamic_pointer_cast<XYSymbol>(object); 
  if (list) {
    // Recurse through the list replacing variables as needed
    intrusive_ptr<XYList> new_list(new XYList());
    for(int i=0; i < list->size(); ++i)
      replacePattern(env, list->at(i), back_inserter(new_list->mList));
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
  using boost::xpressive::optional;
  return optional('-') >> +_d;
}

// Return regex for tokenizing floats
boost::xpressive::sregex re_float() {
  using namespace boost::xpressive;
  using boost::xpressive::optional;
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
