/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Netscape security libraries.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/* This file contains routines that perform vector multiplication.  */

#include "mpi-priv.h"
#include <unistd.h>

#include <stddef.h>
/* #include <sys/systeminfo.h> */
#include <strings.h>

extern void multacc512( 
   int             length,        /* doublewords in multiplicand vector. */
   const mp_digit *scalaraddr,    /* Address of scalar. */
   const mp_digit *multiplicand,  /* The multiplicand vector. */
   mp_digit *      result);       /* Where to accumulate the result. */

extern void maxpy_little(
   int             length,        /* doublewords in multiplicand vector. */
   const mp_digit *scalaraddr,    /* Address of scalar. */
   const mp_digit *multiplicand,  /* The multiplicand vector. */
   mp_digit *      result);       /* Where to accumulate the result. */

extern void add_diag_little(
   int            length,       /* doublewords in input vector. */
   const mp_digit *root,         /* The vector to square. */
   mp_digit *      result);      /* Where to accumulate the result. */

void 
s_mpv_sqr_add_prop(const mp_digit *pa, mp_size a_len, mp_digit *ps)
{
    add_diag_little(a_len, pa, ps);
}

#define MAX_STACK_DIGITS 258
#define MULTACC512_LEN   (512 / MP_DIGIT_BIT)
#define HP_MPY_ADD_FN    (a_len == MULTACC512_LEN ? multacc512 : maxpy_little)

/* c = a * b */
void 
s_mpv_mul_d(const mp_digit *a, mp_size a_len, mp_digit b, mp_digit *c)
{
    mp_digit x[MAX_STACK_DIGITS];
    mp_digit *px = x;
    size_t   xSize = 0;

    if (a == c) {
	if (a_len > MAX_STACK_DIGITS) {
	    xSize = sizeof(mp_digit) * (a_len + 2);
	    px = malloc(xSize);
	    if (!px)
		return;
	}
	memcpy(px, a, a_len * sizeof(*a));
	a = px;
    }
    s_mp_setz(c, a_len + 1);
    HP_MPY_ADD_FN(a_len, &b, a, c);
    if (px != x && px) {
	memset(px, 0, xSize);
	free(px);
    }
}

/* c += a * b, where a is a_len words long. */
void     
s_mpv_mul_d_add(const mp_digit *a, mp_size a_len, mp_digit b, mp_digit *c)
{
    c[a_len] = 0;	/* so carry propagation stops here. */
    HP_MPY_ADD_FN(a_len, &b, a, c);
}

/* c += a * b, where a is y words long. */
void     
s_mpv_mul_d_add_prop(const mp_digit *a, mp_size a_len, mp_digit b, 
			 mp_digit *c)
{
    HP_MPY_ADD_FN(a_len, &b, a, c);
}

