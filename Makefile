CC ?= gcc
CFLAGS ?= -Wall -Wextra -std=c11 -O2 -D_POSIX_C_SOURCE=200809L
FLEX ?= flex
BISON ?= bison

OBJS = g-v2.tab.o g-v2.lex.o ast.o symbol_table.o semantic.o codegen.o

.PHONY: all clean flex bison lexer parser ast semantico codegen test

all: g-v2

g-v2: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

g-v2.tab.c g-v2.tab.h: g-v2.y
	$(BISON) -d -o g-v2.tab.c g-v2.y

g-v2.lex.c: g-v2.l g-v2.tab.h
	$(FLEX) -o g-v2.lex.c g-v2.l

g-v2.tab.o: g-v2.tab.c ast.h semantic.h codegen.h
	$(CC) $(CFLAGS) -c g-v2.tab.c

g-v2.lex.o: g-v2.lex.c g-v2.tab.h
	$(CC) $(CFLAGS) -c g-v2.lex.c

ast.o: ast.c ast.h
	$(CC) $(CFLAGS) -c ast.c

symbol_table.o: symbol_table.c symbol_table.h ast.h
	$(CC) $(CFLAGS) -c symbol_table.c

semantic.o: semantic.c semantic.h symbol_table.h ast.h
	$(CC) $(CFLAGS) -c semantic.c

codegen.o: codegen.c codegen.h symbol_table.h ast.h
	$(CC) $(CFLAGS) -c codegen.c

flex: g-v2.lex.c
bison: g-v2.tab.c g-v2.tab.h
lexer: flex g-v2.lex.o
parser: bison g-v2.tab.o
ast: ast.o
semantico: symbol_table.o semantic.o
codegen: codegen.o

test: g-v2 run-tests.sh
	./run-tests.sh

clean:
	rm -f g-v2 $(OBJS) g-v2.lex.c g-v2.tab.c g-v2.tab.h
