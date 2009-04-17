// Copyright (C) 2009 Chris Double. All Rights Reserved.
// See the license at the end of this file
#include <cassert>
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
#include <boost/test/minimal.hpp>
#include "cf.h"

using namespace std;
using namespace boost;
using namespace boost::lambda;

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

    parse("2 add5.", back_inserter(xy->mY));
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
    parse("[ [a a a +] ] double set 2 double.( 0", back_inserter(xy->mY));

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
