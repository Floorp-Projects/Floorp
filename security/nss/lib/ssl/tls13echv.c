/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Validation functions for ECH public names. */

#include "seccomon.h"

/* Convert a single character `c` into a number `*d` with the given radix.
 * Fails if the character isn't valid for the radix.
 */
static SECStatus
tls13_IpDigit(PRUint8 c, PRUint8 radix, PRUint8 *d)
{
    PRUint8 v = 0xff;
    if (c >= '0' && c <= '9') {
        v = c - '0';
    } else if (radix > 10) {
        if (c >= 'a' && c <= 'f') {
            v = c - 'a';
        } else if (c >= 'A' && c <= 'F') {
            v = c - 'A';
        }
    }
    if (v >= radix) {
        return SECFailure;
    }
    *d = v;
    return SECSuccess;
}

/* This function takes the first couple of characters from `str`, starting at offset
 * `*i` and calculates a radix.  If it starts with "0x" or "0X", then `*i` is moved up
 * by two and `*radix` is set to 16 (hexadecimal).  If it starts with "0", then `*i` is
 * moved up by one and `*radix` is set to 8 (octal).  Otherwise, `*i` is left alone and
 * `*radix` is set to 10 (decimal).
 * Fails if there are no characters remaining or the next character is '.', either at
 * the start or after "0x".
 */
static SECStatus
tls13_IpRadix(const PRUint8 *str, unsigned int len, unsigned int *i, PRUint8 *radix)
{
    if (*i == len || str[*i] == '.') {
        return SECFailure;
    }
    if (str[*i] == '0') {
        (*i)++;
        if (*i < len && (str[*i] == 'x' || str[*i] == 'X')) {
            (*i)++;
            if (*i == len || str[*i] == '.') {
                return SECFailure;
            }
            *radix = 16;
        } else {
            *radix = 8;
        }
    } else {
        *radix = 10;
    }
    return SECSuccess;
}

/* Take a number from `str` from offset `*i` and put the value in `*v`.
 * This calculates the radix and returns a value between 0 and 2^32-1, using all
 * of the digits up to the end of the string (determined by `len`) or a period ('.').
 * Fails if there is no value, if there a non-digit characters, or if the value is
 * too large.
 */
static SECStatus
tls13_IpValue(const PRUint8 *str, unsigned int len, unsigned int *i, PRUint32 *v)
{
    PRUint8 radix;
    SECStatus rv = tls13_IpRadix(str, len, i, &radix);
    if (rv != SECSuccess) {
        return SECFailure;
    }
    PRUint64 part = 0;
    while (*i < len) {
        PRUint8 d;
        rv = tls13_IpDigit(str[*i], radix, &d);
        if (rv != SECSuccess) {
            if (str[*i] != '.') {
                return SECFailure;
            }
            break;
        }
        part = part * radix + d;
        if (part > PR_UINT32_MAX) {
            return SECFailure;
        }
        (*i)++;
    }
    *v = part;
    return SECSuccess;
}

/* Returns true if `end` is true and `v` is within the `limit`. Used to validate the
 * last part of an IPv4 address, which can hold larger numbers if there are fewer then
 * four parts. */
static PRBool
tls13_IpLastPart(PRBool end, PRUint32 v, PRUint32 limit)
{
    if (!end) {
        return PR_FALSE;
    }
    return v <= limit;
}

/* Returns true if `str` contains an IPv4 address. */
PRBool
tls13_IsIp(const PRUint8 *str, unsigned int len)
{
    PRUint32 part;
    PRUint32 v;
    unsigned int i = 0;
    for (part = 0; part < 4; part++) {
        SECStatus rv = tls13_IpValue(str, len, &i, &v);
        if (rv != SECSuccess) {
            return PR_FALSE;
        }
        if (v > 0xff || i == len) {
            return tls13_IpLastPart(i == len, v, PR_UINT32_MAX >> (part * 8));
        }
        PORT_Assert(str[i] == '.');
        i++;
    }

    return tls13_IpLastPart(i == len, v, 0xff);
}

static PRBool
tls13_IsLD(PRUint8 c)
{
    return (c >= 'a' && c <= 'z') ||
           (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') ||
           c == '_'; /* not in spec, but in the world; bug 1136616 */
}

/* Is this a valid dotted LDH string (that is, an A-Label domain name)?
 * This does not tolerate a trailing '.', where the DNS generally does.
 */
PRBool
tls13_IsLDH(const PRUint8 *str, unsigned int len)
{
    unsigned int i = 0;
    while (i < len && tls13_IsLD(str[i])) {
        unsigned int labelEnd = PR_MIN(len, i + 63);
        i++;
        while (i < labelEnd && (tls13_IsLD(str[i]) || str[i] == '-')) {
            i++;
        }
        if (str[i - 1] == '-') {
            /* labels cannot end in a hyphen */
            return PR_FALSE;
        }
        if (i == len) {
            return PR_TRUE;
        }
        if (str[i] != '.') {
            return PR_FALSE;
        }
        i++;
    }
    return PR_FALSE;
}
