src = $(wildcard *.cpp)
obj = $(src:.cpp=.o)
LDFLAGS = -lnl-route-3 -lnl-3 -lyaml-cpp -pthread
INC=-Iinclude/ -Ilibraries/json11 -I/usr/include/libnl3 -I/usr/local/include
CXX=g++ -std=c++2a $(INC)
PACKAGES=xd qemu qemu-interfaces qemu-powerdown
BUILDDIR=build
FLAGS=-o3

.PHONY: clean

all: checkdirs $(PACKAGES)

checkdirs: $(BUILDDIR)

$(BUILDDIR):
	@mkdir -p $@

xd: 
	/usr/bin/xxd -i resources/template.desktop > include/xxd.hpp

qemu-powerdown: main-powerdown.o  src/qemu-manage.o src/qemu-hypervisor.o libraries/json11/json11.o src/qemu-link.o
	$(CXX) $(FLAGS) -o $(BUILDDIR)/$@ $^ $(LDFLAGS)

qemu-interfaces: main-interfaces.o src/qemu-manage.o src/qemu-hypervisor.o libraries/json11/json11.o src/qemu-link.o
	$(CXX) $(FLAGS) -o $(BUILDDIR)/$@ $^ $(LDFLAGS)

qemu: main.o src/qemu-manage.o src/qemu-hypervisor.o libraries/json11/json11.o src/qemu-link.o 
	$(CXX) $(FLAGS) -o $(BUILDDIR)/$@ $^ $(LDFLAGS)

src/qemu-link.cpp:
	$(CXX) $(FLAGS) -o $(BUILDDIR)/$@ $^ $(LDFLAGS)	

src/qemu-hypervisor.cpp:
	$(CXX) $(FLAGS) -o $(BUILDDIR)/$@ $^ $(LDFLAGS)	

src/qemu-manage.cpp:
	$(CXX) $(FLAGS) -o $(BUILDDIR)/$@ $^ $(LDFLAGS)	

libraries/json11/json11.cpp: 
	$(CXX) $(FLAGS) -o $(BUILDDIR)/$@ $^ $(LDFLAGS)	

clean:
	rm -rf *.o $(BUILDDIR) src/*.o 

install: uninstall clean all 
	cp $(BUILDDIR)/* /home/gandalf/bin
	
uninstall:
	rm -f /home/gandalf/bin/qemu-*
