/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
** Portable safe sprintf code.
**
** Author: Kipp E.B. Hickman
*/
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "primpl.h"
#include "prprf.h"
#include "prlong.h"
#include "prlog.h"
#include "prmem.h"

/*
** WARNING: This code may *NOT* call PR_LOG (because PR_LOG calls it)
*/

/*
** XXX This needs to be internationalized!
*/

typedef struct SprintfStateStr SprintfState;

struct SprintfStateStr {
    int (*stuff)(SprintfState *ss, const char *sp, PRUint32 len);

    char *base;
    char *cur;
    PRUint32 maxlen;

    int (*func)(void *arg, const char *sp, PRUint32 len);
    void *arg;
};

/*
** Numbered Argument
*/
struct NumArg {
    int type;           /* type of the numbered argument    */
    union {             /* the numbered argument            */
	int i;
	unsigned int ui;
	PRInt32 i32;
	PRUint32 ui32;
	PRInt64 ll;
	PRUint64 ull;
	double d;
	const char *s;
	int *ip;
#ifdef WIN32
	const WCHAR *ws;
#endif
    } u;
};

#define NAS_DEFAULT_NUM 20  /* default number of NumberedArgument array */


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
#ifdef WIN32
#define TYPE_WSTRING	11
#endif
#define TYPE_UNKNOWN	20

#define FLAG_LEFT	0x1
#define FLAG_SIGNED	0x2
#define FLAG_SPACED	0x4
#define FLAG_ZEROS	0x8
#define FLAG_NEG	0x10

