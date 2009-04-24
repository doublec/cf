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
  shared_ptr<XY> mXY;
  shared_ptr<XY> mParent;

public:
  XYThread(boost::shared_ptr<XY> const& xy, boost::shared_ptr<XY> const& parent);

  void spawn();

  virtual std::string toString(bool parse) const;
  virtual void eval1(boost::shared_ptr<XY> const& xy);
  virtual int compare(boost::shared_ptr<XYObject> rhs);
};

// XYThread
XYThread::XYThread(shared_ptr<XY> const& xy, boost::shared_ptr<XY> const& parent) :
  mXY(xy),
  mParent(parent)
{
}

void XYThread::spawn() {
  mXY->mRepl = false;
  mXY->mService.post(bind(&XY::evalHandler, mXY));
}

std::string XYThread::toString(bool parse) const {
  return "thread";
}

void XYThread::eval1(shared_ptr<XY> const& xy) {
  xy->mX.push_back(shared_from_this());
}

int XYThread::compare(shared_ptr<XYObject> rhs) {
  if (rhs.get() == this)
    return 0;

  return rhs.get() < this;
}


// make-thread [X^stack^queue Y] -> [X^thread Y]
static void primitive_make_thread(boost::shared_ptr<XY> const& xy) {
  xy_assert(xy->mX.size() >= 2, XYError::STACK_UNDERFLOW);
  shared_ptr<XYSequence> queue(dynamic_pointer_cast<XYSequence>(xy->mX.back()));
  xy_assert(queue, XYError::TYPE);
  xy->mX.pop_back();

  shared_ptr<XYSequence> stack(dynamic_pointer_cast<XYSequence>(xy->mX.back()));
  xy_assert(stack, XYError::TYPE);
  xy->mX.pop_back();

  shared_ptr<XY> child(new XY(xy->mService));
  stack->pushBackInto(child->mX);

  XYSequence::List temp;
  queue->pushBackInto(temp);
  child->mY.insert(child->mY.begin(), temp.begin(), temp.end());

  child->mEnv = xy->mEnv;
  child->mP = xy->mP;

  shared_ptr<XYThread> thread(new XYThread(child, xy));

  xy->mX.push_back(thread);
}

// spawn [X^thread Y] -> [X^thread Y]
static void primitive_spawn(boost::shared_ptr<XY> const& xy) {
  xy_assert(xy->mX.size() >= 1, XYError::STACK_UNDERFLOW);
  shared_ptr<XYThread> thread(dynamic_pointer_cast<XYThread>(xy->mX.back()));
  xy_assert(thread, XYError::TYPE);

  thread->spawn();
}

// thread-stacks [X^thread Y] -> [X^stack^queue Y]
static void primitive_thread_stacks(boost::shared_ptr<XY> const& xy) {
  xy_assert(xy->mX.size() >= 1, XYError::STACK_UNDERFLOW);
  shared_ptr<XYThread> thread(dynamic_pointer_cast<XYThread>(xy->mX.back()));
  xy_assert(thread, XYError::TYPE);
  xy->mX.pop_back();

  shared_ptr<XYList> stack(new XYList(thread->mXY->mX.begin(), thread->mXY->mX.end()));
  shared_ptr<XYList> queue(new XYList(thread->mXY->mY.begin(), thread->mXY->mY.end()));

  xy->mX.push_back(stack);
  xy->mX.push_back(queue);
}

void install_thread_primitives(shared_ptr<XY> const& xy) {
  xy->mP["make-thread"] = msp(new XYPrimitive("make-thread", primitive_make_thread));
  xy->mP["thread-stacks"] = msp(new XYPrimitive("thread-stacks", primitive_thread_stacks));
  xy->mP["spawn"] = msp(new XYPrimitive("spawn", primitive_spawn));
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
