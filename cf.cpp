#include <iostream>
#include <map>
#include <vector>
#include <deque>
#include <string>

using namespace std;

class XYObject
{
  public:
    virtual string toString() { return string(""); }
};

class XY {
  private:
    map<string, XYObject*> mEnv;
    vector<XYObject*> mX;
    deque<XYObject*> mY;

  public:
    void print() {
      for(int i=0; i < mX.size(); ++i) {
        cout << mX[i]->toString() << " ";
      }

      cout << "-> ";

      for(int i=0; i < mY.size(); ++i) {
        cout << mY[i]->toString() << " ";
      }

      cout << endl;
    }

};

void testXY() {
  XY* xy = new XY();
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

