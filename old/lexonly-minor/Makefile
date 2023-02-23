CFLAGS=-g -DYYDEBUG
TARGET=minor

$(TARGET): scan.l
	flex -l scan.l
	$(LINK.c) -o $@ lex.yy.c

clean:
	rm $(TARGET) lex.yy.c
