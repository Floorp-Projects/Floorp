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
 * Portions created by the Initial Developer are Copyright (C) 1994-2000
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

#ifndef _SECRNG_H_
#define _SECRNG_H_
/*
 * secrng.h - public data structures and prototypes for the secure random
 *	      number generator
 *
 * $Id: secrng.h,v 1.6 2006/10/12 02:23:49 wtchang%redhat.com Exp $
 */

/******************************************/
/*
** Random number generation. A cryptographically strong random number
** generator.
*/

#include "blapi.h"

/* the number of bytes to read from the system random number generator */
#define SYSTEM_RNG_SEED_COUNT 1024

SEC_BEGIN_PROTOS

/*
** The following functions are provided by the security library
** but are differently implemented for the UNIX, Win, and OS/2
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

/*
** Get maxbytes bytes of random data from the system random number
** generator.
** Returns the number of bytes copied into buf -- maxbytes if success
** or zero if error.
** Errors:
**   PR_NOT_IMPLEMENTED_ERROR   There is no system RNG on the platform.
**   SEC_ERROR_NEED_RANDOM      The system RNG failed.
*/
extern size_t RNG_SystemRNG(void *buf, size_t maxbytes);

SEC_END_PROTOS

#endif /* _SECRNG_H_ */
