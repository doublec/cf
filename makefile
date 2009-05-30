UNAME=$(shell uname -s)

ifeq "$(UNAME)" "Darwin"
INCLUDE=-I/opt/local/include
LIB=-L/opt/local/lib
endif

INCLUDE=
LIB=
CFLAGS=-g
GCDIR=./gc
GCLIB=$(GCDIR)/libgc.a

all: cf testcf leakcf

$(GCLIB):
	make -C $(GCDIR)

cf.o: cf.cpp cf.h gc/gc.h
	g++ $(INCLUDE) $(CFLAGS) -c -o cf.o cf.cpp

socket.o: socket.cpp socket.h cf.h gc/gc.h
	g++ $(INCLUDE) $(CFLAGS) -c -o socket.o socket.cpp

threads.o: threads.cpp threads.h cf.h gc/gc.h
	g++ $(INCLUDE) $(CFLAGS) -c -o threads.o threads.cpp

main.o: main.cpp cf.h gc/gc.h
	g++ $(INCLUDE) $(CFLAGS) -c -o main.o main.cpp

cf: cf.o socket.o threads.o main.o $(GCLIB)
	g++ $(INCLUDE) $(CFLAGS) -o cf cf.o socket.o threads.o main.o $(LIB) -lgmp -lgmpxx -lboost_system -lpthread $(GCLIB)

testmain.o: testmain.cpp cf.h gc/gc.h
	g++ $(INCLUDE) -c -o testmain.o testmain.cpp

testcf: cf.o testmain.o $(GCLIB)
	g++ $(INCLUDE) -o testcf cf.o testmain.o $(LIB) -lgmp -lgmpxx  -lboost_system -lpthread $(GCLIB)

leakmain.o: leakmain.cpp cf.h gc/gc.h
	g++ $(INCLUDE) $(CFLAGS) -c -o leakmain.o leakmain.cpp

leakcf: cf.o leakmain.o $(GCLIB)
	g++ $(INCLUDE) $(CFLAGS) -o leakcf cf.o leakmain.o $(LIB) -lgmp -lgmpxx  -lboost_system -lpthread $(GCLIB)

clean: 
	rm *.o
	rm cf
	rm testcf
	make -C $(GCDIR) clean
