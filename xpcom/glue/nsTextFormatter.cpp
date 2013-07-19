/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* 
 * Portable safe sprintf code.
 *
 * Code based on mozilla/nsprpub/src/io/prprf.c rev 3.7 
 *
 * Contributor(s):
 *   Kipp E.B. Hickman  <kipp@netscape.com>  (original author)
 *   Frank Yung-Fong Tang  <ftang@netscape.com>
 *   Daniele Nicolodi  <daniele@grinta.net>
 */

/*
 * Copied from xpcom/ds/nsTextFormatter.cpp r1.22
 *     Changed to use nsMemory and Frozen linkage
 *                  -- Prasad <prasad@medhas.org>
 */

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "prdtoa.h"
#include "prlog.h"
#include "prprf.h"
#include "prmem.h"
#include "nsCRTGlue.h"
#include "nsTextFormatter.h"
#include "nsMemory.h"

/*
** Note: on some platforms va_list is defined as an array,
** and requires array notation.
*/

#ifdef HAVE_VA_COPY
#define VARARGS_ASSIGN(foo, bar)        VA_COPY(foo,bar)
#elif defined(HAVE_VA_LIST_AS_ARRAY)
#define VARARGS_ASSIGN(foo, bar)	foo[0] = bar[0]
#else
#define VARARGS_ASSIGN(foo, bar)	(foo) = (bar)
#endif

typedef struct SprintfStateStr SprintfState;

struct SprintfStateStr {
    int (*stuff)(SprintfState *ss, const PRUnichar *sp, uint32_t len);

    PRUnichar *base;
    PRUnichar *cur;
    uint32_t maxlen;

    void *stuffclosure;
};

/*
** Numbered Arguement State
*/
struct NumArgState{
    int	    type;		/* type of the current ap                    */
    va_list ap;			/* point to the corresponding position on ap */
};

#define NAS_DEFAULT_NUM 20  /* default number of NumberedArgumentState array */

#define TYPE_INT16	0
#define TYPE_UINT16	1
#define TYPE_INTN	2
#define TYPE_UINTN	3
#define TYPE_INT32	4
#define TYPE_UINT32	5
#define TYPE_INT64	6
#define TYPE_UINT64	7
#define TYPE_STRING	8
#define TYPE_DOUBLE	9
#define TYPE_INTSTR	10
#define TYPE_UNISTRING	11
#define TYPE_UNKNOWN	20

#define _LEFT		0x1
#define _SIGNED		0x2
#define _SPACED		0x4
#define _ZEROS		0x8
#define _NEG		0x10

#define ELEMENTS_OF(array_) (sizeof(array_) / sizeof(array_[0]))

/*
** Fill into the buffer using the data in src
*/
static int fill2(SprintfState *ss, const PRUnichar *src, int srclen, 
                 int width, int flags)
{
    PRUnichar space = ' ';
    int rv;
    
    width -= srclen;
    /* Right adjusting */
    if ((width > 0) && ((flags & _LEFT) == 0)) {
	if (flags & _ZEROS) {
	    space = '0';
	}
	while (--width >= 0) {
	    rv = (*ss->stuff)(ss, &space, 1);
	    if (rv < 0) {
		return rv;
	    }
	}
    }

    /* Copy out the source data */
    rv = (*ss->stuff)(ss, src, srclen);
    if (rv < 0) {
	return rv;
    }
    
    /* Left adjusting */
    if ((width > 0) && ((flags & _LEFT) != 0)) {
	while (--width >= 0) {
	    rv = (*ss->stuff)(ss, &space, 1);
	    if (rv < 0) {
		return rv;
	    }
	}
    }
    return 0;
}

