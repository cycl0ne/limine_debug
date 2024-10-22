# Limine Extension of the C Template

This repository is based of the C-Template from Limine. Instead of giving you just a screen with a line, this code here shows you some first steps on using the template.
Included is a printf to Serial out (thus the -serial stdio).

Its headless at the moment, so you can use it in the Github Codespace to play with it.
Included is also a PML4 Page Table "Walker", showing you the Table with its Flags (I assumed the following to be important: Read, Write, eXecute and Cache)

As you can see from the PML4 Table, all Access to memory on Bootup should be done through the HHDM (Higher Half Direct Map). Accessing the Memory normaly will result in an Page Fault.

Next Steps would be:
* Initialize an own GDT
* Initialize an own IDT
* Create Functions for FRAME/VMM

## Makefile targets

Running `make all` will compile the kernel (from the `kernel/` directory) and then generate a bootable ISO image.

Running `make all-hdd` will compile the kernel and then generate a raw image suitable to be flashed onto a USB stick or hard drive/SSD.

Running `make run` will build the kernel and a bootable ISO (equivalent to make all) and then run it using `qemu` (if installed).

Running `make run-hdd` will build the kernel and a raw HDD image (equivalent to make all-hdd) and then run it using `qemu` (if installed).

For x86_64, the `run-bios` and `run-hdd-bios` targets are equivalent to their non `-bios` counterparts except that they boot `qemu` using the default SeaBIOS firmware instead of OVMF.
