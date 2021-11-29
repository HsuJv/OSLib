XX = /usr/bin/env nasm
SOURCES = $(wildcard *.asm)
OBJECTS = $(patsubst %.asm, %.bin, $(SOURCES))

all : $(OBJECTS)

$(OBJECTS) : %.bin : %.asm 
	$(XX) $< -o $@

install : $(OBJECTS)
	install $^ $(BINARIES)/

clean:
	rm -f *.bin
