.SUFFIXES: .asm
LIB=run
CC=gcc -m32 # produce elf32 code in 64-bit machines
CFLAGS=-g -Dunderscore -fno-stack-protector # when __stack_chk_fail in Ubuntu
RUNTIME=lib$(LIB).a # stand-alone runtime
AS=nasm -felf32 # assembler command

all:: $(RUNTIME)

$(RUNTIME): linux.o lib.o sys.o minor.o fact.o
	ar crl $@ $^

fact.asm:
	../minor fact.min fact.asm

minor.o: minor.c
	gcc -m32 -c -DPOSTFIX minor.c

.asm.o:
	$(AS) $< -o $@

install:: $(RUNTIME)
	cp $(RUNTIME) ..

clean::
	rm -f $(RUNTIME) *.o a.out core fact.asm
