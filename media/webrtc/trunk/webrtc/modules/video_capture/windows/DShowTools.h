/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBRTC_MODULES_VIDEO_CAPTURE_MAIN_SOURCE_WINDOWS_DSHOWTOOLS_H_
#define WEBRTC_MODULES_VIDEO_CAPTURE_MAIN_SOURCE_WINDOWS_DSHOWTOOLS_H_

#define NS_IF_ADDREF(expr) \
  if (expr) { \
    (expr)->AddRef(); \
  }


#define NS_IF_RELEASE(expr) \
  if (expr) { \
    (expr)->Release(); \
  }

namespace mozilla {

/**
 * CriticalSection
 * Java-like monitor.
 * When possible, use CriticalSectionAutoEnter to hold this monitor within a
 * scope, instead of calling Enter/Exit directly.
 **/
class CriticalSection
{
public:
    /**
     * CriticalSection
     * @param aName A name which can reference this monitor
     */
    CriticalSection(const char* aName)
    {
      ::InitializeCriticalSection(&mCriticalSection);
    }

    /**
     * ~CriticalSection
     **/
    ~CriticalSection()
    {
      ::DeleteCriticalSection(&mCriticalSection);
    }

    /** 
     * Enter
     * @see prmon.h 
     **/
    void Enter()
    {
      ::EnterCriticalSection(&mCriticalSection);
    }

    /** 
     * Exit
     * @see prmon.h 
     **/
    void Leave()
    {
      ::LeaveCriticalSection(&mCriticalSection);
    }

private:
    CriticalSection();
    CriticalSection(const CriticalSection&);
    CriticalSection& operator =(const CriticalSection&);

    CRITICAL_SECTION mCriticalSection;
};


/**
 * CriticalSectionAutoEnter
 * Enters the CriticalSection when it enters scope, and exits it when
 * it leaves scope.
 *
 * MUCH PREFERRED to bare calls to CriticalSection.Enter and Exit.
 */ 
class CriticalSectionAutoEnter
{
public:
    /**
     * Constructor
     * The constructor aquires the given lock.  The destructor
     * releases the lock.
     * 
     * @param aCriticalSection A valid mozilla::CriticalSection*. 
     **/
    CriticalSectionAutoEnter(mozilla::CriticalSection &aCriticalSection) :
        mCriticalSection(&aCriticalSection)
    {
        assert(mCriticalSection);
        mCriticalSection->Enter();
    }
    
    ~CriticalSectionAutoEnter(void)
    {
        mCriticalSection->Leave();
    }
 

private:
    CriticalSectionAutoEnter();
    CriticalSectionAutoEnter(const CriticalSectionAutoEnter&);
    CriticalSectionAutoEnter& operator =(const CriticalSectionAutoEnter&);
    static void* operator new(size_t) throw();
    static void operator delete(void*);

    mozilla::CriticalSection* mCriticalSection;
};


} // namespace mozilla


#endif // ifndef mozilla_CriticalSection_h
