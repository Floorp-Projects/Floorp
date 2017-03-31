/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ArenaAllocatorExtensions_h
#define mozilla_ArenaAllocatorExtensions_h

#include "mozilla/ArenaAllocator.h"
#include "mozilla/CheckedInt.h"
#include "nsAString.h"

/**
 * Extensions to the ArenaAllocator class.
 */
namespace mozilla {

namespace detail {

template<typename T, size_t ArenaSize, size_t Alignment>
T* DuplicateString(const T* aSrc, const CheckedInt<size_t>& aLen,
                   ArenaAllocator<ArenaSize, Alignment>& aArena);

} // namespace detail

/**
 * Makes an arena allocated null-terminated copy of the source string. The
 * source string must be null-terminated.
 *
 * @param aSrc String to copy.
 * @param aArena The arena to allocate the string copy out of.
 * @return An arena allocated null-terminated string.
 */
template<size_t ArenaSize, size_t Alignment>
char* ArenaStrdup(const char* aStr,
                  ArenaAllocator<ArenaSize, Alignment>& aArena)
{
  return detail::DuplicateString(aStr, strlen(aStr), aArena);
}

/**
 * Makes an arena allocated null-terminated copy of the source string.
 *
 * @param aSrc String to copy.
 * @param aArena The arena to allocate the string copy out of.
 * @return An arena allocated null-terminated string.
 */
template<size_t ArenaSize, size_t Alignment>
nsAString::char_type* ArenaStrdup(
    const nsAString& aStr, ArenaAllocator<ArenaSize, Alignment>& aArena)
{
  return detail::DuplicateString(aStr.BeginReading(), aStr.Length(), aArena);
}

/**
 * Makes an arena allocated null-terminated copy of the source string.
 *
 * @param aSrc String to copy.
 * @param aArena The arena to allocate the string copy out of.
 * @return An arena allocated null-terminated string.
 */
template<size_t ArenaSize, size_t Alignment>
nsACString::char_type* ArenaStrdup(
    const nsACString& aStr, ArenaAllocator<ArenaSize, Alignment>& aArena)
{
  return detail::DuplicateString(aStr.BeginReading(), aStr.Length(), aArena);
}

/**
 * Copies the source string and adds a null terminator. Source string does not
 * have to be null terminated.
 */
template<typename T, size_t ArenaSize, size_t Alignment>
T* detail::DuplicateString(const T* aSrc, const CheckedInt<size_t>& aLen,
                           ArenaAllocator<ArenaSize, Alignment>& aArena)
{
  const auto byteLen = (aLen + 1) * sizeof(T);
  if (!byteLen.isValid()) {
    return nullptr;
  }

  T* p = static_cast<T*>(aArena.Allocate(byteLen.value(), mozilla::fallible));
  if (p) {
    memcpy(p, aSrc, byteLen.value() - sizeof(T));
    p[aLen.value()] = T(0);
  }

  return p;
}

} // namespace mozilla

#endif // mozilla_ArenaAllocatorExtensions_h
