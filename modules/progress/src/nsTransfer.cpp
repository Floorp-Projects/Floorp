/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nsTransfer.h"
#include "plstr.h"
#include "prtime.h"

////////////////////////////////////////////////////////////////////////

PRUint32 nsTransfer::DefaultMSecRemaining = 1000;

nsTransfer::nsTransfer(const URL_Struct* url)
    : fURL(url),
      fBytesReceived(0),
      fContentLength(0),
      fStart(PR_Now()),
      fIsSuspended(PR_FALSE),
      fStatus(NULL),
      fComplete(PR_FALSE),
      fResultCode(0)
{
}


nsTransfer::~nsTransfer(void)
{
    if (fStatus) {
        PL_strfree(fStatus);
        fStatus = NULL;
    }
}


NS_IMPL_ISUPPORTS(nsTransfer, kISupportsIID);


void
nsTransfer::SetProgress(PRUint32 bytesReceived, PRUint32 contentLength)
{
    fBytesReceived = bytesReceived;
    fContentLength = contentLength;
    if (fContentLength < fBytesReceived)
        fContentLength = fBytesReceived;
}


PRUint32
nsTransfer::GetBytesReceived(void)
{
    return fBytesReceived;
}


PRUint32
nsTransfer::GetContentLength(void)
{
    return fContentLength;
}


double
nsTransfer::GetTransferRate(void)
{
    if (fContentLength == 0 || fBytesReceived == 0)
        return 0;

    nsInt64 dt = nsTime(PR_Now()) - fStart;
    PRUint32 dtMSec = dt / nsInt64((PRUint32) PR_USEC_PER_MSEC);

    if (dtMSec == 0)
        return 0;

    return ((double) fBytesReceived) / ((double) dtMSec);
}

PRUint32
nsTransfer::GetMSecRemaining(void)
{
    if (IsComplete())
        return 0;

    if (fContentLength == 0 || fBytesReceived == 0)
        // no content length and/or no bytes received
        return DefaultMSecRemaining;

    PRUint32 cbRemaining = fContentLength - fBytesReceived;
    if (cbRemaining == 0)
        // not complete, but content length == bytes received.
        return DefaultMSecRemaining;

    double bytesPerMSec = GetTransferRate();
    if (bytesPerMSec == 0)
        return DefaultMSecRemaining;

    PRUint32 msecRemaining = (PRUint32) (((double) cbRemaining) / bytesPerMSec);

    return msecRemaining;
}



void
nsTransfer::SetStatus(const char* message)
{
    if (fStatus)
        PL_strfree(fStatus);

    fStatus = PL_strdup(message);
}


const char*
nsTransfer::GetStatus(void)
{
    return fStatus;
}



void
nsTransfer::Suspend(void)
{
    if (fIsSuspended)
        return;

    fSuspendStart = PR_Now();
}



void
nsTransfer::Resume(void)
{
    if (! fIsSuspended)
        return;

    nsInt64 dt = nsTime(PR_Now()) - fSuspendStart;
    fStart += dt;
}


PRBool
nsTransfer::IsSuspended(void)
{
    return fIsSuspended;
}


void
nsTransfer::MarkComplete(PRInt32 resultCode)
{
    fComplete = PR_TRUE;
    fResultCode = resultCode;

    if (fResultCode >= 0) {
        nsInt64 elapsed = nsTime(PR_Now()) - fStart;
        PRUint32 elapsedMSec= (PRUint32) (elapsed / nsInt64((PRUint32) PR_USEC_PER_MSEC));
        UpdateDefaultMSecRemaining(elapsedMSec);
    }
}


PRBool
nsTransfer::IsComplete(void)
{
    return fComplete;
}


void
nsTransfer::UpdateDefaultMSecRemaining(PRUint32 msec)
{
    // very simple. just maintain a rolling average.
    DefaultMSecRemaining += msec;
    DefaultMSecRemaining /= 2;
}

