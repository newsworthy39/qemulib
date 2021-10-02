src = $(wildcard *.cpp)
obj = $(src:.cpp=.o)
LDFLAGS = -lnl-route-3 -lnl-3 
HOSTLDFLAGS = ${LDFLAGS} -lhiredis -levent -pthread
INC=-Iinclude/ -Ilibraries/json11 -I/usr/include/libnl3
CXX=g++ -std=c++2a ${INC}
PACKAGES=qemu-kvm qemu-hostd qemu-launch qemu-stop qemu-reboot qemu-reservations
BUILDDIR=build
FLAGS=-o3

.PHONY: clean

all: ${PACKAGES}

qemu-reservations: main-reservations.o src/qemu-manage.o src/qemu-hypervisor.o libraries/json11/json11.o src/qemu-link.o
	$(CXX) $(FLAGS) -o ${BUILDDIR}/$@ $^ $(HOSTLDFLAGS)

qemu-stop: main-stop.o src/qemu-manage.o src/qemu-hypervisor.o libraries/json11/json11.o src/qemu-link.o
	$(CXX) $(FLAGS) -o ${BUILDDIR}/$@ $^ $(HOSTLDFLAGS)

qemu-reboot: main-reboot.o src/qemu-manage.o src/qemu-hypervisor.o libraries/json11/json11.o src/qemu-link.o
	$(CXX) $(FLAGS) -o ${BUILDDIR}/$@ $^ $(HOSTLDFLAGS)

qemu-launch: main-launch.o src/qemu-manage.o src/qemu-hypervisor.o libraries/json11/json11.o src/qemu-link.o
	$(CXX) $(FLAGS) -o ${BUILDDIR}/$@ $^ $(HOSTLDFLAGS)

qemu-hostd: main-hostd.o src/qemu-manage.o src/qemu-hypervisor.o libraries/json11/json11.o src/qemu-link.o
	$(CXX) $(FLAGS) -o ${BUILDDIR}/$@ $^ $(HOSTLDFLAGS)

qemu-kvm: main-kvm.o src/qemu-manage.o src/qemu-hypervisor.o libraries/json11/json11.o src/qemu-link.o
	$(CXX) $(FLAGS) -o ${BUILDDIR}/$@ $^ $(LDFLAGS)

qemu-image: main-image.o src/qemu-manage.o src/qemu-manage.o src/qemu-hypervisor.o libraries/json11/json11.o src/qemu-link.o
	$(CXX) $(FLAGS) -o ${BUILDDIR}/$@ $^ $(LDFLAGS)	

src/qemu-link.cpp:
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
	rm -f /home/gandalf/bin/${PACKAGES}
