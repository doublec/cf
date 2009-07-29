// Copyright (C) 2009 Chris Double. All Rights Reserved.
// See the license at the end of this file
#if !defined(cf_h)
#define cf_h

#include <string>
#include <map>
#include <set>
#include <vector>
#include <deque>
#include <sstream>
#include <boost/xpressive/xpressive.hpp>
#include <boost/asio.hpp>
#include <gmpxx.h>
#include "gc/gc.h"

// XY is the object that contains the state of the running
// system. For example, the stack (X), the queue (Y) and
// the environment.
class XY;

// Forward declare classes
class XYObject;
class XYList;
class XYPrimitive;
class XYFloat;
class XYInteger;
class XYSequence;

// Macros to declare double dispatched math operators
#define DD(name) \
    virtual XYObject* name(XYObject* rhs);   \
    virtual XYObject* name(XYFloat* lhs);    \
    virtual XYObject* name(XYInteger* lhs);  \
    virtual XYObject* name(XYSequence* lhs)		   

// XYObjects form a prototype object system similar to that in the
// 'Self' programming language. 
// An XY object has zero or more slots. A slot has a name and a 
// quotation. The quotation has stack effect ( ... object -- ... ).
// Slots can optionally have a value. This is used for data slots
// where the getter and setter change this value.
// A slot can be a parent slot, in which case the value stored there
// is used in the prototype lookup chain. A parent slot must be a data
// slot.
//
// The value held in the XYObject Slots map. This holds data
// for the given slot.
class XYSlot : public GCObject
{
 public:
  XYObject* mMethod;
  XYObject* mValue;
  bool mParent;

 public:
  XYSlot(XYObject* method, XYObject* value, bool parent);

  // GCObject method
  virtual void markChildren();
};

// Base class for all objects in the XY system. Anything
// stored on the stack, in the queue, in the the environment
// must be derived from this. 
//
class XYObject : public GCObject
{
 public:    
  // Mapping of slot name to getter/setter.
  typedef std::map<std::string, XYSlot*> Slots;
  Slots mSlots;

 public:
  XYObject();

  // Ensure virtual destructors for base classes
  virtual ~XYObject() { }

  // GCObject method
  virtual void markChildren();

  // Find the slot with the given name,
  // searching down the prototype chain as needed.
  // 'circular' is used to prevent infinite loops
  // while lookup up parent objects. The 'context'
  // will hold a pointer to the object that where the
  // slot was found. 'context' can be passed null in
  // which case it is ignored.
  XYSlot* lookup(std::string const& name, 
		 std::set<XYObject*>& circular,
		 XYObject** context);

  // Get the slot object if there is one, without
  // following the prototype lookup chain.
  XYSlot* getSlot(std::string const& name);

  // Adds a slot
  void addSlot(std::string const& name, 
	       XYObject* method,
	       XYObject* value,
	       bool parent);
  
  // Remove a slot
  void removeSlot(std::string const& name);

  // Adds a method Slot. Different overloadings to make things easier.
  void addMethod(std::string const& name, XYList* method);
  void addMethod(std::string const& name, XYPrimitive* method);
  void addMethod(std::string const& name, XYObject* method);
		 
  // Create a shallow copy of this object.
  virtual XYObject* copy() const;

  // Call when the object has been removed from the XY
  // queue and some action needs to be taken. For
  // literal objects (numbers, strings, etc) this 
  // involves pushing the object on the stack.
  virtual void eval1(XY* xy);

  // Convert the object into a string repesentation for
  // printing. if 'parse' is true then the output
  // should be able to be read back in by the cf parser.
  virtual std::string toString(bool parse) const;

  // This does the actual work of the string conversion for
  // printing. It is overriden by derived classes to write to
  // a string stream. A set containing all objects already
  // printed is passed to enable circular references to be
  // detected.
  typedef std::set<XYObject const*> CircularSet;
  virtual void print(std::ostringstream& stream, CircularSet& seen, bool parse) const;
  
  // Return a negative number if this object is less than
  // the rhs object. Return 0 if they are equal. Returns
  // a positive number if it is greater.
  virtual int compare(XYObject* rhs);

  // Math Operators
  DD(add);
  DD(subtract);
  DD(multiply);
  DD(divide);
  DD(power);
};

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
    virtual XYInteger* as_integer() = 0;
    virtual XYFloat* as_float() = 0;

    // Math Operators
    virtual XYNumber* floor() = 0;
};

// Floating point numbers
class XYFloat : public XYNumber
{
  public:
    mpf_class mValue;

