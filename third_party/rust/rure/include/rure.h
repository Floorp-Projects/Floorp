#ifndef _RURE_H
#define _RURE_H

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * rure is the type of a compiled regular expression.
 *
 * An rure can be safely used from multiple threads simultaneously.
 */
typedef struct rure rure;

/*
 * rure_set is the type of a set of compiled regular expressions.
 *
 * A rure can be safely used from multiple threads simultaneously.
 */
typedef struct rure_set rure_set;

/*
 * rure_options is the set of non-flag configuration options for compiling
 * a regular expression. Currently, only two options are available: setting
 * the size limit of the compiled program and setting the size limit of the
 * cache of states that the DFA uses while searching.
 *
 * For most uses, the default settings will work fine, and NULL can be passed
 * wherever a *rure_options is expected.
*/
typedef struct rure_options rure_options;

/*
 * The flags listed below can be used in rure_compile to set the default
 * flags. All flags can otherwise be toggled in the expression itself using
 * standard syntax, e.g., `(?i)` turns case insensitive matching on and `(?-i)`
 * disables it.
 */
/* The case insensitive (i) flag. */
#define RURE_FLAG_CASEI (1 << 0)
/* The multi-line matching (m) flag. (^ and $ match new line boundaries.) */
#define RURE_FLAG_MULTI (1 << 1)
/* The any character (s) flag. (. matches new line.) */
#define RURE_FLAG_DOTNL (1 << 2)
/* The greedy swap (U) flag. (e.g., + is ungreedy and +? is greedy.) */
#define RURE_FLAG_SWAP_GREED (1 << 3)
/* The ignore whitespace (x) flag. */
#define RURE_FLAG_SPACE (1 << 4)
/* The Unicode (u) flag. */
#define RURE_FLAG_UNICODE (1 << 5)
/* The default set of flags enabled when no flags are set. */
#define RURE_DEFAULT_FLAGS RURE_FLAG_UNICODE

/*
 * rure_match corresponds to the location of a single match in a haystack.
 */
typedef struct rure_match {
    /* The start position. */
    size_t start;
    /* The end position. */
    size_t end;
} rure_match;

/*
 * rure_captures represents storage for sub-capture locations of a match.
 *
 * Computing the capture groups of a match can carry a significant performance
 * penalty, so their use in the API is optional.
 *
 * An rure_captures value can be reused in multiple calls to rure_find_captures,
 * so long as it is used with the compiled regular expression that created
 * it.
 *
 * An rure_captures value may outlive its corresponding rure and can be freed
 * independently.
 *
 * It is not safe to use from multiple threads simultaneously.
 */
typedef struct rure_captures rure_captures;

/*
 * rure_iter is an iterator over successive non-overlapping matches in a
 * particular haystack.
 *
 * An rure_iter value may not outlive its corresponding rure and should be freed
 * before its corresponding rure is freed.
 *
 * It is not safe to use from multiple threads simultaneously.
 */
typedef struct rure_iter rure_iter;

/*
 * rure_iter_capture_names is an iterator over the list of capture group names
 * in this particular rure.
 *
 * An rure_iter_capture_names value may not outlive its corresponding rure,
 * and should be freed before its corresponding rure is freed.
 *
 * It is not safe to use from multiple threads simultaneously.
 */
typedef struct rure_iter_capture_names rure_iter_capture_names;

/*
 * rure_error is an error that caused compilation to fail.
 *
 * Most errors are syntax errors but an error can be returned if the compiled
 * regular expression would be too big.
 *
 * Whenever a function accepts an *rure_error, it is safe to pass NULL. (But
 * you will not get access to the error if one occurred.)
 *
 * It is not safe to use from multiple threads simultaneously.
 */
typedef struct rure_error rure_error;

