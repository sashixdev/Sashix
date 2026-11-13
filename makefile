CC = gcc
LD = ld
OBJCOPY = objcopy
NASM = nasm
QEMU = "C:\Program Files\qemu\qemu-system-x86_64.exe"

CFLAGS = -m32 -march=i386 -mno-sse -mno-sse2 -mno-mmx -ffreestanding -fno-pie -fno-stack-protector -nostdlib -nostdinc -O1 -Wall -Wextra -Isaskernel -Isaskernel/Drivers/screen

OBJS = entry.o files.o sappedit.o sas_vm.o vfs.o notify.o \
       mount.o installer.o idt.o panic.o \
       ata.o screen.o mouse.o keyboard.o \
       vga.o snake.o term.o settings.o media.o engine3d.o

HEADERS = saskernel/Drivers/disk/types.h saskernel/Drivers/disk/io.h \
          saskernel/Drivers/screen/screen.h saskernel/Drivers/screen/mouse.h \
          saskernel/Drivers/screen/notify.h \
          saskernel/Drivers/keyboard/keyboard.h \
          saskernel/gui/apps.h saskernel/vfs.h saskernel/sas_vm.h

all: Sashix.img

bootz.bin: Zet/bootz.asm
	$(NASM) -f bin $< -o $@

inbootz.bin: Zet/inbootz.asm
	$(NASM) -f bin $< -o $@

entry.o: saskernel/entry.asm
	$(NASM) -f win32 --prefix _ $< -o $@

mount.o: saskernel/mount.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

installer.o: saskernel/installer.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

idt.o: saskernel/idt.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

panic.o: saskernel/panic.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

vfs.o: saskernel/vfs.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

notify.o: saskernel/Drivers/screen/notify.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

sas_vm.o: saskernel/sas_vm.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

ata.o: saskernel/Drivers/disk/ata.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

screen.o: saskernel/Drivers/screen/screen.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

mouse.o: saskernel/Drivers/screen/mouse.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

keyboard.o: saskernel/Drivers/keyboard/keyboard.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

vga.o: saskernel/gui/vga.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

snake.o: saskernel/gui/snake.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

term.o: saskernel/gui/term.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

files.o: saskernel/gui/files.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

settings.o: saskernel/gui/settings.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

media.o: saskernel/gui/media.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

engine3d.o: saskernel/gui/engine3d.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

sappedit.o: saskernel/gui/sappedit.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

sasKernel.pe: $(OBJS)
	$(LD) -m i386pe --image-base 0x0 --section-alignment 0x200 --file-alignment 0x200 -Ttext 0x9000 -e _start -o $@ $^

sasKernel.bin: sasKernel.pe
	$(OBJCOPY) -O binary -j .text -j .rodata -j .rdata -j .data $< $@

Sashix.img: bootz.bin inbootz.bin sasKernel.bin
	powershell -NoProfile -Command "$$b=[IO.File]::ReadAllBytes('bootz.bin'); $$i=[IO.File]::ReadAllBytes('inbootz.bin'); $$k=[IO.File]::ReadAllBytes('sasKernel.bin'); $$img=New-Object byte[] (10*1024*1024); [Array]::Copy($$b,0,$$img,0,$$b.Length); [Array]::Copy($$i,0,$$img,512,$$i.Length); [Array]::Copy($$k,0,$$img,4608,$$k.Length); [IO.File]::WriteAllBytes('Sashix.img',$$img)"

run: all
	$(QEMU) -vga std -full-screen -display sdl,window-close=on -audiodev sdl,id=snd0 -machine pcspk-audiodev=snd0 -drive format=raw,file=Sashix.img,if=ide

clean:
	@powershell -NoProfile -Command "Remove-Item -Force -ErrorAction SilentlyContinue *.o, *.bin, *.pe, Sashix.img; exit 0"