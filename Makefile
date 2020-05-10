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
lib: c
	gcc -fPIC -c sha3.c
	gcc -shared -Wl,-soname,libcurve9767.so.0 -o libcurve9767.so.0 sha3.o -lc
	sudo cp -uf libcurve9767.so.0 /usr/local/lib
	sudo ldconfig
