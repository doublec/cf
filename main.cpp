// Copyright (C) 2009 Chris Double. All Rights Reserved.
// See the license at the end of this file
#include <iostream>
#include <sstream>
#include <fstream>
#include <iterator>
#include <algorithm>
#include <functional>
#include <boost/lambda/lambda.hpp>
#include <boost/lambda/bind.hpp>
#include "cf.h"
#include "socket.h"

using namespace std;
using namespace boost;
using namespace boost::lambda;

// A literate file. See literate.lcf for details.
void eval_literate_file(shared_ptr<XY> xy, char* filename) {
  cout << "Loading " << filename << endl;
  ifstream file(filename);
  ostringstream out;
  while (file.good()) {
    string line;
    getline(file, line);

    // Evaluate the Bird style of code
    if (line.size() > 0 && line[0] == '>') {
      out << line.substr(1) << endl;
    }

    if (line == "\\begin{code}") {
      // Evaluate the LaTeX style of code
      while (file.good()) {
	getline(file, line);
	if (line == "\\end{code}")
	  break;

	out << line << endl;
      }
    }
  }
  file.close();

  parse(out.str(), back_inserter(xy->mY));

  xy->eval();
}

void eval_non_literate_file(shared_ptr<XY> xy, char* filename) {
  cout << "Loading " << filename << endl;
  ifstream file(filename);
  ostringstream out;
  while (file.good()) {
    string line;
    getline(file, line);
    out << line << endl;
  }
  file.close();

  parse(out.str(), back_inserter(xy->mY));

  xy->eval();
}

void eval_file(shared_ptr<XY> xy, char* filename) {
  char* ext = strrchr(filename, '.');
  if (ext && strcmp(ext, ".lcf") == 0)
    eval_literate_file(xy, filename);
  else
    eval_non_literate_file(xy, filename);
}

template <class InputIterator>
void eval_files(shared_ptr<XY> xy, InputIterator first, InputIterator last) {
  for_each(first, last, bind(eval_file, xy, _1));
}

int main(int argc, char* argv[]) {
  shared_ptr<XY> xy(new BoostXY());
  install_socket_primitives(xy);

  if (argc > 1) {
    // Load all files given on the command line in order
    try {
      eval_files(xy, argv + 1, argv + argc);
    }
    catch(XYError& error) {
      cout << error.message() << endl;
      xy.reset(new XY());
    }
  }

  // Limit test. If any line input by the user takes
  // longer than this time period to run then a
  // limit exception is thrown.
  //xy->mLimits.push_back(msp(new XYTimeLimit(10000)));

  while (cin.good()) {
    try {
      string input;
      xy->print();
      cout << "ok ";
      getline(cin, input);
      if (cin.good()) {
	parse(input, back_inserter(xy->mY));
	xy->eval();
      }
    }
    catch(XYError& error) {
      cout << error.message() << endl;
    }
  }

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
