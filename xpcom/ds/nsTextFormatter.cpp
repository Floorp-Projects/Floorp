/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 * 
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 * 
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/* The code are copy from rev 3.7 mozilla/nsprpub/src/io/prprf.c */
/*
** Port from prprf.c by : Frank Yung-Fong Tang 
*/
/*
** Portable safe sprintf code.
**
** Author: Kipp E.B. Hickman
*/
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "prlong.h"
#include "prlog.h"
#include "prmem.h"
#include "nsCRT.h"
#include "nsTextFormatter.h"
#include "nsString.h"
#include "nsReadableUtils.h"


/*
** Note: on some platforms va_list is defined as an array,
** and requires array notation.
*/
#ifdef HAVE_VA_LIST_AS_ARRAY
#define VARARGS_ASSIGN(foo, bar)	foo[0] = bar[0]
#else
#define VARARGS_ASSIGN(foo, bar)	(foo) = (bar)
#endif

/*
** WARNING: This code may *NOT* call PR_LOG (because PR_LOG calls it)
*/

/*
** XXX This needs to be internationalized!
*/

typedef struct SprintfStateStr SprintfState;

struct SprintfStateStr {
    int (*stuff)(SprintfState *ss, const PRUnichar *sp, PRUint32 len);

    PRUnichar *base;
    PRUnichar *cur;
    PRUint32 maxlen;

    int (*func)(void *arg, const PRUnichar *sp, PRUint32 len);
    void *arg;
};

/*
** Numbered Arguement State
*/
struct NumArgState{
    int	    type;		/* type of the current ap                    */
    va_list ap;			/* point to the corresponding position on ap */
};

