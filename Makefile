all: ccc cccx cccy cccz

ccc: ccc.cc
	g++ -Wall --std=c++11 -O3 $^ -lpthread -o $@

cccx.o: cccx.cc 
	g++ -c -Wall --std=c++11 -O3 $^ -lpthread -o $@

cccx: cccx.o
	g++ -Wall --std=c++11 -O2 $^ -lpthread -o $@

cccy: cccy.cc
	g++ -Wall --std=c++11 -O3 $^ -lpthread -o $@

cccz: cccz.cc
	g++ -Wall --std=c++11 -O3 $^ -lpthread -o $@