/*
** Fill a number. The order is: optional-sign zero-filling conversion-digits
*/
static int fill_n(SprintfState *ss, const PRUnichar *src, int srclen, 
                  int width, int prec, int type, int flags)
{
    int zerowidth   = 0;
    int precwidth   = 0;
    int signwidth   = 0;
    int leftspaces  = 0;
    int rightspaces = 0;
    int cvtwidth;
    int rv;
    PRUnichar sign;
    PRUnichar space = ' ';
    PRUnichar zero = '0';

    if ((type & 1) == 0) {
	if (flags & _NEG) {
	    sign = '-';
	    signwidth = 1;
	} else if (flags & _SIGNED) {
	    sign = '+';
	    signwidth = 1;
	} else if (flags & _SPACED) {
	    sign = ' ';
	    signwidth = 1;
	}
    }
    cvtwidth = signwidth + srclen;

    if (prec > 0) {
	if (prec > srclen) {
            /* Need zero filling */
	    precwidth = prec - srclen;
	    cvtwidth += precwidth;
	}
    }

    if ((flags & _ZEROS) && (prec < 0)) {
	if (width > cvtwidth) {
            /* Zero filling */
	    zerowidth = width - cvtwidth;
	    cvtwidth += zerowidth;
	}
    }

    if (flags & _LEFT) {
	if (width > cvtwidth) {
	    /* Space filling on the right (i.e. left adjusting) */
	    rightspaces = width - cvtwidth;
	}
    } else {
	if (width > cvtwidth) {
	    /* Space filling on the left (i.e. right adjusting) */
	    leftspaces = width - cvtwidth;
	}
    }
    while (--leftspaces >= 0) {
	rv = (*ss->stuff)(ss, &space, 1);
	if (rv < 0) {
	    return rv;
	}
    }
    if (signwidth) {
	rv = (*ss->stuff)(ss, &sign, 1);
	if (rv < 0) {
	    return rv;
	}
    }
    while (--precwidth >= 0) {
	rv = (*ss->stuff)(ss,  &space, 1);
	if (rv < 0) {
	    return rv;
	}
    }
    while (--zerowidth >= 0) {
	rv = (*ss->stuff)(ss,  &zero, 1);
	if (rv < 0) {
	    return rv;
	}
    }
    rv = (*ss->stuff)(ss, src, srclen);
    if (rv < 0) {
	return rv;
    }
    while (--rightspaces >= 0) {
	rv = (*ss->stuff)(ss,  &space, 1);
	if (rv < 0) {
	    return rv;
	}
    }
    return 0;
}

/*
** Convert a long into its printable form
*/
static int cvt_l(SprintfState *ss, long num, int width, int prec,
                 int radix, int type, int flags, const PRUnichar *hexp)
{
    PRUnichar cvtbuf[100];
    PRUnichar *cvt;
    int digits;

    /* according to the man page this needs to happen */
    if ((prec == 0) && (num == 0)) {
	return 0;
    }

    /*
    ** Converting decimal is a little tricky. In the unsigned case we
    ** need to stop when we hit 10 digits. In the signed case, we can
    ** stop when the number is zero.
    */
    cvt = &cvtbuf[0] + ELEMENTS_OF(cvtbuf);
    digits = 0;
    while (num) {
	int digit = (((unsigned long)num) % radix) & 0xF;
	*--cvt = hexp[digit];
	digits++;
	num = (long)(((unsigned long)num) / radix);
    }
    if (digits == 0) {
	*--cvt = '0';
	digits++;
    }

    /*
    ** Now that we have the number converted without its sign, deal with
    ** the sign and zero padding.
    */
    return fill_n(ss, cvt, digits, width, prec, type, flags);
}

/*
** Convert a 64-bit integer into its printable form
*/
static int cvt_ll(SprintfState *ss, int64_t num, int width, int prec,
                  int radix, int type, int flags, const PRUnichar *hexp)
{
    PRUnichar cvtbuf[100];
    PRUnichar *cvt;
    int digits;
    int64_t rad;

    /* according to the man page this needs to happen */
    if (prec == 0 && num == 0) {
	return 0;
    }

    /*
    ** Converting decimal is a little tricky. In the unsigned case we
    ** need to stop when we hit 10 digits. In the signed case, we can
    ** stop when the number is zero.
    */
    rad = radix;
    cvt = &cvtbuf[0] + ELEMENTS_OF(cvtbuf);
    digits = 0;
    while (num != 0) {
	*--cvt = hexp[int32_t(num % rad) & 0xf];
	digits++;
	num /= rad;
    }
    if (digits == 0) {
	*--cvt = '0';
	digits++;
    }

    /*
    ** Now that we have the number converted without its sign, deal with
    ** the sign and zero padding.
    */
    return fill_n(ss, cvt, digits, width, prec, type, flags);
}

