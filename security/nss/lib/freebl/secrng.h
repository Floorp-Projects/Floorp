/*
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
 * The Original Code is the Netscape security libraries.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1994-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
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
 * may use your version of this file under either the MPL or the
 * GPL.
 */

#ifndef _SECRNG_H_
#define _SECRNG_H_
/*
 * secrng.h - public data structures and prototypes for the secure random
 *	      number generator
 *
 * $Id: secrng.h,v 1.4 2003/05/30 23:31:19 wtc%netscape.com Exp $
 */

/******************************************/
/*
** Random number generation. A cryptographically strong random number
** generator.
*/

#include "blapi.h"

SEC_BEGIN_PROTOS

/*
** The following 3 functions are provided by the security library
** but are differently implemented for the UNIX, Mac and Win
** versions
*/

/*
** Get the "noisiest" information available on the system.
** The amount of data returned depends on the system implementation.
** It will not exceed maxbytes, but may be (much) less.
** Returns number of noise bytes copied into buf, or zero if error.
*/
extern size_t RNG_GetNoise(void *buf, size_t maxbytes);

/*
** RNG_SystemInfoForRNG should be called before any use of SSL. It
** gathers up the system specific information to help seed the
** state of the global random number generator.
*/
extern void RNG_SystemInfoForRNG(void);

/* 
** Use the contents (and stat) of a file to help seed the
** global random number generator.
*/
extern void RNG_FileForRNG(const char *filename);

SEC_END_PROTOS

#endif /* _SECRNG_H_ */
