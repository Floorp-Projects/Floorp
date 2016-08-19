/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * utf8.c
 *
 * This file contains some additional utility routines required for
 * handling UTF8 strings.
 */

#ifndef BASE_H
#include "base.h"
#endif /* BASE_H */

#include "plstr.h"

/*
 * NOTES:
 *
 * There's an "is hex string" function in pki1/atav.c.  If we need
 * it in more places, pull that one out.
 */

/*
 * nssUTF8_CaseIgnoreMatch
 *
 * Returns true if the two UTF8-encoded strings pointed to by the
 * two specified NSSUTF8 pointers differ only in typcase.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_POINTER
 *
 * Return value:
 *  PR_TRUE if the strings match, ignoring case
 *  PR_FALSE if they don't
 *  PR_FALSE upon error
 */

NSS_IMPLEMENT PRBool
nssUTF8_CaseIgnoreMatch(const NSSUTF8 *a, const NSSUTF8 *b, PRStatus *statusOpt)
{
#ifdef NSSDEBUG
    if (((const NSSUTF8 *)NULL == a) || ((const NSSUTF8 *)NULL == b)) {
        nss_SetError(NSS_ERROR_INVALID_POINTER);
        if ((PRStatus *)NULL != statusOpt) {
            *statusOpt = PR_FAILURE;
        }
        return PR_FALSE;
    }
#endif /* NSSDEBUG */

    if ((PRStatus *)NULL != statusOpt) {
        *statusOpt = PR_SUCCESS;
    }

    /*
     * XXX fgmr
     *
     * This is, like, so wrong!
     */
    if (0 == PL_strcasecmp((const char *)a, (const char *)b)) {
        return PR_TRUE;
    } else {
        return PR_FALSE;
    }
}

/*
 * nssUTF8_PrintableMatch
 *
 * Returns true if the two Printable strings pointed to by the
 * two specified NSSUTF8 pointers match when compared with the
 * rules for Printable String (leading and trailing spaces are
 * disregarded, extents of whitespace match irregardless of length,
 * and case is not significant), then PR_TRUE will be returned.
 * Otherwise, PR_FALSE will be returned.  Upon failure, PR_FALSE
 * will be returned.  If the optional statusOpt argument is not
 * NULL, then PR_SUCCESS or PR_FAILURE will be stored in that
 * location.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_POINTER
 *
 * Return value:
 *  PR_TRUE if the strings match, ignoring case
 *  PR_FALSE if they don't
 *  PR_FALSE upon error
 */

NSS_IMPLEMENT PRBool
nssUTF8_PrintableMatch(const NSSUTF8 *a, const NSSUTF8 *b, PRStatus *statusOpt)
{
    PRUint8 *c;
    PRUint8 *d;

#ifdef NSSDEBUG
    if (((const NSSUTF8 *)NULL == a) || ((const NSSUTF8 *)NULL == b)) {
        nss_SetError(NSS_ERROR_INVALID_POINTER);
        if ((PRStatus *)NULL != statusOpt) {
            *statusOpt = PR_FAILURE;
        }
        return PR_FALSE;
    }
#endif /* NSSDEBUG */

    if ((PRStatus *)NULL != statusOpt) {
        *statusOpt = PR_SUCCESS;
    }

    c = (PRUint8 *)a;
    d = (PRUint8 *)b;

    while (' ' == *c) {
        c++;
    }

    while (' ' == *d) {
        d++;
    }

    while (('\0' != *c) && ('\0' != *d)) {
        PRUint8 e, f;

        e = *c;
        f = *d;

        if (('a' <= e) && (e <= 'z')) {
            e -= ('a' - 'A');
        }

        if (('a' <= f) && (f <= 'z')) {
            f -= ('a' - 'A');
        }

        if (e != f) {
            return PR_FALSE;
        }

        c++;
        d++;

        if (' ' == *c) {
            while (' ' == *c) {
                c++;
            }
            c--;
        }

        if (' ' == *d) {
            while (' ' == *d) {
                d++;
            }
            d--;
        }
    }

    while (' ' == *c) {
        c++;
    }

    while (' ' == *d) {
        d++;
    }

    if (*c == *d) {
        /* And both '\0', btw */
        return PR_TRUE;
    } else {
        return PR_FALSE;
    }
}

