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
#include <set>
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
  XYObject* class::name(XYObject* rhs) { return rhs->name(this); } \
  XYObject* class::name(XYFloat* lhs) { return dd_##name(lhs, this); } \
  XYObject* class::name(XYInteger* lhs) { return dd_##name(lhs, this); } \
  XYObject* class::name(XYSequence* lhs) { return dd_##name(lhs, this); }

#define DD_IMPL2(name, op) \
static XYObject* dd_##name(XYFloat* lhs, XYFloat* rhs) { \
  return new XYFloat(lhs->mValue op rhs->mValue);	 \
} \
\
static XYObject* dd_##name(XYFloat* lhs, XYInteger* rhs) { \
  return new XYFloat(lhs->mValue op rhs->as_float()->mValue); \
} \
\
static XYObject* dd_##name(XYFloat* lhs, XYSequence* rhs) { \
  XYList* list(new XYList()); \
  size_t len = rhs->size();\
  for(int i=0; i < len; ++i)\
    list->mList.push_back(lhs->name(rhs->at(i)));	\
  return list; \
}\
\
static XYObject* dd_##name(XYInteger* lhs, XYFloat* rhs) { \
  return new XYFloat(lhs->as_float()->mValue op rhs->mValue); \
} \
\
static XYObject* dd_##name(XYInteger* lhs, XYInteger* rhs) { \
  return new XYInteger(lhs->mValue op rhs->mValue); \
} \
\
static XYObject* dd_##name(XYInteger* lhs, XYSequence* rhs) { \
  XYList* list(new XYList()); \
  size_t len = rhs->size();\
  for(int i=0; i < len; ++i)\
    list->mList.push_back(lhs->name(rhs->at(i)));	\
  return list; \
} \
\
static XYObject* dd_##name(XYSequence* lhs, XYObject* rhs) { \
  XYList* list(new XYList()); \
  size_t len = lhs->size();\
  for(int i=0; i < len; ++i)\
    list->mList.push_back(lhs->at(i)->name(rhs));	\
  return list; \
} \
\
static XYObject* dd_##name(XYSequence* lhs, XYSequence* rhs) { \
  assert(lhs->size() == rhs->size()); \
  XYList* list(new XYList()); \
  size_t lhs_len = lhs->size();\
  size_t rhs_len = rhs->size();\
  for(int li = 0, ri = 0; li < lhs_len && ri < rhs_len; ++li, ++ri)\
    list->mList.push_back(lhs->at(li)->name(rhs->at(ri)));    \
  return list; \
}

DD_IMPL2(add, +)
DD_IMPL2(subtract, -)
DD_IMPL2(multiply, *)

static XYObject* dd_divide(XYFloat* lhs, XYFloat* rhs) {
  return new XYFloat(lhs->mValue / rhs->mValue);
}

static XYObject* dd_divide(XYFloat* lhs, XYInteger* rhs) {
  return new XYFloat(lhs->mValue / rhs->as_float()->mValue);
}

static XYObject* dd_divide(XYFloat* lhs, XYSequence* rhs) { 
  XYList* list(new XYList());
  size_t len = rhs->size();
  for (int i=0; i < len; ++i)
    list->mList.push_back(lhs->divide(rhs->at(i)));
  return list;
}

static XYObject* dd_divide(XYInteger* lhs, XYFloat* rhs) {
  return new XYFloat(lhs->as_float()->mValue / rhs->mValue);
}

static XYObject* dd_divide(XYInteger* lhs, XYInteger* rhs) {
  return new XYFloat(lhs->as_float()->mValue / rhs->as_float()->mValue);
}

static XYObject* dd_divide(XYInteger* lhs, XYSequence* rhs) {
  XYList* list(new XYList());
  size_t len = rhs->size();
  for (int i=0; i < len; ++i)
    list->mList.push_back(lhs->divide(rhs->at(i)));
  return list;
}

static XYObject* dd_divide(XYSequence* lhs, XYObject* rhs) {
  XYList* list(new XYList());
  size_t len = lhs->size();
  for (int i=0; i < len; ++i)
    list->mList.push_back(lhs->at(i)->divide(rhs));
  return list;
}


static XYObject* dd_divide(XYSequence* lhs, XYSequence* rhs) {
  assert(lhs->size() == rhs->size());
  XYList* list(new XYList());
  size_t lhs_len = lhs->size();
  size_t rhs_len = rhs->size();
  for(int li=0, ri=0; li < lhs_len && ri < rhs_len; ++li, ++ri)
    list->mList.push_back(lhs->at(li)->divide(rhs->at(ri)));
  return list; 
}

static XYObject* dd_power(XYFloat* lhs, XYFloat* rhs) {
  return new XYFloat(pow(static_cast<double>(lhs->mValue.get_d()), 
			 static_cast<double>(rhs->mValue.get_d())));
}

static XYObject* dd_power(XYFloat* lhs, XYInteger* rhs) {
  XYFloat* result(new XYFloat(lhs->mValue));
  mpf_pow_ui(result->mValue.get_mpf_t(), lhs->mValue.get_mpf_t(), rhs->as_uint());
  return result;
}

static XYObject* dd_power(XYFloat* lhs, XYSequence* rhs) {
  XYList* list(new XYList());
  size_t len = rhs->size();
  for (int i=0; i < len; ++i)
    list->mList.push_back(lhs->power(rhs->at(i)));
  return list;
}

static XYObject* dd_power(XYInteger* lhs, XYFloat* rhs) {
  return new XYFloat(pow(static_cast<double>(lhs->mValue.get_d()), 
			 static_cast<double>(rhs->mValue.get_d())));
}

static XYObject* dd_power(XYInteger* lhs, XYInteger* rhs) {
  XYInteger* result(new XYInteger(lhs->mValue));
  mpz_pow_ui(result->mValue.get_mpz_t(), lhs->mValue.get_mpz_t(), rhs->as_uint());
  return result;
}

static XYObject* dd_power(XYInteger* lhs, XYSequence* rhs) {
  XYList* list(new XYList());
  size_t len = rhs->size();
  for (int i=0; i < len; ++i)
    list->mList.push_back(lhs->power(rhs->at(i)));
  return list;
}

static XYObject* dd_power(XYSequence* lhs, XYObject* rhs) {
  XYList* list(new XYList());
  size_t len = lhs->size();
  for (int i=0; i < len; ++i)
    list->mList.push_back(lhs->at(i)->power(rhs));
  return list;
}


static XYObject* dd_power(XYSequence* lhs, XYSequence* rhs) {
  assert(lhs->size() == rhs->size());
  XYList* list(new XYList());
  size_t lhs_len = lhs->size();
  size_t rhs_len = rhs->size();
  for (int li=0, ri=0; li < lhs_len && ri < rhs_len; ++li, ++ri)
    list->mList.push_back(lhs->at(li)->power(rhs->at(ri)));
  return list;
}

// XYSlot
XYSlot::XYSlot(XYObject* method, XYObject* value, bool parent) :
  mMethod(method),
  mValue(value),
  mParent(parent) 
{
}

void XYSlot::markChildren() {
  if (mMethod)
    mMethod->mark();
  if (mValue)
    mValue->mark();
}

// XYObject
XYObject::XYObject() { }

void XYObject::markChildren() {
  for (Slots::iterator it = mSlots.begin(); 
       it != mSlots.end(); 
       ++it) {
    (*it).second->mark();
  }
}

XYSlot* XYObject::lookup(std::string const& name, 
			 std::set<XYObject*>& circular,
			 XYObject** context) {
  assert(name.size() > 0);

  if (circular.find(this) != circular.end())
    return 0;

  circular.insert(this);

  Slots::iterator it = mSlots.find(name);
  if (it == mSlots.end()) {
    // Could not find name in the slots of this
    // object. Look for the name in the slots of
    // the parents.
    for (Slots::iterator it = mSlots.begin();
	 it != mSlots.end();
	 ++it) {
      XYSlot* slot = (*it).second;
      if (slot->mParent) {
	XYSlot* found = slot->mValue->lookup(name, circular, context);
	if (found)
	  return found;
      }
    }
    // Not found in any parent object, so slot does not exist
    return 0;
  }

  // Found the slot in this object. Store the object that the slot
  // was found in, if requested, as the context.
  if (context)
    *context = this;

  return (*it).second;
}

XYSlot* XYObject::getSlot(string const& name) {
  assert(name.size() > 0);
  assert(mSlots.find(name) != mSlots.end());
  return mSlots[name];
}

void XYObject::addSlot(std::string const& name, 
		       XYObject* method,
		       XYObject* value,
		       bool parent) {
  assert(name.size() > 0);
  assert(method);
  assert(mSlots.find(name) == mSlots.end());

  mSlots[name] = new XYSlot(method, value, parent);
}
  
void XYObject::removeSlot(std::string const& name) {
  assert(name.size() > 0);
  assert(mSlots.find(name) != mSlots.end());
  
  mSlots.erase(name);
}

void XYObject::addSlot(std::string const& n, XYObject* value, bool readOnly) {
  assert(n.size() > 0);
  assert(value);

  bool parent = false;
  string name = n;
  if (name[name.size()-1] == '*') {
    name = name.substr(0, name.size() - 1);
    parent = true;
  }
  XYList* getter = new XYList();
  getter->mList.push_back(new XYString(name));
  getter->mList.push_back(new XYSymbol("get-slot-value"));
  addSlot(name, getter, value, parent);

  if (!readOnly) {
    XYList* setter = new XYList();
    setter->mList.push_back(new XYString(name));
    setter->mList.push_back(new XYSymbol("set-slot-value"));
    addSlot(name + ":", setter, 0, parent);
  }
}

