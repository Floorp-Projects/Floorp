/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Mozilla Communicator client code.
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
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsPIEventQueueChain_h__
#define nsPIEventQueueChain_h__

#include "nsISupports.h"

// {8f310040-82a7-11d3-95bc-0060083a0bcf}
#define NS_IEVENTQUEUECHAIN_IID \
{ 0x8f310040, 0x82a7, 0x11d3, { 0x95, 0xbc, 0x0, 0x60, 0x8, 0x3a, 0xb, 0xcf } }

class nsIEventQueue;

class nsPIEventQueueChain : public nsISupports
{
public:
    NS_DEFINE_STATIC_IID_ACCESSOR(NS_IEVENTQUEUECHAIN_IID);

    /**
     * Add the given queue as the new youngest member of our chain.
     * It will not be addrefed.
     * @param aQueue the queue. must not be null.
     * @return error indication
     */
    NS_IMETHOD AppendQueue(nsIEventQueue *aQueue) = 0;

    /**
     * Remove this element from the chain.
     * @return NS_OK
     */
    NS_IMETHOD Unlink() = 0;

    /**
     * Fetch (and addref) the youngest member of the chain.
     * @param *aQueue the youngest queue. aQueue must not be null.
     * @return error indication
     */
    NS_IMETHOD GetYoungest(nsIEventQueue **aQueue) = 0;

    /**
     * Fetch (and addref) the youngest member of the chain which is
     * still accepting events, or at least still contains events in need
     * of processing.
     * @param *aQueue the youngest such queue. aQueue must not be null.
     *        *aQueue will be returned null, if no such queue is found.
     * @return error indication -- can be NS_OK even if *aQueue is 0
     */
    NS_IMETHOD GetYoungestActive(nsIEventQueue **aQueue) = 0;

    NS_IMETHOD SetYounger(nsPIEventQueueChain *aQueue) = 0;
    NS_IMETHOD GetYounger(nsIEventQueue **aQueue) = 0;

    NS_IMETHOD SetElder(nsPIEventQueueChain *aQueue) = 0;
    NS_IMETHOD GetElder(nsIEventQueue **aQueue) = 0;
};

#endif /* nsPIEventQueueChain_h___ */

