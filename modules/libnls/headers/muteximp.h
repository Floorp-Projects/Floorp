/*
*****************************************************************************************
*                                                                                       *
* COPYRIGHT:                                                                            *
*   (C) Copyright Taligent, Inc.,  1997                                                 *
*   (C) Copyright International Business Machines Corporation,  1997                    *
*   Licensed Material - Program-Property of IBM - All Rights Reserved.                  *
*   US Government Users Restricted Rights - Use, duplication, or disclosure             *
*   restricted by GSA ADP Schedule Contract with IBM Corp.                              *
*                                                                                       *
*****************************************************************************************
*/
/*
 * File:     muteximp.h
 *
 * Simple mutex class which allows platform-independent thread safety.  The
 * burden is on the library caller to provide the actual locking implementation,
 * if they need thread-safety.  Clients who don't care don't need to do anything.
 *
 * Author:   Alan Liu  1/31/97
 * History:  Helena Shih    Updated   2/4/97
 *   06/04/97         helena            Updated comments as per feedback from 5/21 drop.
 */
#ifndef _MUTEXIMP
#define _MUTEXIMP

#include "ptypes.h"

/*
 * Clients who wish to call the library from multiple threads should create a
 * MutexImplementation struct, with three pointers in it, and then call
 * Mutex::SetImplementation().  Clients are responsible for making this initial
 * call thread-safe.

 * Clients who don't care about thread safety don't have to do anything.
 */
typedef void* MutexPointer;
typedef void (*MutexFunction)(MutexPointer);

#ifdef XP_CPLUSPLUS
struct MutexImplementation
{
    MutexPointer    mutex;  /* Pointer to the actual mutex object */
    MutexFunction   lock;   /* Function to enter mutex */
    MutexFunction   unlock; /* Function to leave mutex */
};
#endif
#ifndef XP_CPLUSPLUS
typedef struct MutexImplementation
{
    MutexPointer    mutex;  /* Pointer to the actual mutex object */
    MutexFunction   lock;   /* Function to enter mutex */
    MutexFunction   unlock; /* Function to leave mutex */
} MutexImplementation;
#endif

/**
 * The following is an example of the mutex implementation on Win32 platforms,
<pre>
.      #include "mutex.h"
.      #include &lt;WINDOWS.H>
.      
.      static CRITICAL_SECTION gMutex;
.      
.      void Win32Lock(MutexPointer p)
.      {
.          CRITICAL_SECTION *mutex = (CRITICAL_SECTION*)p;
.          EnterCriticalSection(mutex);
.      }
.      
.      void Win32Unlock(MutexPointer p)
.      {
.          CRITICAL_SECTION *mutex = (CRITICAL_SECTION*)p;
.          LeaveCriticalSection(mutex);
.      }
.      
.      MutexImplementation gWin32MutexImplementation = { &gMutex, Win32Lock, Win32Unlock };
.      ....
.      
.      Maybe something like this: Client would write this function and then
.      make sure to call it once in one thread before multiple threads start.
.      ....
.      void Win32MutexInitialize()
.      {
.          InitializeCriticalSection(&gMutex);
.          Mutex::SetImplementation(&gWin32MutexImplementation);
.      }
</pre>
*/

#endif
