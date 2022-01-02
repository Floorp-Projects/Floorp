/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_cxxalloc_h
#define mozilla_cxxalloc_h

/*
 * We implement the default operators new/delete as part of
 * libmozalloc, replacing their definitions in libstdc++.  The
 * operator new* definitions in libmozalloc will never return a NULL
 * pointer.
 *
 * Each operator new immediately below returns a pointer to memory
 * that can be delete'd by any of
 *
 *   (1) the matching infallible operator delete immediately below
 *   (2) the matching system |operator delete(void*, std::nothrow)|
 *   (3) the matching system |operator delete(void*) noexcept(false)|
 *
 * NB: these are declared |noexcept(false)|, though they will never
 * throw that exception.  This declaration is consistent with the rule
 * that |::operator new() noexcept(false)| will never return NULL.
 *
 * NB: mozilla::fallible can be used instead of std::nothrow.
 */

#ifndef MOZALLOC_EXPORT_NEW
#  define MOZALLOC_EXPORT_NEW MFBT_API
#endif

MOZALLOC_EXPORT_NEW void* operator new(size_t size) noexcept(false) {
  return moz_xmalloc(size);
}

MOZALLOC_EXPORT_NEW void* operator new(size_t size,
                                       const std::nothrow_t&) noexcept(true) {
  return malloc_impl(size);
}

MOZALLOC_EXPORT_NEW void* operator new[](size_t size) noexcept(false) {
  return moz_xmalloc(size);
}

MOZALLOC_EXPORT_NEW void* operator new[](size_t size,
                                         const std::nothrow_t&) noexcept(true) {
  return malloc_impl(size);
}

MOZALLOC_EXPORT_NEW void operator delete(void* ptr) noexcept(true) {
  return free_impl(ptr);
}

MOZALLOC_EXPORT_NEW void operator delete(void* ptr,
                                         const std::nothrow_t&) noexcept(true) {
  return free_impl(ptr);
}

MOZALLOC_EXPORT_NEW void operator delete[](void* ptr) noexcept(true) {
  return free_impl(ptr);
}

MOZALLOC_EXPORT_NEW void operator delete[](
    void* ptr, const std::nothrow_t&) noexcept(true) {
  return free_impl(ptr);
}

#if defined(XP_WIN)
// We provide the global sized delete overloads unconditionally because the
// MSVC runtime headers do, despite compiling with /Zc:sizedDealloc-
MOZALLOC_EXPORT_NEW void operator delete(void* ptr,
                                         size_t /*size*/) noexcept(true) {
  return free_impl(ptr);
}

MOZALLOC_EXPORT_NEW void operator delete[](void* ptr,
                                           size_t /*size*/) noexcept(true) {
  return free_impl(ptr);
}
#endif

#endif /* mozilla_cxxalloc_h */
