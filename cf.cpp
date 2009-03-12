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

class XY {
  private:
    map<string, XYObject*> mEnv;
    vector<XYObject*> mX;
    deque<XYObject*> mY;

  public:
    void pushX(XYObject* o);
    void pushFrontY(XYObject* o);
    void pushBackY(XYObject* o);
    void print();
    XY* eval1();
};

void XY::pushX(XYObject* o) {
  mX.push_back(o);
}

void XY::pushFrontY(XYObject* o) {
  mY.push_front(o);
}

void XY::pushBackY(XYObject* o) {
  mY.push_back(o);
}

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
 
XYNumber::XYNumber(int v) : mValue(v) { }
string XYNumber::toString() {
  ostringstream s;
  s << mValue;
  return s.str();
}

void XYNumber::eval1(XY* xy) {
  xy->pushX(this);
}


void testXY() {
  XY* xy = new XY();
  xy->pushX(new XYNumber(42));
  xy->pushFrontY(new XYNumber(84));
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

