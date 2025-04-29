#!/bin/bash
set -e

# Create build directory
mkdir -p bin

# Compile our libc
gcc -Wall -Wextra -ffreestanding -fno-builtin -nostdlib -m64 -mno-red-zone \
    -fno-exceptions -fno-stack-protector -g -O0 -mno-sse -mno-sse2 \
    -c limine_libc.c -o bin/limine_libc.o

# Compile syscall stub
as syscall_stub.s -o bin/syscall_stub.o

# Compile each program
PROGRAMS="hello cat echo"
for prog in $PROGRAMS; do
    echo "Building $prog..."
    gcc -Wall -Wextra -ffreestanding -fno-builtin -nostdlib -m64 -mno-red-zone \
        -fno-exceptions -fno-stack-protector -g -O0 -mno-sse -mno-sse2 \
        -c $prog.c -o bin/$prog.o
    
    ld -Tlink.ld -nostdlib -static bin/$prog.o bin/limine_libc.o bin/syscall_stub.o -o bin/$prog
    
    # Strip debug symbols (optional)
    objcopy --strip-debug bin/$prog
done

# Create a temporary directory for initramfs contents
echo "Creating initramfs..."
rm -rf initramfs_temp
mkdir -p initramfs_temp

# Copy programs to the temp directory
cp bin/* initramfs_temp/

# Create example text files for the cat program to read
echo "This is a sample text file for the cat program." > initramfs_temp/sample.txt
echo "Hello, world!" > initramfs_temp/hello.txt

# Create the CPIO archive
echo "Creating CPIO archive..."
(cd initramfs_temp && find . | cpio -o -H newc > ../programs.cpio)

# Copy or merge with the main initramfs
if [ -f "../initramfs.cpio" ]; then
    echo "Merging with existing initramfs..."
    cat ../initramfs.cpio programs.cpio > ../combined.cpio
    mv ../combined.cpio ../initramfs.cpio
else
    echo "Creating new initramfs..."
    mv programs.cpio ../initramfs.cpio
fi

echo "Cleaning up..."
rm -rf initramfs_temp

echo "Programs built and added to initramfs successfully!"
