/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

#ifndef ATOMIC_H_
#define ATOMIC_H_

#include <stdint.h>

// This implements the atomic primatives without any atomicity guarantees. This
// makes the totally unsafe. However we're only using the demuxer in a single
// thread.

namespace stagefright {
static inline int32_t
android_atomic_dec(volatile int32_t* aValue)
{
  return (*aValue)--;
}

static inline int32_t
android_atomic_inc(volatile int32_t* aValue)
{
  return (*aValue)++;
}

static inline int32_t
android_atomic_or(int32_t aModifier, volatile int32_t* aValue)
{
  int32_t ret = *aValue;
  *aValue |= aModifier;
  return ret;
}

static inline int32_t
android_atomic_add(int32_t aModifier, volatile int32_t* aValue)
{
  int32_t ret = *aValue;
  *aValue += aModifier;
  return ret;
}

static inline int32_t
android_atomic_cmpxchg(int32_t aOld, int32_t aNew, volatile int32_t* aValue)
{
  if (*aValue == aOld)
  {
    return *aValue = aNew;
  }
  return aOld;
}
}

#endif
