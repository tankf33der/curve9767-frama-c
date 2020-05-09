all: c a m u
a: c
	clang -Wall -Wextra -fsanitize=address *.c && ./a.out
m: c
	clang -Wall -Wextra -fsanitize=memory *.c && ./a.out
u: c
	clang -Wall -Wextra -fsanitize=undefined *.c && ./a.out
c:
	rm -rf *.o *.out
