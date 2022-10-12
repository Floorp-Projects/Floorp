#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "rure.h"

#ifndef DEBUG
  #define DEBUG false
#endif

bool test_is_match() {
    bool passed = true;
    const char *haystack = "snowman: \xE2\x98\x83";

    rure *re = rure_compile_must("\\p{So}$");
    bool matched = rure_is_match(re, (const uint8_t *)haystack,
                                 strlen(haystack), 0);
    if (!matched) {
        if (DEBUG) {
            fprintf(stderr,
                    "[test_is_match] expected match, but got no match\n");
        }
        passed = false;
    }
    rure_free(re);
    return passed;
}

bool test_shortest_match() {
    bool passed = true;
    const char *haystack = "aaaaa";

    rure *re = rure_compile_must("a+");
    size_t end = 0;
    bool matched = rure_shortest_match(re, (const uint8_t *)haystack,
                                       strlen(haystack), 0, &end);
    if (!matched) {
        if (DEBUG) {
            fprintf(stderr,
                    "[test_shortest_match] expected match, "
                    "but got no match\n");
        }
        passed = false;
    }
    size_t expect_end = 1;
    if (end != expect_end) {
        if (DEBUG) {
            fprintf(stderr,
                    "[test_shortest_match] expected match end location %zu "
                    "but got %zu\n", expect_end, end);
        }
        passed = false;
    }
    rure_free(re);
    return passed;
}

bool test_find() {
    bool passed = true;
    const char *haystack = "snowman: \xE2\x98\x83";

    rure *re = rure_compile_must("\\p{So}$");
    rure_match match = {0};
    bool matched = rure_find(re, (const uint8_t *)haystack, strlen(haystack),
                             0, &match);
    if (!matched) {
        if (DEBUG) {
            fprintf(stderr, "[test_find] expected match, but got no match\n");
        }
        passed = false;
    }
    size_t expect_start = 9;
    size_t expect_end = 12;
    if (match.start != expect_start || match.end != expect_end) {
        if (DEBUG) {
            fprintf(stderr,
                    "[test_find] expected match at (%zu, %zu), but "
                    "got match at (%zu, %zu)\n",
                    expect_start, expect_end, match.start, match.end);
        }
        passed = false;
    }
    rure_free(re);
    return passed;
}

bool test_captures() {
    bool passed = true;
    const char *haystack = "snowman: \xE2\x98\x83";

    rure *re = rure_compile_must(".(.*(?P<snowman>\\p{So}))$");
    rure_match match = {0};
    rure_captures *caps = rure_captures_new(re);
    bool matched = rure_find_captures(re, (const uint8_t *)haystack,
                                      strlen(haystack), 0, caps);
    if (!matched) {
        if (DEBUG) {
            fprintf(stderr,
                    "[test_captures] expected match, but got no match\n");
        }
        passed = false;
    }
    size_t expect_captures_len = 3;
    size_t captures_len = rure_captures_len(caps);
    if (captures_len != expect_captures_len) {
        if (DEBUG) {
            fprintf(stderr,
                    "[test_captures] "
                    "expected capture group length to be %zd, but "
                    "got %zd\n",
                    expect_captures_len, captures_len);
        }
        passed = false;
        goto done;
    }
    int32_t expect_capture_index = 2;
    int32_t capture_index = rure_capture_name_index(re, "snowman");
    if (capture_index != expect_capture_index) {
        if (DEBUG) {
            fprintf(stderr,
                    "[test_captures] "
                    "expected capture index %d for name 'snowman', but "
                    "got %d\n",
                    expect_capture_index, capture_index);
        }
        passed = false;
        goto done;
    }
    size_t expect_start = 9;
    size_t expect_end = 12;
    rure_captures_at(caps, 2, &match);
    if (match.start != expect_start || match.end != expect_end) {
        if (DEBUG) {
            fprintf(stderr,
                    "[test_captures] "
                    "expected capture 2 match at (%zu, %zu), "
                    "but got match at (%zu, %zu)\n",
                    expect_start, expect_end, match.start, match.end);
        }
        passed = false;
    }
done:
    rure_captures_free(caps);
    rure_free(re);
    return passed;
}

