/* ***** BEGIN LICENSE BLOCK *****
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
 * Petr Kostka.
 * Portions created by the Initial Developer are Copyright (C) 2007
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

#ifndef NSPROTECTEDAUTHTHREAD_H_
#define NSPROTECTEDAUTHTHREAD_H_

#include <nsCOMPtr.h>
#include "keyhi.h"
#include "nspr.h"

#include "mozilla/Mutex.h"
#include "nsIProtectedAuthThread.h"

class nsProtectedAuthThread : public nsIProtectedAuthThread
{
private:
    mozilla::Mutex mMutex;

    nsCOMPtr<nsIObserver> mStatusObserver;

    bool        mIAmRunning;
    bool        mStatusObserverNotified;
    bool        mLoginReady;

    PRThread    *mThreadHandle;

    // Slot to do authentication on
    PK11SlotInfo*   mSlot;

    // Result of the authentication
    SECStatus       mLoginResult;

public:

    nsProtectedAuthThread();
    virtual ~nsProtectedAuthThread();

    NS_DECL_ISUPPORTS
    NS_DECL_NSIPROTECTEDAUTHTHREAD

    // Sets parameters for the thread
    void SetParams(PK11SlotInfo *slot);

    // Gets result of the protected authentication operation
    SECStatus GetResult();

    void Join(void);

    void Run(void);
};

#endif // NSPROTECTEDAUTHTHREAD_H_