/*
** Convert a double precision floating point number into its printable
** form.
*/
static int cvt_f(SprintfState *ss, double d, int width, int prec, 
                 const PRUnichar type, int flags)
{
    int    mode = 2;
    int    decpt;
    int    sign;
    char   buf[256];
    char * bufp = buf;
    int    bufsz = 256;
    char   num[256];
    char * nump;
    char * endnum;
    int    numdigits = 0;
    char   exp = 'e';

    if (prec == -1) {
        prec = 6;
    } else if (prec > 50) {
        // limit precision to avoid PR_dtoa bug 108335
        // and to prevent buffers overflows
        prec = 50;
    }

    switch (type) {
    case 'f':  
        numdigits = prec;
        mode = 3;
        break;
    case 'E':
        exp = 'E';
        // no break
    case 'e':
        numdigits = prec + 1;
        mode = 2;
        break;
    case 'G':
        exp = 'E';
        // no break
    case 'g':
        if (prec == 0) {
            prec = 1;
        }
        numdigits = prec;
        mode = 2;
        break;
    default:
        NS_ERROR("invalid type passed to cvt_f");
    }
    
    if (PR_dtoa(d, mode, numdigits, &decpt, &sign, &endnum, num, bufsz) == PR_FAILURE) {
        buf[0] = '\0';
        return -1;
    }
    numdigits = endnum - num;
    nump = num;

    if (sign) {
        *bufp++ = '-';
    } else if (flags & _SIGNED) {
        *bufp++ = '+';
    }

    if (decpt == 9999) {
        while ((*bufp++ = *nump++)) { }
    } else {

        switch (type) {

        case 'E':
        case 'e':

            *bufp++ = *nump++;                
            if (prec > 0) {
                *bufp++ = '.';
                while (*nump) {
                    *bufp++ = *nump++;
                    prec--;
                }
                while (prec-- > 0) {
                    *bufp++ = '0';
                }
            }
            *bufp++ = exp;
            PR_snprintf(bufp, bufsz - (bufp - buf), "%+03d", decpt-1);
            break;

        case 'f':

            if (decpt < 1) {
                *bufp++ = '0';
                if (prec > 0) {
                    *bufp++ = '.';
                    while (decpt++ && prec-- > 0) {
                        *bufp++ = '0';
                    }
                    while (*nump && prec-- > 0) {
                        *bufp++ = *nump++;
                    }
                    while (prec-- > 0) {
                        *bufp++ = '0';
                    }
                }
            } else {
                while (*nump && decpt-- > 0) {
                    *bufp++ = *nump++;
                }
                while (decpt-- > 0) {
                    *bufp++ = '0';
                }
                if (prec > 0) {
                    *bufp++ = '.';
                    while (*nump && prec-- > 0) {
                        *bufp++ = *nump++;
                    }
                    while (prec-- > 0) {
                        *bufp++ = '0';
                    }
                }
            }
            *bufp = '\0';
            break;

        case 'G':
        case 'g':

            if ((decpt < -3) || ((decpt - 1) >= prec)) {
                *bufp++ = *nump++;
                numdigits--;
                if (numdigits > 0) {
                    *bufp++ = '.';
                    while (*nump) {
                        *bufp++ = *nump++;
                    }
                }
                *bufp++ = exp;
                PR_snprintf(bufp, bufsz - (bufp - buf), "%+03d", decpt-1);
            } else {
                if (decpt < 1) {
                    *bufp++ = '0';
                    if (prec > 0) {
                        *bufp++ = '.';
                        while (decpt++) {
                            *bufp++ = '0';
                        }
                        while (*nump) {
                            *bufp++ = *nump++;
                        }
                    }
                } else {
                    while (*nump && decpt-- > 0) {
                        *bufp++ = *nump++;
                        numdigits--;
                    }
                    while (decpt-- > 0) {
                        *bufp++ = '0';
                    }
                    if (numdigits > 0) {
                        *bufp++ = '.';
                        while (*nump) {
                            *bufp++ = *nump++;
                        }
                    }
                }
                *bufp = '\0';
            }
        }
    }

    PRUnichar rbuf[256];
    PRUnichar *rbufp = rbuf;
    bufp = buf;
    // cast to PRUnichar
    while ((*rbufp++ = *bufp++)) { }
    *rbufp = '\0';

    return fill2(ss, rbuf, NS_strlen(rbuf), width, flags);
}

