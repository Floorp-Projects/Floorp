/* -*- Mode: c++; tab-width: 40; indent-tabs-mode: nil; c-basic-offset: 4; -*- */
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
 *   Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2009-2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir@pobox.com>
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

#ifndef nsAppShell_h__
#define nsAppShell_h__

#include "mozilla/CondVar.h"
#include "mozilla/Mutex.h"
#include "nsBaseAppShell.h"
#include "nsCOMPtr.h"
#include "nsTArray.h"
#include "nsInterfaceHashtable.h"

namespace mozilla {
class AndroidGeckoEvent;
bool ProcessNextEvent();
void NotifyEvent();
}

class nsWindow;

class nsAppShell :
    public nsBaseAppShell
{
    typedef mozilla::CondVar CondVar;
    typedef mozilla::Mutex Mutex;

public:
    static nsAppShell *gAppShell;
    static mozilla::AndroidGeckoEvent *gEarlyEvent;

    nsAppShell();

    NS_DECL_ISUPPORTS_INHERITED
    NS_DECL_NSIOBSERVER

    nsresult Init();

    void NotifyNativeEvent();

    virtual bool ProcessNextNativeEvent(bool mayWait);

    void PostEvent(mozilla::AndroidGeckoEvent *event);
    void OnResume();

    nsresult AddObserver(const nsAString &aObserverKey, nsIObserver *aObserver);
    void CallObserver(const nsAString &aObserverKey, const nsAString &aTopic, const nsAString &aData);
    void RemoveObserver(const nsAString &aObserverKey);
    void NotifyObservers(nsISupports *aSupports, const char *aTopic, const PRUnichar *aData);
    void ResendLastResizeEvent(nsWindow* aDest);

protected:
    virtual void ScheduleNativeEventCallback();
    virtual ~nsAppShell();

    Mutex mQueueLock;
    Mutex mCondLock;
    CondVar mQueueCond;
    int mNumDraws;
    mozilla::AndroidGeckoEvent *mLastDrawEvent;
    nsTArray<mozilla::AndroidGeckoEvent *> mEventQueue;
    nsInterfaceHashtable<nsStringHashKey, nsIObserver> mObserversHash;

    mozilla::AndroidGeckoEvent *PopNextEvent();
    mozilla::AndroidGeckoEvent *PeekNextEvent();
};

#endif // nsAppShell_h__