/*
 * rure_compile_must compiles the given pattern into a regular expression. If
 * compilation fails for any reason, an error message is printed to stderr and
 * the process is aborted.
 *
 * The pattern given should be in UTF-8. For convenience, this accepts a C
 * string, which means the pattern cannot usefully contain NUL. If your pattern
 * may contain NUL, consider using a regular expression escape sequence, or
 * just use rure_compile.
 *
 * This uses RURE_DEFAULT_FLAGS.
 *
 * The compiled expression returned may be used from multiple threads
 * simultaneously.
 */
rure *rure_compile_must(const char *pattern);

/*
 * rure_compile compiles the given pattern into a regular expression. The
 * pattern must be valid UTF-8 and the length corresponds to the number of
 * bytes in the pattern.
 *
 * flags is a bitfield. Valid values are constants declared with prefix
 * RURE_FLAG_.
 *
 * options contains non-flag configuration settings. If it's NULL, default
 * settings are used. options may be freed immediately after a call to
 * rure_compile.
 *
 * error is set if there was a problem compiling the pattern (including if the
 * pattern is not valid UTF-8). If error is NULL, then no error information
 * is returned. In all cases, if an error occurs, NULL is returned.
 *
 * The compiled expression returned may be used from multiple threads
 * simultaneously.
 */
rure *rure_compile(const uint8_t *pattern, size_t length,
                   uint32_t flags, rure_options *options,
                   rure_error *error);

/*
 * rure_free frees the given compiled regular expression.
 *
 * This must be called at most once for any rure.
 */
void rure_free(rure *re);

/*
 * rure_is_match returns true if and only if re matches anywhere in haystack.
 *
 * haystack may contain arbitrary bytes, but ASCII compatible text is more
 * useful. UTF-8 is even more useful. Other text encodings aren't supported.
 * length should be the number of bytes in haystack.
 *
 * start is the position at which to start searching. Note that setting the
 * start position is distinct from incrementing the pointer, since the regex
 * engine may look at bytes before the start position to determine match
 * information. For example, if the start position is greater than 0, then the
 * \A ("begin text") anchor can never match.
 *
 * rure_is_match should be preferred to rure_find since it may be faster.
 *
 * N.B. The performance of this search is not impacted by the presence of
 * capturing groups in your regular expression.
 */
bool rure_is_match(rure *re, const uint8_t *haystack, size_t length,
                   size_t start);

/*
 * rure_find returns true if and only if re matches anywhere in haystack.
 * If a match is found, then its start and end offsets (in bytes) are set
 * on the match pointer given.
 *
 * haystack may contain arbitrary bytes, but ASCII compatible text is more
 * useful. UTF-8 is even more useful. Other text encodings aren't supported.
 * length should be the number of bytes in haystack.
 *
 * start is the position at which to start searching. Note that setting the
 * start position is distinct from incrementing the pointer, since the regex
 * engine may look at bytes before the start position to determine match
 * information. For example, if the start position is greater than 0, then the
 * \A ("begin text") anchor can never match.
 *
 * rure_find should be preferred to rure_find_captures since it may be faster.
 *
 * N.B. The performance of this search is not impacted by the presence of
 * capturing groups in your regular expression.
 */
bool rure_find(rure *re, const uint8_t *haystack, size_t length,
               size_t start, rure_match *match);

/*
 * rure_find_captures returns true if and only if re matches anywhere in
 * haystack. If a match is found, then all of its capture locations are stored
 * in the captures pointer given.
 *
 * haystack may contain arbitrary bytes, but ASCII compatible text is more
 * useful. UTF-8 is even more useful. Other text encodings aren't supported.
 * length should be the number of bytes in haystack.
 *
 * start is the position at which to start searching. Note that setting the
 * start position is distinct from incrementing the pointer, since the regex
 * engine may look at bytes before the start position to determine match
 * information. For example, if the start position is greater than 0, then the
 * \A ("begin text") anchor can never match.
 *
 * Only use this function if you specifically need access to capture locations.
 * It is not necessary to use this function just because your regular
 * expression contains capturing groups.
 *
 * Capture locations can be accessed using the rure_captures_* functions.
 *
 * N.B. The performance of this search can be impacted by the number of
 * capturing groups. If you're using this function, it may be beneficial to
 * use non-capturing groups (e.g., `(?:re)`) where possible.
 */
