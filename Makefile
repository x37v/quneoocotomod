CXX = clang++

CXXFLAGS = -std=c++11 -g -Wall

CXXFLAGS += -I. -I../midicpp -Iext/yaml-cpp/include/
LDFLAGS += -lrtmidi ext/yaml-cpp/build/libyaml-cpp.a

SRC = ../midicpp/midicpp.cpp main.cpp
OBJ = ${SRC:.cpp=.o}

.cpp.o:
	${CXX} -c ${CXXFLAGS} -o $*.o $<

test: ${OBJ}
	${CXX} ${CXXFLAGS} -o test ${OBJ} ${LDFLAGS} 

clean:
	rm -rf test ${OBJ}
