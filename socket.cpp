// Copyright (C) 2009 Chris Double. All Rights Reserved.
// See the license at the end of this file
#include <iostream>
#include <istream>
#include <ostream>
#include <sstream>
#include "cf.h"
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include "socket.h"

using namespace std;
using namespace boost;
using boost::asio::ip::tcp;

class XYSocket : public XYObject {
public:
  boost::asio::io_service& mService;
  tcp::socket mSocket;

public:
  XYSocket(boost::asio::io_service& service);

  void connect(string const& host, std::string const& port);
  void writeln(XYString* seq);
  void writeln(XYSequence* seq);
  void close();

  virtual void print(std::ostringstream& stream, CircularSet& seen, bool parse) const;
  virtual void eval1(XY* xy);
  virtual int compare(XYObject* rhs);
};

class XYLineChannel : public XYObject {
public:
  XYQueue mLines;
  boost::asio::streambuf mResponse;
  XYSocket* mSocket;

  // Set to interpreters that are waiting for
  // lines to appear.
  XYWaitingList mWaiting;

public:
  XYLineChannel(XYSocket* socket);

  void handleRead(boost::system::error_code const& err);

  virtual void print(std::ostringstream& stream, CircularSet& seen, bool parse) const;
  virtual void eval1(XY* xy);
  virtual int compare(XYObject* rhs);
};

// XYSocket
XYSocket::XYSocket(boost::asio::io_service& service) :
  mService(service),
  mSocket(service)
{
}

void XYSocket::connect(string const& host, std::string const& port) {
  tcp::resolver resolver(mService);
  tcp::resolver::query query(tcp::v4(), host, port);
  tcp::resolver::iterator iterator = resolver.resolve(query);
  mSocket.connect(*iterator);
}

void XYSocket::writeln(XYString* str) {
  boost::asio::streambuf request;
  std::ostream request_stream(&request);
  request_stream << str->mValue << "\r\n";
  boost::asio::write(mSocket, request);
}

void XYSocket::writeln(XYSequence* seq) {
  boost::asio::streambuf request;
  std::ostream request_stream(&request);
  for(int i=0; i < seq->size(); ++i) {
    XYInteger* n(dynamic_cast<XYInteger*>(seq->at(i)));
    assert(n);
    request_stream << (char)(n->as_uint());
  }
  request_stream << "\r\n";
  boost::asio::write(mSocket, request);
}

void XYSocket::close() {
  mSocket.close();
}

void XYSocket::print(std::ostringstream& stream, CircularSet&, bool) const {
  stream << "socket";
}

void XYSocket::eval1(XY* xy) {
  xy->mX.push_back(this);
}

int XYSocket::compare(XYObject* rhs) {
  if (rhs == this)
    return 0;

  return rhs < this;
}

// XYLineChannel
XYLineChannel::XYLineChannel(XYSocket* socket) :
  mSocket(socket) {
  // read lines until EOF.
  boost::asio::async_read_until(mSocket->mSocket, 
				mResponse,
				"\r\n",
				boost::bind(&XYLineChannel::handleRead, 
					    this,
					    boost::asio::placeholders::error));
}

void XYLineChannel::handleRead(boost::system::error_code const& err) {
  if (!err) {
    //    cout << "Got some data" << endl;
    istream stream(&mResponse);
    string result;
    std::getline(stream, result);
    if (result[result.size()-1] == 13)
      result = result.substr(0, result.size()-1);
    mLines.push_back(new XYString(result));
    if (mWaiting.size() > 0) {
      for(XYWaitingList::iterator it = mWaiting.begin(); it != mWaiting.end(); ++it ) {
	(*it)->mService.post(bind(&XY::evalHandler, (*it)));
      }
      mWaiting.clear();
    }

    // read lines until EOF.
    boost::asio::async_read_until(mSocket->mSocket, 
				  mResponse,
				  "\r\n",
				  boost::bind(&XYLineChannel::handleRead, 
					      this,
					      boost::asio::placeholders::error));
  }
  else if (err != boost::asio::error::eof) {
    cout << "Socket error: " << err << endl;
  }
}

void XYLineChannel::print(std::ostringstream& stream, CircularSet&, bool) const {
  stream << "line-channel(" << mLines.size() << ")";
}

void XYLineChannel::eval1(XY* xy) {
  xy->mX.push_back(this);
}

int XYLineChannel::compare(XYObject* rhs) {
  if (rhs == this)
    return 0;

  return rhs < this;
}


// socket [X^host^port Y] -> [X^socket Y]
static void primitive_socket(XY* xy) {
  xy_assert(xy->mX.size() >= 2, XYError::STACK_UNDERFLOW);
  XYObject* port(xy->mX.back());
  xy->mX.pop_back();

  XYString* host(dynamic_cast<XYString*>(xy->mX.back()));
  xy_assert(host, XYError::TYPE);
  xy->mX.pop_back();

  XYSocket* socket(new XYSocket(xy->mService));
  socket->connect(host->mValue, port->toString(false));
  xy->mX.push_back(socket);
}

