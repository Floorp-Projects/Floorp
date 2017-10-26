// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ref_counted.h"

#include "base/threading/thread_collision_warner.h"

namespace base {
namespace {

#if DCHECK_IS_ON()
std::atomic_int g_cross_thread_ref_count_access_allow_count(0);
#endif

}  // namespace

namespace subtle {

bool RefCountedThreadSafeBase::HasOneRef() const {
  return ref_count_.IsOne();
}

RefCountedThreadSafeBase::~RefCountedThreadSafeBase() {
#if DCHECK_IS_ON()
  DCHECK(in_dtor_) << "RefCountedThreadSafe object deleted without "
                      "calling Release()";
#endif
}

#if !defined(ARCH_CPU_X86_FAMILY)
bool RefCountedThreadSafeBase::Release() const {
  return ReleaseImpl();
}
void RefCountedThreadSafeBase::AddRef() const {
  AddRefImpl();
}
#endif

#if DCHECK_IS_ON()
bool RefCountedBase::CalledOnValidSequence() const {
#if defined(MOZ_SANDBOX)
  return true;
#else
  return sequence_checker_.CalledOnValidSequence() ||
         g_cross_thread_ref_count_access_allow_count.load() != 0;
#endif
}
#endif

}  // namespace subtle

#if DCHECK_IS_ON()
ScopedAllowCrossThreadRefCountAccess::ScopedAllowCrossThreadRefCountAccess() {
  ++g_cross_thread_ref_count_access_allow_count;
}

ScopedAllowCrossThreadRefCountAccess::~ScopedAllowCrossThreadRefCountAccess() {
  --g_cross_thread_ref_count_access_allow_count;
}
#endif

}  // namespace base
