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
/*****************************************************************************
 *
 *
 *
 *****************************************************************************
 */

#ifndef NULL
#define NULL 0x00000000
#endif

#define SSMPR_BYTES_PER_INT  4
#define SSMPR_BYTES_PER_LONG  4

/******************************************************************
 *  No NSPR - define all the SSMPR values and functions here   
 ******************************************************************
 */

typedef enum { SSMPR_SUCCESS = 0,  SSMPR_FAILURE = -1 } SSMPRStatus;
enum { 
  SSMPR_INVALID_ARGUMENT_ERROR = -6000,
  SSMPR_OUT_OF_MEMORY_ERROR =  -5987
};

#if SSMPR_BYTES_PER_INT == 4
typedef unsigned int SSMPRUint32;
typedef int SSMPRInt32; 
#elif SSMPR_BYTES_PER_LONG == 4
typedef unsigned long SSMPRUint32;
typedef long SSMPRInt32;
#else
#error No suitable type for SSMPRInt32/SSMPRUint32
#endif

/*******************************************************************
 * Use libc functions instead 
 *******************************************************************
 */
#include <sys/types.h>
#ifdef WIN32
#include <winsock.h>
#else
#include <netinet/in.h>
#endif
#define SSMPR_ntohl  ntohl
#define SSMPR_htonl  htonl

#include <stdlib.h>
#define SSMPORT_Free   free
#define SSMPR_sprint   printf
#define SSMPORT_ZAlloc malloc

extern SSMPRInt32 ssmprErrno;
#define SSMPR_SetError(x, y) SSMPORT_SetError(x)
#define SSMPR_GetError SSMPORT_GetError
void SSMPORT_SetError(SSMPRInt32 errorcode);