bool test_iter() {
    bool passed = true;
    const uint8_t *haystack = (const uint8_t *)"abc xyz";
    size_t haystack_len = strlen((const char *)haystack);

    rure *re = rure_compile_must("\\w+(\\w)");
    rure_match match = {0};
    rure_captures *caps = rure_captures_new(re);
    rure_iter *it = rure_iter_new(re);

    bool matched = rure_iter_next(it, haystack, haystack_len, &match);
    if (!matched) {
        if (DEBUG) {
            fprintf(stderr,
                    "[test_iter] expected first match, but got no match\n");
        }
        passed = false;
        goto done;
    }
    size_t expect_start = 0;
    size_t expect_end = 3;
    if (match.start != expect_start || match.end != expect_end) {
        if (DEBUG) {
            fprintf(stderr,
                    "[test_iter] expected first match at (%zu, %zu), but "
                    "got match at (%zu, %zu)\n",
                    expect_start, expect_end, match.start, match.end);
        }
        passed = false;
        goto done;
    }

    matched = rure_iter_next_captures(it, haystack, haystack_len, caps);
    if (!matched) {
        if (DEBUG) {
            fprintf(stderr,
                    "[test_iter] expected second match, but got no match\n");
        }
        passed = false;
        goto done;
    }
    rure_captures_at(caps, 1, &match);
    expect_start = 6;
    expect_end = 7;
    if (match.start != expect_start || match.end != expect_end) {
        if (DEBUG) {
            fprintf(stderr,
                    "[test_iter] expected second match at (%zu, %zu), but "
                    "got match at (%zu, %zu)\n",
                    expect_start, expect_end, match.start, match.end);
        }
        passed = false;
        goto done;
    }
done:
    rure_iter_free(it);
    rure_captures_free(caps);
    rure_free(re);
    return passed;
}

bool test_iter_capture_name(char *expect, char *given) {
    bool passed = true;
    if (strcmp(expect, given)) {
        if (DEBUG) {
            fprintf(stderr,
                    "[test_iter_capture_name] expected first capture "
                    "name '%s' got '%s'\n",
                    expect, given);
        }
        passed = false;
    }
    return passed;
}

bool test_iter_capture_names() {
    bool passed = true;

    char *name;
    rure *re = rure_compile_must(
        "(?P<year>\\d{4})-(?P<month>\\d{2})-(?P<day>\\d{2})");
    rure_iter_capture_names *it = rure_iter_capture_names_new(re);

    bool result = rure_iter_capture_names_next(it, &name);
    if (!result) {
        if (DEBUG) {
            fprintf(stderr,
                    "[test_iter_capture_names] expected a second name, "
                    "but got none\n");
        }
        passed = false;
        goto done;
    }

    result = rure_iter_capture_names_next(it, &name);
    passed = test_iter_capture_name("year", name);
    if (!passed) {
        goto done;
    }

    result = rure_iter_capture_names_next(it, &name);
    passed = test_iter_capture_name("month", name);
    if (!passed) {
        goto done;
    }

    result = rure_iter_capture_names_next(it, &name);
    passed = test_iter_capture_name("day", name);
    if (!passed) {
        goto done;
    }
done:
    rure_iter_capture_names_free(it);
    rure_free(re);
    return passed;
}

/*
 * This tests whether we can set the flags correctly. In this case, we disable
 * all flags, which includes disabling Unicode mode. When we disable Unicode
 * mode, we can match arbitrary possibly invalid UTF-8 bytes, such as \xFF.
 * (When Unicode mode is enabled, \xFF won't match .)
 */
bool test_flags() {
    bool passed = true;
    const char *pattern = ".";
    const char *haystack = "\xFF";

    rure *re = rure_compile((const uint8_t *)pattern, strlen(pattern),
                            0, NULL, NULL);
    bool matched = rure_is_match(re, (const uint8_t *)haystack,
                                 strlen(haystack), 0);
    if (!matched) {
        if (DEBUG) {
            fprintf(stderr, "[test_flags] expected match, but got no match\n");
        }
        passed = false;
    }
    rure_free(re);
    return passed;
}

