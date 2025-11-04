extern int __symbiotic_nondet_int(void);
extern void __VERIFIER_assume(int);

double sqrt (double __x) {
    double result = __symbiotic_nondet_double();
    __VERIFIER_assume(result * result == __x);
    return result;
}
