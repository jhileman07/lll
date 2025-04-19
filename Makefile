CXX = g++
CXXFLAGS = -std=c++20 -Wall -Wextra -Ofast
DEBUGFLAGS = -std=c++20 -Wall -Wextra -Ofast -g

MAKEFILE_DIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))

all: test

test: tests.cpp
	$(CXX) $(CXXFLAGS) -o tests tests.cpp engine.cpp
	./tests

debug: tests.cpp
	$(CXX) $(DEBUGFLAGS) -o tests tests.cpp engine.cpp
	gdb ./tests

submit: engine.cpp
	$(CXX) $(CXXFLAGS) -fPIC -c engine.cpp -o engine.o
	$(CXX) $(CXXFLAGS) -shared -o engine.so engine.o
	lll-bench $(MAKEFILE_DIR)engine.so -d 1

clean:
	rm -f tests engine.o engine.so
