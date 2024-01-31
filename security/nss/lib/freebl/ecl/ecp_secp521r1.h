/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __ecp_secp521r1_h_
#define __ecp_secp521r1_h_

/*-
 * Wrapper: simultaneous scalar mutiplication.
 * outx, outy := a * G + b * P
 * where P = (inx, iny).
 * Everything is LE byte ordering.
 */
void point_mul_two_secp521r1(unsigned char outx[66], unsigned char outy[66],
                             const unsigned char a[66], const unsigned char b[66], const unsigned char inx[66], const unsigned char iny[66]);

/*-
 * Wrapper: fixed scalar mutiplication.
 * outx, outy := scalar * G
 * Everything is LE byte ordering.
 */
void point_mul_g_secp521r1(unsigned char outx[66], unsigned char outy[66],
                           const unsigned char scalar[66]);

/*-
 * Wrapper: variable point scalar mutiplication.
 * outx, outy := scalar * P
 * where P = (inx, iny).
 * Everything is LE byte ordering.
 */
void point_mul_secp521r1(unsigned char outx[66], unsigned char outy[66], const unsigned char scalar[66], const unsigned char inx[66], const unsigned char iny[66]);

#endif