bool rure_find_captures(rure *re, const uint8_t *haystack, size_t length,
                        size_t start, rure_captures *captures);

/*
 * rure_shortest_match returns true if and only if re matches anywhere in
 * haystack. If a match is found, then its end location is stored in the
 * pointer given. The end location is the place at which the regex engine
 * determined that a match exists, but may occur before the end of the proper
 * leftmost-first match.
 *
 * haystack may contain arbitrary bytes, but ASCII compatible text is more
 * useful. UTF-8 is even more useful. Other text encodings aren't supported.
 * length should be the number of bytes in haystack.
 *
 * start is the position at which to start searching. Note that setting the
 * start position is distinct from incrementing the pointer, since the regex
 * engine may look at bytes before the start position to determine match
 * information. For example, if the start position is greater than 0, then the
 * \A ("begin text") anchor can never match.
 *
 * rure_shortest_match should be preferred to rure_find since it may be faster.
 *
 * N.B. The performance of this search is not impacted by the presence of
 * capturing groups in your regular expression.
 */
bool rure_shortest_match(rure *re, const uint8_t *haystack, size_t length,
                         size_t start, size_t *end);

/*
 * rure_capture_name_index returns the capture index for the name given. If
 * no such named capturing group exists in re, then -1 is returned.
 *
 * The capture index may be used with rure_captures_at.
 *
 * This function never returns 0 since the first capture group always
 * corresponds to the entire match and is always unnamed.
 */
int32_t rure_capture_name_index(rure *re, const char *name);

/*
 * rure_iter_capture_names_new creates a new capture_names iterator.
 *
 * An iterator will report all successive capture group names of re.
 */
rure_iter_capture_names *rure_iter_capture_names_new(rure *re);

/*
 * rure_iter_capture_names_free frees the iterator given.
 *
 * It must be called at most once.
 */
void rure_iter_capture_names_free(rure_iter_capture_names *it);

/*
 * rure_iter_capture_names_next advances the iterator and returns true
 * if and only if another capture group name exists.
 *
 * The value of the capture group name is written to the provided pointer.
 */
bool rure_iter_capture_names_next(rure_iter_capture_names *it, char **name);

/*
 * rure_iter_new creates a new iterator.
 *
 * An iterator will report all successive non-overlapping matches of re.
 * When calling iterator functions, the same haystack and length must be
 * supplied to all invocations. (Strict pointer equality is, however, not
 * required.)
 */
rure_iter *rure_iter_new(rure *re);

/*
 * rure_iter_free frees the iterator given.
 *
 * It must be called at most once.
 */
void rure_iter_free(rure_iter *it);

/*
 * rure_iter_next advances the iterator and returns true if and only if a
 * match was found. If a match is found, then the match pointer is set with the
 * start and end location of the match, in bytes.
 *
 * If no match is found, then subsequent calls will return false indefinitely.
 *
 * haystack may contain arbitrary bytes, but ASCII compatible text is more
 * useful. UTF-8 is even more useful. Other text encodings aren't supported.
 * length should be the number of bytes in haystack. The given haystack must
 * be logically equivalent to all other haystacks given to this iterator.
 *
 * rure_iter_next should be preferred to rure_iter_next_captures since it may
 * be faster.
 *
 * N.B. The performance of this search is not impacted by the presence of
 * capturing groups in your regular expression.
 */
bool rure_iter_next(rure_iter *it, const uint8_t *haystack, size_t length,
                    rure_match *match);