bool test_compile_error() {
    bool passed = true;
    rure_error *err = rure_error_new();
    rure *re = rure_compile((const uint8_t *)"(", 1, 0, NULL, err);
    if (re != NULL) {
        if (DEBUG) {
            fprintf(stderr,
                    "[test_compile_error] "
                    "expected NULL regex pointer, but got non-NULL pointer\n");
        }
        passed = false;
        rure_free(re);
    }
    const char *msg = rure_error_message(err);
    if (NULL == strstr(msg, "unclosed group")) {
        if (DEBUG) {
            fprintf(stderr,
                    "[test_compile_error] "
                    "expected an 'unclosed parenthesis' error message, but "
                    "got this instead: '%s'\n", msg);
        }
        passed = false;
    }
    rure_error_free(err);
    return passed;
}

bool test_compile_error_size_limit() {
    bool passed = true;
    rure_options *opts = rure_options_new();
    rure_options_size_limit(opts, 0);
    rure_error *err = rure_error_new();
    rure *re = rure_compile((const uint8_t *)"\\w{100}", 8, 0, opts, err);
    if (re != NULL) {
        if (DEBUG) {
            fprintf(stderr,
                    "[test_compile_error_size_limit] "
                    "expected NULL regex pointer, but got non-NULL pointer\n");
        }
        passed = false;
        rure_free(re);
    }
    const char *msg = rure_error_message(err);
    if (NULL == strstr(msg, "exceeds size")) {
        if (DEBUG) {
            fprintf(stderr,
                    "[test_compile_error] "
                    "expected an 'exceeds size' error message, but "
                    "got this instead: '%s'\n", msg);
        }
        passed = false;
    }
    rure_options_free(opts);
    rure_error_free(err);
    return passed;
}

bool test_regex_set_matches() {

#define PAT_COUNT 6

    bool passed = true;
    const char *patterns[] = {
        "foo", "barfoo", "\\w+", "\\d+", "foobar", "bar"
    };
    const size_t patterns_lengths[] = {
        3, 6, 3, 3, 6, 3
    };

    rure_error *err = rure_error_new();
    rure_set *re = rure_compile_set((const uint8_t **) patterns,
                                    patterns_lengths,
                                    PAT_COUNT,
                                    0,
                                    NULL,
                                    err);
    if (re == NULL) {
        passed = false;
        goto done2;
    }

    if (rure_set_len(re) != PAT_COUNT) {
        passed = false;
        goto done1;
    }

    if (!rure_set_is_match(re, (const uint8_t *) "foobar", 6, 0)) {
        passed = false;
        goto done1;
    }

    if (rure_set_is_match(re, (const uint8_t *) "", 0, 0)) {
        passed = false;
        goto done1;
    }

    bool matches[PAT_COUNT];
    if (!rure_set_matches(re, (const uint8_t *) "foobar", 6, 0, matches)) {
        passed = false;
        goto done1;
    }

    const bool match_target[] = {
        true, false, true, false, true, true
    };

    int i;
    for (i = 0; i < PAT_COUNT; ++i) {
        if (matches[i] != match_target[i]) {
            passed = false;
            goto done1;
        }
    }

done1:
    rure_set_free(re);
done2:
    rure_error_free(err);
    return passed;

#undef PAT_COUNT
}

