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

