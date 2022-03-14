PACKAGES=metadataservice
BUILDDIR=build
FLAGS=-o3
CC=g++

.PHONY: clean all

all: checkdirs $(PACKAGES)

checkdirs: $(BUILDDIR)

$(BUILDDIR):
	@mkdir -p $@

metadataservice: metadataservice.o
	$(CC) -o $(BUILDDIR)/$@ $^

clean:
	rm -rf *.o $(BUILDDIR) src/*.o 
