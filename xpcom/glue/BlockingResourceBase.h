/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: sw=4 ts=4 et :
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
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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


#ifndef mozilla_BlockingResourceBase_h
#define mozilla_BlockingResourceBase_h

#include "nscore.h"
#include "prlog.h"
#include "nsError.h"
#include "nsDebug.h"

//
// This header is not meant to be included by client code.
//

namespace mozilla {


/**
 * Base class of resources that might block clients trying to acquire them.  
 * Aids in debugging, deadlock detection, etc.
 **/
class NS_COM_GLUE BlockingResourceBase
{
protected:
    BlockingResourceBase() {
    }
    // Needs to be kept in sync with kResourceTypeNames.
    enum BlockingResourceType { eMutex, eMonitor, eCondVar };

#ifdef DEBUG
    ~BlockingResourceBase();

    /**
     * Initialize this blocking resource.  Also hooks the resource into
     * instrumentation code.
     * @param aResource Unique handle for the underlying blocking resource.
     * @param aName A meaningful, unique name that can be used in
     *              error messages, et al.
     * @param aType The specific type of |aResource|, if any.
     **/
    void Init(void* aResource, 
              const char* aName,
              BlockingResourceType aType);

    /*
     * The following variables and methods are used by the deadlock detector.
     * To understand how they're used, it's best to give a quick overview
     * of how the deadlock detector works.
     *
     * The deadlock detector ensures that all blocking resources are
     * acquired according to a partial order P.  One type of blocking
     * resource is a lock.  If a lock l1 is acquired (locked) before l2, 
     * then we say that $l1 <_P l2$.  The detector flags an error if two
     * locks l1 and l2 have an inconsistent ordering in P; that is, if both
     * $l1 <_P l2$ and $l2 <_P l1$.  This is a potential error because a
     * thread acquiring l1,l2 according to the first order might simultaneous
     * run with a thread acquiring them according to the second order.
     * If this happens under the right conditions, then the
     * acquisitions will deadlock.
     *
     * This deadlock detector doesn't know at compile-time what P is.  So, 
     * it tries to discover the order at run time.  More precisely, it 
     * finds <i>some</i> order P, then tries to find chains of resource
     * acquisitions that violate P.  An example acquisition sequence, and
     * the orders they impose, is the following:
     *   l1.lock()   // current chain: [ l1 ]
     *               // order: { }
     *
     *   l2.lock()   // current chain: [ l1, l2 ]
     *               // order: { l1 <_P l2 }
     *
     *   l3.lock()   // current chain: [ l1, l2, l3 ]
     *               // order: { l1 <_P l2, l2 <_P l3, l1 <_P l3 }
     *               // (note: <_P is transitive, so also $l1 <_P l3$)
     *
     *   l2.unlock() // current chain: [ l1, l3 ]
     *               // order: { l1 <_P l2, l2 <_P l3, l1 <_P l3 }
     *               // (note: it's OK, but weird, that l2 was unlocked out
     *               //  of order.  we still have l1 <_P l3).
     *
     *   l2.lock()   // current chain: [ l1, l3, l2 ]
     *               // order: { l1 <_P l2, l2 <_P l3, l1 <_P l3,
     *                                      l3 <_P l2 (!!!) }
     * BEEP BEEP!  Here the detector will flag a potential error, since 
     * l2 and l3 were used inconsistently (and potentially deadlocking-
     * inducingly).
     *
     * In reality, this is somewhat more complicated because each thread
     * has its own acquisition chain.  In addition, the example above 
     * elides several important details from the detector implementation.
     */

    /**
     * mResource
     * Some handle to a resource that may block acquisition.  Used to 
     * identify the resource in acquisition chains.
     **/
    void* mResource;

    /**
     * mType
     * The more specific type of this resource.  Used to implement special
     * semantics (e.g., reentrancy of monitors.)
     **/
    BlockingResourceType mType;

    /**
     * mChainPrev
     * A series of resource acquisitions creates a chain of orders.  This
     * chain is implemented as a linked list; |mChainPrev| points to the
     * resource most recently Show()'n before this one.
     **/
    BlockingResourceBase* mChainPrev;

    /**
     * Acquire
     * Add this resource to the front of the current thread's acquisition 
     * chain.  Should be called by, e.g., Mutex::Lock().
     **/
    void Acquire();
    
    /**
     * Release
     * Remove this resource from the current thread's acquisition chain.
     * The resource does not have to be at the front of the chain, although
     * it is confusing to release resources in a different order than they
     * are acquired.
     **/
    void Release();

#else
    ~BlockingResourceBase() {
    }

    void Init(void* aResource, 
              const char* aName,
              BlockingResourceType aType) { }

    void            Acquire() { }
    void            Release() { }

#endif
};


} // namespace mozilla


#endif // mozilla_BlockingResourceBase_h