/*
** Convert a string into its printable form. "width" is the output
** width. "prec" is the maximum number of characters of "s" to output,
** where -1 means until NUL.
*/
static int cvt_S(SprintfState *ss, const PRUnichar *s, int width,
                 int prec, int flags)
{
    int slen;

    if (prec == 0) {
	return 0;
    }

    /* Limit string length by precision value */
    slen = s ? NS_strlen(s) : 6;
    if (prec > 0) {
	if (prec < slen) {
	    slen = prec;
	}
    }

    /* and away we go */
    NS_NAMED_LITERAL_STRING(nullstr, "(null)");

    return fill2(ss, s ? s : nullstr.get(), slen, width, flags);
}

/*
** Convert a string into its printable form.  "width" is the output
** width. "prec" is the maximum number of characters of "s" to output,
** where -1 means until NUL.
*/
static int cvt_s(SprintfState *ss, const char *s, int width,
                 int prec, int flags)
{
    NS_ConvertUTF8toUTF16 utf16Val(s);
    return cvt_S(ss, utf16Val.get(), width, prec, flags);
}

/*
** BuildArgArray stands for Numbered Argument list Sprintf
** for example,  
**	fmp = "%4$i, %2$d, %3s, %1d";
** the number must start from 1, and no gap among them
*/

static struct NumArgState* BuildArgArray(const PRUnichar *fmt, 
                                         va_list ap, int * rv, 
                                         struct NumArgState * nasArray)
{
    int number = 0, cn = 0, i;
    const PRUnichar* p;
    PRUnichar  c;
    struct NumArgState* nas;

    /*
    **	first pass:
    **	detemine how many legal % I have got, then allocate space
    */
    p = fmt;
    *rv = 0;
    i = 0;
    while ((c = *p++) != 0) {
	if (c != '%') {
	    continue;
        }
        /* skip %% case */
	if ((c = *p++) == '%') {
	    continue;
        }

	while( c != 0 ){
	    if (c > '9' || c < '0') {
                /* numbered argument csae */
		if (c == '$') {
		    if (i > 0) {
			*rv = -1;
			return NULL;
		    }
		    number++;
		    break;

		} else {
                    /* non-numbered argument case */
		    if (number > 0) {
			*rv = -1;
			return NULL;
		    }
		    i = 1;
		    break;
		}
	    }
	    c = *p++;
	}
    }

    if (number == 0) {
	return NULL;
    }
    
    if (number > NAS_DEFAULT_NUM) {
	nas = (struct NumArgState*)nsMemory::Alloc(number * sizeof(struct NumArgState));
	if (!nas) {
	    *rv = -1;
	    return NULL;
	}
    } else {
	nas = nasArray;
    }

    for (i = 0; i < number; i++) {
	nas[i].type = TYPE_UNKNOWN;
    }

    /*
    ** second pass:
    ** set nas[].type
    */
    p = fmt;
    while ((c = *p++) != 0) {
    	if (c != '%') {
            continue;
        }
        c = *p++;
	if (c == '%') {
            continue;
        }
	cn = 0;
        /* should imporve error check later */
	while (c && c != '$') {
	    cn = cn*10 + c - '0';
	    c = *p++;
	}

	if (!c || cn < 1 || cn > number) {
	    *rv = -1;
	    break;
        }

	/* nas[cn] starts from 0, and make sure 
           nas[cn].type is not assigned */
        cn--;
	if (nas[cn].type != TYPE_UNKNOWN) {
	    continue;
        }

        c = *p++;

        /* width */
        if (c == '*') {
	    /* not supported feature, for the argument is not numbered */
	    *rv = -1;
	    break;
	} else {
	    while ((c >= '0') && (c <= '9')) {
	        c = *p++;
	    }
	}

	/* precision */
	if (c == '.') {
	    c = *p++;
	    if (c == '*') {
	        /* not supported feature, for the argument is not numbered */
	        *rv = -1;
	        break;
	    } else {
	        while ((c >= '0') && (c <= '9')) {
		    c = *p++;
		}
	    }
	}

	/* size */
	nas[cn].type = TYPE_INTN;
	if (c == 'h') {
	    nas[cn].type = TYPE_INT16;
	    c = *p++;
	} else if (c == 'L') {
	    /* XXX not quite sure here */
	    nas[cn].type = TYPE_INT64;
	    c = *p++;
	} else if (c == 'l') {
	    nas[cn].type = TYPE_INT32;
	    c = *p++;
	    if (c == 'l') {
	        nas[cn].type = TYPE_INT64;
	        c = *p++;
	    }
	}

	/* format */
	switch (c) {
	case 'd':
	case 'c':
	case 'i':
	case 'o':
	case 'u':
	case 'x':
	case 'X':
	    break;

	case 'e':
	case 'f':
	case 'g':
	    nas[cn].type = TYPE_DOUBLE;
	    break;

	case 'p':
	    /* XXX should use cpp */
	    if (sizeof(void *) == sizeof(int32_t)) {
		nas[cn].type = TYPE_UINT32;
	    } else if (sizeof(void *) == sizeof(int64_t)) {
	        nas[cn].type = TYPE_UINT64;
	    } else if (sizeof(void *) == sizeof(int)) {
	        nas[cn].type = TYPE_UINTN;
	    } else {
	        nas[cn].type = TYPE_UNKNOWN;
	    }
	    break;

	case 'C':
	    /* XXX not supported I suppose */
	    PR_ASSERT(0);
	    nas[cn].type = TYPE_UNKNOWN;
	    break;

	case 'S':
	    nas[cn].type = TYPE_UNISTRING;
	    break;

	case 's':
	    nas[cn].type = TYPE_STRING;
	    break;

	case 'n':
	    nas[cn].type = TYPE_INTSTR;
	    break;

	default:
	    PR_ASSERT(0);
	    nas[cn].type = TYPE_UNKNOWN;
	    break;
	}

	/* get a legal para. */
	if (nas[cn].type == TYPE_UNKNOWN) {
	    *rv = -1;
	    break;
	}
    }


