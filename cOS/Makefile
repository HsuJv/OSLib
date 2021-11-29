HOME_DIR = $(shell pwd)

SUBDIRS = $(HOME_DIR)/kernel
BOOT = $(HOME_DIR)/boot
BINARIES = $(HOME_DIR)/bin
MOUNTDIR = ./mnt
FLOPPYFILE = boot.img
CC = /usr/bin/env gcc

export HOME_DIR BINARIES CC

all : clean umount
	@echo -e "\033[31mDone\033[0m"	

# entering hints
# do anytime
echo :
	@echo -e "\033[31mbegin compile $(SUBDIRS) \033[0m"

# sub-directories
# do after the boot done
$(SUBDIRS) : echo
	+$(MAKE) -j12 -C $@

# create the floppy disk
# do anytime
boot.img :
	bximage -mode=create -fd=1.44M $(FLOPPYFILE) -q

# copy the floppy
# do after the boot.img created and the boot done
dd : boot.img
	install -d $(BINARIES)
	+$(MAKE) -C $(BOOT)
	$(foreach N, $(BOOT), make -C $(N) install;)
	dd if=$(BINARIES)/boot.bin of=$(FLOPPYFILE) bs=512 count=1 conv=notrunc


# mount the floppy
# do after dd called
mount : dd
	- sudo mkdir -p $(MOUNTDIR)
	sudo mount $(FLOPPYFILE) $(MOUNTDIR)/ -t vfat -o loop
	
# install the files
# do after the floppy is mount 
install : mount $(SUBDIRS)
	$(foreach N, $(SUBDIRS), make -C $(N) install;)
	sudo cp $(BINARIES)/*.bin $(MOUNTDIR)/

# umount the floppy
umount : install
	sudo sync
	sudo umount $(MOUNTDIR)/
	- sudo rm -rf $(MOUNTDIR)

clean:
	$(foreach N, $(BOOT), make -C $(N) clean;)
	$(foreach N, $(SUBDIRS), make -C $(N) clean;)
	rm -f $(FLOPPYFILE)
	rm -rf $(BINARIES)