/*
 * nssUTF8_Duplicate
 *
 * This routine duplicates the UTF8-encoded string pointed to by the
 * specified NSSUTF8 pointer.  If the optional arenaOpt argument is
 * not null, the memory required will be obtained from that arena;
 * otherwise, the memory required will be obtained from the heap.
 * A pointer to the new string will be returned.  In case of error,
 * an error will be placed on the error stack and NULL will be
 * returned.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_POINTER
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 */

NSS_IMPLEMENT NSSUTF8 *
nssUTF8_Duplicate(const NSSUTF8 *s, NSSArena *arenaOpt)
{
    NSSUTF8 *rv;
    PRUint32 len;

#ifdef NSSDEBUG
    if ((const NSSUTF8 *)NULL == s) {
        nss_SetError(NSS_ERROR_INVALID_POINTER);
        return (NSSUTF8 *)NULL;
    }

    if ((NSSArena *)NULL != arenaOpt) {
        if (PR_SUCCESS != nssArena_verifyPointer(arenaOpt)) {
            return (NSSUTF8 *)NULL;
        }
    }
#endif /* NSSDEBUG */

    len = PL_strlen((const char *)s);
#ifdef PEDANTIC
    if ('\0' != ((const char *)s)[len]) {
        /* must have wrapped, e.g., too big for PRUint32 */
        nss_SetError(NSS_ERROR_NO_MEMORY);
        return (NSSUTF8 *)NULL;
    }
#endif     /* PEDANTIC */
    len++; /* zero termination */

    rv = nss_ZAlloc(arenaOpt, len);
    if ((void *)NULL == rv) {
        return (NSSUTF8 *)NULL;
    }

    (void)nsslibc_memcpy(rv, s, len);
    return rv;
}

/*
 * nssUTF8_Size
 *
 * This routine returns the length in bytes (including the terminating
 * null) of the UTF8-encoded string pointed to by the specified
 * NSSUTF8 pointer.  Zero is returned on error.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_POINTER
 *  NSS_ERROR_VALUE_TOO_LARGE
 *
 * Return value:
 *  0 on error
 *  nonzero length of the string.
 */

NSS_IMPLEMENT PRUint32
nssUTF8_Size(const NSSUTF8 *s, PRStatus *statusOpt)
{
    PRUint32 sv;

#ifdef NSSDEBUG
    if ((const NSSUTF8 *)NULL == s) {
        nss_SetError(NSS_ERROR_INVALID_POINTER);
        if ((PRStatus *)NULL != statusOpt) {
            *statusOpt = PR_FAILURE;
        }
        return 0;
    }
#endif /* NSSDEBUG */

    sv = PL_strlen((const char *)s) + 1;
#ifdef PEDANTIC
    if ('\0' != ((const char *)s)[sv - 1]) {
        /* wrapped */
        nss_SetError(NSS_ERROR_VALUE_TOO_LARGE);
        if ((PRStatus *)NULL != statusOpt) {
            *statusOpt = PR_FAILURE;
        }
        return 0;
    }
#endif /* PEDANTIC */

    if ((PRStatus *)NULL != statusOpt) {
        *statusOpt = PR_SUCCESS;
    }

    return sv;
}

/*
 * nssUTF8_Length
 *
 * This routine returns the length in characters (not including the
 * terminating null) of the UTF8-encoded string pointed to by the
 * specified NSSUTF8 pointer.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_POINTER
 *  NSS_ERROR_VALUE_TOO_LARGE
 *  NSS_ERROR_INVALID_STRING
 *
 * Return value:
 *  length of the string (which may be zero)
 *  0 on error
 */