    /*
    ** third pass
    ** fill the nas[cn].ap
    */
    if (*rv < 0) {
	if( nas != nasArray ) {
	    PR_DELETE(nas);
        }
	return NULL;
    }

    cn = 0;
    while (cn < number) {
	if (nas[cn].type == TYPE_UNKNOWN) {
	    cn++;
	    continue;
	}

	VARARGS_ASSIGN(nas[cn].ap, ap);

	switch (nas[cn].type) {
	case TYPE_INT16:
	case TYPE_UINT16:
	case TYPE_INTN:
	case TYPE_UINTN:     (void)va_arg(ap, int);         break;

	case TYPE_INT32:     (void)va_arg(ap, int32_t);     break;

	case TYPE_UINT32:    (void)va_arg(ap, uint32_t);    break;

	case TYPE_INT64:     (void)va_arg(ap, int64_t);     break;

	case TYPE_UINT64:    (void)va_arg(ap, uint64_t);    break;

	case TYPE_STRING:    (void)va_arg(ap, char*);       break;

	case TYPE_INTSTR:    (void)va_arg(ap, int*);        break;

	case TYPE_DOUBLE:    (void)va_arg(ap, double);      break;

	case TYPE_UNISTRING: (void)va_arg(ap, PRUnichar*);  break;

	default:
	    if( nas != nasArray ) {
		PR_DELETE( nas );
            }
	    *rv = -1;
	    return NULL;
	}
	cn++;
    }
    return nas;
}

