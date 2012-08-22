/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsAtomicRefcnt_h__
#define nsAtomicRefcnt_h__

#include "nscore.h"
#include "pratom.h"

class nsAutoRefCnt;

// This header defines functions for modifying refcounts which wrap the
// PR_ATOMIC_* macros.

#if defined(XP_WIN)

#if PR_BYTES_PER_LONG == 4
typedef volatile long nsAtomicRefcnt;
#else
#error "Windows should have 4 bytes per long."
#endif

#else /* !defined(XP_WIN) */

typedef int32_t  nsAtomicRefcnt;

#endif

inline int32_t
NS_AtomicIncrementRefcnt(int32_t &refcnt)
{
  return PR_ATOMIC_INCREMENT(&refcnt);
}

inline nsrefcnt
NS_AtomicIncrementRefcnt(nsrefcnt &refcnt)
{
  return (nsrefcnt) PR_ATOMIC_INCREMENT((nsAtomicRefcnt*)&refcnt);
}

inline nsrefcnt
NS_AtomicIncrementRefcnt(nsAutoRefCnt &refcnt)
{
  // This cast is safe since nsAtomicRefCnt contains just one member, its refcount.
  return (nsrefcnt) PR_ATOMIC_INCREMENT((nsAtomicRefcnt*)&refcnt);
}

inline nsrefcnt
NS_AtomicDecrementRefcnt(nsrefcnt &refcnt)
{
  return (nsrefcnt) PR_ATOMIC_DECREMENT((nsAtomicRefcnt*)&refcnt);
}

inline nsrefcnt
NS_AtomicDecrementRefcnt(nsAutoRefCnt &refcnt)
{
  return (nsrefcnt) PR_ATOMIC_DECREMENT((nsAtomicRefcnt*)&refcnt);
}

inline int32_t
NS_AtomicDecrementRefcnt(int32_t &refcnt)
{
  return PR_ATOMIC_DECREMENT(&refcnt);
}

#endif
