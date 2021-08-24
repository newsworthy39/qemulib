src = $(wildcard *.cpp)
obj = $(src:.cpp=.o)
LDFLAGS = 
INC=-Iinclude/
CXX=g++ -std=c++2a ${INC}

.PHONY: clean

all: qemu-headless qemu-gtk qemu-instances

qemu-headless: main-headless.o qemu.o 
	$(CXX) -o $@ $^ $(LDFLAGS)

qemu-gtk: main-gtk.o qemu.o 
	$(CXX) -o $@ $^ $(LDFLAGS)

qemu-instances: main-instances.o qemu.o 
	$(CXX) -o $@ $^ $(LDFLAGS)	

clean:
	rm -f $(obj) qemu-main qemu-headless

install:
	cp qemu-gtk /usr/local/bin/
	cp qemu-headless /usr/local/bin/
	cp qemu-instances /usr/local/bin/

uninstall:
	rm -f /usr/local/bin/qemu-gtk /usr/local/bin/qemu-headless /usr/local/bin/qemu-instances