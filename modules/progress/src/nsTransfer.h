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

#ifndef nsTransfer_h__
#define nsTransfer_h__

#include "prtypes.h"
#include "structs.h"
#include "nsISupports.h"
#include "nsTime.h"

/**
 * This class encapsulates the transfer state for each stream that will
 * be transferring data, is currently transferring data, or has previously
 * transferred data in the current context.
 */
class nsTransfer
{
protected:
    const URL_Struct* fURL;
    PRUint32          fBytesReceived;
    PRUint32          fContentLength;
    nsTime            fStart;
    nsTime            fSuspendStart;
    PRBool            fIsSuspended;
    char*             fStatus;
    PRBool            fComplete;
    PRInt32           fResultCode;

    static PRUint32 DefaultMSecRemaining;
    static void UpdateDefaultMSecRemaining(PRUint32 time);

public:
    nsTransfer(const URL_Struct* url);
    virtual ~nsTransfer(void);

    NS_DECL_ISUPPORTS

    void         SetProgress(PRUint32 bytesReceived, PRUint32 contentLength);
    PRUint32     GetBytesReceived(void);
    PRUint32     GetContentLength(void);
    double       GetTransferRate(void);
    PRUint32     GetMSecRemaining(void);
    void         SetStatus(const char* message);
    const char*  GetStatus(void);
    void         Suspend(void);
    void         Resume(void);
    PRBool       IsSuspended(void);
    void         MarkComplete(PRInt32 resultCode);
    PRBool       IsComplete(void);
};


#endif // nsTransfer_h__