/*
** Fill into the buffer using the data in src
*/
static int fill2(SprintfState *ss, const char *src, int srclen, int width,
		int flags)
{
    char space = ' ';
    int rv;

    width -= srclen;
    if ((width > 0) && ((flags & FLAG_LEFT) == 0)) {	/* Right adjusting */
	if (flags & FLAG_ZEROS) {
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

    if ((width > 0) && ((flags & FLAG_LEFT) != 0)) {	/* Left adjusting */
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
static int fill_n(SprintfState *ss, const char *src, int srclen, int width,
		  int prec, int type, int flags)
{
    int zerowidth = 0;
    int precwidth = 0;
    int signwidth = 0;
    int leftspaces = 0;
    int rightspaces = 0;
    int cvtwidth;
    int rv;
    char sign;

    if ((type & 1) == 0) {
	if (flags & FLAG_NEG) {
	    sign = '-';
	    signwidth = 1;
	} else if (flags & FLAG_SIGNED) {
	    sign = '+';
	    signwidth = 1;
	} else if (flags & FLAG_SPACED) {
	    sign = ' ';
	    signwidth = 1;
	}
    }
    cvtwidth = signwidth + srclen;

    if (prec > 0) {
	if (prec > srclen) {
	    precwidth = prec - srclen;		/* Need zero filling */
	    cvtwidth += precwidth;
	}
    }

    if ((flags & FLAG_ZEROS) && (prec < 0)) {
	if (width > cvtwidth) {
	    zerowidth = width - cvtwidth;	/* Zero filling */
	    cvtwidth += zerowidth;
	}
    }

    if (flags & FLAG_LEFT) {
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
	rv = (*ss->stuff)(ss, " ", 1);
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
	rv = (*ss->stuff)(ss, "0", 1);
	if (rv < 0) {
	    return rv;
	}
    }
    while (--zerowidth >= 0) {
	rv = (*ss->stuff)(ss, "0", 1);
	if (rv < 0) {
	    return rv;
	}
    }
    rv = (*ss->stuff)(ss, src, srclen);
    if (rv < 0) {
	return rv;
    }
    while (--rightspaces >= 0) {
	rv = (*ss->stuff)(ss, " ", 1);
	if (rv < 0) {
	    return rv;
	}
    }
    return 0;
}

/*
** Convert a long into its printable form
*/
static int cvt_l(SprintfState *ss, long num, int width, int prec, int radix,
		 int type, int flags, const char *hexp)
{
    char cvtbuf[100];
    char *cvt;
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
    cvt = cvtbuf + sizeof(cvtbuf);
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
static int cvt_ll(SprintfState *ss, PRInt64 num, int width, int prec, int radix,
		  int type, int flags, const char *hexp)
{
    char cvtbuf[100];
    char *cvt;
    int digits;
    PRInt64 rad;

    /* according to the man page this needs to happen */
    if ((prec == 0) && (LL_IS_ZERO(num))) {
	return 0;
    }

    /*
    ** Converting decimal is a little tricky. In the unsigned case we
    ** need to stop when we hit 10 digits. In the signed case, we can
    ** stop when the number is zero.
    */
    LL_I2L(rad, radix);
    cvt = cvtbuf + sizeof(cvtbuf);
    digits = 0;
    while (!LL_IS_ZERO(num)) {
	PRInt32 digit;
	PRInt64 quot, rem;
	LL_UDIVMOD(&quot, &rem, num, rad);
	LL_L2I(digit, rem);
	*--cvt = hexp[digit & 0xf];
	digits++;
	num = quot;
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
**
** XXX stop using sprintf to convert floating point
*/
static int cvt_f(SprintfState *ss, double d, const char *fmt0, const char *fmt1)
{
    char fin[20];
    char fout[300];
    int amount = fmt1 - fmt0;

    PR_ASSERT((amount > 0) && (amount < sizeof(fin)));
    if (amount >= sizeof(fin)) {
	/* Totally bogus % command to sprintf. Just ignore it */
	return 0;
    }
    memcpy(fin, fmt0, amount);
    fin[amount] = 0;

    /* Convert floating point using the native sprintf code */
#ifdef DEBUG
    {
        const char *p = fin;
        while (*p) {
            PR_ASSERT(*p != 'L');
            p++;
        }
    }
#endif
    sprintf(fout, fin, d);

    /*
    ** This assert will catch overflow's of fout, when building with
    ** debugging on. At least this way we can track down the evil piece
    ** of calling code and fix it!
    */
    PR_ASSERT(strlen(fout) < sizeof(fout));

    return (*ss->stuff)(ss, fout, strlen(fout));
}

/*
** Convert a string into its printable form.  "width" is the output
** width. "prec" is the maximum number of characters of "s" to output,
** where -1 means until NUL.
*/
static int cvt_s(SprintfState *ss, const char *str, int width, int prec,
		 int flags)
{
    int slen;

    if (prec == 0)
	return 0;

    /* Limit string length by precision value */
    if (!str) {
    	str = "(null)";
    } 
    if (prec > 0) {
	/* this is:  slen = strnlen(str, prec); */
	register const char *s;

	for(s = str; prec && *s; s++, prec-- )
	    ;
	slen = s - str;
    } else {
	slen = strlen(str);
    }

    /* and away we go */
    return fill2(ss, str, slen, width, flags);
}

/*
** BuildArgArray stands for Numbered Argument list Sprintf
** for example,  
**	fmp = "%4$i, %2$d, %3s, %1d";
** the number must start from 1, and no gap among them
*/

static struct NumArg* BuildArgArray( const char *fmt, va_list ap, int* rv, struct NumArg* nasArray )
{
    int number = 0, cn = 0, i;
    const char* p;
    char  c;
    struct NumArg* nas;
    

    /*
    **	first pass:
    **	determine how many legal % I have got, then allocate space
    */

    p = fmt;
    *rv = 0;
    i = 0;
    while( ( c = *p++ ) != 0 ){
	if( c != '%' )
	    continue;
	if( ( c = *p++ ) == '%' )	/* skip %% case */
	    continue;

	while( c != 0 ){
	    if( c > '9' || c < '0' ){
		if( c == '$' ){		/* numbered argument case */
		    if( i > 0 ){
			*rv = -1;
			return NULL;
		    }
		    number++;
		} else{			/* non-numbered argument case */
		    if( number > 0 ){
			*rv = -1;
			return NULL;
		    }
		    i = 1;
		}
		break;
	    }

	    c = *p++;
	}
    }

    if( number == 0 ){
	return NULL;
    }

    
    if( number > NAS_DEFAULT_NUM ){
	nas = (struct NumArg*)PR_MALLOC( number * sizeof( struct NumArg ) );
	if( !nas ){
	    *rv = -1;
	    return NULL;
	}
    } else {
	nas = nasArray;
    }

    for( i = 0; i < number; i++ ){
	nas[i].type = TYPE_UNKNOWN;
    }


    /*
    ** second pass:
    ** set nas[].type
    */

    p = fmt;
    while( ( c = *p++ ) != 0 ){
    	if( c != '%' )	continue;
	    c = *p++;
	if( c == '%' )	continue;

	cn = 0;
	while( c && c != '$' ){	    /* should imporve error check later */
	    cn = cn*10 + c - '0';
	    c = *p++;
	}

	if( !c || cn < 1 || cn > number ){
	    *rv = -1;
	    break;
        }

	/* nas[cn] starts from 0, and make sure nas[cn].type is not assigned */
        cn--;
	if( nas[cn].type != TYPE_UNKNOWN )
	    continue;

        c = *p++;

        /* width */
        if (c == '*') {
	    /* not supported feature, for the argument is not numbered */
	    *rv = -1;
	    break;
	}

	while ((c >= '0') && (c <= '9')) {
	    c = *p++;
	}

	/* precision */
	if (c == '.') {
	    c = *p++;
	    if (c == '*') {
	        /* not supported feature, for the argument is not numbered */
	        *rv = -1;
	        break;
	    }

	    while ((c >= '0') && (c <= '9')) {
		c = *p++;
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
	    nas[ cn ].type = TYPE_DOUBLE;
	    break;

	case 'p':
	    /* XXX should use cpp */
	    if (sizeof(void *) == sizeof(PRInt32)) {
		nas[ cn ].type = TYPE_UINT32;
	    } else if (sizeof(void *) == sizeof(PRInt64)) {
	        nas[ cn ].type = TYPE_UINT64;
	    } else if (sizeof(void *) == sizeof(PRIntn)) {
	        nas[ cn ].type = TYPE_UINTN;
	    } else {
	        nas[ cn ].type = TYPE_UNKNOWN;
	    }
	    break;

	case 'S':
#ifdef WIN32
	    nas[ cn ].type = TYPE_WSTRING;
	    break;
#endif
	case 'C':
	case 'E':
	case 'G':
	    /* XXX not supported I suppose */
	    PR_ASSERT(0);
	    nas[ cn ].type = TYPE_UNKNOWN;
	    break;

	case 's':
	    nas[ cn ].type = TYPE_STRING;
	    break;

	case 'n':
	    nas[ cn ].type = TYPE_INTSTR;
	    break;

	default:
	    PR_ASSERT(0);
	    nas[ cn ].type = TYPE_UNKNOWN;
	    break;
	}

	/* get a legal para. */
	if( nas[ cn ].type == TYPE_UNKNOWN ){
	    *rv = -1;
	    break;
	}
    }


    /*
    ** third pass
    ** fill the nas[cn].ap
    */

    if( *rv < 0 ){
	if( nas != nasArray )
	    PR_DELETE( nas );
	return NULL;
    }

    cn = 0;
    while( cn < number ){
	if( nas[cn].type == TYPE_UNKNOWN ){
	    cn++;
	    continue;
	}

	switch( nas[cn].type ){
	case TYPE_INT16:
	case TYPE_UINT16:
	case TYPE_INTN:
	    nas[cn].u.i = va_arg( ap, int );
	    break;

	case TYPE_UINTN:
	    nas[cn].u.ui = va_arg( ap, unsigned int );
	    break;

	case TYPE_INT32:
	    nas[cn].u.i32 = va_arg( ap, PRInt32 );
	    break;

	case TYPE_UINT32:
	    nas[cn].u.ui32 = va_arg( ap, PRUint32 );
	    break;

	case TYPE_INT64:
	    nas[cn].u.ll = va_arg( ap, PRInt64 );
	    break;

	case TYPE_UINT64:
	    nas[cn].u.ull = va_arg( ap, PRUint64 );
	    break;

	case TYPE_STRING:
	    nas[cn].u.s = va_arg( ap, char* );
	    break;

#ifdef WIN32
	case TYPE_WSTRING:
	    nas[cn].u.ws = va_arg( ap, WCHAR* );
	    break;
#endif

	case TYPE_INTSTR:
	    nas[cn].u.ip = va_arg( ap, int* );
	    break;

	case TYPE_DOUBLE:
	    nas[cn].u.d = va_arg( ap, double );
	    break;

	default:
	    if( nas != nasArray )
		PR_DELETE( nas );
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
static int dosprintf(SprintfState *ss, const char *fmt, va_list ap)
{
    char c;
    int flags, width, prec, radix, type;
    union {
	char ch;
	int i;
	long l;
	PRInt64 ll;
	double d;
	const char *s;
	int *ip;
#ifdef WIN32
	const WCHAR *ws;
#endif
    } u;
    const char *fmt0;
    static char *hex = "0123456789abcdef";
    static char *HEX = "0123456789ABCDEF";
    char *hexp;
    int rv, i;
    struct NumArg* nas = NULL;
    struct NumArg* nap;
    struct NumArg  nasArray[ NAS_DEFAULT_NUM ];
    char  pattern[20];
    const char* dolPt = NULL;  /* in "%4$.2f", dolPt will point to . */
#ifdef WIN32
    char *pBuf = NULL;
#endif

    /*
    ** build an argument array, IF the fmt is numbered argument
    ** list style, to contain the Numbered Argument list pointers
    */

    nas = BuildArgArray( fmt, ap, &rv, nasArray );
    if( rv < 0 ){
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
	fmt0 = fmt - 1;

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

	if( nas != NULL ){
	    /* the fmt contains the Numbered Arguments feature */
	    i = 0;
	    while( c && c != '$' ){	    /* should imporve error check later */
		i = ( i * 10 ) + ( c - '0' );
		c = *fmt++;
	    }

	    if( nas[i-1].type == TYPE_UNKNOWN ){
		if( nas && ( nas != nasArray ) )
		    PR_DELETE( nas );
		return -1;
	    }

	    nap = &nas[i-1];
	    dolPt = fmt;
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
	    if (c == '-') flags |= FLAG_LEFT;
	    if (c == '+') flags |= FLAG_SIGNED;
	    if (c == ' ') flags |= FLAG_SPACED;
	    if (c == '0') flags |= FLAG_ZEROS;
	    c = *fmt++;
	}
	if (flags & FLAG_SIGNED) flags &= ~FLAG_SPACED;
	if (flags & FLAG_LEFT) flags &= ~FLAG_ZEROS;

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
	hexp = hex;
	switch (c) {
	  case 'd': case 'i':			/* decimal/integer */
	    radix = 10;
	    goto fetch_and_convert;

	  case 'o':				/* octal */
	    radix = 8;
	    type |= 1;
	    goto fetch_and_convert;

	  case 'u':				/* unsigned decimal */
	    radix = 10;
	    type |= 1;
	    goto fetch_and_convert;

	  case 'x':				/* unsigned hex */
	    radix = 16;
	    type |= 1;
	    goto fetch_and_convert;

	  case 'X':				/* unsigned HEX */
	    radix = 16;
	    hexp = HEX;
	    type |= 1;
	    goto fetch_and_convert;

	  fetch_and_convert:
	    switch (type) {
	      case TYPE_INT16:
		u.l = nas ? nap->u.i : va_arg(ap, int);
		if (u.l < 0) {
		    u.l = -u.l;
		    flags |= FLAG_NEG;
		}
		goto do_long;
	      case TYPE_UINT16:
		u.l = (nas ? nap->u.i : va_arg(ap, int)) & 0xffff;
		goto do_long;
	      case TYPE_INTN:
		u.l = nas ? nap->u.i : va_arg(ap, int);
		if (u.l < 0) {
		    u.l = -u.l;
		    flags |= FLAG_NEG;
		}
		goto do_long;
	      case TYPE_UINTN:
		u.l = (long)(nas ? nap->u.ui : va_arg(ap, unsigned int));
		goto do_long;

	      case TYPE_INT32:
		u.l = nas ? nap->u.i32 : va_arg(ap, PRInt32);
		if (u.l < 0) {
		    u.l = -u.l;
		    flags |= FLAG_NEG;
		}
		goto do_long;
	      case TYPE_UINT32:
		u.l = (long)(nas ? nap->u.ui32 : va_arg(ap, PRUint32));
	      do_long:
		rv = cvt_l(ss, u.l, width, prec, radix, type, flags, hexp);
		if (rv < 0) {
		    return rv;
		}
		break;

	      case TYPE_INT64:
		u.ll = nas ? nap->u.ll : va_arg(ap, PRInt64);
		if (!LL_GE_ZERO(u.ll)) {
		    LL_NEG(u.ll, u.ll);
		    flags |= FLAG_NEG;
		}
		goto do_longlong;
	      case TYPE_UINT64:
		u.ll = nas ? nap->u.ull : va_arg(ap, PRUint64);
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
	    u.d = nas ? nap->u.d : va_arg(ap, double);
	    if( nas != NULL ){
		i = fmt - dolPt;
		if( i < sizeof( pattern ) ){
		    pattern[0] = '%';
		    memcpy( &pattern[1], dolPt, i );
		    rv = cvt_f(ss, u.d, pattern, &pattern[i+1] );
		}
	    } else
		rv = cvt_f(ss, u.d, fmt0, fmt);

	    if (rv < 0) {
		return rv;
	    }
	    break;

	  case 'c':
	    u.ch = nas ? nap->u.i : va_arg(ap, int);
            if ((flags & FLAG_LEFT) == 0) {
                while (width-- > 1) {
                    rv = (*ss->stuff)(ss, " ", 1);
                    if (rv < 0) {
                        return rv;
                    }
                }
            }
	    rv = (*ss->stuff)(ss, &u.ch, 1);
	    if (rv < 0) {
		return rv;
	    }
            if (flags & FLAG_LEFT) {
                while (width-- > 1) {
                    rv = (*ss->stuff)(ss, " ", 1);
                    if (rv < 0) {
                        return rv;
                    }
                }
            }
	    break;

	  case 'p':
	    if (sizeof(void *) == sizeof(PRInt32)) {
	    	type = TYPE_UINT32;
	    } else if (sizeof(void *) == sizeof(PRInt64)) {
	    	type = TYPE_UINT64;
	    } else if (sizeof(void *) == sizeof(int)) {
		type = TYPE_UINTN;
	    } else {
		PR_ASSERT(0);
		break;
	    }
	    radix = 16;
	    goto fetch_and_convert;

#ifndef WIN32
	  case 'S':
	    /* XXX not supported I suppose */
	    PR_ASSERT(0);
	    break;
#endif

#if 0
	  case 'C':
	  case 'E':
	  case 'G':
	    /* XXX not supported I suppose */
	    PR_ASSERT(0);
	    break;
#endif

#ifdef WIN32
	  case 'S':
	    u.ws = nas ? nap->u.ws : va_arg(ap, const WCHAR*);

	    /* Get the required size in rv */
	    rv = WideCharToMultiByte(CP_ACP, 0, u.ws, -1, NULL, 0, NULL, NULL);
	    if (rv == 0)
		rv = 1;
	    pBuf = PR_MALLOC(rv);
	    WideCharToMultiByte(CP_ACP, 0, u.ws, -1, pBuf, (int)rv, NULL, NULL);
	    pBuf[rv-1] = '\0';

	    rv = cvt_s(ss, pBuf, width, prec, flags);

	    /* We don't need the allocated buffer anymore */
	    PR_Free(pBuf);
	    if (rv < 0) {
		return rv;
	    }
	    break;

#endif

	  case 's':
	    u.s = nas ? nap->u.s : va_arg(ap, const char*);
	    rv = cvt_s(ss, u.s, width, prec, flags);
	    if (rv < 0) {
		return rv;
	    }
	    break;

	  case 'n':
	    u.ip = nas ? nap->u.ip : va_arg(ap, int*);
	    if (u.ip) {
		*u.ip = ss->cur - ss->base;
	    }
	    break;

	  default:
	    /* Not a % token after all... skip it */
#if 0
	    PR_ASSERT(0);
#endif
	    rv = (*ss->stuff)(ss, "%", 1);
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
    rv = (*ss->stuff)(ss, "\0", 1);

    if( nas && ( nas != nasArray ) ){
	PR_DELETE( nas );
    }

    return rv;
}

/************************************************************************/

static int FuncStuff(SprintfState *ss, const char *sp, PRUint32 len)
{
    int rv;

    rv = (*ss->func)(ss->arg, sp, len);
    if (rv < 0) {
	return rv;
    }
    ss->maxlen += len;
    return 0;
}

PR_IMPLEMENT(PRUint32) PR_sxprintf(PRStuffFunc func, void *arg, 
                                 const char *fmt, ...)
{
    va_list ap;
    PRUint32 rv;

    va_start(ap, fmt);
    rv = PR_vsxprintf(func, arg, fmt, ap);
    va_end(ap);
    return rv;
}

PR_IMPLEMENT(PRUint32) PR_vsxprintf(PRStuffFunc func, void *arg, 
                                  const char *fmt, va_list ap)
{
    SprintfState ss;
    int rv;

    ss.stuff = FuncStuff;
    ss.func = func;
    ss.arg = arg;
    ss.maxlen = 0;
    rv = dosprintf(&ss, fmt, ap);
    return (rv < 0) ? (PRUint32)-1 : ss.maxlen;
}

/*
** Stuff routine that automatically grows the malloc'd output buffer
** before it overflows.
*/
static int GrowStuff(SprintfState *ss, const char *sp, PRUint32 len)
{
    ptrdiff_t off;
    char *newbase;
    PRUint32 newlen;

    off = ss->cur - ss->base;
    if (off + len >= ss->maxlen) {
	/* Grow the buffer */
	newlen = ss->maxlen + ((len > 32) ? len : 32);
	if (ss->base) {
	    newbase = (char*) PR_REALLOC(ss->base, newlen);
	} else {
	    newbase = (char*) PR_MALLOC(newlen);
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
    PR_ASSERT((PRUint32)(ss->cur - ss->base) <= ss->maxlen);
    return 0;
}

/*
** sprintf into a malloc'd buffer
*/
PR_IMPLEMENT(char *) PR_smprintf(const char *fmt, ...)
{
    va_list ap;
    char *rv;

    va_start(ap, fmt);
    rv = PR_vsmprintf(fmt, ap);
    va_end(ap);
    return rv;
}

/*
** Free memory allocated, for the caller, by PR_smprintf
*/
PR_IMPLEMENT(void) PR_smprintf_free(char *mem)
{
	PR_DELETE(mem);
}

PR_IMPLEMENT(char *) PR_vsmprintf(const char *fmt, va_list ap)
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
static int LimitStuff(SprintfState *ss, const char *sp, PRUint32 len)
{
    PRUint32 limit = ss->maxlen - (ss->cur - ss->base);

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
PR_IMPLEMENT(PRUint32) PR_snprintf(char *out, PRUint32 outlen, const char *fmt, ...)
{
    va_list ap;
    PRUint32 rv;

    va_start(ap, fmt);
    rv = PR_vsnprintf(out, outlen, fmt, ap);
    va_end(ap);
    return rv;
}

PR_IMPLEMENT(PRUint32) PR_vsnprintf(char *out, PRUint32 outlen,const char *fmt,
                                  va_list ap)
{
    SprintfState ss;
    PRUint32 n;

    PR_ASSERT((PRInt32)outlen > 0);
    if ((PRInt32)outlen <= 0) {
	return 0;
    }

    ss.stuff = LimitStuff;
    ss.base = out;
    ss.cur = out;
    ss.maxlen = outlen;
    (void) dosprintf(&ss, fmt, ap);

    /* If we added chars, and we didn't append a null, do it now. */
    if( (ss.cur != ss.base) && (*(ss.cur - 1) != '\0') )
        *(ss.cur - 1) = '\0';

    n = ss.cur - ss.base;
    return n ? n - 1 : n;
}

PR_IMPLEMENT(char *) PR_sprintf_append(char *last, const char *fmt, ...)
{
    va_list ap;
    char *rv;

    va_start(ap, fmt);
    rv = PR_vsprintf_append(last, fmt, ap);
    va_end(ap);
    return rv;
}

PR_IMPLEMENT(char *) PR_vsprintf_append(char *last, const char *fmt, va_list ap)
{
    SprintfState ss;
    int rv;

    ss.stuff = GrowStuff;
    if (last) {
	int lastlen = strlen(last);
	ss.base = last;
	ss.cur = last + lastlen;
	ss.maxlen = lastlen;
    } else {
	ss.base = 0;
	ss.cur = 0;
	ss.maxlen = 0;
    }
    rv = dosprintf(&ss, fmt, ap);
    if (rv < 0) {
	if (ss.base) {
	    PR_DELETE(ss.base);
	}
	return 0;
    }
    return ss.base;
}

