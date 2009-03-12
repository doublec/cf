all: cf

cf.o: cf.cpp
	g++ -c -o cf.o cf.cpp

cf: cf.o
	g++ -o cf cf.o


