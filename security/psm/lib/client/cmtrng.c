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
  cmtrng.c -- Support for PSM random number generator and the seeding
              thereof with data from the client.

  Created by mwelch 1999 Oct 21
 */
#include "cmtcmn.h"
#include "cmtutils.h"
#include "messages.h"
#include "rsrcids.h"
#include <string.h>

CMTStatus
CMT_EnsureInitializedRNGBuf(PCMT_CONTROL control)
{
    if (control->rng.outBuf == NULL)
    {
        control->rng.outBuf = (char *) calloc(RNG_OUT_BUFFER_LEN, sizeof(char));
        if (control->rng.outBuf == NULL)
            goto loser;

        control->rng.validOutBytes = 0;
        control->rng.out_cur = control->rng.outBuf;
        control->rng.out_end = control->rng.out_cur + RNG_OUT_BUFFER_LEN;
        
        control->rng.inBuf = (char *) calloc(RNG_IN_BUFFER_LEN, sizeof(char));
        if (control->rng.outBuf == NULL)
            goto loser;
    }

    return CMTSuccess;

 loser:
    if (control->rng.outBuf != NULL)
    {
        free(control->rng.outBuf);
        control->rng.outBuf = NULL;
    }
    if (control->rng.inBuf != NULL)
    {
        free(control->rng.inBuf);
        control->rng.inBuf = NULL;
    }

    return CMTFailure;
}


size_t 
CMT_RequestPSMRandomData(PCMT_CONTROL control, 
                         void *buf, CMUint32 maxbytes)
{
    SingleNumMessage req;
    SingleItemMessage reply;
    CMTItem message;
    size_t rv = 0;
    
    /* Parameter checking */
    if (!control || !buf || (maxbytes == 0))
        goto loser;

    /* Initialization. */
    memset(&reply, 0, sizeof(SingleItemMessage));

    /* Ask PSM for the data. */
    req.value = maxbytes;
    if (CMT_EncodeMessage(SingleNumMessageTemplate, &message, &req) != CMTSuccess)
        goto loser;

    /* Set the message request type */
    message.type = SSM_REQUEST_MESSAGE | SSM_MISC_ACTION | SSM_MISC_GET_RNG_DATA;
    
    /* Send the message and get the response */
    if (CMT_SendMessage(control, &message) == CMTFailure)
        goto loser;

    /* Validate the message reply type */
    if (message.type != (SSM_REPLY_OK_MESSAGE | SSM_MISC_ACTION | SSM_MISC_GET_RNG_DATA))
        goto loser;

    /* Decode message */
    if (CMT_DecodeMessage(SingleItemMessageTemplate, &reply, &message) != CMTSuccess)
        goto loser;

    /* Success - fill the return buf with what we got */
    if (reply.item.len > maxbytes)
        reply.item.len = maxbytes;

    memcpy(buf, reply.item.data, reply.item.len);
    rv = reply.item.len;

 loser:
    if (reply.item.data)
        free(reply.item.data);
    if (message.data)
        free(message.data);

    return rv;
}

size_t 
CMT_GenerateRandomBytes(PCMT_CONTROL control, 
                        void *buf, CMUint32 maxbytes)
{
    CMUint32 remaining = maxbytes;
    CMT_RNGState *rng = &(control->rng);
    char *walk = (char *) buf;

    /* Is there already enough in the incoming cache? */
    while(remaining > rng->validInBytes)
    {
        /* Get what we have on hand. */
        memcpy(walk, rng->in_cur, rng->validInBytes);
        walk += rng->validInBytes;
        remaining -= rng->validInBytes;

        /* Request a buffer from PSM. */
        rng->validInBytes = CMT_RequestPSMRandomData(control, 
                                                     rng->inBuf, 
                                                     RNG_IN_BUFFER_LEN);
        if (rng->validInBytes == 0)
            return (maxbytes - remaining); /* call failed */
        rng->in_cur = rng->inBuf;
    }
    if (remaining > 0)
    {
        memcpy(walk, rng->in_cur, remaining);
        rng->in_cur += remaining;
        rng->validInBytes -= remaining;
    }
    return maxbytes;
}

