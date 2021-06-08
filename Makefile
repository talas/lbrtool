SOURCES := lbr.cpp
OBJS := $(SOURCES:.cpp=.o)

CXXFLAGS+=--std=c++17
LIBS+=-lstdc++fs

all: lbr

lbr: $(OBJS)
	$(CXX) $(CXXFLAGS) -o lbr $(OBJS) $(LFLAGS) $(LIBS)

.cpp.o:
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(INCLUDES) -c $<
