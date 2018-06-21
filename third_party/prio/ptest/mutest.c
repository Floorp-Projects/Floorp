/*
 * This file is part of mutest, a simple micro unit testing framework for C.
 *
 * mutest was written by Leandro Lucarella <llucax@gmail.com> and is released
 * under the BOLA license, please see the LICENSE file or visit:
 * http://blitiri.com.ar/p/bola/
 *
 * This is the main program, it runs all the test suites and shows the
 * results.  The main work (of running the test suite) is done by the (usually)
 * synthesized mu_run_suites() function, which can be generated using the
 * mkmutest script (or written manually).
 *
 * Please, read the README file for more details.
 */

#include <mprio.h>
#include <stdio.h> /* printf(), fprintf() */
#include <string.h> /* strncmp() */

#include "mutest.h" /* MU_* constants, mu_print() */

/*
 * note that all global variables are public because they need to be accessed
 * from other modules, like the test suites or the module implementing
 * mu_run_suites()
 */

/* globals for managing test suites */
const char* mutest_suite_name;
int mutest_failed_suites;
int mutest_passed_suites;
int mutest_skipped_suites;
int mutest_suite_failed;


/* globals for managing test cases */
const char* mutest_case_name;
int mutest_failed_cases;
int mutest_passed_cases;
int mutest_case_failed;


/* globals for managing checks */
int mutest_failed_checks;
int mutest_passed_checks;


/* verbosity level, see mutest.h */
int mutest_verbose_level = 1; /* exported for use in test suites */



/*
 * only -v is supported right now, both "-v -v" and "-vv" are accepted for
 * increasing the verbosity by 2.
 */
void parse_args(__attribute__((unused)) int argc, char* argv[]) {
	while (*++argv) {
		if (strncmp(*argv, "-v", 2) == 0) {
			++mutest_verbose_level;
			char* c = (*argv) + 1;
			while (*++c) {
				if (*c != 'v')
					break;
				++mutest_verbose_level;
			}
		}
	}
}


int main(int argc, char* argv[]) {

  Prio_init ();
	parse_args(argc, argv);

	mu_run_suites();

  Prio_clear ();

	mu_print(MU_SUMMARY, "\n"
			"Tests done:\n"
			"\t%d test suite(s) passed, %d failed, %d skipped.\n"
			"\t%d test case(s) passed, %d failed.\n"
			"\t%d check(s) passed, %d failed.\n"
			"\n",
			mutest_passed_suites, mutest_failed_suites,
			mutest_skipped_suites,
			mutest_passed_cases, mutest_failed_cases,
			mutest_passed_checks, mutest_failed_checks);

	return (mutest_failed_suites + mutest_skipped_suites) ? 1 : 0;
}

