/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


/*
 * This file contains the Q14 radix-2 tables used in ARM9E optimization routines.
 *
 */

extern const unsigned short t_Q14S_rad8[2];
const unsigned short t_Q14S_rad8[2] = {  0x0000,0x2d41 };

//extern const int t_Q30S_rad8[2];
//const int t_Q30S_rad8[2] = {  0x00000000,0x2d413ccd };

extern const unsigned short t_Q14R_rad8[2];
const unsigned short t_Q14R_rad8[2] = {  0x2d41,0x2d41 };

//extern const int t_Q30R_rad8[2];
//const int t_Q30R_rad8[2] = {  0x2d413ccd,0x2d413ccd };
