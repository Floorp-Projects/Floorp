/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#include "progress.h"
#include "nsProgressManager.h"

#ifdef XP_MAC
#pragma export on
#endif

void
PM_EnsureProgressManager(MWContext* context)
{
    nsProgressManager::Ensure(context);
}

void
PM_ReleaseProgressManager(MWContext* context)
{
    nsProgressManager::Release(context);
}

void
PM_StartBinding(MWContext* context, const URL_Struct* url)
{
    if (context->progressManager) {
        context->progressManager->OnStartBinding(url);
    }
}


void
PM_Progress(MWContext* context, const URL_Struct* url, PRUint32 bytesReceived, PRUint32 contentLength)
{
    if (context->progressManager) {
        context->progressManager->OnProgress(url, bytesReceived, contentLength);
    }
}


void
PM_Status(MWContext* context, const URL_Struct* url, const char* message)
{
    if (context->progressManager) {
        context->progressManager->OnStatus(url, message);
    }
}


void
PM_Suspend(MWContext* context, const URL_Struct* url)
{
    if (context->progressManager) {
        context->progressManager->OnSuspend(url);
    }
}


void
PM_Resume(MWContext* context, const URL_Struct* url)
{
    if (context->progressManager) {
        context->progressManager->OnResume(url);
    }
}




void
PM_StopBinding(MWContext* context, const URL_Struct* url, PRInt32 status, const char* message)
{
    if (context->progressManager) {
        context->progressManager->OnStopBinding(url, status, message);
    }
}

#ifdef XP_MAC
#pragma export off
#endif

