CXX=g++
CXXFLAGS=-Wall -Wextra -g
APP=mytftpclient
SRC=$(wildcard *.cpp)
OBJ=$(subst .cpp,.o,$(SRC))

.PHONY: all $(APP) run clean

all: $(APP)

$(APP): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $(APP) $(OBJ) $(LIBS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $^ $(LIBS)

run: $(APP)
	./$(APP)

clean:
	rm -rf $(OBJ) $(APP)
