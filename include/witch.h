#ifndef _WITCH_H_
#define _WITCH_H_

#ifdef __cplusplus
extern "C" {
#endif

/* declarations that we need to have in the code */
extern int __VALIDATOR_branch(unsigned int l, unsigned int c, int e);
extern void __VALIDATOR_assume(int c, int f);
extern int __VALIDATOR_segment(unsigned int s);
extern int __VALIDATOR_switch(unsigned int l, unsigned int c, int e);

#ifdef __cplusplus
}
#endif

#endif /* _WITCH_H_ */