void XYObject::addMethod(std::string const& name, XYList* method) {
  XYObject* o = new XYObject();
  o->addSlot("code", method);
  o->addSlot("args", new XYList());
  addMethod(name, o);
}

void XYObject::addMethod(std::string const& name, XYPrimitive* method) {
  XYList* list = new XYList();
  list->mList.push_back(method);
  addMethod(name, list);
}

void XYObject::addMethod(std::string const& name, XYObject* method) {
  // Convert the method to a quotation that does the work of cloning it,
  // install the frame, etc.
  XYList* frameHandler = new XYList();
  frameHandler->mList.push_back(method);
  frameHandler->mList.push_back(new XYSymbol("call-method"));
  addSlot(name, frameHandler, 0, false);
}

XYObject* XYObject::copy() const {
  XYObject* o = new XYObject();
  for (Slots::const_iterator it = mSlots.begin();
       it != mSlots.end();
       ++it) {
    o->mSlots[(*it).first] = new XYSlot(*((*it).second));
  }
  return o;
}

void XYObject::eval1(XY* xy) {
  xy->mX.push_back(this);
}

void XYObject::print(ostringstream& stream, CircularSet& seen, bool parse) const {
  if (seen.find(this) != seen.end()) {
    stream << "(circular)";
  }
  else {
    seen.insert(this);
    stream << "(| ";
    for (Slots::const_iterator it = mSlots.begin();
	 it != mSlots.end();
	 ++it) {
      string name = (*it).first;
      XYSlot* slot = (*it).second;
      stream << name;
      if (slot->mParent)
	stream << '*';
      stream << "=";
      if (slot->mValue) 
	slot->mValue->print(stream, seen, parse);
      else
	stream << "{" << slot->mMethod << "}";
      stream << " ";
    }
    stream << "|)";
  }
}

int XYObject::compare(XYObject* rhs) {
  if (this < rhs)
    return -1;
  else if (this > rhs)
    return 1;
  
  return 0;
}

string XYObject::toString(bool parse) const {
  CircularSet printed;
  ostringstream str;
  print(str, printed, parse);
  return str.str();
}

XYObject* XYObject::add(XYObject* rhs) {
  assert(1==0);
}

XYObject* XYObject::add(XYFloat* rhs) {
  assert(1==0);
}

XYObject* XYObject::add(XYInteger* rhs) {
  assert(1==0);
}

XYObject* XYObject::add(XYSequence* rhs) {
  assert(1==0);
}

XYObject* XYObject::subtract(XYObject* rhs) {
  assert(1==0);
}

XYObject* XYObject::subtract(XYFloat* rhs) {
  assert(1==0);
}

XYObject* XYObject::subtract(XYInteger* rhs) {
  assert(1==0);
}

XYObject* XYObject::subtract(XYSequence* rhs) {
  assert(1==0);
}

XYObject* XYObject::multiply(XYObject* rhs) {
  assert(1==0);
}

XYObject* XYObject::multiply(XYFloat* rhs) {
  assert(1==0);
}

XYObject* XYObject::multiply(XYInteger* rhs) {
  assert(1==0);
}

XYObject* XYObject::multiply(XYSequence* rhs) {
  assert(1==0);
}

XYObject* XYObject::divide(XYObject* rhs) {
  assert(1==0);
}

XYObject* XYObject::divide(XYFloat* rhs) {
  assert(1==0);
}

XYObject* XYObject::divide(XYInteger* rhs) {
  assert(1==0);
}

XYObject* XYObject::divide(XYSequence* rhs) {
  assert(1==0);
}

XYObject* XYObject::power(XYObject* rhs) {
  assert(1==0);
}

XYObject* XYObject::power(XYFloat* rhs) {
  assert(1==0);
}

XYObject* XYObject::power(XYInteger* rhs) {
  assert(1==0);
}

