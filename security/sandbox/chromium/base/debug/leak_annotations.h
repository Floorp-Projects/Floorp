// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_DEBUG_LEAK_ANNOTATIONS_H_
#define BASE_DEBUG_LEAK_ANNOTATIONS_H_

#include "build/build_config.h"

// This file defines macros which can be used to annotate intentional memory
// leaks. Support for annotations is implemented in HeapChecker and
// LeakSanitizer. Annotated objects will be treated as a source of live
// pointers, i.e. any heap objects reachable by following pointers from an
// annotated object will not be reported as leaks.
//
// ANNOTATE_SCOPED_MEMORY_LEAK: all allocations made in the current scope
// will be annotated as leaks.
// ANNOTATE_LEAKING_OBJECT_PTR(X): the heap object referenced by pointer X will
// be annotated as a leak.
//
// Note that HeapChecker will report a fatal error if an object which has been
// annotated with ANNOTATE_LEAKING_OBJECT_PTR is later deleted (but
// LeakSanitizer won't).

#if defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_NACL) && \
    defined(USE_HEAPCHECKER)

#include "third_party/tcmalloc/chromium/src/gperftools/heap-checker.h"

#define ANNOTATE_SCOPED_MEMORY_LEAK \
    HeapLeakChecker::Disabler heap_leak_checker_disabler; static_cast<void>(0)

#define ANNOTATE_LEAKING_OBJECT_PTR(X) \
    HeapLeakChecker::IgnoreObject(X)

#elif defined(LEAK_SANITIZER) && !defined(OS_NACL)

extern "C" {
void __lsan_disable();
void __lsan_enable();
void __lsan_ignore_object(const void *p);
}  // extern "C"

class ScopedLeakSanitizerDisabler {
 public:
  ScopedLeakSanitizerDisabler() { __lsan_disable(); }
  ~ScopedLeakSanitizerDisabler() { __lsan_enable(); }
 private:
  DISALLOW_COPY_AND_ASSIGN(ScopedLeakSanitizerDisabler);
};

#define ANNOTATE_SCOPED_MEMORY_LEAK \
    ScopedLeakSanitizerDisabler leak_sanitizer_disabler; static_cast<void>(0)

#define ANNOTATE_LEAKING_OBJECT_PTR(X) __lsan_ignore_object(X);

#else

// If neither HeapChecker nor LSan are used, the annotations should be no-ops.
#define ANNOTATE_SCOPED_MEMORY_LEAK ((void)0)
#define ANNOTATE_LEAKING_OBJECT_PTR(X) ((void)0)

#endif

#endif  // BASE_DEBUG_LEAK_ANNOTATIONS_H_
