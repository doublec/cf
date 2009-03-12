#include <cassert>
#include <iostream>
#include <sstream>
#include <map>
#include <vector>
#include <deque>
#include <string>

class XY;
class XYObject;

using namespace std;

class XYObject
{
  public:
    virtual void eval1(XY* xy) = 0;
    virtual string toString() = 0;
};

class XYNumber : public XYObject
{
  private:
    int mValue;

  public:
    XYNumber(int v = 0);
    virtual string toString();
    virtual void eval1(XY* xy);
};

class XYSymbol : public XYObject
{
  private:
    string mValue;

  public:
    XYSymbol(string v);
    virtual string toString();
    virtual void eval1(XY* xy);
};

class XY {
  public:
    map<string, XYObject*> mEnv;
    vector<XYObject*> mX;
    deque<XYObject*> mY;

  public:
    void print();
    XY* eval1();
    XY* eval();
};

// XY
void XY::print() {
  for(int i=0; i < mX.size(); ++i) {
    cout << mX[i]->toString() << " ";
  }

  cout << "-> ";

  for(int i=0; i < mY.size(); ++i) {
    cout << mY[i]->toString() << " ";
  }

  cout << endl;
}

XY* XY::eval1() {
  assert(mY.size() > 0);

  XYObject* o = mY.front();
  assert(o);

  mY.pop_front();
  o->eval1(this);
  return this;
}

XY* XY::eval() {
  while (mY.size() > 0) {
    eval1();
  }
  return this;
}

// XYNumber
XYNumber::XYNumber(int v) : mValue(v) { }

string XYNumber::toString() {
  ostringstream s;
  s << mValue;
  return s.str();
}

void XYNumber::eval1(XY* xy) {
  xy->mX.push_back(this);
}

// XYSymbol
XYSymbol::XYSymbol(string v) : mValue(v) { }

string XYSymbol::toString() {
  ostringstream s;
  s << mValue;
  return s.str();
}

void XYSymbol::eval1(XY* xy) {
  xy->mX.push_back(this);
}

void testXY() {
  XY* xy = new XY();
  XYObject* program1[] = {
    new XYNumber(1),
    new XYNumber(2),
    new XYNumber(3),
    new XYSymbol("*"),
    new XYSymbol("+")
  };

  xy->mY.insert(xy->mY.begin(), program1, program1 + (sizeof(program1)/sizeof(XYObject*)));

  while(xy->mY.size() > 0) {
    xy->print();
    xy->eval1();
  }

  xy->print();
  delete xy;
}

void runTests() {
  testXY();
}

int main() {
  runTests();

  return 0;
}

