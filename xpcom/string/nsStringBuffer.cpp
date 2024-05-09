/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsStringBuffer.h"

#include "mozilla/MemoryReporting.h"
#include "mozilla/RefPtr.h"

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
  MOZ_ASSERT(aSize != 0, "zero capacity allocation not allowed");
  MOZ_ASSERT(sizeof(nsStringBuffer) + aSize <= size_t(uint32_t(-1)) &&
                 sizeof(nsStringBuffer) + aSize > aSize,
             "mStorageSize will truncate");

  // no point in trying to save ourselves if we hit this assertion
  MOZ_ASSERT(!aHdr->IsReadonly(), "|Realloc| attempted on readonly string");

  // Treat this as a release and addref for refcounting purposes, since we
  // just asserted that the refcount is 1.  If we don't do that, refcount
  // logging will claim we've leaked all sorts of stuff.
  {
    mozilla::detail::RefCountLogger::ReleaseLogger logger(aHdr);
    logger.logRelease(0);
  }

  aHdr = (nsStringBuffer*)realloc(aHdr, sizeof(nsStringBuffer) + aSize);
  if (aHdr) {
    mozilla::detail::RefCountLogger::logAddRef(aHdr, 1);
    aHdr->mStorageSize = aSize;
  }

  return aHdr;
}

size_t nsStringBuffer::SizeOfIncludingThisIfUnshared(
    mozilla::MallocSizeOf aMallocSizeOf) const {
  return IsReadonly() ? 0 : aMallocSizeOf(this);
}

size_t nsStringBuffer::SizeOfIncludingThisEvenIfShared(
    mozilla::MallocSizeOf aMallocSizeOf) const {
  return aMallocSizeOf(this);
}
