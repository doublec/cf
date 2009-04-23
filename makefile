UNAME=$(shell uname -s)

ifeq "$(UNAME)" "Darwin"
INCLUDE=-I/opt/local/include
LIB=-L/opt/local/lib
endif

all: cf testcf

cf.o: cf.cpp cf.h
	g++ $(INCLUDE) -g -c -o cf.o cf.cpp

socket.o: socket.cpp cf.h
	g++ $(INCLUDE) -g -c -o socket.o socket.cpp

main.o: main.cpp cf.h
	g++ $(INCLUDE) -g -c -o main.o main.cpp

cf: cf.o socket.o main.o
	g++ $(INCLUDE) -g -o cf cf.o socket.o main.o $(LIB) -lgmp -lgmpxx -lboost_system -lpthread

testmain.o: testmain.cpp cf.h
	g++ $(INCLUDE) -c -o testmain.o testmain.cpp

testcf: cf.o testmain.o
	g++ $(INCLUDE) -o testcf cf.o testmain.o $(LIB) -lgmp -lgmpxx

clean: 
	rm *.o
	rm cf
	rm testcf
