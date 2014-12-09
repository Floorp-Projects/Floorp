/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#include "cpr_types.h"
#include "cpr_stdlib.h"
#include "cpr_string.h"
#include "cpr_strings.h"


/**
 * cpr_strdup
 *
 * @brief The CPR wrapper for strdup

 * The cpr_strdup shall return a pointer to a new string, which is a duplicate
 * of the string pointed to by "str" argument. A null pointer is returned if the
 * new string cannot be created.
 *
 * @param[in] str  - The string that needs to be duplicated
 *
 * @return The duplicated string or NULL in case of no memory
 *
 */
char *
cpr_strdup (const char *str)
{
    char *dup;
    size_t len;

    if (!str) {
        return (char *) NULL;
    }

    len = strlen(str);
    if (len == 0) {
        return (char *) NULL;
    }
    len++;

    dup = cpr_malloc(len * sizeof(char));
    if (!dup) {
        return (char *) NULL;
    }
    (void) memcpy(dup, str, len);
    return dup;
}


/**
 * cpr_strcasecmp
 *
 * @brief The CPR wrapper for strcasecmp
 *
 * The cpr_strcasecmp performs case insensitive string comparison of the "s1"
 * and the "s2" strings.
 *
 * @param[in] s1  - The first string
 * @param[in] s2  - The second string
 *
 * @return integer <,=,> 0 depending on whether s1 is <,=,> s2
 */
#ifndef CPR_USE_OS_STRCASECMP
int
cpr_strcasecmp (const char *s1, const char *s2)
{
    const unsigned char *us1 = (const unsigned char *) s1;
    const unsigned char *us2 = (const unsigned char *) s2;

    /* No match if only one ptr is NULL */
    if ((!s1 && s2) || (s1 && !s2)) {
        /*
         * If one of these is NULL it will be the lesser of the two
         * values and therefore we'll get the proper sign in the int
         */
        return (int) (s1 - s2);
    }

    /* Match if both ptrs the same (e.g. NULL) */
    if (s1 == s2)
        return 0;

    while (*us1 != '\0' && *us2 != '\0' && toupper(*us1) == toupper(*us2)) {
        us1++;
        us2++;
    }

    return (toupper(*us1) - toupper(*us2));
}

/**
 * cpr_strncasecmp
 *
 * @brief The CPR wrapper for strncasecmp
 *
 * The cpr_strncasecmp performs case insensitive string comparison for specific
 * length "len".
 *
 * @param[in] s1  - The first string
 * @param[in] s2  - The second string
 * @param[in] len  - The length to be compared
 *
 * @return integer <,=,> 0 depending on whether s1 is <,=,> s2
 */
int
cpr_strncasecmp (const char *s1, const char *s2, size_t len)
{
    const unsigned char *us1 = (const unsigned char *) s1;
    const unsigned char *us2 = (const unsigned char *) s2;

    /* No match if only one ptr is NULL */
    if ((!s1 && s2) || (s1 && !s2))
        return ((int) (s1 - s2));

    if ((len == 0) || (s1 == s2))
        return 0;

    while (len-- > 0 && toupper(*us1) == toupper(*us2)) {
        if (len == 0 || *us1 == '\0' || *us2 == '\0')
            break;
        us1++;
        us2++;
    }

    return (toupper(*us1) - toupper(*us2));
}
#endif

/**
 * strcasestr
 *
 * @brief The same as strstr, but ignores case
 *
 * The strcasestr performs the strstr function, but ignores the case.
 * This function shall locate the first occurrence in the string
 * pointed to by s1 of the sequence of bytes (excluding the terminating
 * null byte) in the string pointed to by s2.
 *
 * @param[in] s1  - The input string
 * @param[in] s2  - The pattern to be matched
 *
 * @return A pointer to the first occurrence of string s2 found
 *           in string s1 or NULL if not found.  If s2 is an empty
 *           string then s1 is returned.
 */
char *
strcasestr (const char *s1, const char *s2)
{
    unsigned int i;

    if (!s1)
        return (char *) NULL;

    if (!s2 || (s1 == s2) || (*s2 == '\0'))
        return (char *) s1;

    while (*s1) {
        i = 0;
        do {
            if (s2[i] == '\0')
                return (char *) s1;
            if (s1[i] == '\0')
                return (char *) NULL;
            if (toupper(s1[i]) != toupper(s2[i]))
                break;
            i++;
        } while (1);
        s1++;
    }

    return (char *) NULL;
}

