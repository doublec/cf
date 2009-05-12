UNAME=$(shell uname -s)

ifeq "$(UNAME)" "Darwin"
INCLUDE=-I/opt/local/include
LIB=-L/opt/local/lib
endif

INCLUDE=
LIB=-lgc -lgccpp

all: cf testcf leakcf

cf.o: cf.cpp cf.h
	g++ $(INCLUDE) -g -c -o cf.o cf.cpp

socket.o: socket.cpp socket.h cf.h
	g++ $(INCLUDE) -g -c -o socket.o socket.cpp

threads.o: threads.cpp threads.h cf.h
	g++ $(INCLUDE) -g -c -o threads.o threads.cpp

main.o: main.cpp cf.h
	g++ $(INCLUDE) -g -c -o main.o main.cpp

cf: cf.o socket.o threads.o main.o
	g++ $(INCLUDE) -g -o cf cf.o socket.o threads.o main.o $(LIB) -lgmp -lgmpxx -lboost_system -lpthread 

testmain.o: testmain.cpp cf.h
	g++ $(INCLUDE) -c -o testmain.o testmain.cpp

testcf: cf.o testmain.o
	g++ $(INCLUDE) -o testcf cf.o testmain.o $(LIB) -lgmp -lgmpxx  -lboost_system -lpthread

leakmain.o: leakmain.cpp cf.h
	g++ $(INCLUDE) -g -c -o leakmain.o leakmain.cpp

leakcf: cf.o leakmain.o
	g++ $(INCLUDE) -g -o leakcf cf.o leakmain.o $(LIB) -lgmp -lgmpxx  -lboost_system -lpthread  -lgc -lgccpp

clean: 
	rm *.o
	rm cf
	rm testcf
