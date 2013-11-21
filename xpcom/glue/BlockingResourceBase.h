/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: sw=4 ts=4 et :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#ifndef mozilla_BlockingResourceBase_h
#define mozilla_BlockingResourceBase_h

#include "prlog.h"

#include "nscore.h"
#include "nsDebug.h"
#include "nsError.h"
#include "nsTraceRefcnt.h"

#ifdef DEBUG
#include "prinit.h"
#include "prthread.h"

#include "nsStringGlue.h"

#include "mozilla/DeadlockDetector.h"
#include "nsXPCOM.h"
#endif

//
// This header is not meant to be included by client code.
//

namespace mozilla {


/**
 * BlockingResourceBase
 * Base class of resources that might block clients trying to acquire them.  
 * Does debugging and deadlock detection in DEBUG builds.
 **/
class NS_COM_GLUE BlockingResourceBase
{
public:
    // Needs to be kept in sync with kResourceTypeNames.
    enum BlockingResourceType { eMutex, eReentrantMonitor, eCondVar };

    /**
     * kResourceTypeName
     * Human-readable version of BlockingResourceType enum.
     */
    static const char* const kResourceTypeName[];


#ifdef DEBUG

private:
    // forward declaration for the following typedef
    struct DeadlockDetectorEntry;

    // ``DDT'' = ``Deadlock Detector Type''
    typedef DeadlockDetector<DeadlockDetectorEntry> DDT;

    /**
     * DeadlockDetectorEntry
     * We free BlockingResources, but we never free entries in the
     * deadlock detector.  This struct outlives its BlockingResource
     * and preserves all the state needed to print subsequent
     * error messages.
     *
     * These objects are owned by the deadlock detector.
     */
    struct DeadlockDetectorEntry
    {
        DeadlockDetectorEntry(const char* aName,
                              BlockingResourceType aType) :
            mName(aName),
            mType(aType),
            mAcquisitionContext(CallStack::kNone)
        {
            NS_ABORT_IF_FALSE(mName, "Name must be nonnull");
        }
        
        /**
         * Print
         * Write a description of this blocking resource to |out|.  If
         * the resource appears to be currently acquired, the current
         * acquisition context is printed and true is returned.
         * Otherwise, we print the context from |aFirstSeen|, the
         * first acquisition from which the code calling |Print()|
         * became interested in us, and return false.  |Print()| can
         * be forced to print the context from |aFirstSeen| regardless
         * by passing |aPrintFirstSeenCx=true|.
         *
         * *NOT* thread safe.  Reads |mAcquisitionContext| without
         * synchronization, but this will not cause correctness
         * problems.
         *
         * FIXME bug 456272: hack alert: because we can't write call
         * contexts into strings, all info is written to stderr, but
         * only some info is written into |out|
         */
        bool Print(const DDT::ResourceAcquisition& aFirstSeen,
                   nsACString& out,
                   bool aPrintFirstSeenCx=false) const;

        /**
         * mName
         * A descriptive name for this resource.  Used in error
         * messages etc.
         */
        const char* mName;
        /**
         * mType
         * The more specific type of this resource.  Used to implement
         * special semantics (e.g., reentrancy of monitors).
         **/
        BlockingResourceType mType;
        /**
         * mAcquisitionContext
         * The calling context from which this resource was acquired, or
         * |CallStack::kNone| if it is currently free (or freed).
         */
        CallStack mAcquisitionContext;
    };

protected:
    /**
     * BlockingResourceBase
     * Initialize this blocking resource.  Also hooks the resource into
     * instrumentation code.
     *
     * Thread safe.
     *
     * @param aName A meaningful, unique name that can be used in
     *              error messages, et al.
     * @param aType The specific type of |this|, if any.
     **/
    BlockingResourceBase(const char* aName, BlockingResourceType aType);

    ~BlockingResourceBase();

    /**
     * CheckAcquire
     *
     * Thread safe.
     *
     * @param aCallContext the client's calling context from which the
     *        original acquisition request was made.
     **/
    void CheckAcquire(const CallStack& aCallContext);

    /**
     * Acquire
     *
     * *NOT* thread safe.  Requires ownership of underlying resource.
     *
     * @param aCallContext the client's calling context from which the
     *        original acquisition request was made.
     **/
    void Acquire(const CallStack& aCallContext); //NS_NEEDS_RESOURCE(this)

    /**
     * Release
     * Remove this resource from the current thread's acquisition chain.
     * The resource does not have to be at the front of the chain, although
     * it is confusing to release resources in a different order than they
     * are acquired.  This generates a warning.
     *
     * *NOT* thread safe.  Requires ownership of underlying resource.
     **/
    void Release();             //NS_NEEDS_RESOURCE(this)

