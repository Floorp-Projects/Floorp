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
 * The Original Code is interface file for SPARC Montgomery multiply functions.
 *
 * The Initial Developer of the Original Code is
 * Sun Microsystems Inc.
 * Portions created by the Initial Developer are Copyright (C) 1999-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Netscape Communications Corporation
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

/*  The functions that are to be called from outside of the .s file have the
 *  following interfaces and array size requirements:
 */


void conv_i32_to_d32(double *d32, unsigned int *i32, int len);

/*  Converts an array of int's to an array of doubles, so that each double
 *  corresponds to an int.  len is the number of items converted.
 *  Does not allocate the output array.
 *  The pointers d32 and i32 should point to arrays of size at least  len
 *  (doubles and unsigned ints, respectively)
 */


void conv_i32_to_d16(double *d16, unsigned int *i32, int len);

/*  Converts an array of int's to an array of doubles so that each element
 *  of the int array is converted to a pair of doubles, the first one
 *  corresponding to the lower (least significant) 16 bits of the int and
 *  the second one corresponding to the upper (most significant) 16 bits of
 *  the 32-bit int. len is the number of ints converted.
 *  Does not allocate the output array.
 *  The pointer d16 should point to an array of doubles of size at least
 *  2*len and i32 should point an array of ints of size at least  len
 */


void conv_i32_to_d32_and_d16(double *d32, double *d16, 
			     unsigned int *i32, int len);

/*  Does the above two conversions together, it is much faster than doing
 *  both of those in succession
 */


void mont_mulf_noconv(unsigned int *result,
		     double *dm1, double *dm2, double *dt,
		     double *dn, unsigned int *nint,
		     int nlen, double dn0);

/*  Does the Montgomery multiplication of the numbers stored in the arrays
 *  pointed to by dm1 and dm2, writing the result to the array pointed to by
 *  result. It uses the array pointed to by dt as a temporary work area.
 *  nint should point to the modulus in the array-of-integers representation, 
 *  dn should point to its array-of-doubles as obtained as a result of the
 *  function call   conv_i32_to_d32(dn, nint, nlen);
 *  nlen is the length of the array containing the modulus.
 *  The representation used for dm1 is the one that is a result of the function
 *  call   conv_i32_to_d32(dm1, m1, nlen), the representation for dm2 is the
 *  result of the function call   conv_i32_to_d16(dm2, m2, nlen).
 *  Note that m1 and m2 should both be of length nlen, so they should be
 *  padded with 0's if necessary before the conversion. The result comes in 
 *  this form (int representation, padded with 0's).
 *  dn0 is the value of the 16 least significant bits of n0'.
 *  The function does not allocate memory for any of the arrays, so the 
 *  pointers should point to arrays with the following minimal sizes:
 *  result - nlen+1
 *  dm1    - nlen
 *  dm2    - 2*nlen+1  ( the +1 is necessary for technical reasons )
 *  dt     - 4*nlen+2
 *  dn     - nlen
 *  nint   - nlen
 *  No two arrays should point to overlapping areas of memory.
 */  
