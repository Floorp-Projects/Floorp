/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

#include "primpl.h"

static PRInt64 ll_zero = LL_INIT( 0x00000000,0x00000000 );
static PRInt64 ll_maxint = LL_INIT( 0x7fffffff, 0xffffffff );
static PRInt64 ll_minint = LL_INIT( 0x80000000, 0x00000000 );

#if defined(HAVE_WATCOM_BUG_2)
PRInt64 __pascal __loadds __export
    LL_Zero(void) { return ll_zero; }
PRInt64 __pascal __loadds __export
    LL_MaxInt(void) { return ll_maxint; }
PRInt64 __pascal __loadds __export
    LL_MinInt(void) { return ll_minint; }
#else
PR_IMPLEMENT(PRInt64) LL_Zero(void) { return ll_zero; }
PR_IMPLEMENT(PRInt64) LL_MaxInt(void) { return ll_maxint; }
PR_IMPLEMENT(PRInt64) LL_MinInt(void) { return ll_minint; }
#endif

#ifndef HAVE_LONG_LONG
/*
** Divide 64-bit a by 32-bit b, which must be normalized so its high bit is 1.
*/
static void norm_udivmod32(PRUint32 *qp, PRUint32 *rp, PRUint64 a, PRUint32 b)
{
    PRUint32 d1, d0, q1, q0;
    PRUint32 r1, r0, m;

    d1 = _hi16(b);
    d0 = _lo16(b);
    r1 = a.hi % d1;
    q1 = a.hi / d1;
    m = q1 * d0;
    r1 = (r1 << 16) | _hi16(a.lo);
    if (r1 < m) {
        q1--, r1 += b;
        if (r1 >= b	/* i.e., we didn't get a carry when adding to r1 */
	    && r1 < m) {
	    q1--, r1 += b;
	}
    }
    r1 -= m;
    r0 = r1 % d1;
    q0 = r1 / d1;
    m = q0 * d0;
    r0 = (r0 << 16) | _lo16(a.lo);
    if (r0 < m) {
        q0--, r0 += b;
        if (r0 >= b
	    && r0 < m) {
	    q0--, r0 += b;
	}
    }
    *qp = (q1 << 16) | q0;
    *rp = r0 - m;
}

static PRUint32 CountLeadingZeros(PRUint32 a)
{
    PRUint32 t;
    PRUint32 r = 32;

    if ((t = a >> 16) != 0)
	r -= 16, a = t;
    if ((t = a >> 8) != 0)
	r -= 8, a = t;
    if ((t = a >> 4) != 0)
	r -= 4, a = t;
    if ((t = a >> 2) != 0)
	r -= 2, a = t;
    if ((t = a >> 1) != 0)
	r -= 1, a = t;
    if (a & 1)
	r--;
    return r;
}

PR_IMPLEMENT(void) ll_udivmod(PRUint64 *qp, PRUint64 *rp, PRUint64 a, PRUint64 b)
{
    PRUint32 n0, n1, n2;
    PRUint32 q0, q1;
    PRUint32 rsh, lsh;

    n0 = a.lo;
    n1 = a.hi;

    if (b.hi == 0) {
	if (b.lo > n1) {
	    /* (0 q0) = (n1 n0) / (0 D0) */

	    lsh = CountLeadingZeros(b.lo);

	    if (lsh) {
		/*
		 * Normalize, i.e. make the most significant bit of the
		 * denominator be set.
		 */
		b.lo = b.lo << lsh;
		n1 = (n1 << lsh) | (n0 >> (32 - lsh));
		n0 = n0 << lsh;
	    }

	    a.lo = n0, a.hi = n1;
	    norm_udivmod32(&q0, &n0, a, b.lo);
	    q1 = 0;

	    /* remainder is in n0 >> lsh */
	} else {
	    /* (q1 q0) = (n1 n0) / (0 d0) */

	    if (b.lo == 0)		/* user wants to divide by zero! */
		b.lo = 1 / b.lo;	/* so go ahead and crash */

	    lsh = CountLeadingZeros(b.lo);

	    if (lsh == 0) {
		/*
		 * From (n1 >= b.lo)
		 *   && (the most significant bit of b.lo is set),
		 * conclude that
		 *	(the most significant bit of n1 is set)
		 *   && (the leading quotient digit q1 = 1).
		 *
		 * This special case is necessary, not an optimization
		 * (Shifts counts of 32 are undefined).
		 */
		n1 -= b.lo;
		q1 = 1;
	    } else {
		/*
		 * Normalize.
		 */
		rsh = 32 - lsh;

		b.lo = b.lo << lsh;
		n2 = n1 >> rsh;
		n1 = (n1 << lsh) | (n0 >> rsh);
		n0 = n0 << lsh;

		a.lo = n1, a.hi = n2;
		norm_udivmod32(&q1, &n1, a, b.lo);
	    }

	    /* n1 != b.lo... */

	    a.lo = n0, a.hi = n1;
	    norm_udivmod32(&q0, &n0, a, b.lo);

	    /* remainder in n0 >> lsh */
	}

	if (rp) {
	    rp->lo = n0 >> lsh;
	    rp->hi = 0;
	}
    } else {
	if (b.hi > n1) {
	    /* (0 0) = (n1 n0) / (D1 d0) */

	    q0 = 0;
	    q1 = 0;

	    /* remainder in (n1 n0) */
	    if (rp) {
		rp->lo = n0;
		rp->hi = n1;
	    }
	} else {
	    /* (0 q0) = (n1 n0) / (d1 d0) */

	    lsh = CountLeadingZeros(b.hi);
	    if (lsh == 0) {
		/*
		 * From (n1 >= b.hi)
		 *   && (the most significant bit of b.hi is set),
		 * conclude that
		 *      (the most significant bit of n1 is set)
		 *   && (the quotient digit q0 = 0 or 1).
		 *
		 * This special case is necessary, not an optimization.
		 */

		/*
		 * The condition on the next line takes advantage of that
		 * n1 >= b.hi (true due to control flow).
		 */
		if (n1 > b.hi || n0 >= b.lo) {
		    q0 = 1;
		    a.lo = n0, a.hi = n1;
		    LL_SUB(a, a, b);
		} else {
		    q0 = 0;
		}
		q1 = 0;

		if (rp) {
		    rp->lo = n0;
		    rp->hi = n1;
		}
	    } else {
		PRInt64 m;

		/*
		 * Normalize.
		 */
		rsh = 32 - lsh;

		b.hi = (b.hi << lsh) | (b.lo >> rsh);
		b.lo = b.lo << lsh;
		n2 = n1 >> rsh;
		n1 = (n1 << lsh) | (n0 >> rsh);
		n0 = n0 << lsh;

		a.lo = n1, a.hi = n2;
		norm_udivmod32(&q0, &n1, a, b.hi);
		LL_MUL32(m, q0, b.lo);

		if ((m.hi > n1) || ((m.hi == n1) && (m.lo > n0))) {
		    q0--;
		    LL_SUB(m, m, b);
		}

		q1 = 0;

		/* Remainder is ((n1 n0) - (m1 m0)) >> lsh */
		if (rp) {
		    a.lo = n0, a.hi = n1;
		    LL_SUB(a, a, m);
		    rp->lo = (a.hi << rsh) | (a.lo >> lsh);
		    rp->hi = a.hi >> lsh;
		}
	    }
	}
    }

    if (qp) {
	qp->lo = q0;
	qp->hi = q1;
    }
}
#endif /* !HAVE_LONG_LONG */
