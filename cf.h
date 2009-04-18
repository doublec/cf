// Copyright (C) 2009 Chris Double. All Rights Reserved.
// See the license at the end of this file
#if !defined(cf_h)
#define cf_h

#include <string>
#include <map>
#include <vector>
#include <deque>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/xpressive/xpressive.hpp>
#include <gmpxx.h>

// Helper function for creating a shared pointer of a type.
// Replace: shared_ptr<T> x = new shared_ptr<T>(new T());
// With:    shared_ptr<T> x = msp(new T());   
template <class T> boost::shared_ptr<T> msp(T* o)
{
  return boost::shared_ptr<T>(o);
}

// XY is the object that contains the state of the running
// system. For example, the stack (X), the queue (Y) and
// the environment.
class XY;

// Base class for all objects in the XY system. Anything
// stored on the stack, in the queue, in the the environment
// must be derived from this.
//
// Memory allocation and freeing is handled via the shared_ptr
// class. This means all allocation via 'new' must be assigned
// to a shared_ptr.
class XYObject : public boost::enable_shared_from_this<XYObject>
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
    virtual std::string toString(bool parse) const = 0;

    // Return a negative number if this object is less than
    // the rhs object. Return 0 if they are equal. Returns
    // a positive number if it is greater.
    virtual int compare(boost::shared_ptr<XYObject> rhs) const = 0;
};

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
    virtual boost::shared_ptr<XYInteger> as_integer() = 0;
    virtual boost::shared_ptr<XYFloat> as_float() = 0;

    // Math Operators
    virtual boost::shared_ptr<XYNumber> add(boost::shared_ptr<XYNumber> rhs) = 0;
    virtual boost::shared_ptr<XYNumber> subtract(boost::shared_ptr<XYNumber> rhs) = 0;
    virtual boost::shared_ptr<XYNumber> multiply(boost::shared_ptr<XYNumber> rhs) = 0;
    virtual boost::shared_ptr<XYNumber> divide(boost::shared_ptr<XYNumber> rhs) = 0;
    virtual boost::shared_ptr<XYNumber> power(boost::shared_ptr<XYNumber> rhs) = 0;
    virtual boost::shared_ptr<XYNumber> floor() = 0;
};

// Floating point numbers
class XYFloat : public XYNumber
{
  public:
    mpf_class mValue;

  public:
    XYFloat(long v = 0);
    XYFloat(std::string v);
    XYFloat(mpf_class const& v);
    virtual std::string toString(bool parse) const;
    virtual void eval1(XY* xy);
    virtual int compare(boost::shared_ptr<XYObject> rhs) const;
    virtual bool is_zero() const;
    virtual unsigned int as_uint() const;
    virtual boost::shared_ptr<XYInteger> as_integer();
    virtual boost::shared_ptr<XYFloat> as_float();
    virtual boost::shared_ptr<XYNumber> add(boost::shared_ptr<XYNumber> rhs);
    virtual boost::shared_ptr<XYNumber> subtract(boost::shared_ptr<XYNumber> rhs);
    virtual boost::shared_ptr<XYNumber> multiply(boost::shared_ptr<XYNumber> rhs);
    virtual boost::shared_ptr<XYNumber> divide(boost::shared_ptr<XYNumber> rhs);
    virtual boost::shared_ptr<XYNumber> power(boost::shared_ptr<XYNumber> rhs);
    virtual boost::shared_ptr<XYNumber> floor();
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
    virtual std::string toString(bool parse) const;
    virtual void eval1(XY* xy);
    virtual int compare(boost::shared_ptr<XYObject> rhs) const;
    virtual bool is_zero() const;
    virtual unsigned int as_uint() const;
    virtual boost::shared_ptr<XYInteger> as_integer();
    virtual boost::shared_ptr<XYFloat> as_float();
    virtual boost::shared_ptr<XYNumber> add(boost::shared_ptr<XYNumber> rhs);
    virtual boost::shared_ptr<XYNumber> subtract(boost::shared_ptr<XYNumber> rhs);
    virtual boost::shared_ptr<XYNumber> multiply(boost::shared_ptr<XYNumber> rhs);
    virtual boost::shared_ptr<XYNumber> divide(boost::shared_ptr<XYNumber> rhs);
    virtual boost::shared_ptr<XYNumber> power(boost::shared_ptr<XYNumber> rhs);
    virtual boost::shared_ptr<XYNumber> floor();
};

// A symbol is an unquoted string.
class XYSymbol : public XYObject
{
  public:
    std::string mValue;

  public:
    XYSymbol(std::string v);
    virtual std::string toString(bool parse) const;
    virtual void eval1(XY* xy);
    virtual int compare(boost::shared_ptr<XYObject> rhs) const;
};

// A string
class XYString : public XYObject
{
  public:
    std::string mValue;

  public:
    XYString(std::string v);
    virtual std::string toString(bool parse) const;
    virtual void eval1(XY* xy);
    virtual int compare(boost::shared_ptr<XYObject> rhs) const;
};

// A shuffle symbol describes pattern to rearrange the stack.
class XYShuffle : public XYObject
{
  public:
    std::string mBefore;
    std::string mAfter;

  public:
    XYShuffle(std::string v);
    virtual std::string toString(bool parse) const;
    virtual void eval1(XY* xy);
    virtual int compare(boost::shared_ptr<XYObject> rhs) const;
};

