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
/************************************************************************
 * Code to handle password requests from the the PSM module.
 * 
 ************************************************************************
 */

#include "cmtcmn.h"
#include "cmtutils.h"
#include "messages.h"

void CMT_SetAppFreeCallback(PCMT_CONTROL control, 
			    applicationFreeCallback_fn f)
{
  control->userFuncs.userFree = f;
}
    
void CMT_ServicePasswordRequest(PCMT_CONTROL cm_control, CMTItem * requestData)
{ 
  CMTItem response = {0, NULL, 0};
  PasswordRequest request;
  PasswordReply reply;
  void * clientContext;

  /********************************************
   *  What we trying to do here: 
   *  1) Throw up a dialog box and request a password.
   *  2) Create a message and send it to the PSM module.
   ********************************************
   */
  
  /* Decode the request */
  if (CMT_DecodeMessage(PasswordRequestTemplate, &request, requestData) != CMTSuccess) {
      goto loser;
  }

  /* Copy the client context to a pointer */
  clientContext = CMT_CopyItemToPtr(request.clientContext);

  if (cm_control->userFuncs.promptCallback == NULL) {
      goto loser;
  }
  reply.passwd = 
    cm_control->userFuncs.promptCallback(cm_control->userFuncs.promptArg, 
					 request.prompt, clientContext, 1);
  reply.tokenID = request.tokenKey;
  if (!reply.passwd) {
      /* the user cancelled the prompt or other errors occurred */
      reply.result = -1;
  }
  else {
      /* note that this includes an empty string (zero length password) */
      reply.result = 0;
  }

  /* Encode the reply */
  if (CMT_EncodeMessage(PasswordReplyTemplate, &response, &reply) != CMTSuccess) {
      goto loser;
  }

  /* Set the message response type */
  response.type = SSM_EVENT_MESSAGE | SSM_AUTH_EVENT;
  CMT_TransmitMessage(cm_control, &response);
  goto done;
loser:
  /* something has gone wrong */

done:
  /*clean up anyway */
  /* We can't just free up memory allocated by the host
     application because the versions of free may not match up.
     When you run the plug-in with an optimized older browser,
     you'll see tons of Asserts (why they still have asserts in an
     optimized build is a different question, but without them 
     I wouldn't have figured out this problem) about a pointer not
     being a valid heap pointer and eventually crash.  This was 
     the offending free line.
     So we need to call a function within the browser that 
     calls the free linked in with it.  js_free is
     such a function.  But this is extremely ugly.
   */
  if (reply.passwd) 
    cm_control->userFuncs.userFree(reply.passwd);
  if (request.prompt) 
    free(request.prompt);
  return;
}

