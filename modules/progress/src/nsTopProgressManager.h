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


#ifndef nsTopProgressManager_h__
#define nsTopProgressManager_h__

#include "nsProgressManager.h"
#include "nsTime.h"

struct AggregateTransferInfo {
    PRUint32 ObjectCount;
    PRUint32 CompleteCount;
    PRUint32 SuspendedCount;
    PRUint32 MSecRemaining;
    PRUint32 BytesReceived;
    PRUint32 ContentLength;
    PRUint32 UnknownLengthCount;
};



/**
 * An <b>nsTopProgressManager</b> is a progress manager that is associated
 * with a top-level window context. It does the dirty work of managing the
 * progress bar and the status text.
 */
class nsTopProgressManager : public nsProgressManager
{
protected:
    /**
     * A table of URLs that we are monitoring progress for. The table is
     * keyed by <b>URL_Structs</b>, the values are <b>nsTransfer</b>
     * objects.
     */
    PLHashTable* fURLs;

    /**
     * The time at which the progress manager was created.
     */
    nsTime fActualStart;

    /**
     * The time at which the progress bar thinks we were created. This is used
     * with to make progress seem "monotonic": it is "slipped" backwards
     * when a transfer stalls so that the percentage complete stays constant.
     */
    nsTime fProgressBarStart;

    /**
     * The current amount of progress shown on the chrome's progress bar
     */
    PRUint16 fProgress;

    /**
     * The default status message to display if nothing more appropriate
     * is around.
     */
    char* fDefaultStatus;

    /**
     * An FE timeout object that is used to periodically update the
     * status bar.
     */
    void* fTimeout;

    /**
     * The FE timeout callback.
     */
    static void TimeoutCallback(void* self);

    /**
     * Called to update the status bar.
     */
    virtual void Tick(void);


    /**
     * Update the progress bar
     */
    virtual void UpdateProgressBar(AggregateTransferInfo& info);


    /**
     * Update the status message
     */
    virtual void UpdateStatusMessage(AggregateTransferInfo& info);


public:
    nsTopProgressManager(MWContext* context);
    virtual ~nsTopProgressManager(void);

    NS_IMETHOD
    OnStartBinding(const URL_Struct* url);

    NS_IMETHOD
    OnProgress(const URL_Struct* url, PRUint32 bytesReceived, PRUint32 contentLength);

    NS_IMETHOD
    OnStatus(const URL_Struct* url, const char* message);

    NS_IMETHOD
    OnSuspend(const URL_Struct* url);

    NS_IMETHOD
    OnResume(const URL_Struct* url);

    NS_IMETHOD
    OnStopBinding(const URL_Struct* url, PRInt32 status, const char* message);
};


#endif // nsTopProgressManager_h__

