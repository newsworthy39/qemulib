INC=-Iinclude/ -Ilibraries/json11 -I/usr/include/libnl3 -I/usr/local/include
CXX=g++ -std=c++2a $(INC)
BUILDDIR=build
FLAGS=-o3

.PHONY: clean

all: checkdirs libqemu.a libjson.a build_examples

checkdirs: $(BUILDDIR)

$(BUILDDIR):
	@mkdir -p $@

build_examples: 
	$(MAKE) -C examples 

libqemu.a: src/qemu-link.o src/qemu-hypervisor.o src/qemu-manage.o
	ar -r -o $(BUILDDIR)/$@ $^

libjson.a: libraries/json11/json11.o
	ar -r -o $(BUILDDIR)/$@ $^ 


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
	$(MAKE) -C examples clean