void
cmt_rng_xor(void *dstBuf, void *srcBuf, int len)
{
    unsigned char *s = (unsigned char*) srcBuf;
    unsigned char *d = (unsigned char*) dstBuf;
    unsigned char tmp;
    int i;

    for(i=0; i<len; i++, s++, d++)
    {
        tmp = *d;
        /* I wish C had circular shift operators. So do others on the team. */
        tmp = ((tmp << 1) | (tmp >> 7));
        *d = tmp ^ *s;
    }
}

CMTStatus 
CMT_RandomUpdate(PCMT_CONTROL control, void *data, size_t numbytes)
{
    size_t dataLeft = numbytes, cacheLeft;
    char *walk = (char *) data;

    if (CMT_EnsureInitializedRNGBuf(control) != CMTSuccess)
        goto loser;

    /* If we have more than what the buffer can handle, wrap around. */
    cacheLeft = (control->rng.out_end - control->rng.out_cur);
    while (dataLeft >= cacheLeft)
    {
        cmt_rng_xor(control->rng.out_cur, walk, cacheLeft);
        walk += cacheLeft;
        dataLeft -= cacheLeft;

        control->rng.out_cur = control->rng.outBuf;

        /* Max out used space */
        control->rng.validOutBytes = cacheLeft = RNG_OUT_BUFFER_LEN;
    }

    /* 
       We now have less seed data available than we do space in the buf.
       Write what we have and update validOutBytes if we're not looping already.
     */
    cmt_rng_xor(control->rng.out_cur, walk, dataLeft);
    control->rng.out_cur += dataLeft;
    if (control->rng.validOutBytes < RNG_OUT_BUFFER_LEN)
        control->rng.validOutBytes += dataLeft;

    return CMTSuccess;
 loser:
    return CMTFailure;
}

size_t
CMT_GetNoise(PCMT_CONTROL control, void *buf, CMUint32 maxbytes)
{
    /* ### mwelch - GetNoise and GenerateRandomBytes can be the 
       same function now, because presumably the RNG is being 
       seeded with environmental noise on the PSM end before we 
       make any of these requests */
    return CMT_GenerateRandomBytes(control, buf, maxbytes);
}

CMTStatus 
CMT_FlushPendingRandomData(PCMT_CONTROL control)
{
    CMTItem message;

    memset(&message, 0, sizeof(CMTItem));

    if (CMT_EnsureInitializedRNGBuf(control) != CMTSuccess)
        return CMTFailure; /* couldn't initialize RNG buffer */

    if (control->rng.validOutBytes == 0)
        return CMTSuccess; /* no random data available == we're flushed */

    /* We have random data available. Send this to PSM.
       We're sending an event, so no reply is needed. */
    message.type = SSM_EVENT_MESSAGE 
        | SSM_MISC_ACTION 
        | SSM_MISC_PUT_RNG_DATA;
    message.len = control->rng.validOutBytes;
    message.data = (unsigned char *) calloc(message.len, sizeof(char));
    if (!message.data)
        goto loser;
    memcpy(message.data, control->rng.outBuf, message.len);

    if (CMT_TransmitMessage(control, &message) == CMTFailure)
        goto loser;

    /* Clear the RNG ring buffer, we've used that data */
    control->rng.out_cur = control->rng.outBuf;
    control->rng.validOutBytes = 0;
    /* zero the buffer, because we XOR in new data */
    memset(control->rng.outBuf, 0, RNG_OUT_BUFFER_LEN);

    goto done;
 loser:
    if (message.data)
        free(message.data);
    return CMTFailure;
 done:
    return CMTSuccess;
}
