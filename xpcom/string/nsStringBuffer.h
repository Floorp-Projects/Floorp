/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsStringBuffer_h__
#define nsStringBuffer_h__

#include <atomic>
#include "mozilla/MemoryReporting.h"
#include "nsStringFwd.h"

template <class T>
struct already_AddRefed;

/**
 * This structure precedes the string buffers "we" allocate.  It may be the
 * case that nsTAString::mData does not point to one of these special
 * buffers.  The mDataFlags member variable distinguishes the buffer type.
 *
 * When this header is in use, it enables reference counting, and capacity
 * tracking.  NOTE: A string buffer can be modified only if its reference
 * count is 1.
 */
class nsStringBuffer {
 private:
  friend class CheckStaticAtomSizes;

  std::atomic<uint32_t> mRefCount;
  uint32_t mStorageSize;

 public:
  /**
   * Allocates a new string buffer, with given size in bytes and a
   * reference count of one.  When the string buffer is no longer needed,
   * it should be released via Release.
   *
   * It is up to the caller to set the bytes corresponding to the string
   * buffer by calling the Data method to fetch the raw data pointer.  Care
   * must be taken to properly null terminate the character array.  The
   * storage size can be greater than the length of the actual string
   * (i.e., it is not required that the null terminator appear in the last
   * storage unit of the string buffer's data).
   *
   * This guarantees that StorageSize() returns aStorageSize if the returned
   * buffer is non-null. Some callers like nsAttrValue rely on it.
   *
   * @return new string buffer or null if out of memory.
   */
  static already_AddRefed<nsStringBuffer> Alloc(size_t aStorageSize);

  /**
   * Returns a string buffer initialized with the given string on it, or null on
   * OOM.
   * Note that this will allocate extra space for the trailing null byte, which
   * this method will add.
   */
  static already_AddRefed<nsStringBuffer> Create(const char16_t* aData,
                                                 size_t aLength);
  static already_AddRefed<nsStringBuffer> Create(const char* aData,
                                                 size_t aLength);

  /**
   * Resizes the given string buffer to the specified storage size.  This
   * method must not be called on a readonly string buffer.  Use this API
   * carefully!!
   *
   * This method behaves like the ANSI-C realloc function.  (i.e., If the
   * allocation fails, null will be returned and the given string buffer
   * will remain unmodified.)
   *
   * @see IsReadonly
   */
  static nsStringBuffer* Realloc(nsStringBuffer* aBuf, size_t aStorageSize);

  /**
   * Increment the reference count on this string buffer.
   */
  void NS_FASTCALL AddRef();

  /**
   * Decrement the reference count on this string buffer.  The string
   * buffer will be destroyed when its reference count reaches zero.
   */
  void NS_FASTCALL Release();

  /**
   * This method returns the string buffer corresponding to the given data
   * pointer.  The data pointer must have been returned previously by a
   * call to the nsStringBuffer::Data method.
   */
  static nsStringBuffer* FromData(void* aData) {
    return reinterpret_cast<nsStringBuffer*>(aData) - 1;
  }

  /**
   * This method returns the data pointer for this string buffer.
   */
  void* Data() const {
    return const_cast<char*>(reinterpret_cast<const char*>(this + 1));
  }

  /**
   * This function returns the storage size of a string buffer in bytes.
   * This value is the same value that was originally passed to Alloc (or
   * Realloc).
   */
  uint32_t StorageSize() const { return mStorageSize; }

  /**
   * If this method returns false, then the caller can be sure that their
   * reference to the string buffer is the only reference to the string
   * buffer, and therefore it has exclusive access to the string buffer and
   * associated data.  However, if this function returns true, then other
   * consumers may rely on the data in this buffer being immutable and
   * other threads may access this buffer simultaneously.
   */
  bool IsReadonly() const {
    // This doesn't lead to the destruction of the buffer, so we don't
    // need to perform acquire memory synchronization for the normal
    // reason that a reference count needs acquire synchronization
    // (ensuring that all writes to the object made on other threads are
    // visible to the thread destroying the object).
    //
    // We then need to consider the possibility that there were prior
    // writes to the buffer on a different thread:  one that has either
    // since released its reference count, or one that also has access
    // to this buffer through the same reference.  There are two ways
    // for that to happen: either the buffer pointer or a data structure
    // (e.g., string object) pointing to the buffer was transferred from
    // one thread to another, or the data structure pointing to the
    // buffer was already visible on both threads.  In the first case
    // (transfer), the transfer of data from one thread to another would
    // have handled the memory synchronization.  In the latter case
    // (data structure visible on both threads), the caller needed some
    // sort of higher level memory synchronization to protect against
    // the string object being mutated at the same time on multiple
    // threads.

    // See bug 1603504. TSan might complain about a race when using
    // memory_order_relaxed, so use memory_order_acquire for making TSan
    // happy.
#if defined(MOZ_TSAN)
    return mRefCount.load(std::memory_order_acquire) > 1;
#else
    return mRefCount.load(std::memory_order_relaxed) > 1;
#endif
  }

  /**
   * The FromString methods return a string buffer for the given string
   * object or null if the string object does not have a string buffer.
   * The reference count of the string buffer is NOT incremented by these
   * methods.  If the caller wishes to hold onto the returned value, then
   * the returned string buffer must have its reference count incremented
   * via a call to the AddRef method.
   */
  static nsStringBuffer* FromString(const nsAString& aStr);
  static nsStringBuffer* FromString(const nsACString& aStr);

  /**
   * The ToString methods assign this string buffer to a given string
   * object.  If the string object does not support sharable string
   * buffers, then its value will be set to a copy of the given string
   * buffer.  Otherwise, these methods increment the reference count of the
   * given string buffer.  It is important to specify the length (in
   * storage units) of the string contained in the string buffer since the
   * length of the string may be less than its storage size.  The string
   * must have a null terminator at the offset specified by |len|.
   *
   * NOTE: storage size is measured in bytes even for wide strings;
   *       however, string length is always measured in storage units
   *       (2-byte units for wide strings).
   */
  void ToString(uint32_t aLen, nsAString& aStr, bool aMoveOwnership = false);
  void ToString(uint32_t aLen, nsACString& aStr, bool aMoveOwnership = false);

  /**
   * This measures the size only if the StringBuffer is unshared.
   */
  size_t SizeOfIncludingThisIfUnshared(
      mozilla::MallocSizeOf aMallocSizeOf) const;

  /**
   * This measures the size regardless of whether the StringBuffer is
   * unshared.
   *
   * WARNING: Only use this if you really know what you are doing, because
   * it can easily lead to double-counting strings.  If you do use them,
   * please explain clearly in a comment why it's safe and won't lead to
   * double-counting.
   */
  size_t SizeOfIncludingThisEvenIfShared(
      mozilla::MallocSizeOf aMallocSizeOf) const;
};

#endif /* !defined(nsStringBuffer_h__ */