/*
 * rure_iter_next_captures advances the iterator and returns true if and only if a
 * match was found. If a match is found, then all of its capture locations are
 * stored in the captures pointer given.
 *
 * If no match is found, then subsequent calls will return false indefinitely.
 *
 * haystack may contain arbitrary bytes, but ASCII compatible text is more
 * useful. UTF-8 is even more useful. Other text encodings aren't supported.
 * length should be the number of bytes in haystack. The given haystack must
 * be logically equivalent to all other haystacks given to this iterator.
 *
 * Only use this function if you specifically need access to capture locations.
 * It is not necessary to use this function just because your regular
 * expression contains capturing groups.
 *
 * Capture locations can be accessed using the rure_captures_* functions.
 *
 * N.B. The performance of this search can be impacted by the number of
 * capturing groups. If you're using this function, it may be beneficial to
 * use non-capturing groups (e.g., `(?:re)`) where possible.
 */
bool rure_iter_next_captures(rure_iter *it,
                             const uint8_t *haystack, size_t length,
                             rure_captures *captures);

/*
 * rure_captures_new allocates storage for all capturing groups in re.
 *
 * An rure_captures value may be reused on subsequent calls to
 * rure_find_captures or rure_iter_next_captures.
 *
 * An rure_captures value may be freed independently of re, although any
 * particular rure_captures should be used only with the re given here.
 *
 * It is not safe to use an rure_captures value from multiple threads
 * simultaneously.
 */
rure_captures *rure_captures_new(rure *re);

/*
 * rure_captures_free frees the given captures.
 *
 * This must be called at most once.
 */
void rure_captures_free(rure_captures *captures);

/*
 * rure_captures_at returns true if and only if the capturing group at the
 * index given was part of a match. If so, the given match pointer is populated
 * with the start and end location (in bytes) of the capturing group.
 *
 * If no capture group with the index i exists, then false is
 * returned. (A capturing group exists if and only if i is less than
 * rure_captures_len(captures).)
 *
 * Note that index 0 corresponds to the full match.
 */
bool rure_captures_at(rure_captures *captures, size_t i, rure_match *match);

/*
 * rure_captures_len returns the number of capturing groups in the given
 * captures.
 */
size_t rure_captures_len(rure_captures *captures);

/*
 * rure_options_new allocates space for options.
 *
 * Options may be freed immediately after a call to rure_compile, but otherwise
 * may be freely used in multiple calls to rure_compile.
 *
 * It is not safe to set options from multiple threads simultaneously. It is
 * safe to call rure_compile from multiple threads simultaneously using the
 * same options pointer.
 */
rure_options *rure_options_new();

/*
 * rure_options_free frees the given options.
 *
 * This must be called at most once.
 */
void rure_options_free(rure_options *options);

/*
 * rure_options_size_limit sets the appoximate size limit of the compiled
 * regular expression.
 *
 * This size limit roughly corresponds to the number of bytes occupied by a
 * single compiled program. If the program would exceed this number, then a
 * compilation error will be returned from rure_compile.
 */
void rure_options_size_limit(rure_options *options, size_t limit);

/*
 * rure_options_dfa_size_limit sets the approximate size of the cache used by
 * the DFA during search.
 *
 * This roughly corresponds to the number of bytes that the DFA will use while
 * searching.
 *
 * Note that this is a *per thread* limit. There is no way to set a global
 * limit. In particular, if a regular expression is used from multiple threads
 * simultaneously, then each thread may use up to the number of bytes
 * specified here.
 */
void rure_options_dfa_size_limit(rure_options *options, size_t limit);

/*
 * rure_compile_set compiles the given list of patterns into a single regular
 * expression which can be matched in a linear-scan. Each pattern in patterns
 * must be valid UTF-8 and the length of each pattern in patterns corresponds
 * to a byte length in patterns_lengths.
 *
 * The number of patterns to compile is specified by patterns_count. patterns
 * must contain at least this many entries.
 *
 * flags is a bitfield. Valid values are constants declared with prefix
 * RURE_FLAG_.
 *
 * options contains non-flag configuration settings. If it's NULL, default
 * settings are used. options may be freed immediately after a call to
 * rure_compile.
 *
 * error is set if there was a problem compiling the pattern.
 *
 * The compiled expression set returned may be used from multiple threads.
 */
rure_set *rure_compile_set(const uint8_t **patterns,
                           const size_t *patterns_lengths,
                           size_t patterns_count,
                           uint32_t flags,
                           rure_options *options,
                           rure_error *error);

