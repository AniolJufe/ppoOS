CC = gcc
AS = as
LD = ld

CFLAGS = -Wall -Wextra -ffreestanding -fno-builtin -nostdlib -m64 -mno-red-zone \
         -fno-exceptions -fno-rtti -fno-stack-protector -g -O0 -mno-sse -mno-sse2 -fno-pie

LDFLAGS = -Tlink.ld -nostdlib -static -no-pie

PROG_NAMES = hello cat echo ls test_write test_write_normal test_fork
PROGRAMS = $(patsubst %,bin/%,$(PROG_NAMES))

.PHONY: all clean

all: directories $(PROGRAMS)

directories:
	mkdir -p bin

# Build the C library (split sources)
bin/limine_libc.o: limine_libc/stdio.c limine_libc/string.c limine_libc/syscall.c limine_libc/stdio.h limine_libc/string.h limine_libc/syscall.h limine_libc.h
	$(CC) $(CFLAGS) -Ilimine_libc -c limine_libc/stdio.c -o bin/stdio.o
	$(CC) $(CFLAGS) -Ilimine_libc -c limine_libc/string.c -o bin/string.o
	$(CC) $(CFLAGS) -Ilimine_libc -c limine_libc/syscall.c -o bin/syscall.o
	ld -r bin/stdio.o bin/string.o bin/syscall.o -o bin/limine_libc.o

# Build _syscall stub
bin/syscall_stub.o: syscall_stub.s
	$(AS) $< -o $@

# Build individual programs
bin/%: %.c bin/limine_libc.o bin/syscall_stub.o link.ld
	$(CC) $(CFLAGS) -Ilimine_libc -c $< -o bin/$*.o
	$(LD) $(LDFLAGS) bin/$*.o bin/limine_libc.o bin/syscall_stub.o -o $@
	objcopy --only-keep-debug $@ $@.debug
	objcopy --strip-debug $@

clean:
	rm -rf bin

# Install programs to initramfs
install: all
	@echo "Creating program CPIO archive..."
	# Create the archive in the programs dir first, containing only built executables
	cd bin && find . -maxdepth 1 -type f -print | cpio -o -H newc > ../initramfs_programs.cpio

	@echo "Moving CPIO archive to root..."
	# Move the programs-only archive to the root, overwriting any previous one
	mv initramfs_programs.cpio ../initramfs.cpio

.PHONY: build install clean
build: install