bool test_regex_set_match_start() {

#define PAT_COUNT 3

    bool passed = true;
    const char *patterns[] = {
        "foo", "bar", "fooo"
    };
    const size_t patterns_lengths[] = {
        3, 3, 4
    };

    rure_error *err = rure_error_new();
    rure_set *re = rure_compile_set((const uint8_t **) patterns,
                                    patterns_lengths,
                                    PAT_COUNT,
                                    0,
                                    NULL,
                                    err);
    if (re == NULL) {
        passed = false;
        goto done2;
    }

    if (rure_set_len(re) != PAT_COUNT) {
        passed = false;
        goto done1;
    }

    if (rure_set_is_match(re, (const uint8_t *)"foobiasdr", 7, 2)) {
        passed = false;
        goto done1;
    }

    {
        bool matches[PAT_COUNT];
        if (!rure_set_matches(re, (const uint8_t *)"fooobar", 8, 0, matches)) {
            passed = false;
            goto done1;
        }

        const bool match_target[] = {
            true, true, true
        };

        int i;
        for (i = 0; i < PAT_COUNT; ++i) {
            if (matches[i] != match_target[i]) {
                passed = false;
                goto done1;
            }
        }
    }

    {
        bool matches[PAT_COUNT];
        if (!rure_set_matches(re, (const uint8_t *)"fooobar", 7, 1, matches)) {
            passed = false;
            goto done1;
        }

        const bool match_target[] = {
            false, true, false
        };

        int i;
        for (i = 0; i < PAT_COUNT; ++i) {
            if (matches[i] != match_target[i]) {
                passed = false;
                goto done1;
            }
        }
    }

done1:
    rure_set_free(re);
done2:
    rure_error_free(err);
    return passed;

#undef PAT_COUNT
}

bool test_regex_set_options() {

    bool passed = true;
    rure_options *opts = rure_options_new();
    rure_options_size_limit(opts, 0);
    rure_error *err = rure_error_new();

    const char *patterns[] = { "\\w{100}" };
    const size_t patterns_lengths[] = { 8 };

    rure_set *re = rure_compile_set(
        (const uint8_t **) patterns, patterns_lengths, 1, 0, opts, err);
    if (re != NULL) {
        if (DEBUG) {
            fprintf(stderr,
                    "[test_compile_error_size_limit] "
                    "expected NULL regex pointer, but got non-NULL pointer\n");
        }
        passed = false;
        rure_set_free(re);
    }
    const char *msg = rure_error_message(err);
    if (NULL == strstr(msg, "exceeds size")) {
        if (DEBUG) {
            fprintf(stderr,
                    "[test_compile_error] "
                    "expected an 'exceeds size' error message, but "
                    "got this instead: '%s'\n", msg);
        }
        passed = false;
    }
    rure_options_free(opts);
    rure_error_free(err);
    return passed;
}

bool test_escape() {
    bool passed = true;

    const char *pattern = "^[a-z]+.*$";
    const char *expected_escaped = "\\^\\[a\\-z\\]\\+\\.\\*\\$";

    const char *escaped = rure_escape_must(pattern);
    if (!escaped) {
        if (DEBUG) {
            fprintf(stderr,
                    "[test_captures] expected escaped, but got no escaped\n");
        }
        passed = false;
    } else if (strcmp(escaped, expected_escaped) != 0) {
        if (DEBUG) {
            fprintf(stderr,
                    "[test_captures] expected \"%s\", but got \"%s\"\n",
                    expected_escaped, escaped);
        }
        passed = false;
    }
    rure_cstring_free((char *) escaped);
    return passed;
}

void run_test(bool (test)(), const char *name, bool *passed) {
    if (!test()) {
        *passed = false;
        fprintf(stderr, "FAILED: %s\n", name);
    } else {
        fprintf(stderr, "PASSED: %s\n", name);
    }
}

int main() {
    bool passed = true;

    run_test(test_is_match, "test_is_match", &passed);
    run_test(test_shortest_match, "test_shortest_match", &passed);
    run_test(test_find, "test_find", &passed);
    run_test(test_captures, "test_captures", &passed);
    run_test(test_iter, "test_iter", &passed);
    run_test(test_iter_capture_names, "test_iter_capture_names", &passed);
    run_test(test_flags, "test_flags", &passed);
    run_test(test_compile_error, "test_compile_error", &passed);
    run_test(test_compile_error_size_limit, "test_compile_error_size_limit",
             &passed);
    run_test(test_regex_set_matches, "test_regex_set_match", &passed);
    run_test(test_regex_set_options, "test_regex_set_options", &passed);
    run_test(test_regex_set_match_start, "test_regex_set_match_start",
             &passed);
    run_test(test_escape, "test_escape", &passed);

    if (!passed) {
        exit(1);
    }
    return 0;
}
