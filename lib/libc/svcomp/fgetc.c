#include <stdio.h>
extern _Bool __symbiotic_nondet__Bool(void);

extern void klee_warning_once(const char *);
void klee_silent_exit(int) __attribute__((noreturn));

int fgetc(FILE *f) {
	// model failure
	if (__symbiotic_nondet__Bool())
		return EOF;
	return (int)__symbiotic_nondet_uchar();
}
