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
#include "protocol.h"
#include "prmem.h"
#include "prnetdb.h"
#include <string.h>

#ifndef NSPR20
#include "protocolport.c"
#endif

CMStatus SSM_SSMStringToString(char ** string,
			       int *len, 
			       SSMString * ssmString) 
{
  char * str = NULL;
  int realLen;
  PRStatus rv =PR_SUCCESS;

  if (!ssmString || !string ) { 
    rv = PR_INVALID_ARGUMENT_ERROR;
    goto loser;
  }

  /* in case we fail */
  *string = NULL;
  if (len) *len = 0;

  /* Convert from net byte order */
  realLen = SSMPR_ntohl(ssmString->m_length);

  str = (char *)PR_CALLOC(realLen+1); /* add 1 byte for end 0 */
  if (!str) {
    rv = PR_OUT_OF_MEMORY_ERROR;
    goto loser;
  }
  
  memcpy(str, (char *) &(ssmString->m_data), realLen);
  /* str[realLen]=0; */

  if (len) *len = realLen;
  *string = str;
  return rv;
  
loser:
  if (str) 
    PR_Free(str);
  if (string && *string) {
    PR_Free(*string);
    *string = NULL;
  }
  if (rv == PR_SUCCESS) 
    rv = PR_FAILURE;
  return rv;
}


CMStatus SSM_StringToSSMString(SSMString ** ssmString, int length, 
				  char * string)
{
  SSMPRUint32 len;
  SSMString *result = NULL;
  PRStatus rv = PR_SUCCESS;
  
  if (!string || !ssmString) {
    rv = PR_INVALID_ARGUMENT_ERROR;
    goto loser;
  }

  *ssmString = NULL; /* in case we fail */

  if (length) len = length; 
  else len = strlen(string);
  if (len <= 0) {
    rv = PR_INVALID_ARGUMENT_ERROR;
    goto loser;
  }
  result = (SSMString *) PR_CALLOC(sizeof(PRUint32) + 
				   SSMSTRING_PADDED_LENGTH(len));
  if (!result) {
    rv = PR_OUT_OF_MEMORY_ERROR;
    goto loser;
  }

  result->m_length = SSMPR_htonl(len);
  memcpy((char *) (&(result->m_data)), string, len);

  *ssmString = result;
  goto done;
  
loser:
  if (result)
    PR_Free(result);
  *ssmString = NULL;
  if (rv == PR_SUCCESS)
    rv = PR_FAILURE;
done:
  return rv;
}