// socket-close [X^socket Y] -> [X Y]
static void primitive_socket_close(XY* xy) {
  xy_assert(xy->mX.size() >= 1, XYError::STACK_UNDERFLOW);
  XYSocket* socket(dynamic_cast<XYSocket*>(xy->mX.back()));
  xy_assert(socket, XYError::TYPE);
  xy->mX.pop_back();

  socket->close();
}

// socket-writeln [X^string^socket Y] -> [X Y]
static void primitive_socket_writeln(XY* xy) {
  xy_assert(xy->mX.size() >= 2, XYError::STACK_UNDERFLOW);
  XYSocket* socket(dynamic_cast<XYSocket*>(xy->mX.back()));
  xy_assert(socket, XYError::TYPE);
  xy->mX.pop_back();

  XYString* str(dynamic_cast<XYString*>(xy->mX.back()));
  XYSequence* seq(dynamic_cast<XYSequence*>(xy->mX.back()));
  xy_assert(str || seq, XYError::TYPE);
  xy->mX.pop_back();
  
  if(str) 
    socket->writeln(str);
  else
    socket->writeln(seq);
}

// line-channel [X^socket Y] -> [X^channel Y]
static void primitive_line_channel(XY* xy) {
  xy_assert(xy->mX.size() >= 1, XYError::STACK_UNDERFLOW);
  XYSocket* socket(dynamic_cast<XYSocket*>(xy->mX.back()));
  xy_assert(socket, XYError::TYPE);
  xy->mX.pop_back();

  xy->mX.push_back(new XYLineChannel(socket));
}

// line-channel-get [X^channel Y] -> [X^str Y]
static void primitive_line_channel_get(XY* xy) {
  xy_assert(xy->mX.size() >= 1, XYError::STACK_UNDERFLOW);

  XYLineChannel* channel(dynamic_cast<XYLineChannel*>(xy->mX.back()));
  xy_assert(channel, XYError::TYPE);

  if (channel->mLines.size() == 0) {
    channel->mWaiting.push_back(xy);
    xy->mY.push_front(new XYPrimitive("line-channel-get", primitive_line_channel_get));
    throw XYError(xy, XYError::WAITING_FOR_ASYNC_EVENT);
  }
    
  xy->mX.pop_back();
  
  XYString* line(dynamic_cast<XYString*>(channel->mLines.front()));
  xy_assert(line, XYError::TYPE);
  channel->mLines.pop_front();

  xy->mX.push_back(line);		     
}

// line-channel-getall [X^channel Y] -> [X^seq Y]
static void primitive_line_channel_getall(XY* xy) {
  xy_assert(xy->mX.size() >= 1, XYError::STACK_UNDERFLOW);

  XYLineChannel* channel(dynamic_cast<XYLineChannel*>(xy->mX.back()));
  xy_assert(channel, XYError::TYPE);

  if (channel->mLines.size() == 0) {
    channel->mWaiting.push_back(xy);
    xy->mY.push_front(new XYPrimitive("line-channel-getall", primitive_line_channel_getall));
    throw XYError(xy, XYError::WAITING_FOR_ASYNC_EVENT);
  }

  xy->mX.pop_back();

  XYList* list(new XYList());
  while (channel->mLines.size() != 0) {
    XYString* line(dynamic_cast<XYString*>(channel->mLines.front()));
    xy_assert(line, XYError::TYPE);
    channel->mLines.pop_front();
    list->mList.push_back(line);
  }
  
  xy->mX.push_back(list);		     
}

// line-channel-count [X^channel Y] -> [X^n Y]
static void primitive_line_channel_count(XY* xy) {
  xy_assert(xy->mX.size() >= 1, XYError::STACK_UNDERFLOW);
  XYLineChannel* channel(dynamic_cast<XYLineChannel*>(xy->mX.back()));
  xy_assert(channel, XYError::TYPE);
  xy->mX.pop_back();

  xy->mX.push_back(new XYInteger(channel->mLines.size()));
}

void install_socket_primitives(XY* xy) {
  xy->mP["socket"] = new XYPrimitive("socket", primitive_socket);
  xy->mP["socket-writeln"] = new XYPrimitive("socket-writeln", primitive_socket_writeln);
  xy->mP["socket-close"] = new XYPrimitive("socket-close", primitive_socket_close);
  xy->mP["line-channel"] = new XYPrimitive("line-channel", primitive_line_channel);
  xy->mP["line-channel-get"] = new XYPrimitive("line-channel-get", primitive_line_channel_get);
  xy->mP["line-channel-getall"] = new XYPrimitive("line-channel-getall", primitive_line_channel_getall);
  xy->mP["line-channel-count"] = new XYPrimitive("line-channel-count", primitive_line_channel_count);
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
