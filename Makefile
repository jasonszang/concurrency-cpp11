CPPFLAGS =  -D_GNU_SOURCE
CXXFLAGS =  -g -O2 -Wall -fmessage-length=0 --std=c++11
LDFLAGS  =  -lpthread

HEADERS =   $(wildcard test/*.h)
HEADERS +=  $(wildcard concurrency/*.h)

OBJS =      main.o
OBJS +=     $(patsubst %.cpp, %.o, $(wildcard test/*.cpp))

LIBS =

TARGET =	main.exe

.PHONY: all
all:	$(TARGET)

$(OBJS): $(HEADERS)

$(TARGET):	$(OBJS) $(HEADERS)
	$(CXX) --std=c++11 -o $(TARGET) $(OBJS) $(LDFLAGS) $(LIBS)

.PHONY: run
run:    $(TARGET)
	./$(TARGET)

.PHONY: clean
clean:
#	rm -f $(OBJS) $(TARGET)
	find . -name "*.o" -type f -print0 -or -name "*.exe" -type f -print0 | xargs -0 rm -f

.PHONY: remake
remake: clean all