NSS_IMPLEMENT PRUint32
nssUTF8_Length(const NSSUTF8 *s, PRStatus *statusOpt)
{
    PRUint32 l = 0;
    const PRUint8 *c = (const PRUint8 *)s;

#ifdef NSSDEBUG
    if ((const NSSUTF8 *)NULL == s) {
        nss_SetError(NSS_ERROR_INVALID_POINTER);
        goto loser;
    }
#endif /* NSSDEBUG */

    /*
     * From RFC 2044:
     *
     * UCS-4 range (hex.)           UTF-8 octet sequence (binary)
     * 0000 0000-0000 007F   0xxxxxxx
     * 0000 0080-0000 07FF   110xxxxx 10xxxxxx
     * 0000 0800-0000 FFFF   1110xxxx 10xxxxxx 10xxxxxx
     * 0001 0000-001F FFFF   11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
     * 0020 0000-03FF FFFF   111110xx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx
     * 0400 0000-7FFF FFFF   1111110x 10xxxxxx ... 10xxxxxx
     */

    while (0 != *c) {
        PRUint32 incr;
        if ((*c & 0x80) == 0) {
            incr = 1;
        } else if ((*c & 0xE0) == 0xC0) {
            incr = 2;
        } else if ((*c & 0xF0) == 0xE0) {
            incr = 3;
        } else if ((*c & 0xF8) == 0xF0) {
            incr = 4;
        } else if ((*c & 0xFC) == 0xF8) {
            incr = 5;
        } else if ((*c & 0xFE) == 0xFC) {
            incr = 6;
        } else {
            nss_SetError(NSS_ERROR_INVALID_STRING);
            goto loser;
        }

        l += incr;

#ifdef PEDANTIC
        if (l < incr) {
            /* Wrapped-- too big */
            nss_SetError(NSS_ERROR_VALUE_TOO_LARGE);
            goto loser;
        }

        {
            PRUint8 *d;
            for (d = &c[1]; d < &c[incr]; d++) {
                if ((*d & 0xC0) != 0xF0) {
                    nss_SetError(NSS_ERROR_INVALID_STRING);
                    goto loser;
                }
            }
        }
#endif /* PEDANTIC */

        c += incr;
    }

    if ((PRStatus *)NULL != statusOpt) {
        *statusOpt = PR_SUCCESS;
    }

    return l;

loser:
    if ((PRStatus *)NULL != statusOpt) {
        *statusOpt = PR_FAILURE;
    }

    return 0;
}

/*
 * nssUTF8_Create
 *
 * This routine creates a UTF8 string from a string in some other
 * format.  Some types of string may include embedded null characters,
 * so for them the length parameter must be used.  For string types
 * that are null-terminated, the length parameter is optional; if it
 * is zero, it will be ignored.  If the optional arena argument is
 * non-null, the memory used for the new string will be obtained from
 * that arena, otherwise it will be obtained from the heap.  This
 * routine may return NULL upon error, in which case it will have
 * placed an error on the error stack.
 *
 * The error may be one of the following:
 *  NSS_ERROR_INVALID_POINTER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_UNSUPPORTED_TYPE
 *
 * Return value:
 *  NULL upon error
 *  A non-null pointer to a new UTF8 string otherwise
 */

extern const NSSError NSS_ERROR_INTERNAL_ERROR; /* XXX fgmr */