static PRBool  l10n_debug_init = PR_FALSE;
static PRBool  l10n_debug = PR_FALSE;

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
static int fill2(SprintfState *ss, const PRUnichar *src, int srclen, int width,
		int flags)
{
    PRUnichar space = ' ';
    int rv;

    width -= srclen;
    if ((width > 0) && ((flags & _LEFT) == 0)) {	/* Right adjusting */
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

    if ((width > 0) && ((flags & _LEFT) != 0)) {	/* Left adjusting */
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
static int fill_n(SprintfState *ss, const PRUnichar *src, int srclen, int width,
		  int prec, int type, int flags)
{
    int zerowidth = 0;
    int precwidth = 0;
    int signwidth = 0;
    int leftspaces = 0;
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
	    precwidth = prec - srclen;		/* Need zero filling */
	    cvtwidth += precwidth;
	}
    }

    if ((flags & _ZEROS) && (prec < 0)) {
	if (width > cvtwidth) {
	    zerowidth = width - cvtwidth;	/* Zero filling */
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
static int cvt_l(SprintfState *ss, long num, int width, int prec, int radix,
		 int type, int flags, const PRUnichar *hexp)
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
static int cvt_ll(SprintfState *ss, PRInt64 num, int width, int prec, int radix,
		  int type, int flags, const PRUnichar *hexp)
{
    PRUnichar cvtbuf[100];
    PRUnichar *cvt;
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
    cvt = &cvtbuf[0] + ELEMENTS_OF(cvtbuf);
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
static int cvt_f(SprintfState *ss, double d, const PRUnichar *fmt0, const PRUnichar *fmt1)
{
    char fin[20];
    char fout[300];
    PRUnichar fout2[300];
    int amount = fmt1 - fmt0;
    int i;

    PR_ASSERT((amount > 0) && (amount < (int)sizeof(fin)));
    if (amount >= (int)sizeof(fin)) {
	/* Totally bogus % command to sprintf. Just ignore it */
	return 0;
    }
    for(i=0;i<amount;i++)
      fin[i] = (char) fmt0[i]; // cast down here
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
    PR_ASSERT((nsCRT::strlen(fout)*2) < sizeof(fout));
    for(i=0; fout[i]; i++)
        fout2[i]=fout[i];
    fout2[i] = 0;


    return (*ss->stuff)(ss, fout2, nsCRT::strlen(fout2));
}

/*
** Convert a string into its printable form.  "width" is the output
** width. "prec" is the maximum number of characters of "s" to output,
** where -1 means until NUL.
*/
static int cvt_S(SprintfState *ss, const PRUnichar *s, int width, int prec,
		 int flags)
{
    int slen;

    if (prec == 0)
	return 0;

    /* Limit string length by precision value */
    slen = s ? nsCRT::strlen(s) : 6;
    if (prec > 0) {
	if (prec < slen) {
	    slen = prec;
	}
    }

    /* and away we go */
    nsAutoString nullstr;
    nullstr.AssignWithConversion("(null)");

    return fill2(ss, s ? s : nullstr.get(), slen, width, flags);
}

static PRUnichar* UTF8ToUCS2(const char *aSrc, PRUint32 aSrcLen, PRUnichar* aDest, PRUint32 aDestLen)
{
  const char *in, *inend;
  inend = aSrc + aSrcLen;
  PRUnichar *out;
  PRUint32 state;
  PRUint32 ucs4;
  // decide the length of the UCS2 first.
  PRUint32 needLen = 0;
  for(in=aSrc,state=0,ucs4=0;in < inend; in++)
  {
     if(0 == state) {
        if( 0 == (0x80 & (*in))) {
            needLen++;
        } else if( 0xC0 == (0xE0 & (*in))) {
            needLen++;
            state=1;
        } else if( 0xE0 == (0xF0 & (*in))) {
            needLen++;
            state=2;
        } else if( 0xF0 == (0xF8 & (*in))) {
            needLen+=2;
            state=3;
        } else if( 0xF8 == (0xFC & (*in))) {
            needLen+=2;
            state=4;
        } else if( 0xFC == (0xFE & (*in))) {
            needLen+=2;
            state=5;
        } else {
            needLen++;
            state=0;
        }
    } else {
        NS_ASSERTION( (0x80 == (0xC0 & (*in))) , "The input string is not in utf8");
        if(0x80 == (0xC0 & (*in)))
        {
            state--;
        } else {
            state=0;
        }
    }
  }
  needLen++; // add null termination.
  if(needLen >= aDestLen)
     aDest = (PRUnichar*)PR_MALLOC(sizeof(PRUnichar) * needLen);
  if(nsnull == aDest)
     return nsnull;
  out= aDest;

  for(in=aSrc,state=0,ucs4=0;in < inend; in++)
  {
     if(0 == state) {
        if( 0 == (0x80 & (*in))) {
            // ASCII
            *out++ = (PRUnichar)*in;
        } else if( 0xC0 == (0xE0 & (*in))) {
            // 2 bytes UTF8
            ucs4 = (PRUint32)(*in);
            ucs4 = (ucs4 << 6) & 0x000007C0L;
            state=1;
        } else if( 0xE0 == (0xF0 & (*in))) {
            ucs4 = (PRUint32)(*in);
            ucs4 = (ucs4 << 12) & 0x0000F000L;
            state=2;
        } else if( 0xF0 == (0xF8 & (*in))) {
            ucs4 = (PRUint32)(*in);
            ucs4 = (ucs4 << 18) & 0x001F0000L;
            state=3;
        } else if( 0xF8 == (0xFC & (*in))) {
            ucs4 = (PRUint32)(*in);
            ucs4 = (ucs4 << 24) & 0x03000000L;
            state=4;
        } else if( 0xFC == (0xFE & (*in))) {
            ucs4 = (PRUint32)(*in);
            ucs4 = (ucs4 << 30) & 0x40000000L;
            state=5;
        } else {
            NS_ASSERTION(0, "The input string is not in utf8");
            state=0;
            ucs4=0;
        }
    } else {
        NS_ASSERTION( (0x80 == (0xC0 & (*in))) , "The input string is not in utf8");
        if(0x80 == (0xC0 & (*in)))
        {
            PRUint32 tmp = (*in);
            int shift = (state-1) * 6;
            tmp = (tmp << shift ) & ( 0x0000003FL << shift);
            ucs4 |= tmp;
            if(0 == --state)
            {
                if(ucs4 >= 0x00010000) {
                   if(ucs4 >= 0x001F0000) {
                     *out++ = 0xFFFD;
                   } else {
                     ucs4 -= 0x00010000;
                     *out++ = 0xD800 | (0x000003FF & (ucs4 >> 10));
                     *out++ = 0xDC00 | (0x000003FF & ucs4);
                   }
                } else {
                   *out++ = ucs4;
                }
                ucs4=0;
            }
        } else {
            state=0;
            ucs4=0;
        }
    }
  }
  *out = 0x0000;
  return aDest;
}
/*
** Convert a string into its printable form.  "width" is the output
** width. "prec" is the maximum number of characters of "s" to output,
** where -1 means until NUL.
*/
static int cvt_s(SprintfState *ss, const char *s, int width, int prec,
		 int flags)
{
  // convert s from  UTF8 to PRUnichar*
  // Fix me !!!
  PRUnichar buf[256];
  PRUnichar *retbuf = nsnull;

  if (s) {
    retbuf = UTF8ToUCS2(s, nsCRT::strlen(s), buf, 256);
  
    if(nsnull == retbuf)
      return -1;
  }
  int ret = cvt_S(ss, retbuf, width, prec, flags);

  if(retbuf != buf)
     PR_DELETE(retbuf);
  return ret;
}

/*
** BiuldArgArray stands for Numbered Argument list Sprintf
** for example,  
**	fmp = "%4$i, %2$d, %3s, %1d";
** the number must start from 1, and no gap among them
*/

static struct NumArgState* BuildArgArray( const PRUnichar *fmt, va_list ap, int* rv, struct NumArgState* nasArray )
{
    int number = 0, cn = 0, i;
    const PRUnichar* p;
    PRUnichar  c;
    struct NumArgState* nas;
    

    /*
    ** set the l10n_debug flag
    ** this routine should be executed only once
    ** 'cause getenv does take time
    */
    if( !l10n_debug_init ){
	l10n_debug_init = PR_TRUE;
        const char *env;
	env = getenv( "NETSCAPE_LOCALIZATION_DEBUG" );
	if( ( env != NULL ) && ( *env == '1' ) ){
	    l10n_debug = PR_TRUE;
	}
    }


    /*
    **	first pass:
    **	detemine how many legal % I have got, then allocate space
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
		if( c == '$' ){		/* numbered argument csae */
		    if( i > 0 ){
			*rv = -1;
			return NULL;
		    }
		    number++;
		    break;

		} else{			/* non-numbered argument case */
		    if( number > 0 ){
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

    if( number == 0 ){
	return NULL;
    }

    
    if( number > NAS_DEFAULT_NUM ){
	nas = (struct NumArgState*)PR_MALLOC( number * sizeof( struct NumArgState ) );
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

	case 'C':
	//case 'S':
	case 'E':
	case 'G':
	    /* XXX not supported I suppose */
	    PR_ASSERT(0);
	    nas[ cn ].type = TYPE_UNKNOWN;
	    break;

	case 'S':
	    nas[ cn ].type = TYPE_UNISTRING;
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

	VARARGS_ASSIGN(nas[cn].ap, ap);

	switch( nas[cn].type ){
	case TYPE_INT16:
	case TYPE_UINT16:
	case TYPE_INTN:
	case TYPE_UINTN:		(void)va_arg( ap, PRIntn );		break;

	case TYPE_INT32:		(void)va_arg( ap, PRInt32 );		break;

	case TYPE_UINT32:	(void)va_arg( ap, PRUint32 );	break;

	case TYPE_INT64:	(void)va_arg( ap, PRInt64 );		break;

	case TYPE_UINT64:	(void)va_arg( ap, PRUint64 );		break;

	case TYPE_STRING:	(void)va_arg( ap, char* );		break;

	case TYPE_INTSTR:	(void)va_arg( ap, PRIntn* );		break;

	case TYPE_DOUBLE:	(void)va_arg( ap, double );		break;

	case TYPE_UNISTRING:	(void)va_arg( ap, PRUnichar* );		break;

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
static int dosprintf(SprintfState *ss, const PRUnichar *fmt, va_list ap)
{
    PRUnichar c;
    int flags, width, prec, radix, type;
    union {
	PRUnichar ch;
	int i;
	long l;
	PRInt64 ll;
	double d;
	const char *s;
	const PRUnichar *S;
	int *ip;
    } u;
    PRUnichar space = ' ';
    const PRUnichar *fmt0;

    nsAutoString hex;
    hex.AssignWithConversion("0123456789abcdef");

    nsAutoString HEX;
    HEX.AssignWithConversion("0123456789ABCDEF");

    const PRUnichar *hexp;
    int rv, i;
    struct NumArgState* nas = NULL;
    struct NumArgState  nasArray[ NAS_DEFAULT_NUM ];
    PRUnichar  pattern[20];
    const PRUnichar* dolPt = NULL;  /* in "%4$.2f", dolPt will poiont to . */


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

	    ap = nas[i-1].ap;
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
		u.l = va_arg(ap, PRInt32);
		if (u.l < 0) {
		    u.l = -u.l;
		    flags |= _NEG;
		}
		goto do_long;
	      case TYPE_UINT32:
		u.l = (long)va_arg(ap, PRUint32);
	      do_long:
		rv = cvt_l(ss, u.l, width, prec, radix, type, flags, hexp);
		if (rv < 0) {
		    return rv;
		}
		break;

	      case TYPE_INT64:
		u.ll = va_arg(ap, PRInt64);
		if (!LL_GE_ZERO(u.ll)) {
		    LL_NEG(u.ll, u.ll);
		    flags |= _NEG;
		}
		goto do_longlong;
	      case TYPE_UINT64:
		u.ll = va_arg(ap, PRUint64);
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
	    u.d = va_arg(ap, double);
	    if( nas != NULL ){
		i = fmt - dolPt;
		if( i < (int)ELEMENTS_OF(pattern) ){
		    pattern[0] = '%';
		    memcpy( &pattern[1], dolPt, i*sizeof(PRUnichar) );
		    rv = cvt_f(ss, u.d, pattern, &pattern[i+1] );
		}
	    } else
		rv = cvt_f(ss, u.d, fmt0, fmt);

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

#if 0
	  case 'C':
	  //case 'S':
	  case 'E':
	  case 'G':
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

#if 0
static int FuncStuff(SprintfState *ss, const PRUnichar *sp, PRUint32 len)
{
    int rv;

    rv = (*ss->func)(ss->arg, sp, len);
    if (rv < 0) {
	return rv;
    }
    ss->maxlen += len;
    return 0;
}


PRUint32 nsTextFormatter::sxprintf(PRStuffFunc func, void *arg,
                                 const PRUnichar *fmt, ...)
{
    va_list ap;
    int rv;

    va_start(ap, fmt);
    rv = nsTextFormatter::vsxprintf(func, arg, fmt, ap);
    va_end(ap);
    return rv;
}

PRUint32) vsxprintf(PRStuffFunc func, void *arg, 
                                  const PRUnichar *fmt, va_list ap)
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

#endif 

/*
** Stuff routine that automatically grows the malloc'd output buffer
** before it overflows.
*/
static int GrowStuff(SprintfState *ss, const PRUnichar *sp, PRUint32 len)
{
    ptrdiff_t off;
    PRUnichar *newbase;
    PRUint32 newlen;

    off = ss->cur - ss->base;
    if (off + len >= ss->maxlen) {
	/* Grow the buffer */
	newlen = ss->maxlen + ((len > 32) ? len : 32);
	if (ss->base) {
	    newbase = (PRUnichar*) PR_REALLOC(ss->base, newlen*sizeof(PRUnichar));
	} else {
	    newbase = (PRUnichar*) PR_MALLOC(newlen*sizeof(PRUnichar));
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
PRUnichar * nsTextFormatter::smprintf(const PRUnichar *fmt, ...)
{
    va_list ap;
    PRUnichar *rv;

    va_start(ap, fmt);
    rv = nsTextFormatter::vsmprintf(fmt, ap);
    va_end(ap);
    return rv;
}

/*
** Free memory allocated, for the caller, by smprintf
*/
void nsTextFormatter::smprintf_free(PRUnichar *mem)
{
	PR_DELETE(mem);
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
static int LimitStuff(SprintfState *ss, const PRUnichar *sp, PRUint32 len)
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
PRUint32 nsTextFormatter::snprintf(PRUnichar *out, PRUint32 outlen, const PRUnichar *fmt, ...)
{
    va_list ap;
    int rv;

    PR_ASSERT((PRInt32)outlen > 0);
    if ((PRInt32)outlen <= 0) {
	return 0;
    }

    va_start(ap, fmt);
    rv = nsTextFormatter::vsnprintf(out, outlen, fmt, ap);
    va_end(ap);
    return rv;
}

PRUint32 nsTextFormatter::vsnprintf(PRUnichar *out, PRUint32 outlen,const PRUnichar *fmt,
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
        *(--ss.cur) = '\0';

    n = ss.cur - ss.base;
    return n ? n - 1 : n;
}

PRUnichar * nsTextFormatter::sprintf_append(PRUnichar *last, const PRUnichar *fmt, ...)
{
    va_list ap;
    PRUnichar *rv;

    va_start(ap, fmt);
    rv = nsTextFormatter::vsprintf_append(last, fmt, ap);
    va_end(ap);
    return rv;
}

PRUnichar * nsTextFormatter::vsprintf_append(PRUnichar *last, const PRUnichar *fmt, va_list ap)
{
    SprintfState ss;
    int rv;

    ss.stuff = GrowStuff;
    if (last) {
	int lastlen = nsCRT::strlen(last);
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
#ifdef DEBUG
PRBool nsTextFormatter::SelfTest()
{ 
   PRBool passed = PR_TRUE ;
   nsAutoString fmt;
   fmt.AssignWithConversion("%3$s %4$S %1$d %2$d");

   char utf8[] = "Hello";
   PRUnichar ucs2[]={'W', 'o', 'r', 'l', 'd', 0x4e00, 0xAc00, 0xFF45, 0x0103};
   int d=3;


   PRUnichar buf[256];
   int ret;
   ret = nsTextFormatter::snprintf(buf, 256, fmt.get(), d, 333, utf8, ucs2);
   printf("ret = %d\n", ret);
   nsAutoString out(buf);
   printf("%s \n", NS_LossyConvertUCS2toASCII(out).get());
   const PRUnichar *uout = out.get();
   for(PRUint32 i=0;i<out.Length();i++)
      printf("%2X ", uout[i]);

   return passed;
}
#endif

