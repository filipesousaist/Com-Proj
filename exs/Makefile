EXT=.min
EXE=../minor
LIB=../libminor.a
AS=nasm -felf32
LD=ld -m elf_i386
DIR=.

.SUFFIXES: .asm $(EXT) .run

ALL= ack alloc argc args argv0 array at atoi call ciclo cmp div downto doze \
	elif eng ex extern fargs fib find foobar forfor forstop gcd hanoi \
	hello histograma hondt ifelse if in intindex iter logical nl \
	novrfl ob2 obfus pointer powfunc pow prtch rand recurs repeat \
	revlist setch setindex strchr strcmp sum tri varuse world keyw \
	add sort toupper printn isbn10 decls
EXS= hello.min ack.min cast.min div.min gcd.min fib.min find.min hanoi.min histograma.min hondt.min isbn10.min fact.min iter.min recurs.min

all::
	for i in $(ALL); do $(MAKE) $(MFLAGS) $$i; done

run::
	for i in $(ALL:%=%.run); do $(MAKE) $(MFLAGS) $$i; done

arm::
	$(MAKE) $(MFLAGS) AS=as LD=ld

exs.tgz: $(EXS)
	tar zcvf $@ $^

$(EXT).asm:
	$(EXE) $< $@
.asm.o:
	$(AS) $< -o $@
%: %.o
	$(LD) -o $@ $< $(LIB)

clean::
	rm -f *.asm *.o $(ALL)

iter: iter.o fact.o
	$(LD) -o $@ $^ $(LIB)
recurs: recurs.o fact.o
	$(LD) -o $@ $^ $(LIB)
extern: extern.o export.o
	$(LD) -o $@ $^ $(LIB)
varuse: varuse.o vardecl.o
	$(LD) -o $@ $^ $(LIB)
decls: decls.o hello.o forward.o cast.o
	$(LD) -o $@ $^ $(LIB)
sort: sort.o bubble.o
	$(LD) -o $@ $^ $(LIB)

%.run: %
	echo 12 | $(DIR)/$*

ex.run: ex
	echo '14\n3' | $(DIR)/$<
ack.run: ack
	$(DIR)/ack 3 9
args.run: args
	$(DIR)/args um dois tres
