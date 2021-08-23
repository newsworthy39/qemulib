src = $(wildcard *.cpp)
obj = $(src:.cpp=.o)
LDFLAGS = 
INC=-Iinclude/
CXX=g++ -std=c++2a ${INC}

all: qemu-headless qemu-gtk

qemu-headless: main-headless.o qemu.o 
	$(CXX) -o $@ $^ $(LDFLAGS)

qemu-gtk: main-gtk.o qemu.o 
	$(CXX) -o $@ $^ $(LDFLAGS)

.PHONY: clean
clean:
	rm -f $(obj) qemu-main qemu-headless