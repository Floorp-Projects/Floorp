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
#include "cmtcmn.h"
#include "cmtutils.h"
#include "messages.h"
#ifdef XP_MAC
#include "cmtmac.h"
#endif

CMTStatus CMT_SecurityAdvisor(PCMT_CONTROL control, CMTSecurityAdvisorData* data, CMUint32 *resID)
{
    CMTItem message = {0, NULL, 0};
    SecurityAdvisorRequest request;
    SingleNumMessage reply;

    if (!control) {
        return CMTFailure;
    }

    if (!data) {
        return CMTFailure;
    }

    request.infoContext = data->infoContext;
    request.resID = data->resID;
    request.hostname = data->hostname;
	request.senderAddr = data->senderAddr;
	request.encryptedP7CInfo = data->encryptedP7CInfo;
	request.signedP7CInfo = data->signedP7CInfo;
	request.decodeError = data->decodeError;
	request.verifyError = data->verifyError;
	request.encryptthis = data->encryptthis;
	request.signthis = data->signthis;
	request.numRecipients = data->numRecipients;
	request.recipients = data->recipients;
    message.type = SSM_REQUEST_MESSAGE | SSM_SECURITY_ADVISOR;

    if (CMT_EncodeMessage(SecurityAdvisorRequestTemplate, &message, &request) != CMTSuccess) {
        goto loser;
    }

    /* Send the message and get the response */
	if (CMT_SendMessage(control, &message) != CMTSuccess) {
        goto loser;
	}

    if (message.type != (SSM_REPLY_OK_MESSAGE | SSM_SECURITY_ADVISOR)) {
        goto loser;
    }

    /* Decode the message */
    if (CMT_DecodeMessage(SingleNumMessageTemplate, &reply, &message) != CMTSuccess) {
        goto loser;
    }

    *resID = reply.value;

    return CMTSuccess;
loser:

    if (message.data) {
        free(message.data);
    }
    return CMTFailure;
}




