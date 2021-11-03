#!/bin/sh

qemu-system-x86_64 \
    -m 256M \
    -kernel linux-5.8.1/arch/x86/boot/bzImage \
    -initrd initramfs.cpio.gz \
    -nographic \
    -cpu qemu64,+smep,-smap -smp cores=2 \
    -append "nokaslr nopti nosmap root=/dev/ram rw console=ttyS0 loglevel=2 oops=panic panic=1 init_on_alloc=0 init_on_free=0" \
    -monitor none \
    -no-reboot \
    -nodefaults -snapshot \
    -no-kvm \
    -s \
    -chardev stdio,id=char0 -serial chardev:char0 \
    -sandbox on,obsolete=deny,elevateprivileges=deny,spawn=deny,resourcecontrol=deny
