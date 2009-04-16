all: cf testcf

cf.o: cf.cpp cf.h
	g++ -g -c -o cf.o cf.cpp

main.o: main.cpp cf.h
	g++ -g -c -o main.o main.cpp

cf: cf.o main.o
	g++ -g -o cf cf.o main.o -lgmp -lgmpxx

testmain.o: testmain.cpp cf.h
	g++ -c -o testmain.o testmain.cpp

testcf: cf.o testmain.o
	g++ -o testcf cf.o testmain.o -lgmp -lgmpxx

clean: 
	rm *.o
	rm cf
	rm testcf