/*
** The workhorse sprintf code.
*/
static int dosprintf(SprintfState *ss, const PRUnichar *fmt, va_list ap)
{
    PRUnichar c;
    int flags, width, prec, radix, type;
    union {
	PRUnichar ch;
	int i;
	long l;
	int64_t ll;
	double d;
	const char *s;
	const PRUnichar *S;
	int *ip;
    } u;
    PRUnichar space = ' ';

    nsAutoString hex;
    hex.AssignLiteral("0123456789abcdef");

    nsAutoString HEX;
    HEX.AssignLiteral("0123456789ABCDEF");

    const PRUnichar *hexp;
    int rv, i;
    struct NumArgState* nas = NULL;
    struct NumArgState  nasArray[NAS_DEFAULT_NUM];


    /*
    ** build an argument array, IF the fmt is numbered argument
    ** list style, to contain the Numbered Argument list pointers
    */
    nas = BuildArgArray (fmt, ap, &rv, nasArray);
    if (rv < 0) {
	/* the fmt contains error Numbered Argument format, jliu@netscape.com */
	PR_ASSERT(0);
	return rv;
    }

    while ((c = *fmt++) != 0) {
	if (c != '%') {
	    rv = (*ss->stuff)(ss, fmt - 1, 1);
	    if (rv < 0) {
		return rv;
	    }
	    continue;
	}

	/*
	** Gobble up the % format string. Hopefully we have handled all
	** of the strange cases!
	*/
	flags = 0;
	c = *fmt++;
	if (c == '%') {
	    /* quoting a % with %% */
	    rv = (*ss->stuff)(ss, fmt - 1, 1);
	    if (rv < 0) {
		return rv;
	    }
	    continue;
	}

	if (nas != NULL) {
	    /* the fmt contains the Numbered Arguments feature */
	    i = 0;
	    /* should imporve error check later */
	    while (c && c != '$') {
		i = (i * 10) + (c - '0');
		c = *fmt++;
	    }

	    if (nas[i-1].type == TYPE_UNKNOWN) {
		if (nas && (nas != nasArray)) {
		    PR_DELETE(nas);
                }
		return -1;
	    }

	    VARARGS_ASSIGN(ap, nas[i-1].ap);
	    c = *fmt++;
	}

	/*
	 * Examine optional flags.  Note that we do not implement the
	 * '#' flag of sprintf().  The ANSI C spec. of the '#' flag is
	 * somewhat ambiguous and not ideal, which is perhaps why
	 * the various sprintf() implementations are inconsistent
	 * on this feature.
	 */
	while ((c == '-') || (c == '+') || (c == ' ') || (c == '0')) {
	    if (c == '-') flags |= _LEFT;
	    if (c == '+') flags |= _SIGNED;
	    if (c == ' ') flags |= _SPACED;
	    if (c == '0') flags |= _ZEROS;
	    c = *fmt++;
	}
	if (flags & _SIGNED) flags &= ~_SPACED;
	if (flags & _LEFT) flags &= ~_ZEROS;

	/* width */
	if (c == '*') {
	    c = *fmt++;
	    width = va_arg(ap, int);
	} else {
	    width = 0;
	    while ((c >= '0') && (c <= '9')) {
		width = (width * 10) + (c - '0');
		c = *fmt++;
	    }
	}

	/* precision */
	prec = -1;
	if (c == '.') {
	    c = *fmt++;
	    if (c == '*') {
		c = *fmt++;
		prec = va_arg(ap, int);
	    } else {
		prec = 0;
		while ((c >= '0') && (c <= '9')) {
		    prec = (prec * 10) + (c - '0');
		    c = *fmt++;
		}
	    }
	}

	/* size */
	type = TYPE_INTN;
	if (c == 'h') {
	    type = TYPE_INT16;
	    c = *fmt++;
	} else if (c == 'L') {
	    /* XXX not quite sure here */
	    type = TYPE_INT64;
	    c = *fmt++;
	} else if (c == 'l') {
	    type = TYPE_INT32;
	    c = *fmt++;
	    if (c == 'l') {
		type = TYPE_INT64;
		c = *fmt++;
	    }
	}

	/* format */
	hexp = hex.get();
	switch (c) {
        case 'd': 
        case 'i':                               /* decimal/integer */
	    radix = 10;
	    goto fetch_and_convert;

        case 'o':                               /* octal */
	    radix = 8;
	    type |= 1;
	    goto fetch_and_convert;

        case 'u':                               /* unsigned decimal */
	    radix = 10;
	    type |= 1;
	    goto fetch_and_convert;

        case 'x':                               /* unsigned hex */
	    radix = 16;
	    type |= 1;
	    goto fetch_and_convert;

        case 'X':                               /* unsigned HEX */
	    radix = 16;
	    hexp = HEX.get();
	    type |= 1;
	    goto fetch_and_convert;

        fetch_and_convert:
	    switch (type) {
            case TYPE_INT16:
		u.l = va_arg(ap, int);
		if (u.l < 0) {
		    u.l = -u.l;
		    flags |= _NEG;
		}
		goto do_long;
            case TYPE_UINT16:
		u.l = va_arg(ap, int) & 0xffff;
		goto do_long;
            case TYPE_INTN:
		u.l = va_arg(ap, int);
		if (u.l < 0) {
		    u.l = -u.l;
		    flags |= _NEG;
		}
		goto do_long;
            case TYPE_UINTN:
		u.l = (long)va_arg(ap, unsigned int);
		goto do_long;

            case TYPE_INT32:
		u.l = va_arg(ap, int32_t);
		if (u.l < 0) {
		    u.l = -u.l;
		    flags |= _NEG;
		}
		goto do_long;
            case TYPE_UINT32:
		u.l = (long)va_arg(ap, uint32_t);
            do_long:
		rv = cvt_l(ss, u.l, width, prec, radix, type, flags, hexp);
		if (rv < 0) {
		    return rv;
		}
		break;

            case TYPE_INT64:
		u.ll = va_arg(ap, int64_t);
		if (u.ll < 0) {
		    u.ll = -u.ll;
		    flags |= _NEG;
		}
		goto do_longlong;
            case TYPE_UINT64:
		u.ll = va_arg(ap, uint64_t);
            do_longlong:
		rv = cvt_ll(ss, u.ll, width, prec, radix, type, flags, hexp);
		if (rv < 0) {
		    return rv;
		}
		break;
	    }
	    break;

        case 'e':
        case 'E':
        case 'f':
        case 'g':
        case 'G':
	    u.d = va_arg(ap, double);
            rv = cvt_f(ss, u.d, width, prec, c, flags);
	    if (rv < 0) {
		return rv;
	    }
	    break;

        case 'c':
	    u.ch = va_arg(ap, int);
            if ((flags & _LEFT) == 0) {
                while (width-- > 1) {
                    rv = (*ss->stuff)(ss, &space, 1);
                    if (rv < 0) {
                        return rv;
                    }
                }
            }
	    rv = (*ss->stuff)(ss, &u.ch, 1);
	    if (rv < 0) {
		return rv;
	    }
            if (flags & _LEFT) {
                while (width-- > 1) {
                    rv = (*ss->stuff)(ss, &space, 1);
                    if (rv < 0) {
                        return rv;
                    }
                }
            }
	    break;

        case 'p':
	    if (sizeof(void *) == sizeof(int32_t)) {
	    	type = TYPE_UINT32;
	    } else if (sizeof(void *) == sizeof(int64_t)) {
	    	type = TYPE_UINT64;
	    } else if (sizeof(void *) == sizeof(int)) {
		type = TYPE_UINTN;
	    } else {
		PR_ASSERT(0);
		break;
	    }
	    radix = 16;
	    goto fetch_and_convert;

#if 0
        case 'C':
	    /* XXX not supported I suppose */
	    PR_ASSERT(0);
	    break;
#endif

        case 'S':
	    u.S = va_arg(ap, const PRUnichar*);
	    rv = cvt_S(ss, u.S, width, prec, flags);
	    if (rv < 0) {
		return rv;
	    }
	    break;

        case 's':
	    u.s = va_arg(ap, const char*);
	    rv = cvt_s(ss, u.s, width, prec, flags);
	    if (rv < 0) {
		return rv;
	    }
	    break;

        case 'n':
	    u.ip = va_arg(ap, int*);
	    if (u.ip) {
		*u.ip = ss->cur - ss->base;
	    }
	    break;

        default:
	    /* Not a % token after all... skip it */
#if 0
	    PR_ASSERT(0);
#endif
            PRUnichar perct = '%'; 
	    rv = (*ss->stuff)(ss, &perct, 1);
	    if (rv < 0) {
		return rv;
	    }
	    rv = (*ss->stuff)(ss, fmt - 1, 1);
	    if (rv < 0) {
		return rv;
	    }
	}
    }

    /* Stuff trailing NUL */
    PRUnichar null = '\0';

    rv = (*ss->stuff)(ss, &null, 1);

    if( nas && ( nas != nasArray ) ){
	PR_DELETE( nas );
    }

    return rv;
}

