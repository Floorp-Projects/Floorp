/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

#ifndef nsNetThread_h___
#define nsNetThread_h___

#include "nspr.h"
#include "plevent.h"
#include "net.h" /* needed for net_CallExitRoutineProxy prototype below */
#include "nsIStreamListener.h" /* needed for ns_NewStreamListenerProxy prototype below */
#include "nsINetService.h" /* needed for NS_InitializeHttpURLFactory prototype below */

class nsNetlibThread
{
public:
    nsNetlibThread();
    ~nsNetlibThread();

    nsresult Start(void);
    nsresult Stop(void);

private:
    static void NetlibThreadMain(void* aParam);

    void NetlibMainLoop(void);

    PRBool mIsNetlibThreadRunning;
    PREventQueue* mNetlibEventQueue;
    PRThread* mThread;
};

extern "C" void NET_ClientProtocolInitialize();
nsresult NS_InitNetlib(void);
nsIStreamListener* ns_NewStreamListenerProxy(nsIStreamListener* aListener);

extern "C" MODULE_PRIVATE void
net_CallExitRoutineProxy(Net_GetUrlExitFunc* exit_routine,
                         URL_Struct*         URL_s,
                         int                 status,
                         FO_Present_Types    format_out,
                         MWContext*          window_id);

extern "C" NS_NET nsresult
NS_InitializeHttpURLFactory(nsINetService* inet);

#endif /* nsNetThread_h__ */

