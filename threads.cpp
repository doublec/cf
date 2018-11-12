// Copyright (C) 2009 Chris Double. All Rights Reserved.
// See the license at the end of this file
#include "cf.h"
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include "threads.h"

using namespace std;
using namespace boost;

class XYThread : public XYObject {
public:
  XY* mXY;
  XY* mParent;

public:
  XYThread(XY* xy, XY* parent);

  virtual void markChildren();
  void spawn();

  virtual void print(std::ostringstream& stream, CircularSet& seen, bool parse) const;
  virtual void eval1(XY* xy);
  virtual int compare(XYObject* rhs);
};

// XYThread
XYThread::XYThread(XY* xy, XY* parent) :
  mXY(xy),
  mParent(parent)
{
}

void XYThread::markChildren() {
  mXY->mark();
  if (mParent)
    mParent->mark();
}

void XYThread::spawn() {
  mXY->mRepl = false;
  for(XYLimits::iterator it = mXY->mLimits.begin(); it != mXY->mLimits.end(); ++it) {
    (*it)->start(mXY);
  }

  boost::asio::post(mXY->mIOContext, bind(&XY::evalHandler, mXY));
}

void XYThread::print(std::ostringstream& stream, CircularSet&, bool) const {
  stream << "thread";
}

void XYThread::eval1(XY* xy) {
  xy->mX.push_back(this);
}

int XYThread::compare(XYObject* rhs) {
  if (rhs == this)
    return 0;

  return rhs < this;
}


// make-thread [X^stack^queue Y] -> [X^thread Y]
static void primitive_make_thread(XY* xy) {
  xy_assert(xy->mX.size() >= 2, XYError::STACK_UNDERFLOW);
  XYSequence* queue(dynamic_cast<XYSequence*>(xy->mX.back()));
  xy_assert(queue, XYError::TYPE);
  xy->mX.pop_back();

  XYSequence* stack(dynamic_cast<XYSequence*>(xy->mX.back()));
  xy_assert(stack, XYError::TYPE);
  xy->mX.pop_back();

  XY* child(new XY(xy->mIOContext));
  stack->pushBackInto(child->mX);

  XYSequence::List temp;
  queue->pushBackInto(temp);
  child->mY.insert(child->mY.begin(), temp.begin(), temp.end());

  child->mEnv = xy->mEnv;
  child->mP = xy->mP;
  child->mLimits = xy->mLimits;

  XYThread* thread(new XYThread(child, xy));

  xy->mX.push_back(thread);
}

// make-limited-thread [X^stack^queue^ms Y] -> [X^thread Y]
// If the thread takes longer to execute than the given milliseconds
// then it is aborted.
static void primitive_make_limited_thread(XY* xy) {
  xy_assert(xy->mX.size() >= 3, XYError::STACK_UNDERFLOW);
  XYNumber* ms(dynamic_cast<XYNumber*>(xy->mX.back()));
  xy_assert(ms, XYError::TYPE);
  xy->mX.pop_back();

  XYSequence* queue(dynamic_cast<XYSequence*>(xy->mX.back()));
  xy_assert(queue, XYError::TYPE);
  xy->mX.pop_back();

  XYSequence* stack(dynamic_cast<XYSequence*>(xy->mX.back()));
  xy_assert(stack, XYError::TYPE);
  xy->mX.pop_back();

  XY* child(new XY(xy->mIOContext));
  stack->pushBackInto(child->mX);

  XYSequence::List temp;
  queue->pushBackInto(temp);
  child->mY.insert(child->mY.begin(), temp.begin(), temp.end());

  child->mEnv = xy->mEnv;
  child->mP = xy->mP;
  child->mLimits = xy->mLimits;

  child->mLimits.push_back(new XYTimeLimit(ms->as_uint()));

  XYThread* thread(new XYThread(child, xy));

  xy->mX.push_back(thread);
}

// spawn [X^thread Y] -> [X^thread Y]
static void primitive_spawn(XY* xy) {
  xy_assert(xy->mX.size() >= 1, XYError::STACK_UNDERFLOW);
  XYThread* thread(dynamic_cast<XYThread*>(xy->mX.back()));
  xy_assert(thread, XYError::TYPE);

  thread->spawn();
}

// thread-resume [X^[...]^thread Y] -> [X^thread Y]
static void primitive_thread_resume(XY* xy) {
  xy_assert(xy->mX.size() >= 2, XYError::STACK_UNDERFLOW);
  XYThread* thread(dynamic_cast<XYThread*>(xy->mX.back()));
  xy_assert(thread, XYError::TYPE);
  xy->mX.pop_back();

  XYSequence* y(dynamic_cast<XYSequence*>(xy->mX.back()));
  xy_assert(y, XYError::TYPE);
  xy->mX.pop_back();
  
  int n = y->size();
  for (int i=0; i < n; ++i)
    thread->mXY->mY.push_front(y->at(n-i-1));

  thread->spawn();
  xy->mX.push_back(thread);
}

// thread-stacks [X^thread Y] -> [X^stack^queue Y]
static void primitive_thread_stacks(XY* xy) {
  xy_assert(xy->mX.size() >= 1, XYError::STACK_UNDERFLOW);
  XYThread* thread(dynamic_cast<XYThread*>(xy->mX.back()));
  xy_assert(thread, XYError::TYPE);
  xy->mX.pop_back();

  XYList* stack(new XYList(thread->mXY->mX.begin(), thread->mXY->mX.end()));
  XYList* queue(new XYList(thread->mXY->mY.begin(), thread->mXY->mY.end()));

  xy->mX.push_back(stack);
  xy->mX.push_back(queue);
}

// thread-join [X^thread Y] -> [X^stack Y]
// Block current thread until the given thread has completed, leaving
// its stack on the current thread's stack.
static void primitive_thread_join(XY* xy) {
  xy_assert(xy->mX.size() >= 1, XYError::STACK_UNDERFLOW);
  XYThread* thread(dynamic_cast<XYThread*>(xy->mX.back()));
  xy_assert(thread, XYError::TYPE);

  if (thread->mXY->mY.size() != 0) {
    xy->mY.push_front(new XYPrimitive("thread-join", primitive_thread_join));
    thread->mXY->mWaiting.push_back(xy);
    throw XYError(xy, XYError::WAITING_FOR_ASYNC_EVENT);
  }

  xy->mX.pop_back();

  XYList* stack(new XYList(thread->mXY->mX.begin(), thread->mXY->mX.end()));

  xy->mX.push_back(stack);
}

void install_thread_primitives(XY* xy) {
  xy->mP["make-limited-thread"] = new XYPrimitive("make-limited-thread", primitive_make_limited_thread);
  xy->mP["make-thread"] = new XYPrimitive("make-thread", primitive_make_thread);
  xy->mP["thread-stacks"] = new XYPrimitive("thread-stacks", primitive_thread_stacks);
  xy->mP["thread-join"] = new XYPrimitive("thread-join", primitive_thread_join);
  xy->mP["thread-resume"] = new XYPrimitive("thread-resume", primitive_thread_resume);
  xy->mP["spawn"] = new XYPrimitive("spawn", primitive_spawn);
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