XYObject* XYObject::power(XYSequence* rhs) {
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

void XYFloat::print(ostringstream& stream, CircularSet&, bool) const {
  stream << lexical_cast<string>(mValue);
}

int XYFloat::compare(XYObject* rhs) {
  XYFloat* o = dynamic_cast<XYFloat*>(rhs);
  if (!o) {
    XYInteger* i = dynamic_cast<XYInteger*>(rhs);
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

XYInteger* XYFloat::as_integer() {
  return new XYInteger(mValue);
}

XYFloat* XYFloat::as_float() {
  return dynamic_cast<XYFloat*>(this);
}

XYNumber* XYFloat::floor() {
  XYFloat* result(new XYFloat(::floor(mValue)));
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

void XYInteger::print(ostringstream& stream, CircularSet&, bool) const {
  stream << lexical_cast<string>(mValue);
}

int XYInteger::compare(XYObject* rhs) {
  XYInteger* o = dynamic_cast<XYInteger*>(rhs);
  if (!o) {
    XYFloat* f = dynamic_cast<XYFloat*>(rhs);
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

XYInteger* XYInteger::as_integer() {
  return dynamic_cast<XYInteger*>(this);
}

XYFloat* XYInteger::as_float() {
  return new XYFloat(mValue);
}

XYNumber* XYInteger::floor() {
  return dynamic_cast<XYNumber*>(this);
}

// XYSymbol
XYSymbol::XYSymbol(string v) : mValue(v) { }

void XYSymbol::print(ostringstream& stream, CircularSet&, bool) const {
  stream << mValue;
}

void XYSymbol::eval1(XY* xy) {
  XYEnv::iterator it = xy->mP.find(mValue);
  if (it != xy->mP.end()) {
    // Primitive symbol, execute immediately
    (*it).second->eval1(xy);
    return;
  }

  // Look up primitives object. If it's a slot in there,
  // execute immediately.
  it = xy->mEnv.find("primitives");
  if (it != xy->mEnv.end()) {
    XYObject* p = (*it).second;
    set<XYObject*> circular;
    XYSlot* slot = p->lookup(mValue, circular, 0);
    if (slot) {
      xy->mY.push_front(new XYSymbol("."));
      xy->mY.push_front(slot->mMethod);
      xy->mY.push_front(p);
      return;
    }
  }

  xy->mX.push_back(this);
}

int XYSymbol::compare(XYObject* rhs) {
  XYSymbol* o = dynamic_cast<XYSymbol*>(rhs);
  if (!o)
    return toString(true).compare(rhs->toString(true));

  return mValue.compare(o->mValue);
}

// XYString
XYString::XYString(string v) : mValue(v) { }

void XYString::print(ostringstream& stream, CircularSet&, bool parse) const {
  if (parse) {
    stream << '\"' << escape(mValue) << '\"';
  }
  else {
    stream << mValue;
  }
}

int XYString::compare(XYObject* rhs) {
  XYString* o = dynamic_cast<XYString*>(rhs);
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
    list.push_back(new XYInteger(*it));
}

XYObject* XYString::at(size_t n)
{
  return new XYInteger(mValue[n]);
}

void XYString::set_at(size_t n, XYObject* v)
{
  assert(n < mValue.size());
  XYInteger* c(dynamic_cast<XYInteger*>(v));
  //  xy_assert(c, XYError::TYPE);
  assert(c);

  mValue[n] = c->as_uint(); 
}

XYObject* XYString::head()
{
  assert(mValue.size() > 0);
  return new XYInteger(mValue[0]);
}

XYSequence* XYString::tail()
{
  if (mValue.size() <= 1) 
    return new XYString("");

  return new XYString(mValue.substr(1));
}

XYSequence* XYString::join(XYSequence* rhs)
{
  XYString const* rhs_string = dynamic_cast<XYString const*>(rhs);
  if (rhs_string) {
    return new XYString(mValue + rhs_string->mValue);
  }

  XYSequence* self(dynamic_cast<XYSequence*>(this));

  if (dynamic_cast<XYJoin const*>(rhs)) {
    // Pointer is shared, we have to copy the data
    XYJoin* join_rhs(dynamic_cast<XYJoin*>(rhs));
    XYJoin* result(new XYJoin());
    result->mSequences.push_back(self);
    result->mSequences.insert(result->mSequences.end(), 
			      join_rhs->mSequences.begin(), join_rhs->mSequences.end());
    return result;
  }

  return new XYJoin(self, rhs);
}

// XYShuffle
XYShuffle::XYShuffle(string v) { 
  vector<string> result;
  split(result, v, is_any_of("-"));
  assert(result.size() == 2);
  mBefore = result[0];
  mAfter  = result[1];
}

void XYShuffle::print(ostringstream& stream, CircularSet&, bool) const {
  stream << mBefore << "-" << mAfter;
}

void XYShuffle::eval1(XY* xy) {
  xy_assert(xy->mX.size() >= mBefore.size(), XYError::STACK_UNDERFLOW);
  map<char, XYObject*> env;
  for(string::reverse_iterator it = mBefore.rbegin(); it != mBefore.rend(); ++it) {
    env[*it] = xy->mX.back();
    xy->mX.pop_back();
  }

  for(string::iterator it = mAfter.begin(); it != mAfter.end(); ++it) {
    assert(env.find(*it) != env.end());
    xy->mX.push_back(env[*it]);
  }
}

int XYShuffle::compare(XYObject* rhs) {
  XYShuffle* o = dynamic_cast<XYShuffle*>(rhs);
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

int XYSequence::compare(XYObject* rhs) {
  XYSequence* o = dynamic_cast<XYSequence*>(rhs);
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

void XYList::markChildren() {
  for (iterator it = mList.begin(); it != mList.end(); ++it)
    (*it)->mark();
}

void XYList::print(ostringstream& stream, CircularSet& seen, bool parse) const {
  if (seen.find(this) != seen.end()) {
    stream << "(circular)";
  }
  else {
    seen.insert(this);
    stream << "[ ";
    for (XYSequence::const_iterator it = mList.begin(); it != mList.end(); ++it) {
      (*it)->print(stream, seen, parse);
      stream << " ";
    }

    stream << "]";
  }
}

size_t XYList::size()
{
  return mList.size();
}

void XYList::pushBackInto(List& list) {
  for(List::iterator it = mList.begin(); it != mList.end(); ++it)
    list.push_back(*it);
}

XYObject* XYList::at(size_t n)
{
  return mList[n];
}

void XYList::set_at(size_t n, XYObject* v)
{
  assert(n < mList.size());
  mList[n] = v; 
}

XYObject* XYList::head()
{
  assert(mList.size() > 0);
  return mList[0];
}

XYSequence* XYList::tail()
{
  if (mList.size() <= 1) 
    return new XYList();

  return new XYSlice(dynamic_cast<XYSequence*>(this), 1, mList.size());
}

XYSequence* XYList::join(XYSequence* rhs)
{
  if (dynamic_cast<XYJoin*>(rhs)) {
    // Modify the existing join
    XYJoin* join_rhs = dynamic_cast<XYJoin*>(rhs);
    join_rhs->mSequences.push_front(this);
    return join_rhs;
  }

  return new XYJoin(this, rhs);
}

// XYSlice
XYSlice::XYSlice(XYSequence* original,
                 int begin,
		 int end)  :
  mOriginal(original),
  mBegin(begin),
  mEnd(end)
{
  XYSlice* slice;
  while ((slice = dynamic_cast<XYSlice*>(mOriginal))) {
    // Find the original, non-slice sequence. Without this we can
    // corrupt the C stack due to too much recursion when destroying
    // the tree of slices.
    mOriginal = slice->mOriginal;
    mBegin += slice->mBegin;
    mEnd += slice->mBegin;
  }
}

void XYSlice::markChildren() {
  mOriginal->mark();
}

void XYSlice::print(ostringstream& stream, CircularSet& seen, bool parse) const {
  if (seen.find(this) != seen.end()) {
    stream << "(circular)";
  }
  else {
    seen.insert(this);
    stream << "[ ";
    for (int i=mBegin; i < mEnd; ++i) {
      mOriginal->at(i)->print(stream, seen, parse);
      stream << " ";
    }

    stream << "]";
  }
}

size_t XYSlice::size()
{
  return mEnd - mBegin;
}

void XYSlice::pushBackInto(List& list) {
  for (int i = mBegin; i != mEnd; ++i)
    list.push_back(mOriginal->at(i));
}

XYObject* XYSlice::at(size_t n)
{
  assert(mBegin + n < mEnd);
  return mOriginal->at(mBegin + n);
}

void XYSlice::set_at(size_t n, XYObject* v)
{
  assert(mBegin + n < mEnd);
  mOriginal->set_at(mBegin + n, v); 
}

XYObject* XYSlice::head()
{
  assert(mBegin != mEnd);
  return mOriginal->at(mBegin);
}

XYSequence* XYSlice::tail()
{
  if (size() <= 1)
    return new XYList();

  return new XYSlice(mOriginal, mBegin+1, mEnd);
}

XYSequence* XYSlice::join(XYSequence* rhs)
{
  if (dynamic_cast<XYJoin*>(rhs)) {
    // Modify the existing join
    XYJoin* join_rhs = dynamic_cast<XYJoin*>(rhs);
    join_rhs->mSequences.push_front(this);
    return join_rhs;
  }

  return new XYJoin(this, rhs);
}

// XYJoin
XYJoin::XYJoin(XYSequence* first, XYSequence* second)
{ 
  mSequences.push_back(first);
  mSequences.push_back(second);
}

void XYJoin::markChildren() {
  for (iterator it = mSequences.begin(); it != mSequences.end(); ++it)
    (*it)->mark();
}

void XYJoin::print(ostringstream& stream, CircularSet& seen, bool parse) const {
  if (seen.find(this) != seen.end()) {
    stream << "(circular)";
  }
  else {
    seen.insert(this);
    stream << "[ ";
    for(const_iterator it = mSequences.begin(); it != mSequences.end(); ++it) {
      for (int i=0; i < (*it)->size(); ++i) {
	(*it)->at(i)->print(stream, seen, parse);
	stream << " ";
      }
    }
    stream << "]";
  }
}

size_t XYJoin::size()
{
  size_t s = 0;
  for(iterator it = mSequences.begin(); it != mSequences.end(); ++it) 
    s += (*it)->size();

  return s;
}

XYObject* XYJoin::at(size_t n)
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

void XYJoin::set_at(size_t n, XYObject* v)
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

XYObject* XYJoin::head()
{
  return at(0);
}

XYSequence* XYJoin::tail()
{
  if (size() <= 1)
    return new XYList();

  return new XYSlice(dynamic_cast<XYSequence*>(this), 1, size());
}

XYSequence* XYJoin::join(XYSequence* rhs)
{
  if (dynamic_cast<XYJoin*>(rhs)) {
    // Modify ourselves
    XYJoin* join_rhs(dynamic_cast<XYJoin*>(rhs));
    mSequences.insert(mSequences.end(), 
		      mSequences.begin(), mSequences.end());
    return this;
  }

  // rhs is not a join
  mSequences.push_back(rhs);
  return this;
}

// XYPrimitive
XYPrimitive::XYPrimitive(string n, void (*func)(XY*)) : mName(n), mFunc(func) { }

void XYPrimitive::print(ostringstream& stream, CircularSet&, bool) const {
  stream << mName;
}

void XYPrimitive::eval1(XY* xy) {
  mFunc(xy);
}

int XYPrimitive::compare(XYObject* rhs) {
  XYPrimitive* o = dynamic_cast<XYPrimitive*>(rhs);
  if (!o)
    return toString(true).compare(rhs->toString(true));

  return mName.compare(o->mName);
}
 
// Primitive Implementations

// + [X^lhs^rhs] Y] -> [X^lhs+rhs Y]
static void primitive_addition(XY* xy) {
  xy_assert(xy->mX.size() >= 2, XYError::STACK_UNDERFLOW);
  XYObject* rhs(xy->mX.back());
  assert(rhs);
  xy->mX.pop_back();

  XYObject* lhs(xy->mX.back());
  assert(lhs);
  xy->mX.pop_back();

  xy->mX.push_back(lhs->add(rhs));
}

// - [X^lhs^rhs] Y] -> [X^lhs-rhs Y]
static void primitive_subtraction(XY* xy) {
  xy_assert(xy->mX.size() >= 2, XYError::STACK_UNDERFLOW);
  XYObject* rhs(xy->mX.back());
  assert(rhs);
  xy->mX.pop_back();

  XYObject* lhs(xy->mX.back());
  assert(lhs);
  xy->mX.pop_back();

  xy->mX.push_back(lhs->subtract(rhs));
}

// * [X^lhs^rhs] Y] -> [X^lhs*rhs Y]
static void primitive_multiplication(XY* xy) {
  xy_assert(xy->mX.size() >= 2, XYError::STACK_UNDERFLOW);
  XYObject* rhs(xy->mX.back());
  assert(rhs);
  xy->mX.pop_back();

  XYObject* lhs(xy->mX.back());
  assert(lhs);
  xy->mX.pop_back();

  xy->mX.push_back(lhs->multiply(rhs));
}

// % [X^lhs^rhs] Y] -> [X^lhs/rhs Y]
static void primitive_division(XY* xy) {
  xy_assert(xy->mX.size() >= 2, XYError::STACK_UNDERFLOW);
  XYObject* rhs(xy->mX.back());
  assert(rhs);
  xy->mX.pop_back();

  XYObject* lhs(xy->mX.back());
  assert(lhs);
  xy->mX.pop_back();

  xy->mX.push_back(lhs->divide(rhs));
}

// ^ [X^lhs^rhs] Y] -> [X^lhs**rhs Y]
static void primitive_power(XY* xy) {
  xy_assert(xy->mX.size() >= 2, XYError::STACK_UNDERFLOW);
  XYObject* rhs(xy->mX.back());
  assert(rhs);
  xy->mX.pop_back();

  XYObject* lhs(xy->mX.back());
  assert(lhs);
  xy->mX.pop_back();

  xy->mX.push_back(lhs->power(rhs));
}

// _ floor [X^n] Y] -> [X^n Y]
static void primitive_floor(XY* xy) {
  xy_assert(xy->mX.size() >= 1, XYError::STACK_UNDERFLOW);
  XYNumber* n = dynamic_cast<XYNumber*>(xy->mX.back());
  xy_assert(n, XYError::TYPE);
  xy->mX.pop_back();

  xy->mX.push_back(n->floor());
}

// set [X^value^name Y] -> [X Y] 
static void primitive_set(XY* xy) {
  xy_assert(xy->mX.size() >= 2, XYError::STACK_UNDERFLOW);
  XYSymbol* name = dynamic_cast<XYSymbol*>(xy->mX.back());
  xy_assert(name, XYError::TYPE);
  xy->mX.pop_back();

  XYObject* value = xy->mX.back();
  xy->mX.pop_back();

  xy->mEnv[name->mValue] = value;
}

// get [X^name Y] [X^value Y]
static void primitive_get(XY* xy) {
  // The plan is for the 'get' primitive to be a
  // 'message send' primitive. It will do a lookup
  // of the symbol on the object on the stack. 
  // To support backwards compatibility during the
  // prototype object testing phase, look up the
  // symbol in the environment first. If it's not there
  // then check the object.
  xy_assert(xy->mX.size() >= 1, XYError::STACK_UNDERFLOW);
  XYSymbol* name = dynamic_cast<XYSymbol*>(xy->mX.back());
  xy_assert(name, XYError::TYPE);
  xy->mX.pop_back();

  XYEnv::iterator it = xy->mEnv.find(name->mValue);
  if (it == xy->mEnv.end()) {
    // Not in environment, look up object
    xy_assert(xy->mX.size() >= 1, XYError::STACK_UNDERFLOW);
    XYObject* object = xy->mX.back();
    xy_assert(object, XYError::TYPE);
    xy->mX.pop_back();

    set<XYObject*> circular;
    XYObject* context = 0;
    XYSlot* slot = object->lookup(name->mValue, circular, &context);
    xy_assert(slot, XYError::SLOT_NOT_FOUND);
    xy_assert(slot->mMethod, XYError::INVALID_SLOT_TYPE);
    xy_assert(context, XYError::INVALID_SLOT_TYPE);

    xy->mX.push_back(object);
    xy->mX.push_back(slot->mMethod);
  }
  else {
    xy_assert(it != xy->mEnv.end(), XYError::SYMBOL_NOT_FOUND);

    XYObject* value = (*it).second;
    xy->mX.push_back(value);
  }
}

// puncons [X^{O1..On} Y] [X^O1^{O2..On} Y]
static void primitive_uncons(XY* xy) {
  xy_assert(xy->mX.size() >= 1, XYError::STACK_UNDERFLOW);
  XYObject* o = xy->mX.back();
  xy->mX.pop_back();

  XYSequence* list = dynamic_cast<XYSequence*>(o);

  if (list && list->size() > 0) {
    xy->mX.push_back(list->head());
    xy->mX.push_back(list->tail());
  }
  else {
    xy->mX.push_back(o);
    xy->mX.push_back(new XYList());
  }
}

// . [X^{O1..On} Y] [X O1^..^On^Y]
static void primitive_unquote(XY* xy) {
  xy_assert(xy->mX.size() >= 1, XYError::STACK_UNDERFLOW);
  XYObject* o = xy->mX.back();
  xy->mX.pop_back();

  XYSequence* list = dynamic_cast<XYSequence*>(o);

  if (list) {
    XYStack temp;
    list->pushBackInto(temp);

    xy->mY.insert(xy->mY.begin(), temp.begin(), temp.end());
  }
  else {
    XYSymbol* symbol = dynamic_cast<XYSymbol*>(o);
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
  xy_assert(xy->mX.size() >= 1, XYError::STACK_UNDERFLOW);

  // Get the pattern from the stack
  XYSequence* pattern = dynamic_cast<XYSequence*>(xy->mX.back());
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
    XYList* list(new XYList());    
    xy->replacePattern(env, new XYSlice(pattern, ++start, end), back_inserter(list->mList));
    assert(list->size() > 0);

    // Append to stack
    list = dynamic_cast<XYList*>(list->mList[0]);
    xy_assert(list, XYError::TYPE);
    xy->mX.insert(xy->mX.end(), list->mList.begin(), list->mList.end());
  }
}

// ( [X^{pattern} Y] [X result^Y]
static void primitive_pattern_sq(XY* xy) {
  xy_assert(xy->mX.size() >= 1, XYError::STACK_UNDERFLOW);

  // Get the pattern from the stack
  XYSequence* pattern = dynamic_cast<XYSequence*>(xy->mX.back());
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
    XYList* list(new XYList());
    xy->replacePattern(env, new XYSlice(pattern, ++start, end), back_inserter(list->mList));
    assert(list->size() > 0);

    // Prepend to queue
    list = dynamic_cast<XYList*>(list->mList[0]);
    xy_assert(list, XYError::TYPE);
    xy->mY.insert(xy->mY.begin(), list->mList.begin(), list->mList.end());
  }
}

// ` dip [X^b^{a0..an} Y] [X a0..an^b^Y]
static void primitive_dip(XY* xy) {
  xy_assert(xy->mX.size() >= 2, XYError::STACK_UNDERFLOW);
  XYSequence* list = dynamic_cast<XYSequence*>(xy->mX.back());
  xy_assert(list, XYError::TYPE);
  xy->mX.pop_back();

  XYObject* o = xy->mX.back();
  assert(o);
  xy->mX.pop_back();

  xy->mY.push_front(o);
  XYStack temp;
  list->pushBackInto(temp);
  xy->mY.insert(xy->mY.begin(), temp.begin(), temp.end());
}

// | reverse [X^{a0..an} Y] [X^{an..a0} Y]
static void primitive_reverse(XY* xy) {
  xy_assert(xy->mX.size() >= 1, XYError::STACK_UNDERFLOW);
  XYSequence* list = dynamic_cast<XYSequence*>(xy->mX.back());
  xy_assert(list, XYError::TYPE);
  xy->mX.pop_back();

  XYList* reversed(new XYList());
  list->pushBackInto(reversed->mList);
  reverse(reversed->mList.begin(), reversed->mList.end());
  xy->mX.push_back(reversed);
}

// \ quote [X^o Y] [X^{o} Y]
static void primitive_quote(XY* xy) {
  assert(xy->mY.size() >= 1);
  XYObject* o = xy->mY.front();
  assert(o);
  xy->mY.pop_front();

  XYList* list = new XYList();
  list->mList.push_back(o);
  xy->mX.push_back(list);
}

// , join [X^a^b Y] [X^{...} Y]
static void primitive_join(XY*xy) {
  xy_assert(xy->mX.size() >= 2, XYError::STACK_UNDERFLOW);
  XYObject* rhs = xy->mX.back();
  assert(rhs);
  xy->mX.pop_back();

  XYObject* lhs = xy->mX.back();
  assert(lhs);
  xy->mX.pop_back();

  XYSequence* list_lhs = dynamic_cast<XYSequence*>(lhs);
  XYSequence* list_rhs = dynamic_cast<XYSequence*>(rhs);

  if (list_lhs && list_rhs) {
    // Two lists are concatenated
    xy->mX.push_back(list_lhs->join(list_rhs));
  }
  else if(list_lhs) {
    // If rhs is not a list, it is added to the end of the list.
    if (dynamic_cast<XYList*>(lhs)) {
      // Optimisation for a list on the lhs. We modify the list.
      dynamic_cast<XYList*>(list_lhs)->mList.push_back(rhs);
    }
    else {
      XYList* list(new XYList());
      list->mList.push_back(rhs);
      xy->mX.push_back(list_lhs->join(list));
    }
    xy->mX.push_back(list_lhs);
  }
  else if(list_rhs) {
    // If lhs is not a list, it is added to the front of the list
    XYList* list(new XYList());
    list->mList.push_back(lhs);
    xy->mX.push_back(list->join(list_rhs));
  }
  else {
    // If neither are lists, a list is made containing the two items
    XYList* list(new XYList());
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
  xy_assert(xy->mX.size() >= 1, XYError::STACK_UNDERFLOW);
  XYSequence* list  = dynamic_cast<XYSequence*>(xy->mX.back());
  xy_assert(list, XYError::TYPE);
  xy->mX.pop_back();

  XYList* stack(new XYList(xy->mX.begin(), xy->mX.end()));
  XYList* queue(new XYList(xy->mY.begin(), xy->mY.end()));

  xy->mX.push_back(stack);
  xy->mX.push_back(queue);
  xy->mY.push_front(new XYSymbol("$$"));
  XYStack temp;
  list->pushBackInto(temp);
  xy->mY.insert(xy->mY.begin(), temp.begin(), temp.end()); 
}

// $$ stackqueue - Helper word for '$'. Given a stack and queue on the
// stack, replaces the existing stack and queue with them.
static void primitive_stackqueue(XY* xy) {
  xy_assert(xy->mX.size() >= 2, XYError::STACK_UNDERFLOW);

  XYSequence* queue = dynamic_cast<XYSequence*>(xy->mX.back());
  xy_assert(queue, XYError::TYPE);
  xy->mX.pop_back();

  XYSequence* stack = dynamic_cast<XYSequence*>(xy->mX.back());
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
static void primitive_equals(XY* xy) {
  xy_assert(xy->mX.size() >= 2, XYError::STACK_UNDERFLOW);

  XYObject* rhs = xy->mX.back();
  assert(rhs);
  xy->mX.pop_back();

  XYObject* lhs = xy->mX.back();
  assert(lhs);
  xy->mX.pop_back();

  if (lhs->compare(rhs) == 0)
    xy->mX.push_back(new XYInteger(1));
  else
    xy->mX.push_back(new XYInteger(0));
}

// <  [X^a^b Y] [X^? Y] 
static void primitive_lessThan(XY* xy) {
  xy_assert(xy->mX.size() >= 2, XYError::STACK_UNDERFLOW);

  XYObject* rhs = xy->mX.back();
  assert(rhs);
  xy->mX.pop_back();

  XYObject* lhs = xy->mX.back();
  assert(lhs);
  xy->mX.pop_back();

  if (lhs->compare(rhs) < 0)
    xy->mX.push_back(new XYInteger(1));
  else
    xy->mX.push_back(new XYInteger(0));
}

// >  [X^a^b Y] [X^? Y] 
static void primitive_greaterThan(XY* xy) {
  xy_assert(xy->mX.size() >= 2, XYError::STACK_UNDERFLOW);

  XYObject* rhs = xy->mX.back();
  assert(rhs);
  xy->mX.pop_back();

  XYObject* lhs = xy->mX.back();
  assert(lhs);
  xy->mX.pop_back();

  if (lhs->compare(rhs) > 0)
    xy->mX.push_back(new XYInteger(1));
  else
    xy->mX.push_back(new XYInteger(0));
}

// <=  [X^a^b Y] [X^? Y] 
static void primitive_lessThanEqual(XY* xy) {
  xy_assert(xy->mX.size() >= 2, XYError::STACK_UNDERFLOW);

  XYObject* rhs = xy->mX.back();
  assert(rhs);
  xy->mX.pop_back();

  XYObject* lhs = xy->mX.back();
  assert(lhs);
  xy->mX.pop_back();

  if (lhs->compare(rhs) <= 0)
    xy->mX.push_back(new XYInteger(1));
  else
    xy->mX.push_back(new XYInteger(0));
}

// >=  [X^a^b Y] [X^? Y] 
static void primitive_greaterThanEqual(XY* xy) {
  xy_assert(xy->mX.size() >= 2, XYError::STACK_UNDERFLOW);

  XYObject* rhs = xy->mX.back();
  assert(rhs);
  xy->mX.pop_back();

  XYObject* lhs = xy->mX.back();
  assert(lhs);
  xy->mX.pop_back();

  if (lhs->compare(rhs) >= 0)
    xy->mX.push_back(new XYInteger(1));
  else
    xy->mX.push_back(new XYInteger(0));
}


// not not [X^a Y] [X^? Y] 
static void primitive_not(XY* xy) {
  xy_assert(xy->mX.size() >= 1, XYError::STACK_UNDERFLOW);

  XYObject* o = xy->mX.back();
  assert(o);
  xy->mX.pop_back();

  XYNumber* n = dynamic_cast<XYNumber*>(o);
  if (n && n->is_zero()) {
    xy->mX.push_back(new XYInteger(1));
  }
  else {
    XYSequence* l = dynamic_cast<XYSequence*>(o);
    if(l && l->size() == 0)
      xy->mX.push_back(new XYInteger(1));
    else
      xy->mX.push_back(new XYInteger(0));
  }
}

// @ nth [X^n^{...} Y] [X^o Y] 
static void primitive_nth(XY* xy) {
  xy_assert(xy->mX.size() >= 2, XYError::STACK_UNDERFLOW);

  XYSequence* list = dynamic_cast<XYSequence*>(xy->mX.back());
  xy_assert(list, XYError::TYPE);
  xy->mX.pop_back();

  XYObject* index(xy->mX.back());
  assert(index);
  xy->mX.pop_back();

  XYNumber* n = dynamic_cast<XYNumber*>(index);
  XYSequence* s = dynamic_cast<XYSequence*>(index);
  xy_assert(n || s, XYError::TYPE);

  if (n) {
    // Index is a number, do a direct index into the list
    if (n->as_uint() >= list->size()) 
      xy->mX.push_back(new XYInteger(list->size()));
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
      XYObject* head = s->head();
      XYSequence* tail = s->tail();

      // If head is a list, then it's a request to get multiple values
      XYSequence* headlist = dynamic_cast<XYSequence*>(head);
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
	    code.push_back(new XYSymbol("@"));
	    code.push_back(new XYSymbol("'"));
	    code.push_back(new XYSymbol("'"));
	    code.push_back(new XYSymbol("`"));	  
	  }	
	  for (int i=0; i < list->size() - 1; ++i) {
	    code2.push_back(new XYSymbol(","));
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
	  code.push_back(new XYSymbol("@"));
	  code.push_back(new XYSymbol("'"));
	  code.push_back(new XYSymbol("'"));
	  code.push_back(new XYSymbol("`"));
	}
	
	for (int i=0; i < headlist->size()-1; ++i) {
	  code2.push_back(new XYSymbol(","));
	}

	xy->mY.insert(xy->mY.begin(), code2.begin(), code2.end());
	xy->mY.insert(xy->mY.begin(), code.begin(), code.end());
      }
      else if (tail->size() == 0) {
	xy->mX.push_back(head);
	xy->mX.push_back(list);
	xy->mY.push_front(new XYSymbol("@"));
      }
      else {
	xy->mX.push_back(head);
	xy->mX.push_back(list);
	xy->mY.push_front(new XYSymbol("@"));
	xy->mY.push_front(new XYShuffle("ab-ba"));
	xy->mY.push_front(tail);
	xy->mY.push_front(new XYSymbol("@"));
      }
    }
  }
}

// ! set-nth [X^v^n^{...} Y] [X Y] 
static void primitive_set_nth(XY* xy) {
  xy_assert(xy->mX.size() >= 3, XYError::STACK_UNDERFLOW);

  XYSequence* list = dynamic_cast<XYSequence*>(xy->mX.back());
  xy_assert(list, XYError::TYPE);
  xy->mX.pop_back();

  XYNumber* index(dynamic_cast<XYNumber*>(xy->mX.back()));
  xy_assert(index, XYError::TYPE);
  xy->mX.pop_back();

  XYObject* v(xy->mX.back());
  assert(v);
  xy->mX.pop_back();

  unsigned int n = index->as_uint();
  xy_assert(n < list->size(), XYError::RANGE);

  list->set_at(n, v);
}

// print [X^n Y] [X Y] 
static void primitive_print(XY* xy) {
  xy_assert(xy->mX.size() >= 1, XYError::STACK_UNDERFLOW);

  XYObject* o(xy->mX.back());
  assert(o);
  xy->mX.pop_back();

  boost::asio::streambuf buffer;
  ostream stream(&buffer);
  stream << o->toString(true);

  boost::asio::write(xy->mOutputStream, buffer);
}

// println [X^n Y] [X Y] 
static void primitive_println(XY* xy) {
  xy_assert(xy->mX.size() >= 1, XYError::STACK_UNDERFLOW);

  XYObject* o(xy->mX.back());
  assert(o);
  xy->mX.pop_back();

  boost::asio::streambuf buffer;
  ostream stream(&buffer);
  stream << o->toString(true) << endl;
  boost::asio::write(xy->mOutputStream, buffer);
}


// write [X^n Y] [X Y] 
static void primitive_write(XY* xy) {
  xy_assert(xy->mX.size() >= 1, XYError::STACK_UNDERFLOW);

  XYObject* o(xy->mX.back());
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
static void primitive_count(XY* xy) {
  xy_assert(xy->mX.size() >= 1, XYError::STACK_UNDERFLOW);

  XYObject* o(xy->mX.back());
  assert(o);
  xy->mX.pop_back();

  XYSequence* list(dynamic_cast<XYSequence*>(o));
  if (list)
    xy->mX.push_back(new XYInteger(list->size()));
  else {
    XYString* s(dynamic_cast<XYString*>(o));
    if (s)
      xy->mX.push_back(new XYInteger(s->mValue.size()));
    else
      xy->mX.push_back(new XYInteger(1));
  }
}

// split [X^string^seps Y] [X^{...} Y] 
// Splits a string into an array of strings, splitting
// on the specified characters.
static void primitive_split(XY* xy) {
  xy_assert(xy->mX.size() >= 2, XYError::STACK_UNDERFLOW);

  XYString* seps(dynamic_cast<XYString*>(xy->mX.back()));
  xy_assert(seps, XYError::TYPE);
  xy->mX.pop_back();

  XYString* str(dynamic_cast<XYString*>(xy->mX.back()));
  xy_assert(str, XYError::TYPE);
  xy->mX.pop_back();

  vector<string> result;
  split(result, str->mValue, is_any_of(seps->mValue));
 
  XYList* list(new XYList());
  for (vector<string>::iterator it = result.begin(); it != result.end(); ++it)
    list->mList.push_back(new XYString(*it));
  xy->mX.push_back(list);
}

// sdrop [X^seq^n Y] [X^{...} Y] 
// drops n items from the beginning of the sequence
static void primitive_sdrop(XY* xy) {
  xy_assert(xy->mX.size() >= 2, XYError::STACK_UNDERFLOW);

  XYNumber* n(dynamic_cast<XYNumber*>(xy->mX.back()));
  xy_assert(n, XYError::TYPE);
  xy->mX.pop_back();
  
  XYString*   str(dynamic_cast<XYString*>(xy->mX.back()));
  XYSequence* seq(dynamic_cast<XYSequence*>(xy->mX.back()));
  xy_assert(str || seq, XYError::TYPE);
  xy->mX.pop_back();

  if (str) {
    if (n->as_uint() >= str->mValue.size())
      xy->mX.push_back(new XYString(""));
    else
      xy->mX.push_back(new XYString(str->mValue.substr(n->as_uint())));
  }
  else {
    // TODO
    assert(1 == 0);
  }
}

// stake [X^seq^n Y] [X^{...} Y] 
// takes n items from the beginning of the sequence
static void primitive_stake(XY* xy) {
  xy_assert(xy->mX.size() >= 2, XYError::STACK_UNDERFLOW);

  XYNumber* n(dynamic_cast<XYNumber*>(xy->mX.back()));
  xy_assert(n, XYError::TYPE);
  xy->mX.pop_back();
  
  XYString*   str(dynamic_cast<XYString*>(xy->mX.back()));
  XYSequence* seq(dynamic_cast<XYSequence*>(xy->mX.back()));
  xy_assert(str || seq, XYError::TYPE);
  xy->mX.pop_back();

  if (str) {
    xy->mX.push_back(new XYString(str->mValue.substr(0, n->as_uint())));
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
static void primitive_tokenize(XY* xy) {
  xy_assert(xy->mX.size() >= 1, XYError::STACK_UNDERFLOW);

  XYString* s(dynamic_cast<XYString*>(xy->mX.back()));
  xy_assert(s, XYError::TYPE);
  xy->mX.pop_back();

  vector<string> tokens;
  tokenize(s->mValue.begin(), s->mValue.end(), back_inserter(tokens));

  XYList* result(new XYList());
  for(vector<string>::iterator it=tokens.begin(); it != tokens.end(); ++it)
    result->mList.push_back(new XYString(*it));

  xy->mX.push_back(result);
}

// parse [X^{tokens} Y] [X^{...} Y] 
// Given a list of tokens, parses it and returns the program
static void primitive_parse(XY* xy) {
  xy_assert(xy->mX.size() >= 1, XYError::STACK_UNDERFLOW);

  XYSequence* tokens(dynamic_cast<XYSequence*>(xy->mX.back()));
  xy_assert(tokens, XYError::TYPE);
  xy->mX.pop_back();

  vector<string> strings;
  for (int i=0; i < tokens->size(); ++i) {
    XYString* s = dynamic_cast<XYString*>(tokens->at(i));
    xy_assert(s, XYError::TYPE);
    strings.push_back(s->mValue);
  }

  XYList* result(new XYList());
  parse(strings.begin(), strings.end(), back_inserter(result->mList));
  xy->mX.push_back(result);
}

// Asynchronous handler for getline
static void getlineHandler(XY* xy, boost::system::error_code const& err) {
  if (!err) {
    istream stream(&xy->mInputBuffer);
    string input;
    std::getline(stream, input);
    xy->mX.push_back(new XYString(input));
    xy->mService.post(bind(&XY::evalHandler, xy));
  }
}

// getline [X Y] [X^".." Y] 
// Get a line of input from the user
static void primitive_getline(XY* xy) {
  boost::asio::async_read_until(xy->mInputStream,
				xy->mInputBuffer,
				"\n",
				bind(getlineHandler, xy, boost::asio::placeholders::error));

  throw XYError(xy, XYError::WAITING_FOR_ASYNC_EVENT);
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

  xy->mX.push_back(new XYInteger(d.total_milliseconds()));
}

// enum [X^n Y] -> [X^{0..n} Y]
static void primitive_enum(XY* xy) {
  xy_assert(xy->mX.size() >= 1, XYError::STACK_UNDERFLOW);
  XYNumber* n = dynamic_cast<XYNumber*>(xy->mX.back());
  xy_assert(n, XYError::TYPE);
  xy->mX.pop_back();

  int value = n->as_uint();
  XYList* list = new XYList();
  for(int i=0; i < value; ++i)
    list->mList.push_back(new XYInteger(i));
  xy->mX.push_back(list);
}

// clone [X^o Y] -> [X^o Y]
static void primitive_clone(XY* xy) {
  // TODO: Only works for sequences for now
  // Implemented to work around an XYJoin limitation
  xy_assert(xy->mX.size() >= 1, XYError::STACK_UNDERFLOW);
  XYSequence* o = dynamic_cast<XYSequence*>(xy->mX.back());
  xy_assert(o, XYError::TYPE);
  xy->mX.pop_back();

  XYList* r(new XYList());
  o->pushBackInto(r->mList);
  xy->mX.push_back(r);
}

// clone [X^o Y] -> [X^string Y]
static void primitive_to_string(XY* xy) {
  xy_assert(xy->mX.size() >= 1, XYError::STACK_UNDERFLOW);
  XYObject* o(xy->mX.back());
  xy->mX.pop_back();

  xy->mX.push_back(new XYString(o->toString(true)));
}

// foldl [X^seq^seed^quot Y] -> [X^seq Y]
// [1 2 3] 0 [+] foldl
// [2 3] 0 1 + [+] foldl
// [3] 1 2 + [+] foldl
// [] 3 3 + [+] foldl
// 6
static void primitive_foldl(XY* xy) {
  xy_assert(xy->mX.size() >= 3, XYError::STACK_UNDERFLOW);
  XYSequence* quot(dynamic_cast<XYSequence*>(xy->mX.back()));
  xy_assert(quot, XYError::TYPE);
  xy->mX.pop_back();

  XYObject* seed(xy->mX.back());
  xy->mX.pop_back();

  XYSequence* seq(dynamic_cast<XYSequence*>(xy->mX.back()));
  xy_assert(seq, XYError::TYPE);
  xy->mX.pop_back();
			     
  if (seq->size() == 0) {
    xy->mX.push_back(seed);
  }
  else {
    XYObject* head(seq->head());
    XYSequence* tail(seq->tail());
  
    XYStack temp;
    temp.push_back(tail);
    temp.push_back(seed);
    temp.push_back(head);    
    temp.push_back(quot);
    temp.push_back(new XYPrimitive(".", primitive_unquote));
    temp.push_back(quot);
    temp.push_back(new XYPrimitive("foldl", primitive_foldl));
    
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
static void primitive_foldr(XY* xy) {
  xy_assert(xy->mX.size() >= 3, XYError::STACK_UNDERFLOW);
  XYSequence* quot(dynamic_cast<XYSequence*>(xy->mX.back()));
  xy_assert(quot, XYError::TYPE);
  xy->mX.pop_back();

  XYObject* seed(xy->mX.back());
  xy->mX.pop_back();

  XYSequence* seq(dynamic_cast<XYSequence*>(xy->mX.back()));
  xy_assert(seq, XYError::TYPE);
  xy->mX.pop_back();
			     
  if (seq->size() == 0) {
    xy->mX.push_back(seed);
  }
  else {
    XYObject* head(seq->head());
    XYSequence* tail(seq->tail());
    
    XYStack temp;
    temp.push_back(seed);
    temp.push_back(tail);
    temp.push_back(head);    
    temp.push_back(quot);
    temp.push_back(new XYPrimitive("foldr", primitive_foldr));
    temp.push_back(quot);
    temp.push_back(new XYPrimitive(".", primitive_unquote));
    
    xy->mY.insert(xy->mY.begin(), temp.begin(), temp.end());    
  }
}

// if [X^bool^then^else Y] -> [X Y]
// 2 1 = [ ... ] [ ... ] if
static void primitive_if(XY* xy) {
  xy_assert(xy->mX.size() >= 3, XYError::STACK_UNDERFLOW);
  XYSequence* else_quot(dynamic_cast<XYSequence*>(xy->mX.back()));
  xy_assert(else_quot, XYError::TYPE);
  xy->mX.pop_back();

  XYSequence* then_quot(dynamic_cast<XYSequence*>(xy->mX.back()));
  xy_assert(then_quot, XYError::TYPE);
  xy->mX.pop_back();

  XYObject* o(xy->mX.back());
  XYNumber* num(dynamic_cast<XYNumber*>(o));
  XYSequence* seq(dynamic_cast<XYSequence*>(o));
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
static void primitive_find(XY* xy) {
  xy_assert(xy->mX.size() >= 2, XYError::STACK_UNDERFLOW);
  XYObject* elt(xy->mX.back());
  xy->mX.pop_back();

  XYSequence* seq(dynamic_cast<XYSequence*>(xy->mX.back()));
  xy_assert(seq, XYError::TYPE);
  xy->mX.pop_back();

  int i=0;
  for (i=0; i < seq->size(); ++i) {
    if(seq->at(i)->compare(elt) == 0)
      break;
  }

  xy->mX.push_back(new XYInteger(i));
}

// gc gc [X Y] -> [X Y]
static void primitive_gc(XY* xy) {
  GarbageCollector::GC.collect();
}

// copy copy [X^object Y] -> [X^object Y]
// Returns a copy of the object
static void primitive_copy(XY* xy) {
  xy_assert(xy->mX.size() >= 1, XYError::STACK_UNDERFLOW);
  XYObject* o(xy->mX.back());
  xy->mX.pop_back();

  xy->mX.push_back(o->copy());
}

// add-data-slot add-data-slot [X^object^value^name Y] -> [X^object Y]
// Adds a data slot to the object
static void primitive_add_data_slot(XY* xy) {
  xy_assert(xy->mX.size() >= 3, XYError::STACK_UNDERFLOW);

  XYSymbol* name(dynamic_cast<XYSymbol*>(xy->mX.back()));
  xy_assert(name, XYError::TYPE);
  xy->mX.pop_back();

  XYObject* value(xy->mX.back());
  xy_assert(value, XYError::TYPE);
  xy->mX.pop_back();

  XYObject* object(xy->mX.back());
  xy_assert(object, XYError::TYPE);
  xy->mX.pop_back();

  object->addSlot(name->mValue, value, false);
  xy->mX.push_back(object);
}

// add-method-slot add-method-slot [X^object^method^name Y] -> [X^object Y]
// Adds a method slot to the object
static void primitive_add_method_slot(XY* xy) {
  xy_assert(xy->mX.size() >= 3, XYError::STACK_UNDERFLOW);

  XYSymbol* name(dynamic_cast<XYSymbol*>(xy->mX.back()));
  xy_assert(name, XYError::TYPE);
  xy->mX.pop_back();

  // Parent slots can't be methods
  xy_assert(name->mValue[name->mValue.size()-1] != '*', XYError::INVALID_SLOT_TYPE);

  XYObject* method(xy->mX.back());
  xy_assert(method, XYError::TYPE);
  xy->mX.pop_back();

  XYObject* object(xy->mX.back());
  xy_assert(object, XYError::TYPE);
  xy->mX.pop_back();

  XYList* list = dynamic_cast<XYList*>(method);
  if (list)
    object->addMethod(name->mValue, list);
  else
    object->addMethod(name->mValue, method);

  xy->mX.push_back(object);
}

// get-slot-value get-slot-value [X^object^name Y] -> [X^value Y]
// Gets the value held in a slot of an object
static void primitive_get_slot_value(XY* xy) {
  xy_assert(xy->mX.size() >= 2, XYError::STACK_UNDERFLOW);

  XYString* name(dynamic_cast<XYString*>(xy->mX.back()));
  xy_assert(name, XYError::TYPE);
  xy->mX.pop_back();

  XYObject* object(xy->mX.back());
  xy_assert(object, XYError::TYPE);
  xy->mX.pop_back();

#if 0
  XYObject* value = object->getSlot(name->mValue)->mValue;
  xy_assert(value, XYError::INVALID_SLOT_TYPE);
#endif
  set<XYObject*> circular;
  XYSlot* slot = object->lookup(name->mValue, circular, 0);  
  xy_assert(slot, XYError::INVALID_SLOT_TYPE);
  xy_assert(slot->mValue, XYError::INVALID_SLOT_TYPE);
  xy->mX.push_back(slot->mValue);
}

// set-slot-value set-slot-value [X^value^object^name Y] -> [X^object Y]
// Sets the value held in a slot of an object
static void primitive_set_slot_value(XY* xy) {
  xy_assert(xy->mX.size() >= 3, XYError::STACK_UNDERFLOW);

  XYString* name(dynamic_cast<XYString*>(xy->mX.back()));
  xy_assert(name, XYError::TYPE);
  xy->mX.pop_back();

  XYObject* object(xy->mX.back());
  xy_assert(object, XYError::TYPE);
  xy->mX.pop_back();

  XYObject* value(xy->mX.back());
  xy_assert(value, XYError::TYPE);
  xy->mX.pop_back();

  set<XYObject*> circular;
  XYSlot* slot = object->lookup(name->mValue, circular, 0);  
  xy_assert(slot, XYError::INVALID_SLOT_TYPE);
  xy_assert(slot->mValue, XYError::INVALID_SLOT_TYPE);
  slot->mValue = value;
  
  xy->mX.push_back(object);
}

// call-method call-method [X^object^method Y] -> [X Y]
// Calls the method by copying it, installing the copy as
// the current frame, and running the code.
static void primitive_call_method(XY* xy) {
  xy_assert(xy->mX.size() >= 2, XYError::STACK_UNDERFLOW);

  XYObject* method(xy->mX.back());
  xy_assert(method, XYError::TYPE);
  xy->mX.pop_back();

  XYObject* object(xy->mX.back());
  xy_assert(object, XYError::TYPE);
  xy->mX.pop_back();

  XYObject* frame = method->copy();
  frame->addSlot("self*", object);
  
  // Restore the original frame
  XYObject* oldFrame = xy->mFrame;
  xy->mY.push_front(new XYSymbol("set-frame"));
  xy->mY.push_front(oldFrame);

  // Run the method body
  xy->mY.push_front(new XYSymbol("."));

  // Find the list containing the code to run for the method.
  // By doing the lookup at runtime we'll always get the latest
  // code if the method body changes.
  xy->mY.push_front(new XYSymbol("."));
  xy->mY.push_front(new XYSymbol("lookup"));
  xy->mY.push_front(new XYSymbol("code"));
  xy->mY.push_front(frame);

  // Populate the frame's argument slots with items on the stack
  xy->mY.push_front(new XYSymbol("set-method-args"));
  xy->mY.push_front(frame);

  // Get the list of arguments that this method takes.
  xy->mY.push_front(new XYSymbol("."));
  xy->mY.push_front(new XYSymbol("lookup"));
  xy->mY.push_front(new XYSymbol("args"));
  xy->mY.push_front(frame);

  // Set the current frame to be the one for this method call
  xy->mY.push_front(new XYSymbol("set-frame"));
  xy->mY.push_front(frame);
}

// set-method-args set-method-args [X^...^args^method Y] -> [X Y]
// For each argument the method requires, pops an item off
// the stack and sets a slot in the method object with that value.
// 'args' is a list of symbols containing the argument names.
static void primitive_set_method_args(XY* xy) {
  xy_assert(xy->mX.size() >= 1, XYError::STACK_UNDERFLOW);

  XYObject* method(xy->mX.back());
  xy_assert(method, XYError::TYPE);
  xy->mX.pop_back();

  XYSequence* args(dynamic_cast<XYSequence*>(xy->mX.back()));
  xy_assert(args, XYError::TYPE);
  xy->mX.pop_back();

  xy_assert(xy->mX.size() >= args->size(), XYError::STACK_UNDERFLOW);
  for (int i=0; i < args->size(); ++i) {
    XYSymbol* name = dynamic_cast<XYSymbol*>(args->at(i));
    xy_assert(name, XYError::TYPE);

    XYObject* arg(xy->mX.back());
    xy_assert(arg, XYError::TYPE);
    xy->mX.pop_back();

    xy->mY.push_front(new XYShuffle("a-"));
    xy->mY.push_front(new XYSymbol("."));
    xy->mY.push_front(new XYSymbol("lookup"));
    xy->mY.push_front(new XYSymbol(name->mValue + ":"));
    xy->mY.push_front(method);    
    xy->mY.push_front(arg);    
  }
}

// lookup lookup [X^object^name Y] -> [X^object^method Y]
// Find the name in the object slots using the
// prototype chain and return the method there.
static void primitive_lookup(XY* xy) {
  xy_assert(xy->mX.size() >= 2, XYError::STACK_UNDERFLOW);

  XYSymbol* name(dynamic_cast<XYSymbol*>(xy->mX.back()));
  xy_assert(name, XYError::TYPE);
  xy->mX.pop_back();

  XYObject* object(xy->mX.back());
  xy_assert(object, XYError::TYPE);
  xy->mX.pop_back();

  set<XYObject*> circular;
  XYObject* context = 0;
  XYSlot* slot = object->lookup(name->mValue, circular, &context);
  xy_assert(slot, XYError::SLOT_NOT_FOUND);
  xy_assert(slot->mMethod, XYError::INVALID_SLOT_TYPE);
  xy_assert(context, XYError::INVALID_SLOT_TYPE);

  xy->mX.push_back(object);
  xy->mX.push_back(slot->mMethod);
}

// set-frame set-frame [X^frame Y] -> [X Y]
// Sets the frame object to the given frame
static void primitive_set_frame(XY* xy) {
  xy_assert(xy->mX.size() >= 1, XYError::STACK_UNDERFLOW);

  XYObject* frame(xy->mX.back());
  xy_assert(frame, XYError::TYPE);
  xy->mX.pop_back();

  xy->mFrame = frame;
}

// frame frame [X Y] -> [X^frame Y]
// Return the current executing frame object on the stack
static void primitive_frame(XY* xy) {
  xy->mX.push_back(xy->mFrame);
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
XYError::XYError(XY* xy, code c) :
  mXY(xy),
  mCode(c),
  mLine(-1),
  mFile(0)
{
}

XYError::XYError(XY* xy, code c, char const* file, int line) :
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
  mService(service),
  mInputStream(service, ::dup(STDIN_FILENO)),
  mOutputStream(service, ::dup(STDOUT_FILENO)),
  mFrame(0),
  mRepl(true) {
  mP["+"]   = new XYPrimitive("+", primitive_addition);
  mP["-"]   = new XYPrimitive("-", primitive_subtraction);
  mP["*"]   = new XYPrimitive("*", primitive_multiplication);
  mP["%"]   = new XYPrimitive("%", primitive_division);
  mP["^"]   = new XYPrimitive("^", primitive_power);
  mP["_"]   = new XYPrimitive("_", primitive_floor);
  mP["set"] = new XYPrimitive("set", primitive_set);
  mP[";"]   = new XYPrimitive(";", primitive_get);
  mP["."]   = new XYPrimitive(".", primitive_unquote);
  mP["puncons"] = new XYPrimitive("puncons", primitive_uncons);
  mP[")"]   = new XYPrimitive(")", primitive_pattern_ss);
  mP["("]   = new XYPrimitive("(", primitive_pattern_sq);
  mP["`"]   = new XYPrimitive("`", primitive_dip);
  mP["|"]   = new XYPrimitive("|", primitive_reverse);
  mP["'"]   = new XYPrimitive("'", primitive_quote);
  mP[","]   = new XYPrimitive(",", primitive_join);
  mP["$"]   = new XYPrimitive("$", primitive_stack);
  mP["$$"]  = new XYPrimitive("$$", primitive_stackqueue);
  mP["="]   = new XYPrimitive("=", primitive_equals);
  mP["<"]   = new XYPrimitive("<", primitive_lessThan);
  mP["<="]  = new XYPrimitive("<=", primitive_lessThanEqual);
  mP[">"]   = new XYPrimitive(">", primitive_greaterThan);
  mP[">="]  = new XYPrimitive(">=", primitive_greaterThanEqual);
  mP["not"] = new XYPrimitive("not", primitive_not);
  mP["@"]   = new XYPrimitive("@", primitive_nth);
  mP["!"]   = new XYPrimitive("!", primitive_set_nth);
  mP["println"] = new XYPrimitive("print", primitive_println);
  mP["print"] = new XYPrimitive("print", primitive_print);
  mP["write"] = new XYPrimitive("write", primitive_write);
  mP["count"] = new XYPrimitive("count", primitive_count);
  mP["tokenize"] = new XYPrimitive("tokenize", primitive_tokenize);
  mP["parse"] = new XYPrimitive("parse", primitive_parse);
  mP["getline"] = new XYPrimitive("getline", primitive_getline);
  mP["millis"] = new XYPrimitive("millis", primitive_millis);
  mP["enum"]   = new XYPrimitive("+", primitive_enum);
  mP["clone"]   = new XYPrimitive("clone", primitive_clone);
  mP["to-string"] = new XYPrimitive("to-string", primitive_to_string);
  mP["split"] = new XYPrimitive("split", primitive_split);
  mP["sdrop"] = new XYPrimitive("sdrop", primitive_sdrop);
  mP["stake"] = new XYPrimitive("stake", primitive_stake);
  mP["foldl"] = new XYPrimitive("foldl", primitive_foldl);
  mP["foldr"] = new XYPrimitive("foldr", primitive_foldr);
  mP["if"] = new XYPrimitive("if", primitive_if);
  mP["?"] = new XYPrimitive("?", primitive_find);
  mP["gc"] = new XYPrimitive("gc", primitive_gc);

  // Object system test primitives. These will change
  // when the system settles down.
  mP["copy"] = new XYPrimitive("copy", primitive_copy);
  mP["add-data-slot"] = new XYPrimitive("add-data-slot", primitive_add_data_slot);
  mP["add-method-slot"] = new XYPrimitive("add-method-slot", primitive_add_method_slot);
  mP["get-slot-value"] = new XYPrimitive("get-slot-value", primitive_get_slot_value);
  mP["set-slot-value"] = new XYPrimitive("set-slot-value", primitive_set_slot_value);
  mP["call-method"] = new XYPrimitive("call-method", primitive_call_method);
  mP["set-method-args"] = new XYPrimitive("set-method-args", primitive_set_method_args);
  mP["lookup"] = new XYPrimitive("lookup", primitive_lookup);
  mP["frame"] = new XYPrimitive("frame", primitive_frame);
  mP["set-frame"] = new XYPrimitive("set-frame", primitive_set_frame);

  // The object prototype
  mFrame = new XYObject();
  XYObject* primitives = new XYObject();
  primitives->addMethod("printline", new XYPrimitive("println", primitive_println));

  mEnv["object"] = mFrame;
  mEnv["primitives"] = primitives;
}

void XY::markChildren() {
  for (XYWaitingList::iterator it = mWaiting.begin();
       it != mWaiting.end();
       ++it) {
    (*it)->mark();
  }
  for (XYEnv::iterator it = mEnv.begin();
       it != mEnv.end();
       ++it) {
    (*it).second->mark();
  }
  for (XYEnv::iterator it = mP.begin();
       it != mP.end();
       ++it) {
    (*it).second->mark();
  }
  for (XYStack::iterator it = mX.begin();
       it != mX.end();
       ++it) {
    (*it)->mark();
  }
  for (XYQueue::iterator it = mY.begin();
       it != mY.end();
       ++it) {
    (*it)->mark();
  }
  for (XYLimits::iterator it = mLimits.begin();
       it != mLimits.end();
       ++it) {
    (*it)->mark();
  }
  if (mFrame)
    mFrame->mark();
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

    mService.post(bind(&XY::evalHandler, this));
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
				    bind(&XY::stdioHandler, this, boost::asio::placeholders::error));
    }
    else if(mY.size() == 0 && !mRepl) {
      // We've completed. Inform waiting interpreters we're done.
      if (mWaiting.size() > 0) {
	for(XYWaitingList::iterator it = mWaiting.begin(); it != mWaiting.end(); ++it ) {
	  (*it)->mService.post(bind(&XY::evalHandler, (*it)));
	}
	mWaiting.clear();
      }
    }
    else {
      mService.post(bind(&XY::evalHandler, this));
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

      XYList* stack(new XYList(mX.begin(), mX.end()));
      XYList* queue(new XYList(mY.begin(), mY.end()));
      XYList* error(new XYList());

      mX.clear();
      mY.clear();

      error->mList.push_back(new XYSymbol("error"));
      error->mList.push_back(new XYString(e.message()));
      error->mList.push_back(stack);
      error->mList.push_back(queue);
      mX.push_back(error);
      // We've completed. Inform waiting interpreters we're done.
      if (!mRepl && mWaiting.size() > 0) {
	for(XYWaitingList::iterator it = mWaiting.begin(); it != mWaiting.end(); ++it ) {
	  (*it)->mService.post(bind(&XY::evalHandler, (*it)));
	}
	mWaiting.clear();
      }

      if (mRepl) {
	for(XYLimits::iterator it = mLimits.begin(); it != mLimits.end(); ++it) {
	  (*it)->start(this);
	}

	mService.post(bind(&XY::evalHandler, this));
      }
    }
  }
}

void XY::yield() {
  mService.post(bind(&XY::evalHandler, this));
  throw XYError(this, XYError::WAITING_FOR_ASYNC_EVENT);
}

void XY::checkLimits() {
  for(XYLimits::iterator it = mLimits.begin(); it != mLimits.end(); ++it) {
    if ((*it)->check(this)) {
      // This limit was reached, stop executing and throw the error
      throw XYError(this, XYError::LIMIT_REACHED);
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

  XYObject* o = mY.front();
  assert(o);

  mY.pop_front();

  GarbageCollector::GC.addRoot(o);
  o->eval1(this);
  GarbageCollector::GC.removeRoot(o);
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
               XYObject* object,
               XYObject* pattern,
               XYSequence* sequence,
               int i) {
  XYSequence* object_list = dynamic_cast<XYSequence*>(object);
  XYSequence* pattern_list = dynamic_cast<XYSequence*>(pattern);
  XYSymbol* pattern_symbol = dynamic_cast<XYSymbol*>(pattern);
  if (object_list && pattern_list) {
    int pi = 0;
    int oi = 0;
    for(pi=0, oi=0; pi < pattern_list->size() && oi < object_list->size(); ++pi, ++oi) {
      match(out, object_list->at(oi), pattern_list->at(pi), object_list, oi);
    }
    // If there are more pattern items than there are list items,
    // set the pattern value to null.
    while(pi < pattern_list->size()) {
      XYSymbol* s = dynamic_cast<XYSymbol*>(pattern_list->at(pi));
      if (s) {
        *out++ = make_pair(s->mValue, new XYList());
      }
      ++pi;
    }
  }
  else if(pattern_list && !object_list) {
    // If the pattern is a list, but the object is not, 
    // pretend the object is a one element list. This enables:
    // 42 [ [[a A]] a A ] -> 42 []
    XYList* list(new XYList());
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
void XY::getPatternValues(XYObject* pattern, OutputIterator out) {
  XYSequence* list = dynamic_cast<XYSequence*>(pattern);
  if (list) {
    assert(mX.size() >= list->size());
    XYList* stack(new XYList(mX.end() - list->size(), mX.end()));
    match(out, stack, pattern, stack, 0);
    mX.resize(mX.size() - list->size());
  }
  else {
    XYObject* o = mX.back();
    mX.pop_back();
    XYList* list(new XYList());
    match(out, o, pattern, list, list->size());
  }
}
 
template <class OutputIterator>
void XY::replacePattern(XYEnv const& env, XYObject* object, OutputIterator out) {
  XYSequence* list   = dynamic_cast<XYSequence*>(object);
  XYSymbol*   symbol = dynamic_cast<XYSymbol*>(object); 
  if (list) {
    // Recurse through the list replacing variables as needed
    XYList* new_list(new XYList());
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
  return (set= '\\' , '[' , ']' , '{' , '}' , '(' , ')' , ';' , '!' , '.' , ',' , '`' , '\'' , '|', '@', '+', '*');
}

// Return regex for non-specials
boost::xpressive::sregex re_non_special() {
  using namespace boost::xpressive;
  using boost::xpressive::set;
  return ~(set[(set= '\\','[',']','{','}','(',')',';','!','.',',','`','\'','|','@','+','*') | _s]);
}

// Return regex for allowed end of symbols
boost::xpressive::sregex re_symbol_end() {
  using namespace boost::xpressive;
  using boost::xpressive::set;
  return (set= '+', '*');
}

// Return regex for symbols
boost::xpressive::sregex re_symbol() {
  using namespace boost::xpressive;
  return !range('0', '9') >> +(re_non_special()) >> *(re_symbol_end());
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
