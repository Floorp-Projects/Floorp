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
 * Timebomb code 
 *
 *****************************************************************************/

#ifdef TIMEBOMB
#include "prlong.h"
#include "timestamp.h"
extern SSMTimeBombExpired;

SSMStatus SSMControlConnection_ProcessHello(SSMControlConnection *ctrl, 
                                                  SECItem *msg);

SSMStatus SSMTimebomb_SendMessage(SSMControlConnection * control,
                                 SECItem * message);
SECItem * SSMTimebomb_GetMessage(SSMControlConnection * control);

void SSM_CheckTimebomb(void)
{
 PRTime timeNow = PR_Now();
 if (LL_CMP(timeNow, >,expirationTime))
   SSMTimeBombExpired = PR_TRUE;
 else SSMTimeBombExpired = PR_FALSE;
}

void SSM_CartmanHasExpired(SSMControlConnection * control)
{
  /* time has expired for this module */
  SSMStatus rv = PR_SUCCESS;
  SECItem * message = NULL;
  PRInt32 tmp, sent;
  SingleNumMessage reply;

  SSM_DEBUG("timebomb: wait for hello message\n");
 
  /* first, let control connection establish, to get browser to come up */
  message = SSMTimebomb_GetMessage(control);
  if (!message)
    goto loser;
  /* PR_ASSERT(message && 
            (message->type&SSM_TYPE_MASK == SSM_HELLO_MESSAGE));
   */
  tmp = message->type & SSM_TYPE_MASK;
  if (tmp != SSM_HELLO_MESSAGE) 
    SSM_DEBUG("timebomb: type is %x \n", tmp);
  PR_ASSERT(tmp == SSM_HELLO_MESSAGE);
  SSM_FreeMessage(message);
  message = SSM_ConstructMessage(0); 
  if (!message) 
    goto loser;
  message->type = SSM_REPLY_ERR_MESSAGE | SSM_HELLO_MESSAGE;
  reply.value = SSM_TIME_EXPIRED;
  if (CMT_EncodeMessage(SingleNumMessageTemplate, message, &reply) != CMTSuccess) {
      goto loser;
  }
  if (!message->len || !message->data) 
    goto loser;
  SSMTimebomb_SendMessage(control, message);
  SSM_DEBUG("timebomb: sent error reply for hello message, exiting...\n");

 loser: 
   /* there is some error. Clean up and exit */ 
 done: 
   /* just quit */
   exit(0);
}   

#endif /* TIMEBOMB */