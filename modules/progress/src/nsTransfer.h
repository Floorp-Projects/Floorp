/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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


