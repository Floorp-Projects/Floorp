/*
 * This file is part of mutest, a simple micro unit testing framework for C.
 *
 * mutest was written by Leandro Lucarella <llucax@gmail.com> and is released
 * under the BOLA license, please see the LICENSE file or visit:
 * http://blitiri.com.ar/p/bola/
 *
 * This header file should be included in the source files that will make up
 * a test suite. It's used for both C and Python implementation, but when
 * using the Python implementation you should define the MUTEST_PY macro.
 * If you implement your mu_run_suites() function yourself, you probably will
 * need to include this header too (see mkmutest).
 *
 * Please, read the README file for more details.
 */

#include <stdio.h> /* fprintf() */

#ifdef __cplusplus
extern "C" {
#endif

/* verbosity level (each level shows all the previous levels too) */
enum {
	MU_QUIET = 0, /* be completely quiet */
	MU_ERROR,     /* shows errors only */
	MU_SUMMARY,   /* shows a summary */
	MU_SUITE,     /* shows test suites progress */
	MU_CASE,      /* shows test cases progress */
	MU_CHECK      /* shows the current running check */
};

/* print a message according to the verbosity level */
#define mu_print(level, ...) \
	do { \
		if (mutest_verbose_level >= level) { \
			if (mutest_verbose_level == MU_ERROR) \
				fprintf(stderr, __VA_ARGS__); \
			else \
				fprintf(stdout, __VA_ARGS__); \
		} \
	} while (0)

/* print an error message */
#define mu_printerr(name, action) \
		mu_print(MU_ERROR, __FILE__ ":%d: " name " failed, "\
				action " test case\n", __LINE__);

/* modify the internal state so a failure gets counted */
#define mutest_count_err ++mutest_failed_checks; mutest_case_failed = 1;

/* modify the internal state so a success gets counted */
#define mutest_count_suc ++mutest_passed_checks;

#ifdef __cplusplus

#include <exception>

/* print an error message triggered by a C++ exception */
#define mu_printex(name, action, ex) \
		mu_print(MU_ERROR, __FILE__ ":%d: " name " failed, " \
				"exception thrown (%s), " action \
				" test case\n", __LINE__, ex);

#define mutest_try try {
#define mutest_catch(name, action, final) \
		} catch (const std::exception& e) { \
			mutest_count_err \
			mu_printex(name, action, e.what()); \
			final; \
		} catch (...) { \
			mutest_count_err \
			mu_printex(name, action, "[unknown]"); \
			final; \
		}

#else /* !__cplusplus */

#define mutest_try
#define mutest_catch(name, action, exp)

#endif /* __cplusplus */

/* check that an expression evaluates to true, continue if the check fails */
#define mu_check_base(exp, name, action, final) \
	do { \
		mu_print(MU_CHECK, "\t\t* Checking " name "(" #exp ")...\n"); \
		mutest_try \
			if (exp) mutest_count_suc \
			else { \
				mutest_count_err \
				mu_printerr(name "(" #exp ")", action); \
				final; \
			} \
		mutest_catch(name, action, final) \
	} while (0)

/* check that an expression evaluates to true, continue if the check fails */
#define mu_check(exp) mu_check_base(exp, "mu_check", "resuming", continue)

/*
 * ensure that an expression evaluates to true, abort the current test
 * case if the check fails
 */
#define mu_ensure(exp) mu_check_base(exp, "mu_ensure", "aborting", return)

#ifdef __cplusplus

#define mu_echeck_base(ex, exp, name, action, final) \
	do { \
		mu_print(MU_CHECK, "\t\t* Checking " name "(" #ex ", " #exp \
				")...\n"); \
		try { \
			exp; \
			mutest_count_err \
			mu_printerr(name "(" #ex ", " #exp ")", \
					"no exception thrown, "	action); \
			final; \
		} catch (const ex& e) { \
			mutest_count_suc \
		} catch (const std::exception& e) { \
			mutest_count_err \
			mu_printex(name "(" #ex ", " #exp ")", action, \
					e.what()); \
			final; \
		} catch (...) { \
			mutest_count_err \
			mu_printex(name "(" #ex ", " #exp ")", action, \
					"[unknown]"); \
			final; \
		} \
	} while (0)

/*
 * check that an expression throws a particular exception, continue if the
 * check fails
 */
#define mu_echeck(ex, exp) \
	mu_echeck_base(ex, exp, "mu_echeck", "resuming", continue)

/*
 * ensure that an expression throws a particular exception, abort the current
 * test case if the check fails
 */
#define mu_eensure(ex, exp) \
	mu_echeck_base(ex, exp, "mu_eensure", "aborting", return)

#endif /* __cplusplus */

#ifndef MUTEST_PY /* we are using the C implementation */

/*
 * this function implements the test suites execution, you should generate
 * a module with this function using mkmutest, or take a look to that script
 * if you want to implement your own customized version */
void mu_run_suites();

/* macro for running a single initialization function */
#ifndef mu_run_init
#define mu_run_init(name) \
	{ \
		int name(); \
		int r; \
		mu_print(MU_CASE, "\t+ Executing initialization function " \
				"'" #name "'...\n"); \
		if ((r = name())) { \
			mu_print(MU_ERROR, "%s:" #name ": initialization " \
					"function failed (returned %d), " \
					"skipping test suite...\n", \
					mutest_suite_name, r); \
			++mutest_skipped_suites; \
			break; \
		} \
	} do { } while (0)
#endif /* mu_run_init */

/* macro for running a single test case */
#ifndef mu_run_case
#define mu_run_case(name) \
	do { \
		mu_print(MU_CASE, "\t* Executing test case '" #name "'...\n");\
		mutest_case_name = #name; \
		void name(); \
		name(); \
		if (mutest_case_failed) { \
			++mutest_failed_cases; \
			mutest_suite_failed = 1; \
		} else ++mutest_passed_cases; \
		mutest_case_failed = 0; \
	} while (0)
#endif /* mu_run_case */

/* macro for running a single termination function */
#ifndef mu_run_term
#define mu_run_term(name) \
	do { \
		mu_print(MU_CASE, "\t- Executing termination function '" \
				#name "'...\n"); \
		void name(); \
		name(); \
	} while (0)
#endif /* mu_run_term */

/*
 * mutest exported variables for internal use, do not use directly unless you
 *  know what you're doing.
 */
extern const char* mutest_suite_name;
extern int mutest_failed_suites;
extern int mutest_passed_suites;
extern int mutest_skipped_suites;
extern int mutest_suite_failed;
/* test cases */
extern const char* mutest_case_name;
extern int mutest_failed_cases;
extern int mutest_passed_cases;
extern int mutest_case_failed;
/* checks */
extern int mutest_failed_checks;
extern int mutest_passed_checks;
/* verbosity */
extern int mutest_verbose_level;

#else /* MUTEST_PY is defined, using the Python implementation */

/* this increments when the "API" changes, it's just for sanity check */
int mutest_api_version = 1;

int mutest_case_failed; /* unused, for C implementation compatibility */

int mutest_passed_checks;
int mutest_failed_checks;
void mutest_reset_counters() {
	mutest_passed_checks = 0;
	mutest_failed_checks = 0;
}

int mutest_verbose_level = MU_ERROR;
void mutest_set_verbose_level(int val) {
	mutest_verbose_level = val;
}

#endif /* MUTEST_PY */

#ifdef __cplusplus
}
#endif