/************************************************************************/

static int
StringStuff(SprintfState* ss, const PRUnichar* sp, uint32_t len)
{
    if (*sp == '\0')
      return 0;

    ptrdiff_t off = ss->cur - ss->base;
    
    nsAString* str = static_cast<nsAString*>(ss->stuffclosure);
    str->Append(sp, len);

    ss->base = str->BeginWriting();
    ss->cur = ss->base + off;

    return 0;
}

/*
** Stuff routine that automatically grows the malloc'd output buffer
** before it overflows.
*/
static int GrowStuff(SprintfState *ss, const PRUnichar *sp, uint32_t len)
{
    ptrdiff_t off;
    PRUnichar *newbase;
    uint32_t newlen;

    off = ss->cur - ss->base;
    if (off + len >= ss->maxlen) {
	/* Grow the buffer */
	newlen = ss->maxlen + ((len > 32) ? len : 32);
	if (ss->base) {
	    newbase = (PRUnichar*) nsMemory::Realloc(ss->base, newlen*sizeof(PRUnichar));
	} else {
	    newbase = (PRUnichar*) nsMemory::Alloc(newlen*sizeof(PRUnichar));
	}
	if (!newbase) {
	    /* Ran out of memory */
	    return -1;
	}
	ss->base = newbase;
	ss->maxlen = newlen;
	ss->cur = ss->base + off;
    }

    /* Copy data */
    while (len) {
	--len;
	*ss->cur++ = *sp++;
    }
    PR_ASSERT((uint32_t)(ss->cur - ss->base) <= ss->maxlen);
    return 0;
}