  public:
    XYFloat(long v = 0);
    XYFloat(double v = 0.0);
    XYFloat(std::string v);
    XYFloat(mpf_class const& v);
    virtual void print(std::ostringstream& stream, CircularSet& seen, bool parse) const;
    virtual int compare(XYObject* rhs);
    DD(add);
    DD(subtract);
    DD(multiply);
    DD(divide);
    DD(power);
    virtual bool is_zero() const;
    virtual unsigned int as_uint() const;
    virtual XYInteger* as_integer();
    virtual XYFloat* as_float();
    virtual XYNumber* floor();
};

// Integer numbers
class XYInteger : public XYNumber
{
  public:
    mpz_class mValue;

  public:
    XYInteger(long v = 0);
    XYInteger(std::string v);
    XYInteger(mpz_class const& v);
    virtual void print(std::ostringstream& stream, CircularSet& seen, bool parse) const;
    virtual int compare(XYObject* rhs);
    DD(add);
    DD(subtract);
    DD(multiply);
    DD(divide);
    DD(power);
    virtual bool is_zero() const;
    virtual unsigned int as_uint() const;
    virtual XYInteger* as_integer();
    virtual XYFloat* as_float();
    virtual XYNumber* floor();
};

// A symbol is an unquoted string.
class XYSymbol : public XYObject
{
  public:
    std::string mValue;

  public:
    XYSymbol(std::string v);
    virtual void print(std::ostringstream& stream, CircularSet& seen, bool parse) const;
    virtual void eval1(XY* xy);
    virtual int compare(XYObject* rhs);
};

// A shuffle symbol describes pattern to rearrange the stack.
class XYShuffle : public XYObject
{
  public:
    std::string mBefore;
    std::string mAfter;

  public:
    XYShuffle(std::string v);
    virtual void print(std::ostringstream& stream, CircularSet& seen, bool parse) const;
    virtual void eval1(XY* xy);
    virtual int compare(XYObject* rhs);
};

// A base class for a sequence of XYObject's
class XYSequence : public XYObject
{
  public:
    typedef std::vector<XYObject*> List;
    typedef List::iterator iterator;
    typedef List::const_iterator const_iterator;

  public:
    virtual int compare(XYObject* rhs);
    DD(add);
    DD(subtract);
    DD(multiply);
    DD(divide);
    DD(power);

    // Returns the size of the sequence
    virtual size_t size() = 0;

    // Inserts all elements of this sequence into the C++ container
    virtual void pushBackInto(List& list) = 0;

    // Returns the element at index 'n'
    virtual XYObject* at(size_t n) = 0;

    // Set the element at index 'n' to the value 'v'
    virtual void set_at(size_t n, XYObject* v) = 0;

    // Returns the head of a sequence (the first item).
    virtual XYObject* head() = 0;

    // Returns the tail of a sequence (all but the first item)
    virtual XYSequence* tail() = 0;

    // Concatenate two sequences
    virtual XYSequence* join(XYSequence* rhs) = 0;
};

// A string
class XYString : public XYSequence
{
  public:
    std::string mValue;

  public:
    XYString(std::string v);
    virtual void print(std::ostringstream& stream, CircularSet& seen, bool parse) const;
    virtual int compare(XYObject* rhs);
    virtual size_t size();
    virtual void pushBackInto(List& list);
    virtual XYObject* at(size_t n);
    virtual void set_at(size_t n, XYObject* v);
    virtual XYObject* head();
    virtual XYSequence* tail();
    virtual XYSequence* join(XYSequence* rhs);
};

// A list of objects. Can include other nested
// lists. All items in the list are derived from
// XYObject. 
class XYList : public XYSequence
{
  public:
    List mList;

  public:
    XYList();
    template <class InputIterator> XYList(InputIterator first, InputIterator last);
    
    virtual void markChildren();
    virtual void print(std::ostringstream& stream, CircularSet& seen, bool parse) const;
    virtual size_t size();
    virtual void pushBackInto(List& list);
    virtual XYObject* at(size_t n);
    virtual void set_at(size_t n, XYObject* v);
    virtual XYObject* head();
    virtual XYSequence* tail();
    virtual XYSequence* join(XYSequence* rhs);
};

// A slice is a virtual subsequence of an existing list.
class XYSlice : public XYSequence
{
  public:
    // The original sequence we are a slice of
    XYSequence* mOriginal;

    // The start of the slice. 
    int mBegin;

    // The end of the slice.
    int mEnd;

  public:
    XYSlice(XYSequence* original, 
            int start, 
            int end);