    /**
     * PrintCycle
     * Append to |out| detailed information about the circular
     * dependency in |cycle|.  Returns true if it *appears* that this
     * cycle may represent an imminent deadlock, but this is merely a
     * heuristic; the value returned may be a false positive or false
     * negative.
     *
     * *NOT* thread safe.  Calls |Print()|.
     *
     * FIXME bug 456272 hack alert: because we can't write call
     * contexts into strings, all info is written to stderr, but only
     * some info is written into |out|
     */
    static bool PrintCycle(const DDT::ResourceAcquisitionArray* cycle,
                           nsACString& out);

    /**
     * ResourceChainFront
     *
     * Thread safe.
     *
     * @return the front of the resource acquisition chain, i.e., the last
     *         resource acquired.
     */
    static BlockingResourceBase* ResourceChainFront()
    {
        return (BlockingResourceBase*)
            PR_GetThreadPrivate(sResourceAcqnChainFrontTPI);
    }

    /**
     * ResourceChainPrev
     *
     * *NOT* thread safe.  Requires ownership of underlying resource.
     */
    static BlockingResourceBase*
    ResourceChainPrev(const BlockingResourceBase* aResource)
    {
        return aResource->mChainPrev;
    } //NS_NEEDS_RESOURCE(this)

    /**
     * ResourceChainAppend
     * Set |this| to the front of the resource acquisition chain, and link
     * |this| to |aPrev|.
     *
     * *NOT* thread safe.  Requires ownership of underlying resource.
     */
    void ResourceChainAppend(BlockingResourceBase* aPrev)
    {
        mChainPrev = aPrev;
        PR_SetThreadPrivate(sResourceAcqnChainFrontTPI, this);
    } //NS_NEEDS_RESOURCE(this)

    /**
     * ResourceChainRemove
     * Remove |this| from the front of the resource acquisition chain.
     *
     * *NOT* thread safe.  Requires ownership of underlying resource.
     */
    void ResourceChainRemove()
    {
        NS_ASSERTION(this == ResourceChainFront(), "not at chain front");
        PR_SetThreadPrivate(sResourceAcqnChainFrontTPI, mChainPrev);
    } //NS_NEEDS_RESOURCE(this)

    /**
     * GetAcquisitionContext
     * Return the calling context from which this resource was acquired,
     * or CallStack::kNone if it's currently free.
     *
     * *NOT* thread safe.  Requires ownership of underlying resource.
     */
    CallStack
    GetAcquisitionContext()
    {
        return mDDEntry->mAcquisitionContext;
    }

    /**
     * SetAcquisitionContext
     * Set the calling context from which this resource was acquired.
     *
     * *NOT* thread safe.  Requires ownership of underlying resource.
     */
    void
    SetAcquisitionContext(CallStack aAcquisitionContext)
    {
        mDDEntry->mAcquisitionContext = aAcquisitionContext;
    }

    /**
     * mChainPrev
     * A series of resource acquisitions creates a chain of orders.  This
     * chain is implemented as a linked list; |mChainPrev| points to the
     * resource most recently Acquire()'d before this one.
     **/
    BlockingResourceBase* mChainPrev;

private:
    /**
     * mDDEntry
     * The key for this BlockingResourceBase in the deadlock detector.
     */
    DeadlockDetectorEntry* mDDEntry;

    /**
     * sCallOnce
     * Ensures static members are initialized only once, and in a
     * thread-safe way.
     */
    static PRCallOnceType sCallOnce;

    /**
     * sResourceAcqnChainFrontTPI
     * Thread-private index to the front of each thread's resource
     * acquisition chain.
     */
    static unsigned sResourceAcqnChainFrontTPI;

    /**
     * sDeadlockDetector
     * Does as named.
     */
    static DDT* sDeadlockDetector;

    /**
     * InitStatics
     * Inititialize static members of BlockingResourceBase that can't
     * be statically initialized.
     *
     * *NOT* thread safe.
     */
    static PRStatus InitStatics() {
        PR_NewThreadPrivateIndex(&sResourceAcqnChainFrontTPI, 0);
        sDeadlockDetector = new DDT();
        if (!sDeadlockDetector)
            NS_RUNTIMEABORT("can't allocate deadlock detector");
        return PR_SUCCESS;
    }

    /**
     * Shutdown
     * Free static members.
     *
     * *NOT* thread safe.
     */
    static void Shutdown() {
        delete sDeadlockDetector;
        sDeadlockDetector = 0;
    }

#  ifdef MOZILLA_INTERNAL_API
    // so it can call BlockingResourceBase::Shutdown()
    friend void LogTerm();
#  endif  // ifdef MOZILLA_INTERNAL_API

#else  // non-DEBUG implementation

    BlockingResourceBase(const char* aName, BlockingResourceType aType)
    {
    }

    ~BlockingResourceBase()
    {
    }

#endif
};


} // namespace mozilla


#endif // mozilla_BlockingResourceBase_h
