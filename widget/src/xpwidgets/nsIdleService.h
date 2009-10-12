/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
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
 * Gijs Kruitbosch <gijskruitbosch@gmail.com>.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Gijs Kruitbosch <gijskruitbosch@gmail.com> 
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

#ifndef nsIdleService_h__
#define nsIdleService_h__

#include "nsIIdleService.h"
#include "nsCOMPtr.h"
#include "nsITimer.h"
#include "nsTArray.h"
#include "nsIObserver.h"

// Class we can use to store an observer with its associated idle time
// requirement and whether or not the observer thinks it's "idle".
class IdleListener {
public:
    nsCOMPtr<nsIObserver> observer;
    PRUint32 reqIdleTime;
    PRBool isIdle;

    IdleListener(nsIObserver* obs, PRUint32 reqIT, PRBool aIsIdle = PR_FALSE) :
        observer(obs), reqIdleTime(reqIT), isIdle(aIsIdle) {}
    ~IdleListener() {}
};

class nsIdleService : public nsIIdleService
{
public:
    nsIdleService();

    // Implement nsIIdleService methods, but not the idleTime getter,
    // which is platform-dependent.
    NS_IMETHOD AddIdleObserver(nsIObserver* aObserver, PRUint32 aIdleTime);
    NS_IMETHOD RemoveIdleObserver(nsIObserver* aObserver, PRUint32 aIdleTime);

    static void IdleTimerCallback(nsITimer* aTimer, void* aClosure);
    
    void IdleTimeWasModified();

protected:
    void CheckAwayState();
    ~nsIdleService();

private:
    void StartTimer(PRUint32 aDelay);
    void StopTimer();
    nsCOMPtr<nsITimer> mTimer;
    nsTArray<IdleListener> mArrayListeners;
};

#endif // nsIdleService_h__