    virtual void markChildren();
    virtual void print(std::ostringstream& stream, CircularSet& seen, bool parse) const;
    virtual size_t size();
    virtual void pushBackInto(List& list);
    virtual XYObject* at(size_t n);
    virtual void set_at(size_t n, XYObject* v);
    virtual XYObject* head();
    virtual XYSequence* tail();
    virtual XYSequence* join(XYSequence* rhs);
};

// A join is a virtual sequence composed of two other
// sequences. It's primary use is to allow lazy
// appending of two sequences.
class XYJoin : public XYSequence
{
  public:
    // The original sequences we join.
    typedef std::deque<XYSequence*> Vector;
    typedef Vector::iterator iterator;
    typedef Vector::const_iterator const_iterator;

    Vector mSequences;

  public:
    XYJoin() { }
    XYJoin(XYSequence* first, XYSequence* second); 

    virtual void markChildren();
    virtual void print(std::ostringstream& stream, CircularSet& seen, bool parse) const;
    virtual size_t size();
    virtual void pushBackInto(List& list);
    virtual XYObject* at(size_t n);
    virtual void set_at(size_t n, XYObject* v);
    virtual XYObject* head();
    virtual XYSequence* tail();
    virtual XYSequence* join(XYSequence* rhs);
};

// A primitive is the implementation of a core function.
// Primitives execute immediately when taken off the queue
// and do not need to have their value looked up.
class XYPrimitive : public XYObject
{
  public:
    std::string mName;
    void (*mFunc)(XY*);

  public:
    XYPrimitive(std::string name, void (*func)(XY*));
    virtual void print(std::ostringstream& stream, CircularSet& seen, bool parse) const;
    virtual void eval1(XY* xy);
    virtual int compare(XYObject* rhs);
};

// Base class to to provide limits to the executing
// XY program. Limit examples might be a requirement to run
// within a certain number of ticks, time period or
// memory size.
class XYLimit : public GCObject {
 public:
  // Called when execution starts so the limit checker can
  // initialize things like initial start time or tick count.
  virtual void start(XY* xy) = 0;
  
  // Check if the limit has been reached. Return's true if
  // so.
  virtual bool check(XY* xy) = 0;
};

// Limit a call of eval to run within a
// certain number of milliseconds.
class XYTimeLimit : public XYLimit {
 public:
  unsigned int mMilliseconds;
  unsigned int mStart;

 public:
  XYTimeLimit(unsigned int milliseconds);
  virtual void start(XY* xy);
  virtual bool check(XY* xy);
};

// An object that gets thrown when an error occurs
class XYError {
 public:
  // The error codes
  enum code {
    STACK_UNDERFLOW,
    SYMBOL_NOT_FOUND,
    TYPE,
    WAITING_FOR_ASYNC_EVENT,
    LIMIT_REACHED,
    RANGE,
    INVALID_SLOT_TYPE,
    SLOT_NOT_FOUND
  };

  // The interpreter state at the time of the error
  XY* mXY;

  // What error occurred
  code mCode;

  // Location if available
  int mLine;
  char const* mFile;
  
 public:
  XYError(XY* xy, code c);
  XYError(XY* xy, code c, char const* file, int line);

  // A textual representation of the error
  std::string message();
};

// The environment maps names to objects
typedef std::map<std::string, XYObject*> XYEnv;
typedef std::vector<XYObject*> XYStack;
typedef std::deque<XYObject*> XYQueue;
typedef std::vector<XYLimit*> XYLimits;
typedef std::vector<XY*> XYWaitingList;

// The state of the runtime interpreter.
// Holds the environment, stack and queue
// and provides methods to step through or run
// the interpreter.
class XY : public GCObject {
  public:
    // io service for handling asynchronous events
    // The lifetime of the service is controlled by
    // the owner of the XY object and should generally
    // be around for the entire lifetime of the running program.
    boost::asio::io_service& mService;

    // Input stream, allow asyncronous reading of data from stdin.
    boost::asio::posix::stream_descriptor mInputStream;

    // Output stream, allow asyncronous writing of data to stdout.
    boost::asio::posix::stream_descriptor mOutputStream;

    // Input buffer for stdio
    boost::asio::streambuf mInputBuffer;

    // Set to interpreters that are waiting for
    // this interpreter to complete (via thread join).
    XYWaitingList mWaiting;

    // Environment holding mappings of names
    // to objects.
    XYEnv mEnv;

    // Mapping of primitives to their primitive
    // object. These are symbols that are executed
    // implicitly and don't need their value looked up
    // by the user.
    XYEnv mP;

    // The Stack
    XYStack mX;

    // The Queue
    XYQueue mY;

    // The limits that restrict the operation of this
    // interpreter.
    XYLimits mLimits;

    // The current executing frame object
    XYObject* mFrame;

