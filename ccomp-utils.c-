// ccomp helpers

#include <stdlib.h>
#include <stdint.h>

void *
memset(void *__s, int __c, size_t __n)
{

	if (__n != 0) {
		char *d = __s;

		do {
			*d++ = __c;
		} while (--__n != 0);
	}
	return (__s);
}

size_t strlen (const char *str)
{
    for (size_t len = 0;;++len) if (str[len]==0) return len;
}

void *memcpy(void *__restrict __dest, const void *__restrict __src, size_t __n)
{
   char *csrc = (char *)__src;
   char *cdest = (char *)__dest;
   for (size_t i=0; i<__n; i++)
       cdest[i] = csrc[i];
}
