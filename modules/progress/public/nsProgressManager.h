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


#ifndef nsProgressManager_h__
#define nsProgressManager_h__

#include "nsITransferListener.h"
#include "nsTime.h"

#include "prtypes.h"
#include "plhash.h"

#include "structs.h" // for MWContext

/**
 * An <tt>nsProgressManager</tt> object is used to manage the progress
 * bar and status bar for a context.
 */
class nsProgressManager : public nsITransferListener
{
protected:

    /**
     * The context to which the progress manager is bound.
     */
    MWContext* fContext;

    nsProgressManager(MWContext* context);
    virtual ~nsProgressManager(void);

public:

    /**
     * Ensure that a progress manager exists in the specified context,
     * creating a new one if necessary. If the context is a nested grid
     * context, this may recursively create progress managers in
     * parent contexts as well.
     */
    static void Ensure(MWContext* context);

    /**
     * Release the progress manager from the current context. This may
     * or may not destroy the progress manager, depending on the progress
     * manager's reference count. Note that the context's
     * <b>progressManager</b> field is maintained by the progress manager
     * object, and will be set to <b>NULL</b> only when the progress
     * manager's reference count goes to zero and the progress manager
     * is destroyed.
     */
    static void Release(MWContext* context);

    NS_DECL_ISUPPORTS

    // The nsITransferListener interface.

    NS_IMETHOD
    OnStartBinding(const URL_Struct* url) = 0;

    NS_IMETHOD
    OnProgress(const URL_Struct* url, PRUint32 bytesReceived, PRUint32 contentLength) = 0;

    NS_IMETHOD
    OnStatus(const URL_Struct* url, const char* message) = 0;

    NS_IMETHOD
    OnSuspend(const URL_Struct* url) = 0;

    NS_IMETHOD
    OnResume(const URL_Struct* url) = 0;

    NS_IMETHOD
    OnStopBinding(const URL_Struct* url, PRInt32 status, const char* message) = 0;
};




#endif // nsProgressManager_h__