/*
 * rure_set_free frees the given compiled regular expression set.
 *
 * This must be called at most once for any rure_set.
 */
void rure_set_free(rure_set *re);

/*
 * rure_is_match returns true if and only if any regexes within the set
 * match anywhere in the haystack. Once a match has been located, the
 * matching engine will quit immediately.
 *
 * haystack may contain arbitrary bytes, but ASCII compatible text is more
 * useful. UTF-8 is even more useful. Other text encodings aren't supported.
 * length should be the number of bytes in haystack.
 *
 * start is the position at which to start searching. Note that setting the
 * start position is distinct from incrementing the pointer, since the regex
 * engine may look at bytes before the start position to determine match
 * information. For example, if the start position is greater than 0, then the
 * \A ("begin text") anchor can never match.
 */
bool rure_set_is_match(rure_set *re, const uint8_t *haystack, size_t length,
                       size_t start);

/*
 * rure_set_matches compares each regex in the set against the haystack and
 * modifies matches with the match result of each pattern. Match results are
 * ordered in the same way as the rure_set was compiled. For example,
 * index 0 of matches corresponds to the first pattern passed to
 * `rure_compile_set`.
 *
 * haystack may contain arbitrary bytes, but ASCII compatible text is more
 * useful. UTF-8 is even more useful. Other text encodings aren't supported.
 * length should be the number of bytes in haystack.
 *
 * start is the position at which to start searching. Note that setting the
 * start position is distinct from incrementing the pointer, since the regex
 * engine may look at bytes before the start position to determine match
 * information. For example, if the start position is greater than 0, then the
 * \A ("begin text") anchor can never match.
 *
 * matches must be greater than or equal to the number of patterns the
 * rure_set was compiled with.
 *
 * Only use this function if you specifically need to know which regexes
 * matched within the set. To determine if any of the regexes matched without
 * caring which, use rure_set_is_match.
 */
bool rure_set_matches(rure_set *re, const uint8_t *haystack, size_t length,
                      size_t start, bool *matches);

/*
 * rure_set_len returns the number of patterns rure_set was compiled with.
 */
size_t rure_set_len(rure_set *re);

/*
 * rure_error_new allocates space for an error.
 *
 * If error information is desired, then rure_error_new should be called
 * to create an rure_error pointer, and that pointer can be passed to
 * rure_compile. If an error occurred, then rure_compile will return NULL and
 * the error pointer will be set. A message can then be extracted.
 *
 * It is not safe to use errors from multiple threads simultaneously. An error
 * value may be reused on subsequent calls to rure_compile.
 */
rure_error *rure_error_new();

/*
 * rure_error_free frees the error given.
 *
 * This must be called at most once.
 */
void rure_error_free(rure_error *err);

/*
 * rure_error_message returns a NUL terminated string that describes the error
 * message.
 *
 * The pointer returned must not be freed. Instead, it will be freed when
 * rure_error_free is called. If err is used in subsequent calls to
 * rure_compile, then this pointer may change or become invalid.
 */
const char *rure_error_message(rure_error *err);

/*
 * rure_escape_must returns a NUL terminated string where all meta characters
 * have been escaped. If escaping fails for any reason, an error message is
 * printed to stderr and the process is aborted.
 *
 * The pattern given should be in UTF-8. For convenience, this accepts a C
 * string, which means the pattern cannot contain a NUL byte. These correspond
 * to the only two failure conditions of this function. That is, if the caller
 * guarantees that the given pattern is valid UTF-8 and does not contain a
 * NUL byte, then this is guaranteed to succeed (modulo out-of-memory errors).
 *
 * The pointer returned must not be freed directly. Instead, it should be freed
 * by calling rure_cstring_free.
 */
const char *rure_escape_must(const char *pattern);

/*
 * rure_cstring_free frees the string given.
 *
 * This must be called at most once per string.
 */
void rure_cstring_free(char *s);

#ifdef __cplusplus
}
#endif

#endif