NSS_IMPLEMENT NSSUTF8 *
nssUTF8_Create(NSSArena *arenaOpt, nssStringType type, const void *inputString,
               PRUint32 size /* in bytes, not characters */
               )
{
    NSSUTF8 *rv = NULL;

#ifdef NSSDEBUG
    if ((NSSArena *)NULL != arenaOpt) {
        if (PR_SUCCESS != nssArena_verifyPointer(arenaOpt)) {
            return (NSSUTF8 *)NULL;
        }
    }

    if ((const void *)NULL == inputString) {
        nss_SetError(NSS_ERROR_INVALID_POINTER);
        return (NSSUTF8 *)NULL;
    }
#endif /* NSSDEBUG */

    switch (type) {
        case nssStringType_DirectoryString:
            /* This is a composite type requiring BER */
            nss_SetError(NSS_ERROR_UNSUPPORTED_TYPE);
            break;
        case nssStringType_TeletexString:
            /*
             * draft-ietf-pkix-ipki-part1-11 says in part:
             *
             * In addition, many legacy implementations support names encoded
             * in the ISO 8859-1 character set (Latin1String) but tag them as
             * TeletexString.  The Latin1String includes characters used in
             * Western European countries which are not part of the
             * TeletexString charcter set.  Implementations that process
             * TeletexString SHOULD be prepared to handle the entire ISO
             * 8859-1 character set.[ISO 8859-1].
             */
            nss_SetError(NSS_ERROR_INTERNAL_ERROR); /* unimplemented */
            break;
        case nssStringType_PrintableString:
            /*
             * PrintableString consists of A-Za-z0-9 ,()+,-./:=?
             * This is a subset of ASCII, which is a subset of UTF8.
             * So we can just duplicate the string over.
             */

            if (0 == size) {
                rv = nssUTF8_Duplicate((const NSSUTF8 *)inputString, arenaOpt);
            } else {
                rv = nss_ZAlloc(arenaOpt, size + 1);
                if ((NSSUTF8 *)NULL == rv) {
                    return (NSSUTF8 *)NULL;
                }

                (void)nsslibc_memcpy(rv, inputString, size);
            }

            break;
        case nssStringType_UniversalString:
            /* 4-byte unicode */
            nss_SetError(NSS_ERROR_INTERNAL_ERROR); /* unimplemented */
            break;
        case nssStringType_BMPString:
            /* Base Multilingual Plane of Unicode */
            nss_SetError(NSS_ERROR_INTERNAL_ERROR); /* unimplemented */
            break;
        case nssStringType_UTF8String:
            if (0 == size) {
                rv = nssUTF8_Duplicate((const NSSUTF8 *)inputString, arenaOpt);
            } else {
                rv = nss_ZAlloc(arenaOpt, size + 1);
                if ((NSSUTF8 *)NULL == rv) {
                    return (NSSUTF8 *)NULL;
                }

                (void)nsslibc_memcpy(rv, inputString, size);
            }

            break;
        case nssStringType_PHGString:
            /*
             * PHGString is an IA5String (with case-insensitive comparisons).
             * IA5 is ~almost~ ascii; ascii has dollar-sign where IA5 has
             * currency symbol.
             */
            nss_SetError(NSS_ERROR_INTERNAL_ERROR); /* unimplemented */
            break;
        case nssStringType_GeneralString:
            nss_SetError(NSS_ERROR_INTERNAL_ERROR); /* unimplemented */
            break;
        default:
            nss_SetError(NSS_ERROR_UNSUPPORTED_TYPE);
            break;
    }

    return rv;
}

NSS_IMPLEMENT NSSItem *
nssUTF8_GetEncoding(NSSArena *arenaOpt, NSSItem *rvOpt, nssStringType type,
                    NSSUTF8 *string)
{
    NSSItem *rv = (NSSItem *)NULL;
    PRStatus status = PR_SUCCESS;

#ifdef NSSDEBUG
    if ((NSSArena *)NULL != arenaOpt) {
        if (PR_SUCCESS != nssArena_verifyPointer(arenaOpt)) {
            return (NSSItem *)NULL;
        }
    }

    if ((NSSUTF8 *)NULL == string) {
        nss_SetError(NSS_ERROR_INVALID_POINTER);
        return (NSSItem *)NULL;
    }
#endif /* NSSDEBUG */

    switch (type) {
        case nssStringType_DirectoryString:
            nss_SetError(NSS_ERROR_INTERNAL_ERROR); /* unimplemented */
            break;
        case nssStringType_TeletexString:
            nss_SetError(NSS_ERROR_INTERNAL_ERROR); /* unimplemented */
            break;
        case nssStringType_PrintableString:
            nss_SetError(NSS_ERROR_INTERNAL_ERROR); /* unimplemented */
            break;
        case nssStringType_UniversalString:
            nss_SetError(NSS_ERROR_INTERNAL_ERROR); /* unimplemented */
            break;
        case nssStringType_BMPString:
            nss_SetError(NSS_ERROR_INTERNAL_ERROR); /* unimplemented */
            break;
        case nssStringType_UTF8String: {
            NSSUTF8 *dup = nssUTF8_Duplicate(string, arenaOpt);
            if ((NSSUTF8 *)NULL == dup) {
                return (NSSItem *)NULL;
            }

            if ((NSSItem *)NULL == rvOpt) {
                rv = nss_ZNEW(arenaOpt, NSSItem);
                if ((NSSItem *)NULL == rv) {
                    (void)nss_ZFreeIf(dup);
                    return (NSSItem *)NULL;
                }
            } else {
                rv = rvOpt;
            }

            rv->data = dup;
            dup = (NSSUTF8 *)NULL;
            rv->size = nssUTF8_Size(rv->data, &status);
            if ((0 == rv->size) && (PR_SUCCESS != status)) {
                if ((NSSItem *)NULL == rvOpt) {
                    (void)nss_ZFreeIf(rv);
                }
                return (NSSItem *)NULL;
            }
        } break;
        case nssStringType_PHGString:
            nss_SetError(NSS_ERROR_INTERNAL_ERROR); /* unimplemented */
            break;
        default:
            nss_SetError(NSS_ERROR_UNSUPPORTED_TYPE);
            break;
    }

    return rv;
}

