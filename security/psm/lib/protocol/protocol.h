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
/*
  protocol.h - Definitions of various items to support the PSM protocol.
 */

#ifndef __PROTOCOL_H__
#define __PROTOCOL_H__


#include "rsrcids.h"

#define SSMPRStatus SSMStatus
#define SSMPR_SUCCESS SSM_SUCCESS
#define SSMPR_FAILURE SSM_FAILURE

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

/* 
   Current version of PSM protocol. 
   Increment this value when the protocol changes. 
*/

#define SSMSTRING_PADDED_LENGTH(x) ((((x)+3)/4)*4)
#define SSMPORT_ZNEW(type) (type*)SSMPORT_ZAlloc(sizeof(type))
#define SSMPORT_ZNewArray(type,size) (type*)SSMPORT_ZAlloc(sizeof(type)*(size))
/* Various message structs */

struct _SSMHelloRequest {
  CMUint32          m_version; /* Protocol version supported by client */
  struct _SSMString m_profileName; /* Name of user profile (where to find 
				 certs etc) */
};

struct _SSMHelloReply {
  CMInt32           m_result; /* Error, if any, which occurred 
				 (0 == success) */
  CMUint32          m_version; /* Protocol version supported by PSM */
  struct _SSMString m_nonce; /* Session nonce -- must be written to data channels */
};

struct _SSMRequestSSLDataConnection
{
  CMUint32          m_flags; /* Flags to indicate to SSM what to do with
				the connection */
  CMUint32          m_port; /* Port number to connect to */
  struct _SSMString m_hostIP; /* IP address of final target machine (not proxy) */
  /* struct _SSMString m_hostName; Host name of target machine (for server auth) -- not accessed directly */
};

struct _SSMReplySSLDataConnection {
  CMInt32 m_result; /* Error, if any, which occurred (0 == success) */
  CMUint32 m_connectionID; /* Connection ID of newly opened channel */
  CMUint32 m_port; /* Port number to which to connect on PSM */
};


struct _SSMRequestSecurityStatus {
  CMUint32 m_connectionID; /* ID of connection of which to stat */
};

struct _SSMReplySecurityStatus {
  CMInt32 m_result; /* Error, if any, which occurred (0 == success) */
  CMUint32 m_keySize; /* Key size */
  CMUint32 m_secretKeySize; /* Secret key size */
  struct _SSMString m_cipherName; /* Name of cipher in use */
  /* SSMString m_certificate; -- DER encoded cert
     We do not access this as a field, we have to skip over m_cipherName */
};

/* 
   Use this macro to jump over strings.
   For example, if you wanted to access m_certificate above, 
   use a line like the following:

   char *ptr = &(reply->m_cipherName) + SSM_SIZEOF_STRING(reply->m_cipherName);
*/
#define SSM_SIZEOF_STRING(str) (SSMSTRING_PADDED_LENGTH(PR_ntohl((str).m_length)) + sizeof(CMUint32))


typedef struct _SSMHelloRequest SSMHelloRequest;
typedef struct _SSMHelloReply SSMHelloReply;
typedef struct _SSMRequestSSLDataConnection SSMRequestSSLDataConnection;
typedef struct _SSMReplySSLDataConnection SSMReplySSLDataConnection;
typedef struct _SSMRequestSecurityStatus SSMRequestSecurityStatus;
typedef struct _SSMReplySecurityStatus SSMReplySecurityStatus;

/* 
   Functions to convert between an SSMString and a C string.
   Return values are allocated using PR_Malloc (which means that
   SSMPR_Free must be used to free up the memory after use). 
*/
CMTStatus SSM_StringToSSMString(SSMString ** ssmString, int len, char * string);
CMTStatus SSM_SSMStringToString(char ** string,int *len, SSMString * ssmString);


#endif /* __PROTOCOL_H__ */
