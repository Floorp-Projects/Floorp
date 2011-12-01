/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Benoit Girard <bgirard@mozilla.com>
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

#include <pthread.h>
#include "base/atomicops.h"
#include "nscore.h"

// TODO Merge into Sampler.h

extern pthread_key_t pkey_stack;

#define SAMPLER_INIT() mozilla_sampler_init();
#define SAMPLER_DEINIT() mozilla_sampler_deinit();
#define SAMPLE_CHECKPOINT(name_space, info) mozilla::SamplerStackFrameRAII only_one_sampleraii_per_scope(FULLFUNCTION, name_space "::" info);
#define SAMPLE_MARKER(info) mozilla_sampler_add_marker(info);

// STORE_SEQUENCER: Because signals can interrupt our profile modification
//                  we need to make stores are not re-ordered by the compiler
//                  or hardware to make sure the profile is consistent at
//                  every point the signal can fire.
#ifdef ARCH_CPU_ARM_FAMILY
// TODO Is there something cheaper that will prevent
//      memory stores from being reordered
// Uses: pLinuxKernelMemoryBarrier
# define STORE_SEQUENCER() base::subtle::MemoryBarrier();
#elif ARCH_CPU_X86_FAMILY
# define STORE_SEQUENCER() asm volatile("" ::: "memory");
#else
# error "Memory clobber not supported for your platform."
#endif

// Returns a handdle to pass on exit. This can check that we are popping the
// correct callstack.
inline void* mozilla_sampler_call_enter(const char *aInfo);
inline void  mozilla_sampler_call_exit(void* handle);
inline void  mozilla_sampler_add_marker(const char *aInfo);

void mozilla_sampler_init();

namespace mozilla {

class NS_STACK_CLASS SamplerStackFrameRAII {
public:
  SamplerStackFrameRAII(const char *aFuncName, const char *aInfo) {
    mHandle = mozilla_sampler_call_enter(aInfo);
  }
  ~SamplerStackFrameRAII() {
    mozilla_sampler_call_exit(mHandle);
  }
private:
  void* mHandle;
};

} //mozilla

// the SamplerStack members are read by signal
// handlers, so the mutation of them needs to be signal-safe.
struct Stack
{
public:
  Stack()
    : mStackPointer(0)
    , mMarkerPointer(0)
    , mDroppedStackEntries(0)
    , mQueueClearMarker(false)
  { }

  void addMarker(const char *aMarker)
  {
    if (mQueueClearMarker) {
      clearMarkers();
    }
    if (!aMarker) {
      return; //discard
    }
    if (mMarkerPointer == 1024) {
      return; //array full, silently drop
    }
    mMarkers[mMarkerPointer] = aMarker;
    STORE_SEQUENCER();
    mMarkerPointer++;
  }

  // called within signal. Function must be reentrant
  const char* getMarker(int aMarkerId)
  {
    if (mQueueClearMarker) {
      clearMarkers();
    }
    if (aMarkerId >= mMarkerPointer) {
      return NULL;
    }
    return mMarkers[aMarkerId];
  }

  // called within signal. Function must be reentrant
  void clearMarkers()
  {
    mMarkerPointer = 0;
    mQueueClearMarker = false;
  }

  void push(const char *aName)
  {
    if (mStackPointer >= 1024) {
      mDroppedStackEntries++;
      return;
    }

    // Make sure we increment the pointer after the name has
    // been written such that mStack is always consistent.
    mStack[mStackPointer] = aName;
    // Prevent the optimizer from re-ordering these instructions
    asm("":::"memory");
    mStackPointer++;
  }
  void pop()
  {
    if (mDroppedStackEntries > 0) {
      mDroppedStackEntries--;
    } else {
      mStackPointer--;
    }
  }
  bool isEmpty()
  {
    return mStackPointer == 0;
  }

  // Keep a list of active checkpoints
  const char *mStack[1024];
  // Keep a list of active markers to be applied to the next sample taken
  const char *mMarkers[1024];
  sig_atomic_t mStackPointer;
  sig_atomic_t mMarkerPointer;
  sig_atomic_t mDroppedStackEntries;
  // We don't want to modify _markers from within the signal so we allow
  // it to queue a clear operation.
  sig_atomic_t mQueueClearMarker;
};

inline void* mozilla_sampler_call_enter(const char *aInfo)
{
  Stack *stack = (Stack*)pthread_getspecific(pkey_stack);
  if (!stack) {
    return stack;
  }
  stack->push(aInfo);

  // The handle is meant to support future changes
  // but for now it is simply use to save a call to
  // pthread_getspecific on exit. It also supports the
  // case where the sampler is initialized between
  // enter and exit.
  return stack;
}

inline void mozilla_sampler_call_exit(void *aHandle)
{
  if (!aHandle)
    return;

  Stack *stack = (Stack*)aHandle;
  stack->pop();
}

inline void mozilla_sampler_add_marker(const char *aMarker)
{
  Stack *stack = (Stack*)pthread_getspecific(pkey_stack);
  if (!stack) {
    return;
  }
  stack->addMarker(aMarker);
}

