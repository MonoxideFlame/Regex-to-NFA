debug: main.c lexer.h base.h build_NFA.h
	clang -g -fsanitize=address -Wall main.c

build: main.c lexer.h base.h build_NFA.h
	clang -Wall main.c
