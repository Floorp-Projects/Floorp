/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *   Darin Fisher <darin@netscape.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsRequestObserverProxy_h__
#define nsRequestObserverProxy_h__

#include "nsIRequestObserver.h"
#include "nsIRequestObserverProxy.h"
#include "nsIEventQueue.h"
#include "nsIRequest.h"
#include "nsCOMPtr.h"

class nsARequestObserverEvent;

class nsRequestObserverProxy : public nsIRequestObserverProxy
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIREQUESTOBSERVER
    NS_DECL_NSIREQUESTOBSERVERPROXY

    nsRequestObserverProxy() { NS_INIT_ISUPPORTS(); }
    virtual ~nsRequestObserverProxy() {}

    nsIRequestObserver *Observer() { return mObserver; }

    nsresult FireEvent(nsARequestObserverEvent *);
    nsIEventQueue *EventQueue() { return mEventQ.get(); } // debugging aid
    nsresult SetEventQueue(nsIEventQueue *);

protected:
    nsCOMPtr<nsIRequestObserver> mObserver;
    nsCOMPtr<nsIEventQueue>      mEventQ;

    friend class nsOnStartRequestEvent;
    friend class nsOnStopRequestEvent;
};

class nsARequestObserverEvent
{
public:
    nsARequestObserverEvent(nsIRequest *, nsISupports *);
    virtual ~nsARequestObserverEvent() {}

    static nsARequestObserverEvent *FromPLEvent(PLEvent *p)
        { return (nsARequestObserverEvent *)
            ( (char *) p - offsetof(nsARequestObserverEvent, mEvent) ); }
    PLEvent *GetPLEvent() { return &mEvent; }

    /**
     * Implement this method to add code to handle the event
     */
    virtual void HandleEvent() = 0;

protected:
    static void PR_CALLBACK HandlePLEvent(PLEvent *);
    static void PR_CALLBACK DestroyPLEvent(PLEvent *);

    PLEvent mEvent; // this _must_ be the first data member

    nsCOMPtr<nsIRequest>  mRequest;
    nsCOMPtr<nsISupports> mContext;
};

#endif // nsRequestObserverProxy_h__
