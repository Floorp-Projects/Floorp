/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef MKSELECT_H
#define MKSELECT_H

PR_BEGIN_EXTERN_C

extern void NET_SetReadPoll(PRFileDesc *fd); 

extern void NET_ClearReadPoll(PRFileDesc *fd);

extern void NET_SetConnectPoll(PRFileDesc *fd);

extern void NET_ClearConnectPoll(PRFileDesc *fd); 

/* this function turns on and off a reasonably slow timer that will
 * push the netlib along even when it doesn't get any onIdle time.
 * this is unfortunately necessary on windows because when a modal
 * dialog is up it won't call the OnIdle loop which is currently the
 * source of our events.
 */
extern void NET_SetNetlibSlowKickTimer(PRBool set);

/* set and clear the callnetliballthetime busy poller.
 * all reference counting is done internally
 *
 * the caller string is used in debug builds to detect callers that
 * don't set and clear correctly
 */
extern void NET_SetCallNetlibAllTheTime(MWContext *context, char *caller);
extern void NET_ClearCallNetlibAllTheTime(MWContext *context, char *caller);
extern PRBool NET_IsCallNetlibAllTheTimeSet(MWContext *context, char *caller);

extern void NET_SetReadSelect(MWContext *context, PRFileDesc *file_desc);
extern void NET_ClearReadSelect(MWContext *context, PRFileDesc *file_desc);
extern void NET_SetFileReadSelect(MWContext *context, int file_desc);
extern void NET_ClearFileReadSelect(MWContext *context, int file_desc);
extern void NET_SetConnectSelect(MWContext *context, PRFileDesc *file_desc);
extern void NET_ClearConnectSelect(MWContext *context, PRFileDesc *file_desc);
extern void NET_ClearDNSSelect(MWContext *context, PRFileDesc *file_desc);

void net_process_net_timer_callback(void *closure);
void net_process_slow_net_timer_callback(void *closure);

PR_END_EXTERN_C

#endif /* MKSELECT_H */
