src = $(wildcard *.cpp)
obj = $(src:.cpp=.o)
LDFLAGS = 
INC=-Iinclude/ -Ijson11
CXX=g++ -std=c++2a ${INC}
PACKAGES=qemu-headless qemu-gtk qemu-drives qemu-image
BUILDDIR=build

.PHONY: clean

all: ${PACKAGES}

qemu-headless: main-headless.o src/qemu-hypervisor.o src/qemu-images.o  json11/json11.o
	$(CXX) -o ${BUILDDIR}/$@ $^ $(LDFLAGS)

qemu-gtk: main-gtk.o src/qemu-hypervisor.o src/qemu-images.o json11/json11.o
	$(CXX) -o ${BUILDDIR}/$@ $^ $(LDFLAGS)

qemu-drives: main-drives.o src/qemu-hypervisor.o src/qemu-images.o json11/json11.o
	$(CXX) -o ${BUILDDIR}/$@ $^ $(LDFLAGS)	

qemu-image: main-image.o src/qemu-hypervisor.o src/qemu-images.o json11/json11.o
	$(CXX) -o ${BUILDDIR}/$@ $^ $(LDFLAGS)	

src/qemu-images.cpp:
	$(CXX) -o ${BUILDDIR}/$@ $^ $(LDFLAGS)	

src/qemu-hypervisor.cpp:
	$(CXX) -o ${BUILDDIR}/$@ $^ $(LDFLAGS)	

json11/json11.cpp: 
	$(CXX) -o ${BUILDDIR}/$@ $^ $(LDFLAGS)	

clean:
	rm -f $(obj) ${BUILDDIR}/* src/*.o

install:
	cp ${BUILDDIR}/* /usr/local/bin/
	
uninstall:
	rm -f ${PACKAGES}
