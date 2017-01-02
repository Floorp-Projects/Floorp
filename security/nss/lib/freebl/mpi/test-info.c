/*
 *  test-info.c
 *
 *  Arbitrary precision integer arithmetic library
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Table mapping test suite names to index numbers */
const int g_count = 42;
const char *g_names[] = {
    "list",              /* print out a list of the available test suites */
    "copy",              /* test assignment of mp-int structures          */
    "exchange",          /* test exchange of mp-int structures            */
    "zero",              /* test zeroing of an mp-int                     */
    "set",               /* test setting an mp-int to a small constant    */
    "absolute-value",    /* test the absolute value function              */
    "negate",            /* test the arithmetic negation function         */
    "add-digit",         /* test digit addition                           */
    "add",               /* test full addition                            */
    "subtract-digit",    /* test digit subtraction                        */
    "subtract",          /* test full subtraction                         */
    "multiply-digit",    /* test digit multiplication                     */
    "multiply",          /* test full multiplication                      */
    "square",            /* test full squaring function                   */
    "divide-digit",      /* test digit division                           */
    "divide-2",          /* test division by two                          */
    "divide-2d",         /* test division & remainder by 2^d              */
    "divide",            /* test full division                            */
    "expt-digit",        /* test digit exponentiation                     */
    "expt",              /* test full exponentiation                      */
    "expt-2",            /* test power-of-two exponentiation              */
    "square-root",       /* test integer square root function             */
    "modulo-digit",      /* test digit modular reduction                  */
    "modulo",            /* test full modular reduction                   */
    "mod-add",           /* test modular addition                         */
    "mod-subtract",      /* test modular subtraction                      */
    "mod-multiply",      /* test modular multiplication                   */
    "mod-square",        /* test modular squaring function                */
    "mod-expt",          /* test full modular exponentiation              */
    "mod-expt-digit",    /* test digit modular exponentiation             */
    "mod-inverse",       /* test modular inverse function                 */
    "compare-digit",     /* test digit comparison function                */
    "compare-zero",      /* test zero comparison function                 */
    "compare",           /* test general signed comparison                */
    "compare-magnitude", /* test general magnitude comparison             */
    "parity",            /* test parity comparison functions              */
    "gcd",               /* test greatest common divisor functions        */
    "lcm",               /* test least common multiple function           */
    "conversion",        /* test general radix conversion facilities      */
    "binary",            /* test raw output format                        */
    "pprime",            /* test probabilistic primality tester           */
    "fermat"             /* test Fermat pseudoprimality tester            */
};

/* Test function prototypes */
int test_list(void);
int test_copy(void);
int test_exch(void);
int test_zero(void);
int test_set(void);
int test_abs(void);
int test_neg(void);
int test_add_d(void);
int test_add(void);
int test_sub_d(void);
int test_sub(void);
int test_mul_d(void);
int test_mul(void);
int test_sqr(void);
int test_div_d(void);
int test_div_2(void);
int test_div_2d(void);
int test_div(void);
int test_expt_d(void);
int test_expt(void);
int test_2expt(void);
int test_sqrt(void);
int test_mod_d(void);
int test_mod(void);
int test_addmod(void);
int test_submod(void);
int test_mulmod(void);
int test_sqrmod(void);
int test_exptmod(void);
int test_exptmod_d(void);
int test_invmod(void);
int test_cmp_d(void);
int test_cmp_z(void);
int test_cmp(void);
int test_cmp_mag(void);
int test_parity(void);
int test_gcd(void);
int test_lcm(void);
int test_convert(void);
int test_raw(void);
int test_pprime(void);
int test_fermat(void);

/* Table mapping index numbers to functions */
int (*g_tests[])(void) = {
    test_list, test_copy, test_exch, test_zero,
    test_set, test_abs, test_neg, test_add_d,
    test_add, test_sub_d, test_sub, test_mul_d,
    test_mul, test_sqr, test_div_d, test_div_2,
    test_div_2d, test_div, test_expt_d, test_expt,
    test_2expt, test_sqrt, test_mod_d, test_mod,
    test_addmod, test_submod, test_mulmod, test_sqrmod,
    test_exptmod, test_exptmod_d, test_invmod, test_cmp_d,
    test_cmp_z, test_cmp, test_cmp_mag, test_parity,
    test_gcd, test_lcm, test_convert, test_raw,
    test_pprime, test_fermat
};

/* Table mapping index numbers to descriptions */
const char *g_descs[] = {
    "print out a list of the available test suites",
    "test assignment of mp-int structures",
    "test exchange of mp-int structures",
    "test zeroing of an mp-int",
    "test setting an mp-int to a small constant",
    "test the absolute value function",
    "test the arithmetic negation function",
    "test digit addition",
    "test full addition",
    "test digit subtraction",
    "test full subtraction",
    "test digit multiplication",
    "test full multiplication",
    "test full squaring function",
    "test digit division",
    "test division by two",
    "test division & remainder by 2^d",
    "test full division",
    "test digit exponentiation",
    "test full exponentiation",
    "test power-of-two exponentiation",
    "test integer square root function",
    "test digit modular reduction",
    "test full modular reduction",
    "test modular addition",
    "test modular subtraction",
    "test modular multiplication",
    "test modular squaring function",
    "test full modular exponentiation",
    "test digit modular exponentiation",
    "test modular inverse function",
    "test digit comparison function",
    "test zero comparison function",
    "test general signed comparison",
    "test general magnitude comparison",
    "test parity comparison functions",
    "test greatest common divisor functions",
    "test least common multiple function",
    "test general radix conversion facilities",
    "test raw output format",
    "test probabilistic primality tester",
    "test Fermat pseudoprimality tester"
};
