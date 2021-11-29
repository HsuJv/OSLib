
XX = nasm
ASM_FLAGS = -f elf64
SOURCES = $(wildcard *.asm)
OBJECTS = $(patsubst %.asm, %.bin, $(SOURCES))
TARGET = kernel.bin
LD_RULES = linker.ld

all : install

$(OBJECTS): %.bin : %.asm 
	$(Q)$(XX) $(ASM_FLAGS) $< -o $@

link: $(OBJECTS)
	$(Q)ld -n -o $(TARGET) -T $(LD_RULES) $^

install: link
	$(Q) install $(TARGET) $(ISO_ROOT)/boot/

clean:
	$(Q)rm *.bin -f
	$(Q)rm $(ISO_ROOT)/boot/$(TARGET) -f