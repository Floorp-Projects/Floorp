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
/* Cartman Server specific includes */
#include "serv.h"
#include "ctrlconn.h"
#include "messages.h"
#include "msgthread.h"

struct MsgThreadCtx
{
  SSMStatus (*f)(SSMControlConnection *, SECItem *);
  SSMControlConnection *ctrl;
  SECItem *msg;
};
typedef struct MsgThreadCtx MsgThreadCtx;

static void
freectx(MsgThreadCtx *ctx)
{
  if (!ctx) return;

  SSM_FreeResource(&ctx->ctrl->super.super);
  SECITEM_FreeItem(ctx->msg, PR_TRUE);
  PR_Free(ctx);
}

static void
threadfunc(void *arg)
{
  SSMStatus rv;
  MsgThreadCtx *ctx = (MsgThreadCtx*)arg;

  rv = ctx->f(ctx->ctrl, ctx->msg);
  if (rv != SSM_SUCCESS) {
    ssmcontrolconnection_encode_err_reply(ctx->msg, rv);
  }

  ssmcontrolconnection_send_message_to_client(ctx->ctrl, ctx->msg);

  freectx(ctx);
}

/*
 * This function frees the Control Connection and the Message
 * data before returning.
 */
SSMStatus
SSM_ProcessMsgOnThread(SSMStatus (*f)(SSMControlConnection *, SECItem *),
                       SSMControlConnection *ctrl, SECItem *msg)
{
  SSMStatus rv = PR_SUCCESS;
  MsgThreadCtx *ctx = 0;
  PRThread *thrd;

  ctx = (MsgThreadCtx*)PR_Malloc(sizeof (MsgThreadCtx));
  if (!ctx) { rv = PR_FAILURE; goto loser; }

  ctx->f = f;
  ctx->ctrl = ctrl;
  SSM_GetResourceReference(&ctrl->super.super);
  ctx->msg = SECITEM_DupItem(msg);

  thrd = PR_CreateThread(PR_USER_THREAD, threadfunc, ctx, PR_PRIORITY_NORMAL,
                  PR_LOCAL_THREAD, PR_UNJOINABLE_THREAD, 0);
  if (!thrd) goto loser;

  ctx = 0;  /* Thread now owns the context */

loser:
  if (ctx) freectx(ctx);

  return rv;
}

static void
threadfunc1(void *arg)
{
  SSMStatus rv;
  MsgThreadCtx *ctx = (MsgThreadCtx*)arg;

  rv = ctx->f(ctx->ctrl, ctx->msg);
  /* Can't indicate failure */
  if (rv != SSM_SUCCESS) SSM_DEBUG("Thread processing function failed\n");

  freectx(ctx);
}

SSMStatus
SSM_ProcessMsgOnThreadReply(SSMStatus (*f)(SSMControlConnection *, SECItem *),
                            SSMControlConnection *ctrl, SECItem *msg)
{
  SSMStatus rv = PR_SUCCESS;
  MsgThreadCtx *ctx = 0;
  PRThread *thrd;
  SingleNumMessage reply;

  ctx = (MsgThreadCtx*)PR_Malloc(sizeof (MsgThreadCtx));
  if (!ctx) { rv = PR_FAILURE; goto loser; }

  ctx->f = f;
  ctx->ctrl = ctrl;
  SSM_GetResourceReference(&ctrl->super.super);
  ctx->msg = SECITEM_DupItem(msg);

  thrd = PR_CreateThread(PR_USER_THREAD, threadfunc1, ctx, PR_PRIORITY_NORMAL,
                  PR_LOCAL_THREAD, PR_UNJOINABLE_THREAD, 0);
  if (!thrd) goto loser;

  ctx = 0;  /* Thread now owns the context */

  free(msg->data);
  msg->data = 0;

  /* Send response */
  reply.value = 0;
  CMT_EncodeMessage(SingleNumMessageTemplate, (CMTItem*)msg, &reply);

/*  ssmcontrolconnection_send_message_to_client(ctrl, msg); */

loser:
  if (ctx) freectx(ctx);

  return rv;
}
