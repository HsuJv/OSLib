HOME_DIR = $(shell pwd)
BOOT = boot
SUBDIRS = $(BOOT)
ISO_ROOT = $(HOME_DIR)/isofiles
ISO_FILE = my_os.iso

V ?= 1
ifeq ($(V), 0)
	Q = @
else
	Q =
endif

export Q
export ISO_ROOT


all: kernel

multiboot:
	$(Q)make -C $(BOOT)

kernel:
	$(Q)cargo build

iso:
	grub-mkrescue -o $(ISO_FILE) $(ISO_ROOT)

clean:
	$(Q)$(foreach N, $(SUBDIRS), make -C $(N) clean;)
	$(Q)rm *.iso -f

purge: clean
	$(Q)cargo clean

test:
	$(Q)cargo test 2>&1 | tee test.log

run:
#	$(Q)qemu-system-x86_64 -cdrom $(ISO_FILE)
	$(Q)cargo run

novnc:
	$(Q)websockify -D --web=/usr/share/novnc/ 8888 localhost:5900