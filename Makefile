all: c a m u
a: c
	clang -g -Wall -Wextra -fsanitize=address *.c && ./a.out
m: c
	clang -g -Wall -Wextra -fsanitize=memory *.c && ./a.out
u: c
	clang -g -Wall -Wextra -fsanitize=undefined *.c && ./a.out
c:
	rm -rf *.o *.out mike.c
ccomp:
	ccomp mike.c && ./a.out
mike:
	cat *.c > mike.c
interp:
	ccomp -interp -quiet mike.c
