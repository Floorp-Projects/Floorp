/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsStringBuffer.h"

#include "mozilla/MemoryReporting.h"
#include "nsISupportsImpl.h"
#include "nsString.h"

#ifdef DEBUG
#  include "nsStringStats.h"
#else
#  define STRING_STAT_INCREMENT(_s)
#endif

void nsStringBuffer::AddRef() {
  // Memory synchronization is not required when incrementing a
  // reference count.  The first increment of a reference count on a
  // thread is not important, since the first use of the object on a
  // thread can happen before it.  What is important is the transfer
  // of the pointer to that thread, which may happen prior to the
  // first increment on that thread.  The necessary memory
  // synchronization is done by the mechanism that transfers the
  // pointer between threads.
#ifdef NS_BUILD_REFCNT_LOGGING
  uint32_t count =
#endif
      mRefCount.fetch_add(1, std::memory_order_relaxed)
#ifdef NS_BUILD_REFCNT_LOGGING
      + 1
#endif
      ;
  STRING_STAT_INCREMENT(Share);
  NS_LOG_ADDREF(this, count, "nsStringBuffer", sizeof(*this));
}

void nsStringBuffer::Release() {
  // Since this may be the last release on this thread, we need
  // release semantics so that prior writes on this thread are visible
  // to the thread that destroys the object when it reads mValue with
  // acquire semantics.
  uint32_t count = mRefCount.fetch_sub(1, std::memory_order_release) - 1;
  NS_LOG_RELEASE(this, count, "nsStringBuffer");
  if (count == 0) {
    // We're going to destroy the object on this thread, so we need
    // acquire semantics to synchronize with the memory released by
    // the last release on other threads, that is, to ensure that
    // writes prior to that release are now visible on this thread.
    count = mRefCount.load(std::memory_order_acquire);

    STRING_STAT_INCREMENT(Free);
    free(this);  // we were allocated with |malloc|
  }
}

/**
 * Alloc returns a pointer to a new string header with set capacity.
 */
already_AddRefed<nsStringBuffer> nsStringBuffer::Alloc(size_t aSize) {
  NS_ASSERTION(aSize != 0, "zero capacity allocation not allowed");
  NS_ASSERTION(sizeof(nsStringBuffer) + aSize <= size_t(uint32_t(-1)) &&
                   sizeof(nsStringBuffer) + aSize > aSize,
               "mStorageSize will truncate");

  auto* hdr = (nsStringBuffer*)malloc(sizeof(nsStringBuffer) + aSize);
  if (hdr) {
    STRING_STAT_INCREMENT(Alloc);

    hdr->mRefCount = 1;
    hdr->mStorageSize = aSize;
    NS_LOG_ADDREF(hdr, 1, "nsStringBuffer", sizeof(*hdr));
  }
  return already_AddRefed(hdr);
}

template <typename CharT>
static already_AddRefed<nsStringBuffer> DoCreate(const CharT* aData,
                                                 size_t aLength) {
  RefPtr<nsStringBuffer> buffer =
      nsStringBuffer::Alloc((aLength + 1) * sizeof(CharT));
  if (MOZ_UNLIKELY(!buffer)) {
    return nullptr;
  }
  auto* data = reinterpret_cast<CharT*>(buffer->Data());
  memcpy(data, aData, aLength * sizeof(CharT));
  data[aLength] = 0;
  return buffer.forget();
}

already_AddRefed<nsStringBuffer> nsStringBuffer::Create(const char* aData,
                                                        size_t aLength) {
  return DoCreate(aData, aLength);
}

already_AddRefed<nsStringBuffer> nsStringBuffer::Create(const char16_t* aData,
                                                        size_t aLength) {
  return DoCreate(aData, aLength);
}

nsStringBuffer* nsStringBuffer::Realloc(nsStringBuffer* aHdr, size_t aSize) {
  STRING_STAT_INCREMENT(Realloc);

  NS_ASSERTION(aSize != 0, "zero capacity allocation not allowed");
  NS_ASSERTION(sizeof(nsStringBuffer) + aSize <= size_t(uint32_t(-1)) &&
                   sizeof(nsStringBuffer) + aSize > aSize,
               "mStorageSize will truncate");

  // no point in trying to save ourselves if we hit this assertion
  NS_ASSERTION(!aHdr->IsReadonly(), "|Realloc| attempted on readonly string");

  // Treat this as a release and addref for refcounting purposes, since we
  // just asserted that the refcount is 1.  If we don't do that, refcount
  // logging will claim we've leaked all sorts of stuff.
  NS_LOG_RELEASE(aHdr, 0, "nsStringBuffer");

  aHdr = (nsStringBuffer*)realloc(aHdr, sizeof(nsStringBuffer) + aSize);
  if (aHdr) {
    NS_LOG_ADDREF(aHdr, 1, "nsStringBuffer", sizeof(*aHdr));
    aHdr->mStorageSize = aSize;
  }

  return aHdr;
}

nsStringBuffer* nsStringBuffer::FromString(const nsAString& aStr) {
  if (!(aStr.mDataFlags & nsAString::DataFlags::REFCOUNTED)) {
    return nullptr;
  }

  return FromData(aStr.mData);
}

nsStringBuffer* nsStringBuffer::FromString(const nsACString& aStr) {
  if (!(aStr.mDataFlags & nsACString::DataFlags::REFCOUNTED)) {
    return nullptr;
  }

  return FromData(aStr.mData);
}

void nsStringBuffer::ToString(uint32_t aLen, nsAString& aStr,
                              bool aMoveOwnership) {
  char16_t* data = static_cast<char16_t*>(Data());

  MOZ_DIAGNOSTIC_ASSERT(data[aLen] == char16_t(0),
                        "data should be null terminated");

  nsAString::DataFlags flags =
      nsAString::DataFlags::REFCOUNTED | nsAString::DataFlags::TERMINATED;

  if (!aMoveOwnership) {
    AddRef();
  }
  aStr.Finalize();
  aStr.SetData(data, aLen, flags);
}

void nsStringBuffer::ToString(uint32_t aLen, nsACString& aStr,
                              bool aMoveOwnership) {
  char* data = static_cast<char*>(Data());

  MOZ_DIAGNOSTIC_ASSERT(data[aLen] == char(0),
                        "data should be null terminated");

  nsACString::DataFlags flags =
      nsACString::DataFlags::REFCOUNTED | nsACString::DataFlags::TERMINATED;

  if (!aMoveOwnership) {
    AddRef();
  }
  aStr.Finalize();
  aStr.SetData(data, aLen, flags);
}

size_t nsStringBuffer::SizeOfIncludingThisIfUnshared(
    mozilla::MallocSizeOf aMallocSizeOf) const {
  return IsReadonly() ? 0 : aMallocSizeOf(this);
}

size_t nsStringBuffer::SizeOfIncludingThisEvenIfShared(
    mozilla::MallocSizeOf aMallocSizeOf) const {
  return aMallocSizeOf(this);
}
