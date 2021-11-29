CFLAGS = -mcmodel=large -fno-builtin -m64 -c -fno-stack-protector -g -O0 -Wconversion -Wunused -D__WORDSIZE=64 -DDEBUG=1 -Iinclude
CSRCS=$(wildcard src/*.c)
ASSRCS=$(wildcard src/*.sx)
OBJS=$(CSRCS:.c=.o) $(ASSRCS:.sx=.o)
DEPS=$(OBJS:.o=.d)
AS=/usr/bin/env as

-include $(DEPS)

all: system
	objcopy -I elf64-x86-64 -S -R ".eh_frame" -R ".comment" -O binary system kernel.bin

system:	$(OBJS)
	ld -b elf64-x86-64 -z muldefs -o $@ src/head.o $^ -T kernel.lds

%.o: %.c
	$(CC) $(CFLAGS) $< -o $@ -MMD

%.o: %.sx
	$(CC) -E -MMD -Iinclude $< > $*.i
	$(AS) --64 -o $*.o $*.i

install:
	install kernel.bin $(BINARIES)/

clean:
	rm -rf src/*.o src/*.i src/*.d src/*~ 
	rm -rf *.o *.i *.d *~
	rm -rf system kernel.bin 
