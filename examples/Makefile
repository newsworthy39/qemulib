LDFLAGS = -L../build -lqemu -ljson -lnl-route-3 -lnl-3 -lyaml-cpp -pthread
INC=-I../include/ -Iinclude/ -I/usr/include/libnl3 -I/usr/local/include -I../libraries/json11
CXX=g++ -std=c++2a $(INC)
PACKAGES=qemu-powerdown qemu-interfaces qemu qemu-metadataservice
BUILDDIR=build
FLAGS=-o3
CC=g++

.PHONY: clean all

all: checkdirs xd $(PACKAGES)

checkdirs: $(BUILDDIR)

xd: 
	/usr/bin/xxd -i resources/template.desktop > include/xxd.hpp

$(BUILDDIR):
	@mkdir -p $@

qemu-powerdown: main-powerdown.o 
	$(CXX) $(FLAGS) -o $(BUILDDIR)/$@ $^ $(LDFLAGS)

qemu-interfaces: main-interfaces.o 
	$(CXX) $(FLAGS) -o $(BUILDDIR)/$@ $^ $(LDFLAGS)

qemu-metadataservice: main-metadataservice.o 
	$(CXX) $(FLAGS) -o $(BUILDDIR)/$@ $^ $(LDFLAGS)

qemu: main.o
	$(CXX) $(FLAGS) -o $(BUILDDIR)/$@ $^ $(LDFLAGS)

clean:
	rm -rf *.o $(BUILDDIR) src/*.o 

install: uninstall clean all 
	cp $(BUILDDIR)/* /home/gandalf/bin
	
uninstall:
	rm -f /home/gandalf/bin/qemu-*
