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

#define SSMPRStatus PRStatus
#define SSMPR_SUCCESS PR_SUCCESS
#define SSMPR_FAILURE PR_FAILURE

#define SSMPR_INVALID_ARGUMENT_ERROR PR_INVALID_ARGUMENT_ERROR
#define SSMPR_OUT_OF_MEMORY_ERROR    PR_OUT_OF_MEMORY_ERROR

#define SSMPRInt32 PRInt32
#define SSMPRUint32 PRUint32

#define SSMPR_ntohl  PR_ntohl
#define SSMPR_htonl  PR_htonl
#define SSMPORT_Free   PORT_Free
#define SSMPORT_ZAlloc PORT_ZAlloc

#define SSMPR_SetError PR_SetError
#define SSMPR_GetError PR_GetError
#define SSMPORT_SetError PORT_SetError
#define SSMPORT_GetError PORT_GetError