/*
 * nssUTF8_CopyIntoFixedBuffer
 *
 * This will copy a UTF8 string into a fixed-length buffer, making
 * sure that the all characters are valid.  Any remaining space will
 * be padded with the specified ASCII character, typically either
 * null or space.
 *
 * Blah, blah, blah.
 */

NSS_IMPLEMENT PRStatus
nssUTF8_CopyIntoFixedBuffer(NSSUTF8 *string, char *buffer, PRUint32 bufferSize,
                            char pad)
{
    PRUint32 stringSize = 0;

#ifdef NSSDEBUG
    if ((char *)NULL == buffer) {
        nss_SetError(NSS_ERROR_INVALID_POINTER);
        return PR_FALSE;
    }

    if (0 == bufferSize) {
        nss_SetError(NSS_ERROR_INVALID_ARGUMENT);
        return PR_FALSE;
    }

    if ((pad & 0x80) != 0x00) {
        nss_SetError(NSS_ERROR_INVALID_ARGUMENT);
        return PR_FALSE;
    }
#endif /* NSSDEBUG */

    if ((NSSUTF8 *)NULL == string) {
        string = (NSSUTF8 *)"";
    }

    stringSize = nssUTF8_Size(string, (PRStatus *)NULL);
    stringSize--; /* don't count the trailing null */
    if (stringSize > bufferSize) {
        PRUint32 bs = bufferSize;
        (void)nsslibc_memcpy(buffer, string, bufferSize);

        if ((((buffer[bs - 1] & 0x80) == 0x00)) ||
            ((bs > 1) && ((buffer[bs - 2] & 0xE0) == 0xC0)) ||
            ((bs > 2) && ((buffer[bs - 3] & 0xF0) == 0xE0)) ||
            ((bs > 3) && ((buffer[bs - 4] & 0xF8) == 0xF0)) ||
            ((bs > 4) && ((buffer[bs - 5] & 0xFC) == 0xF8)) ||
            ((bs > 5) && ((buffer[bs - 6] & 0xFE) == 0xFC))) {
            /* It fit exactly */
            return PR_SUCCESS;
        }

        /* Too long.  We have to trim the last character */
        for (/*bs*/; bs != 0; bs--) {
            if ((buffer[bs - 1] & 0xC0) != 0x80) {
                buffer[bs - 1] = pad;
                break;
            } else {
                buffer[bs - 1] = pad;
            }
        }
    } else {
        (void)nsslibc_memset(buffer, pad, bufferSize);
        (void)nsslibc_memcpy(buffer, string, stringSize);
    }

    return PR_SUCCESS;
}

/*
 * nssUTF8_Equal
 *
 */

NSS_IMPLEMENT PRBool
nssUTF8_Equal(const NSSUTF8 *a, const NSSUTF8 *b, PRStatus *statusOpt)
{
    PRUint32 la, lb;

#ifdef NSSDEBUG
    if (((const NSSUTF8 *)NULL == a) || ((const NSSUTF8 *)NULL == b)) {
        nss_SetError(NSS_ERROR_INVALID_POINTER);
        if ((PRStatus *)NULL != statusOpt) {
            *statusOpt = PR_FAILURE;
        }
        return PR_FALSE;
    }
#endif /* NSSDEBUG */

    la = nssUTF8_Size(a, statusOpt);
    if (0 == la) {
        return PR_FALSE;
    }

    lb = nssUTF8_Size(b, statusOpt);
    if (0 == lb) {
        return PR_FALSE;
    }

    if (la != lb) {
        return PR_FALSE;
    }

    return nsslibc_memequal(a, b, la, statusOpt);
}
