ccc: ccc.cc
	g++ -Wall --std=c++11 -O3 $^ -lpthread -o $@
