LIB=lib# compiler library directory
UTIL=util# compiler library: lib$(LIB).a
CFLAGS=-g -DYYDEBUG
TARGET=minor

$(TARGET): gram.y scan.l
	make -C $(LIB)
	byacc -dv gram.y
	flex -dl scan.l
	$(LINK.c) -o $@ -I$(LIB) lex.yy.c y.tab.c -L$(LIB) -l$(UTIL)

clean:
	make -C $(LIB) clean
	rm -f *.o lex.yy.c y.tab.c y.tab.h y.output out.asm $(TARGET)

