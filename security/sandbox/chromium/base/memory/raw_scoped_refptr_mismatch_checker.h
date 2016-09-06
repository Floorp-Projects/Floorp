// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MEMORY_RAW_SCOPED_REFPTR_MISMATCH_CHECKER_H_
#define BASE_MEMORY_RAW_SCOPED_REFPTR_MISMATCH_CHECKER_H_

#include "base/memory/ref_counted.h"
#include "base/template_util.h"
#include "base/tuple.h"
#include "build/build_config.h"

// It is dangerous to post a task with a T* argument where T is a subtype of
// RefCounted(Base|ThreadSafeBase), since by the time the parameter is used, the
// object may already have been deleted since it was not held with a
// scoped_refptr. Example: http://crbug.com/27191
// The following set of traits are designed to generate a compile error
// whenever this antipattern is attempted.

namespace base {

// This is a base internal implementation file used by task.h and callback.h.
// Not for public consumption, so we wrap it in namespace internal.
namespace internal {

template <typename T>
struct NeedsScopedRefptrButGetsRawPtr {
#if defined(OS_WIN)
  enum {
    value = base::false_type::value
  };
#else
  enum {
    // Human readable translation: you needed to be a scoped_refptr if you are a
    // raw pointer type and are convertible to a RefCounted(Base|ThreadSafeBase)
    // type.
    value = (is_pointer<T>::value &&
             (is_convertible<T, subtle::RefCountedBase*>::value ||
              is_convertible<T, subtle::RefCountedThreadSafeBase*>::value))
  };
#endif
};

template <typename Params>
struct ParamsUseScopedRefptrCorrectly {
  enum { value = 0 };
};

template <>
struct ParamsUseScopedRefptrCorrectly<Tuple<>> {
  enum { value = 1 };
};

template <typename Head, typename... Tail>
struct ParamsUseScopedRefptrCorrectly<Tuple<Head, Tail...>> {
  enum { value = !NeedsScopedRefptrButGetsRawPtr<Head>::value &&
                 ParamsUseScopedRefptrCorrectly<Tuple<Tail...>>::value };
};

}  // namespace internal

}  // namespace base

#endif  // BASE_MEMORY_RAW_SCOPED_REFPTR_MISMATCH_CHECKER_H_
