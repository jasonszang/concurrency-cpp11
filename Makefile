CXXFLAGS =	-g -O2 -Wall -fmessage-length=0 --std=c++11

#HEADERS =   $(wildcard demo_forward_declaration/*.h)
HEADERS =   

#OBJS =		$(patsubst %.cpp, %.o, $(wildcard demo_forward_declaration/*.cpp)) 
OBJS =      main.o

LIBS =

TARGET =	jason_util.exe

$(OBJS): $(HEADERS)

$(TARGET):	$(OBJS) $(HEADERS)
	$(CXX) --std=c++11 -o $(TARGET) $(OBJS) $(LIBS)

.PHONY: all
all:	$(TARGET)

.PHONY: run
run:    $(TARGET)
	./$(TARGET)

.PHONY: clean
clean:
#	rm -f $(OBJS) $(TARGET)
	find . -name "*.o" -type f -print0 -or -name "*.exe" -type f -print0 | xargs -0 rm -f

.PHONY: remake
remake: clean all