// A base class for a sequence of XYObject's
class XYSequence : public XYObject
{
  public:
    typedef std::vector< boost::shared_ptr<XYObject> > List;
    typedef List::iterator iterator;
    typedef List::const_iterator const_iterator;

  public:
    // Returns iterators to access the sequence
    virtual iterator begin() = 0;
    virtual iterator end() = 0;

    // Returns the size of the sequence
    virtual size_t size() = 0;

    // Returns the element at index 'n'
    virtual boost::shared_ptr<XYObject> at(size_t n) = 0;

    // Returns the head of a sequence (the first item).
    virtual boost::shared_ptr<XYObject> head() = 0;

    // Returns the tail of a sequence (all but the first item)
    virtual boost::shared_ptr<XYSequence> tail() = 0;

    // Concatenate two sequences
    virtual boost::shared_ptr<XYSequence> join(boost::shared_ptr<XYSequence> const& rhs) = 0;
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
    virtual std::string toString(bool parse) const;
    virtual void eval1(XY* xy);
    virtual int compare(boost::shared_ptr<XYObject> rhs) const;
    virtual iterator begin();
    virtual iterator end();
    virtual size_t size();
    virtual boost::shared_ptr<XYObject> at(size_t n);
    virtual boost::shared_ptr<XYObject> head();
    virtual boost::shared_ptr<XYSequence> tail();
    virtual boost::shared_ptr<XYSequence> join(boost::shared_ptr<XYSequence> const& rhs);
};

// A slice is a virtual subsequence of an existing list.
class XYSlice : public XYSequence
{
  public:
    // The original sequence we are a slice of
    // Need to keep a reference to the original to ensure 
    // it doesn't get destroyed while the slice is active.
    boost::shared_ptr<XYObject> mOriginal;

    // The start of the slice. 
    XYSequence::iterator mBegin;

    // The end of the slice.
    XYSequence::iterator mEnd;

  public:
    XYSlice(boost::shared_ptr<XYObject> original, 
            XYSequence::iterator start, 
            XYSequence::iterator end);
    virtual std::string toString(bool parse) const;
    virtual void eval1(XY* xy);
    virtual int compare(boost::shared_ptr<XYObject> rhs) const;
    virtual iterator begin();
    virtual iterator end();
    virtual size_t size();
    virtual boost::shared_ptr<XYObject> at(size_t n);
    virtual boost::shared_ptr<XYObject> head();
    virtual boost::shared_ptr<XYSequence> tail();
    virtual boost::shared_ptr<XYSequence> join(boost::shared_ptr<XYSequence> const& rhs);
};

// A join is a virtual sequence composed of two other
// sequences. It's primary use is to allow lazy
// appending of two sequences.
class XYJoin : public XYSequence
{
  public:
    // The original sequences we join.
    typedef std::deque<boost::shared_ptr<XYSequence> > Vector;
    typedef Vector::iterator iterator;
    typedef Vector::const_iterator const_iterator;

    Vector mSequences;

  public:
    XYJoin() { }
    XYJoin(boost::shared_ptr<XYSequence> first,
           boost::shared_ptr<XYSequence> second); 
    virtual std::string toString(bool parse) const;
    virtual void eval1(XY* xy);
    virtual int compare(boost::shared_ptr<XYObject> rhs) const;
    virtual XYSequence::iterator begin();
    virtual XYSequence::iterator end();
    virtual size_t size();
    virtual boost::shared_ptr<XYObject> at(size_t n);
    virtual boost::shared_ptr<XYObject> head();
    virtual boost::shared_ptr<XYSequence> tail();
    virtual boost::shared_ptr<XYSequence> join(boost::shared_ptr<XYSequence> const& rhs);
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
    virtual std::string toString(bool parse) const;
    virtual void eval1(XY* xy);
    virtual int compare(boost::shared_ptr<XYObject> rhs) const;
};

// The environment maps names to objects
typedef std::map<std::string, boost::shared_ptr<XYObject> > XYEnv;
typedef std::vector<boost::shared_ptr<XYObject> > XYStack;
typedef std::deque<boost::shared_ptr<XYObject> > XYQueue;

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
               boost::shared_ptr<XYObject> object,
               boost::shared_ptr<XYObject> pattern,
               boost::shared_ptr<XYSequence> sequence,
               XYSequence::iterator it);

    // Given a pattern list of symbols (which can contain
    // nested lists of symbols), store in the environment
    // a mapping of symbol name to value from the stack.
    // This operation destructures within lists on the stack.
    template <class OutputIterator>
    void getPatternValues(boost::shared_ptr<XYObject> symbols, OutputIterator out);

    // Given a mapping of names to values in 'env', replaces symbols in 'object'
    // that have the name with the given value. Store the newly created list
    // in 'out'.
    template <class OutputIterator>
    void replacePattern(XYEnv const& env, boost::shared_ptr<XYObject> object, OutputIterator out);
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
  using namespace boost;
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
void parse(std::string s, OutputIterator out) {
  using namespace std;
  using namespace boost;

  vector<string> tokens;
  tokenize(s.begin(), s.end(), back_inserter(tokens));
  parse(tokens.begin(), tokens.end(), out);
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
