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
  void writeln(intrusive_ptr<XYString> const& seq);
  void writeln(intrusive_ptr<XYSequence> const& seq);
  void close();

  virtual std::string toString(bool parse) const;
  virtual void eval1(boost::intrusive_ptr<XY> const& xy);
  virtual int compare(boost::intrusive_ptr<XYObject> rhs);
};

class XYLineChannel : public XYObject {
public:
  XYQueue mLines;
  boost::asio::streambuf mResponse;
  intrusive_ptr<XYSocket> mSocket;

  // Set to interpreters that are waiting for
  // lines to appear.
  vector<intrusive_ptr<XY> > mWaiting;

public:
  XYLineChannel(intrusive_ptr<XYSocket> const& socket);

  void handleRead(boost::system::error_code const& err);

  virtual std::string toString(bool parse) const;
  virtual void eval1(boost::intrusive_ptr<XY> const& xy);
  virtual int compare(boost::intrusive_ptr<XYObject> rhs);
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

void XYSocket::writeln(intrusive_ptr<XYString> const& str) {
  boost::asio::streambuf request;
  std::ostream request_stream(&request);
  request_stream << str->mValue << "\r\n";
  boost::asio::write(mSocket, request);
}

void XYSocket::writeln(intrusive_ptr<XYSequence> const& seq) {
  boost::asio::streambuf request;
  std::ostream request_stream(&request);
  for(int i=0; i < seq->size(); ++i) {
    intrusive_ptr<XYInteger> n(dynamic_pointer_cast<XYInteger>(seq->at(i)));
    assert(n);
    request_stream << (char)(n->as_uint());
  }
  request_stream << "\r\n";
  boost::asio::write(mSocket, request);
}

void XYSocket::close() {
  mSocket.close();
}

std::string XYSocket::toString(bool parse) const {
  return "socket";
}

void XYSocket::eval1(intrusive_ptr<XY> const& xy) {
  xy->mX.push_back(msp(this));
}

int XYSocket::compare(intrusive_ptr<XYObject> rhs) {
  if (rhs.get() == this)
    return 0;

  return rhs.get() < this;
}

// XYLineChannel
XYLineChannel::XYLineChannel(intrusive_ptr<XYSocket> const& socket) :
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
    mLines.push_back(msp(new XYString(result)));
    if (mWaiting.size() > 0) {
      for(vector<intrusive_ptr<XY> >::iterator it = mWaiting.begin(); it != mWaiting.end(); ++it ) {
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

std::string XYLineChannel::toString(bool parse) const {
  ostringstream str;
  str << "line-channel(" << mLines.size() << ")";
  return str.str();
}

void XYLineChannel::eval1(intrusive_ptr<XY> const& xy) {
  xy->mX.push_back(msp(this));
}

int XYLineChannel::compare(intrusive_ptr<XYObject> rhs) {
  if (rhs.get() == this)
    return 0;

  return rhs.get() < this;
}


// socket [X^host^port Y] -> [X^socket Y]
static void primitive_socket(boost::intrusive_ptr<XY> const& xy) {
  xy_assert(xy->mX.size() >= 2, XYError::STACK_UNDERFLOW);
  intrusive_ptr<XYObject> port(xy->mX.back());
  xy->mX.pop_back();

  intrusive_ptr<XYString> host(dynamic_pointer_cast<XYString>(xy->mX.back()));
  xy_assert(host, XYError::TYPE);
  xy->mX.pop_back();

  intrusive_ptr<XYSocket> socket(new XYSocket(xy->mService));
  socket->connect(host->mValue, port->toString(false));
  xy->mX.push_back(socket);
}

// socket-close [X^socket Y] -> [X Y]
static void primitive_socket_close(boost::intrusive_ptr<XY> const& xy) {
  xy_assert(xy->mX.size() >= 1, XYError::STACK_UNDERFLOW);
  intrusive_ptr<XYSocket> socket(dynamic_pointer_cast<XYSocket>(xy->mX.back()));
  xy_assert(socket, XYError::TYPE);
  xy->mX.pop_back();

  socket->close();
}

// socket-writeln [X^string^socket Y] -> [X Y]
static void primitive_socket_writeln(boost::intrusive_ptr<XY> const& xy) {
  xy_assert(xy->mX.size() >= 2, XYError::STACK_UNDERFLOW);
  intrusive_ptr<XYSocket> socket(dynamic_pointer_cast<XYSocket>(xy->mX.back()));
  xy_assert(socket, XYError::TYPE);
  xy->mX.pop_back();

  intrusive_ptr<XYString> str(dynamic_pointer_cast<XYString>(xy->mX.back()));
  intrusive_ptr<XYSequence> seq(dynamic_pointer_cast<XYSequence>(xy->mX.back()));
  xy_assert(str || seq, XYError::TYPE);
  xy->mX.pop_back();
  
  if(str) 
    socket->writeln(str);
  else
    socket->writeln(seq);
}

// line-channel [X^socket Y] -> [X^channel Y]
static void primitive_line_channel(boost::intrusive_ptr<XY> const& xy) {
  xy_assert(xy->mX.size() >= 1, XYError::STACK_UNDERFLOW);
  intrusive_ptr<XYSocket> socket(dynamic_pointer_cast<XYSocket>(xy->mX.back()));
  xy_assert(socket, XYError::TYPE);
  xy->mX.pop_back();

  xy->mX.push_back(msp(new XYLineChannel(socket)));
}

// line-channel-get [X^channel Y] -> [X^str Y]
static void primitive_line_channel_get(boost::intrusive_ptr<XY> const& xy) {
  xy_assert(xy->mX.size() >= 1, XYError::STACK_UNDERFLOW);

  intrusive_ptr<XYLineChannel> channel(dynamic_pointer_cast<XYLineChannel>(xy->mX.back()));
  xy_assert(channel, XYError::TYPE);

  if (channel->mLines.size() == 0) {
    channel->mWaiting.push_back(xy);
    xy->mY.push_front(msp(new XYPrimitive("line-channel-get", primitive_line_channel_get)));
    throw XYError(xy, XYError::WAITING_FOR_ASYNC_EVENT);
  }
    
  xy->mX.pop_back();
  
  intrusive_ptr<XYString> line(dynamic_pointer_cast<XYString>(channel->mLines.front()));
  xy_assert(line, XYError::TYPE);
  channel->mLines.pop_front();

  xy->mX.push_back(line);		     
}

// line-channel-getall [X^channel Y] -> [X^seq Y]
static void primitive_line_channel_getall(boost::intrusive_ptr<XY> const& xy) {
  xy_assert(xy->mX.size() >= 1, XYError::STACK_UNDERFLOW);

  intrusive_ptr<XYLineChannel> channel(dynamic_pointer_cast<XYLineChannel>(xy->mX.back()));
  xy_assert(channel, XYError::TYPE);

  if (channel->mLines.size() == 0) {
    channel->mWaiting.push_back(xy);
    xy->mY.push_front(msp(new XYPrimitive("line-channel-getall", primitive_line_channel_getall)));
    throw XYError(xy, XYError::WAITING_FOR_ASYNC_EVENT);
  }

  xy->mX.pop_back();

  intrusive_ptr<XYList> list(new XYList());
  while (channel->mLines.size() != 0) {
    intrusive_ptr<XYString> line(dynamic_pointer_cast<XYString>(channel->mLines.front()));
    xy_assert(line, XYError::TYPE);
    channel->mLines.pop_front();
    list->mList.push_back(line);
  }
  
  xy->mX.push_back(list);		     
}

// line-channel-count [X^channel Y] -> [X^n Y]
static void primitive_line_channel_count(boost::intrusive_ptr<XY> const& xy) {
  xy_assert(xy->mX.size() >= 1, XYError::STACK_UNDERFLOW);
  intrusive_ptr<XYLineChannel> channel(dynamic_pointer_cast<XYLineChannel>(xy->mX.back()));
  xy_assert(channel, XYError::TYPE);
  xy->mX.pop_back();

  xy->mX.push_back(msp(new XYInteger(channel->mLines.size())));		     
}

void install_socket_primitives(intrusive_ptr<XY> const& xy) {
  xy->mP["socket"] = msp(new XYPrimitive("socket", primitive_socket));
  xy->mP["socket-writeln"] = msp(new XYPrimitive("socket-writeln", primitive_socket_writeln));
  xy->mP["socket-close"] = msp(new XYPrimitive("socket-close", primitive_socket_close));
  xy->mP["line-channel"] = msp(new XYPrimitive("line-channel", primitive_line_channel));
  xy->mP["line-channel-get"] = msp(new XYPrimitive("line-channel-get", primitive_line_channel_get));
  xy->mP["line-channel-getall"] = msp(new XYPrimitive("line-channel-getall", primitive_line_channel_getall));
  xy->mP["line-channel-count"] = msp(new XYPrimitive("line-channel-count", primitive_line_channel_count));
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
