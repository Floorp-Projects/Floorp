/*
 *  logtab.h
 *
 *  Arbitrary precision integer arithmetic library
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is the MPI Arbitrary Precision Integer Arithmetic
 * library.
 *
 * The Initial Developer of the Original Code is Michael J. Fromberger.
 * Portions created by Michael J. Fromberger are 
 * Copyright (C) 1998, 1999, 2000 Michael J. Fromberger. 
 * All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable
 * instead of those above.  If you wish to allow use of your
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the GPL.
 *
 *  $Id: logtab.h,v 1.1 2000/07/30 01:56:35 nelsonb%netscape.com Exp $
 */

const float s_logv_2[] = {
   0.000000000, 0.000000000, 1.000000000, 0.630929754, 	/*  0  1  2  3 */
   0.500000000, 0.430676558, 0.386852807, 0.356207187, 	/*  4  5  6  7 */
   0.333333333, 0.315464877, 0.301029996, 0.289064826, 	/*  8  9 10 11 */
   0.278942946, 0.270238154, 0.262649535, 0.255958025, 	/* 12 13 14 15 */
   0.250000000, 0.244650542, 0.239812467, 0.235408913, 	/* 16 17 18 19 */
   0.231378213, 0.227670249, 0.224243824, 0.221064729, 	/* 20 21 22 23 */
   0.218104292, 0.215338279, 0.212746054, 0.210309918, 	/* 24 25 26 27 */
   0.208014598, 0.205846832, 0.203795047, 0.201849087, 	/* 28 29 30 31 */
   0.200000000, 0.198239863, 0.196561632, 0.194959022, 	/* 32 33 34 35 */
   0.193426404, 0.191958720, 0.190551412, 0.189200360, 	/* 36 37 38 39 */
   0.187901825, 0.186652411, 0.185449023, 0.184288833, 	/* 40 41 42 43 */
   0.183169251, 0.182087900, 0.181042597, 0.180031327, 	/* 44 45 46 47 */
   0.179052232, 0.178103594, 0.177183820, 0.176291434, 	/* 48 49 50 51 */
   0.175425064, 0.174583430, 0.173765343, 0.172969690, 	/* 52 53 54 55 */
   0.172195434, 0.171441601, 0.170707280, 0.169991616, 	/* 56 57 58 59 */
   0.169293808, 0.168613099, 0.167948779, 0.167300179, 	/* 60 61 62 63 */
   0.166666667
};