    // True if we are a 'repl' based interpreter
    bool mRepl;

  public:
    // Constructor installs any primitives into the
    // environment.
    XY(boost::asio::io_service& service);
    virtual ~XY() { }

    virtual void markChildren();

    // Handler for asynchronous i/o events
    void stdioHandler(boost::system::error_code const& err);

    // Handler for asynchronous evaluation events
    void evalHandler();

    // A primitive can yield a timeslice by calling this to
    // post back to the eval handler.
    void yield();

    // Check limits. Throw the limit object if it has
    // been exceeded.
    void checkLimits();

    // Print a representation of the state of the
    // interpter.
    void print();

    // Remove one item from the queue and evaluate it.
    virtual void eval1();

    // Evaluate all items in the queue.
    virtual void eval();

    // Perform a recursive match of pattern values to items
    // in the given stack.
    template <class OutputIterator>
    void match(OutputIterator out, 
               XYObject* object,
               XYObject* pattern,
               XYSequence* sequence,
               int i);

    // Given a pattern list of symbols (which can contain
    // nested lists of symbols), store in the environment
    // a mapping of symbol name to value from the stack.
    // This operation destructures within lists on the stack.
    template <class OutputIterator>
    void getPatternValues(XYObject* symbols, OutputIterator out);

    // Given a mapping of names to values in 'env', replaces symbols in 'object'
    // that have the name with the given value. Store the newly created list
    // in 'out'.
    template <class OutputIterator>
    void replacePattern(XYEnv const& env, XYObject* object, OutputIterator out);
};

// Return regex for tokenizing
boost::xpressive::sregex re_integer();
boost::xpressive::sregex re_float();
boost::xpressive::sregex re_number();
boost::xpressive::sregex re_special();
boost::xpressive::sregex re_non_special();
boost::xpressive::sregex re_symbol();
boost::xpressive::sregex re_stringchar();
boost::xpressive::sregex re_string();
boost::xpressive::sregex re_comment();

// Given an input string, unescape any special characters
std::string unescape(std::string s);
std::string escape(std::string s);

// Returns true if the string is a shuffle pattern
bool is_shuffle_pattern(std::string s);

// Given a string, store a sequence of XY tokens using the 'out' iterator
// to put them in a container.
template <class InputIterator, class OutputIterator>
void tokenize(InputIterator first, InputIterator last, OutputIterator out)
{
  using namespace std;
  using namespace boost::xpressive;

  // inline string regular expression. Without this I get a 'pure virtual function'
  // called inside the boost regular expression code when compiled with -O2 and -O3.
  sregex xy = re_comment() | (as_xpr('\"') >> *(re_stringchar()) >> '\"') | re_float() | re_special() | re_symbol() | re_integer();
  sregex_token_iterator begin(first, last, xy), end;
  copy(begin, end, out);
}

// Parse a sequence of tokens storing the result using the
// given output iterator.
template <class InputIterator, class OutputIterator>
InputIterator parse(InputIterator first, InputIterator last, OutputIterator out) {
  using namespace std;
  using namespace boost::xpressive;

  while (first != last) {
    string token = *first++;
    smatch what;
    if (regex_match(token, what, re_comment())) {
      // Ignore comments
    }
    else if (regex_match(token, what, re_string())) {
      *out++ = new XYString(unescape(token.substr(1, token.size()-2)));
    }
    else if(regex_match(token, re_float()))
      *out++ = new XYFloat(token);
    else if(regex_match(token, re_integer())) {
      *out++ = new XYInteger(token);
    }
    else if(token == "[") {
      XYList* list = new XYList();
      first = parse(first, last, back_inserter(list->mList));
      *out++ = list;
    }
    else if( token == "]") {
      return first;
    }
    else if(is_shuffle_pattern(token)) {
      *out++ = new XYShuffle(token);
    }
    else {
      *out++ = new XYSymbol(token);
    }
  }

  return first;
}

// Parse a string into XY objects, storing the result in the
// container pointer to by the output iterator.
template <class OutputIterator>
void parse(std::string s, OutputIterator out) {
  using namespace std;
  using namespace boost;

  vector<string> tokens;
  tokenize(s.begin(), s.end(), back_inserter(tokens));
  parse(tokens.begin(), tokens.end(), out);
}

// Assert a condition is true and throw an XYError if it is not
#define xy_assert(condition, code) \
  xy_assert_impl((condition), (code), xy, __FILE__, __LINE__)

inline void xy_assert_impl(bool condition, 
			   XYError::code c, 
			   XY* xy, 
			   char const* file, 
			   int line) {
  if (!condition)
    throw XYError(xy, c, file, line);
}

#endif // cf_h
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
