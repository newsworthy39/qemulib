src = $(wildcard *.cpp)
obj = $(src:.cpp=.o)
LDFLAGS = -lnl-route-3 -lnl-3 
HOSTLDFLAGS = ${LDFLAGS} -lhiredis -levent -pthread
INC=-Iinclude/ -Ilibraries/json11 -I/usr/include/libnl3
CXX=g++ -std=c++2a ${INC}
PACKAGES=qemu-kvm qemu-drives qemu-image qemu-hostd
BUILDDIR=build
FLAGS=-o3

.PHONY: clean

all: ${PACKAGES}

qemu-hostd: main-hostd.o src/qemu-hypervisor.o src/qemu-images.o libraries/json11/json11.o src/qemu-link.o
	$(CXX) $(FLAGS) -o ${BUILDDIR}/$@ $^ $(HOSTLDFLAGS)

qemu-kvm: main.o src/qemu-hypervisor.o src/qemu-images.o libraries/json11/json11.o src/qemu-link.o
	$(CXX) $(FLAGS) -o ${BUILDDIR}/$@ $^ $(LDFLAGS)

qemu-drives: main-drives.o src/qemu-hypervisor.o src/qemu-images.o libraries/json11/json11.o src/qemu-link.o
	$(CXX) $(FLAGS) -o ${BUILDDIR}/$@ $^ $(LDFLAGS)	

qemu-image: main-image.o src/qemu-hypervisor.o src/qemu-images.o libraries/json11/json11.o src/qemu-link.o
	$(CXX) $(FLAGS) -o ${BUILDDIR}/$@ $^ $(LDFLAGS)	

src/qemu-link.cpp:
	$(CXX) $(FLAGS) -o ${BUILDDIR}/$@ $^ $(LDFLAGS)	

src/qemu-images.cpp:
	$(CXX) $(FLAGS) -o ${BUILDDIR}/$@ $^ $(LDFLAGS)	

src/qemu-hypervisor.cpp:
	$(CXX) $(FLAGS) -o ${BUILDDIR}/$@ $^ $(LDFLAGS)	

libraries/json11/json11.cpp: 
	$(CXX) $(FLAGS) -o ${BUILDDIR}/$@ $^ $(LDFLAGS)	

clean:
	rm -f *.o ${BUILDDIR}/* src/*.o

install:
	cp ${BUILDDIR}/* /home/gandalf/bin
	
uninstall:
	rm -f ${PACKAGES}
