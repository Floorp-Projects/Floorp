/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: sw=4 ts=4 et :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_Mutex_h
#define mozilla_Mutex_h

#include "prlock.h"

#include "mozilla/BlockingResourceBase.h"
#include "mozilla/GuardObjects.h"

//
// Provides:
//
//  - Mutex, a non-recursive mutex
//  - MutexAutoLock, an RAII class for ensuring that Mutexes are properly 
//    locked and unlocked
//  - MutexAutoUnlock, complementary sibling to MutexAutoLock
//
// Using MutexAutoLock/MutexAutoUnlock is MUCH preferred to making bare
// calls to Mutex.Lock and Unlock.
//
namespace mozilla {


/**
 * Mutex
 * When possible, use MutexAutoLock/MutexAutoUnlock to lock/unlock this
 * mutex within a scope, instead of calling Lock/Unlock directly.
 **/

class NS_COM_GLUE Mutex : BlockingResourceBase
{
public:
    /**
     * Mutex
     * @param name A name which can reference this lock
     * @returns If failure, nullptr
     *          If success, a valid Mutex* which must be destroyed
     *          by Mutex::DestroyMutex()
     **/
    Mutex(const char* name) :
        BlockingResourceBase(name, eMutex)
    {
        MOZ_COUNT_CTOR(Mutex);
        mLock = PR_NewLock();
        if (!mLock)
            NS_RUNTIMEABORT("Can't allocate mozilla::Mutex");
    }

    /**
     * ~Mutex
     **/
    ~Mutex()
    {
        NS_ASSERTION(mLock,
                     "improperly constructed Lock or double free");
        // NSPR does consistency checks for us
        PR_DestroyLock(mLock);
        mLock = 0;
        MOZ_COUNT_DTOR(Mutex);
    }

#ifndef DEBUG
    /**
     * Lock
     * @see prlock.h
     **/
    void Lock()
    {
        PR_Lock(mLock);
    }

    /**
     * Unlock
     * @see prlock.h
     **/
    void Unlock()
    {
        PR_Unlock(mLock);
    }

    /**
     * AssertCurrentThreadOwns
     * @see prlock.h
     **/
    void AssertCurrentThreadOwns () const
    {
    }

    /**
     * AssertNotCurrentThreadOwns
     * @see prlock.h
     **/
    void AssertNotCurrentThreadOwns () const
    {
    }

#else
    void Lock();
    void Unlock();

    void AssertCurrentThreadOwns () const
    {
        PR_ASSERT_CURRENT_THREAD_OWNS_LOCK(mLock);
    }

    void AssertNotCurrentThreadOwns () const
    {
        // FIXME bug 476536
    }

#endif  // ifndef DEBUG

private:
    Mutex();
    Mutex(const Mutex&);
    Mutex& operator=(const Mutex&);

    PRLock* mLock;

    friend class CondVar;
};


/**
 * MutexAutoLock
 * Acquires the Mutex when it enters scope, and releases it when it leaves 
 * scope.
 *
 * MUCH PREFERRED to bare calls to Mutex.Lock and Unlock.
 */ 
template<typename T>
class NS_COM_GLUE NS_STACK_CLASS BaseAutoLock
{
public:
    /**
     * Constructor
     * The constructor aquires the given lock.  The destructor
     * releases the lock.
     * 
     * @param aLock A valid mozilla::Mutex* returned by 
     *              mozilla::Mutex::NewMutex. 
     **/
    BaseAutoLock(T& aLock MOZ_GUARD_OBJECT_NOTIFIER_PARAM) :
        mLock(&aLock)
    {
        MOZ_GUARD_OBJECT_NOTIFIER_INIT;
        NS_ASSERTION(mLock, "null mutex");
        mLock->Lock();
    }
    
    ~BaseAutoLock(void) {
        mLock->Unlock();
    }
 
private:
    BaseAutoLock();
    BaseAutoLock(BaseAutoLock&);
    BaseAutoLock& operator=(BaseAutoLock&);
    static void* operator new(size_t) CPP_THROW_NEW;
    static void operator delete(void*);

    T* mLock;
    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

typedef BaseAutoLock<Mutex> MutexAutoLock;

/**
 * MutexAutoUnlock
 * Releases the Mutex when it enters scope, and re-acquires it when it leaves 
 * scope.
 *
 * MUCH PREFERRED to bare calls to Mutex.Unlock and Lock.
 */ 
template<typename T>
class NS_COM_GLUE NS_STACK_CLASS BaseAutoUnlock 
{
public:
    BaseAutoUnlock(T& aLock MOZ_GUARD_OBJECT_NOTIFIER_PARAM) :
        mLock(&aLock)
    {
        MOZ_GUARD_OBJECT_NOTIFIER_INIT;
        NS_ASSERTION(mLock, "null lock");
        mLock->Unlock();
    }

    ~BaseAutoUnlock() 
    {
        mLock->Lock();
    }

private:
    BaseAutoUnlock();
    BaseAutoUnlock(BaseAutoUnlock&);
    BaseAutoUnlock& operator =(BaseAutoUnlock&);
    static void* operator new(size_t) CPP_THROW_NEW;
    static void operator delete(void*);
     
    T* mLock;
    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

typedef BaseAutoUnlock<Mutex> MutexAutoUnlock;

} // namespace mozilla


#endif // ifndef mozilla_Mutex_h