/*
** sprintf into a malloc'd buffer
*/
PRUnichar * nsTextFormatter::smprintf(const PRUnichar *fmt, ...)
{
    va_list ap;
    PRUnichar *rv;

    va_start(ap, fmt);
    rv = nsTextFormatter::vsmprintf(fmt, ap);
    va_end(ap);
    return rv;
}

uint32_t nsTextFormatter::ssprintf(nsAString& out, const PRUnichar* fmt, ...)
{
    va_list ap;
    uint32_t rv;

    va_start(ap, fmt);
    rv = nsTextFormatter::vssprintf(out, fmt, ap);
    va_end(ap);
    return rv;
}

uint32_t nsTextFormatter::vssprintf(nsAString& out, const PRUnichar* fmt, va_list ap)
{
    SprintfState ss;
    ss.stuff = StringStuff;
    ss.base = 0;
    ss.cur = 0;
    ss.maxlen = 0;
    ss.stuffclosure = &out;

    out.Truncate();
    int n = dosprintf(&ss, fmt, ap);
    return n ? n - 1 : n;
}

PRUnichar * nsTextFormatter::vsmprintf(const PRUnichar *fmt, va_list ap)
{
    SprintfState ss;
    int rv;

    ss.stuff = GrowStuff;
    ss.base = 0;
    ss.cur = 0;
    ss.maxlen = 0;
    rv = dosprintf(&ss, fmt, ap);
    if (rv < 0) {
	if (ss.base) {
	    PR_DELETE(ss.base);
	}
	return 0;
    }
    return ss.base;
}

/*
** Stuff routine that discards overflow data
*/
static int LimitStuff(SprintfState *ss, const PRUnichar *sp, uint32_t len)
{
    uint32_t limit = ss->maxlen - (ss->cur - ss->base);

    if (len > limit) {
	len = limit;
    }
    while (len) {
	--len;
	*ss->cur++ = *sp++;
    }
    return 0;
}

/*
** sprintf into a fixed size buffer. Make sure there is a NUL at the end
** when finished.
*/
uint32_t nsTextFormatter::snprintf(PRUnichar *out, uint32_t outlen, const PRUnichar *fmt, ...)
{
    va_list ap;
    uint32_t rv;

    PR_ASSERT((int32_t)outlen > 0);
    if ((int32_t)outlen <= 0) {
	return 0;
    }

    va_start(ap, fmt);
    rv = nsTextFormatter::vsnprintf(out, outlen, fmt, ap);
    va_end(ap);
    return rv;
}

uint32_t nsTextFormatter::vsnprintf(PRUnichar *out, uint32_t outlen,const PRUnichar *fmt,
                                    va_list ap)
{
    SprintfState ss;
    uint32_t n;

    PR_ASSERT((int32_t)outlen > 0);
    if ((int32_t)outlen <= 0) {
	return 0;
    }

    ss.stuff = LimitStuff;
    ss.base = out;
    ss.cur = out;
    ss.maxlen = outlen;
    (void) dosprintf(&ss, fmt, ap);

    /* If we added chars, and we didn't append a null, do it now. */
    if( (ss.cur != ss.base) && (*(ss.cur - 1) != '\0') )
        *(--ss.cur) = '\0';

    n = ss.cur - ss.base;
    return n ? n - 1 : n;
}

/*
 * Free memory allocated, for the caller, by smprintf
 */
void nsTextFormatter::smprintf_free(PRUnichar *mem)
{
    nsMemory::Free(mem);
}

