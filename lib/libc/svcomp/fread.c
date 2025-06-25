#include "symbiotic-size_t.h"
#include <stdio.h>

extern char __symbiotic_nondet_char();
extern _Bool __symbiotic_nondet__Bool();


size_t fread(void * buf, size_t size, size_t count, FILE * fp)
{
	size_t resid;
	char *p;
	int r;
	size_t total;

	if ((count == 0) || (size == 0))
		return (0);
    for (size_t i = 0; i < count * size; i++) {
        if (symbiotic_nondet__Bool())
            return i / size;
        ((char*)buf)[i] = __symbiotic_nondet_char();
    }
    return count;
}
