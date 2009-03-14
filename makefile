all: cf testcf

testcf.o: cf.cpp
	g++ -c -o testcf.o -DTEST cf.cpp

testcf: testcf.o
	g++ -o testcf testcf.o -lgmp -lgmpxx

cf.o: cf.cpp
	g++ -g -c -o cf.o cf.cpp

cf: cf.o
	g++ -g -o cf cf.o -lgmp -lgmpxx
