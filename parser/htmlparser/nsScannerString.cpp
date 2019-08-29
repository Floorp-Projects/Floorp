/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdlib.h>
#include "nsScannerString.h"
#include "mozilla/CheckedInt.h"

/**
 * nsScannerBufferList
 */

#define MAX_CAPACITY \
  ((UINT32_MAX / sizeof(char16_t)) - (sizeof(Buffer) + sizeof(char16_t)))

nsScannerBufferList::Buffer* nsScannerBufferList::AllocBufferFromString(
    const nsAString& aString) {
  uint32_t len = aString.Length();
  Buffer* buf = AllocBuffer(len);

  if (buf) {
    nsAString::const_iterator source;
    aString.BeginReading(source);
    nsCharTraits<char16_t>::copy(buf->DataStart(), source.get(), len);
  }
  return buf;
}

nsScannerBufferList::Buffer* nsScannerBufferList::AllocBuffer(
    uint32_t capacity) {
  if (capacity > MAX_CAPACITY) return nullptr;

  void* ptr = malloc(sizeof(Buffer) + (capacity + 1) * sizeof(char16_t));
  if (!ptr) return nullptr;

  Buffer* buf = new (ptr) Buffer();

  buf->mUsageCount = 0;
  buf->mDataEnd = buf->DataStart() + capacity;

  // XXX null terminate.  this shouldn't be required, but we do it because
  // nsScanner erroneously thinks it can dereference DataEnd :-(
  *buf->mDataEnd = char16_t(0);
  return buf;
}

void nsScannerBufferList::ReleaseAll() {
  while (!mBuffers.isEmpty()) {
    Buffer* node = mBuffers.popFirst();
    // printf(">>> freeing buffer @%p\n", node);
    free(node);
  }
}

void nsScannerBufferList::SplitBuffer(const Position& pos) {
  // splitting to the right keeps the work string and any extant token
  // pointing to and holding a reference count on the same buffer.

  Buffer* bufferToSplit = pos.mBuffer;
  NS_ASSERTION(bufferToSplit, "null pointer");

  uint32_t splitOffset = pos.mPosition - bufferToSplit->DataStart();
  NS_ASSERTION(pos.mPosition >= bufferToSplit->DataStart() &&
                   splitOffset <= bufferToSplit->DataLength(),
               "split offset is outside buffer");

  uint32_t len = bufferToSplit->DataLength() - splitOffset;
  Buffer* new_buffer = AllocBuffer(len);
  if (new_buffer) {
    nsCharTraits<char16_t>::copy(new_buffer->DataStart(),
                                 bufferToSplit->DataStart() + splitOffset, len);
    InsertAfter(new_buffer, bufferToSplit);
    bufferToSplit->SetDataLength(splitOffset);
  }
}

void nsScannerBufferList::DiscardUnreferencedPrefix(Buffer* aBuf) {
  if (aBuf == Head()) {
    while (!mBuffers.isEmpty() && !Head()->IsInUse()) {
      Buffer* buffer = Head();
      buffer->remove();
      free(buffer);
    }
  }
}

size_t nsScannerBufferList::Position::Distance(const Position& aStart,
                                               const Position& aEnd) {
  size_t result = 0;
  if (aStart.mBuffer == aEnd.mBuffer) {
    result = aEnd.mPosition - aStart.mPosition;
  } else {
    result = aStart.mBuffer->DataEnd() - aStart.mPosition;
    for (Buffer* b = aStart.mBuffer->Next(); b != aEnd.mBuffer; b = b->Next())
      result += b->DataLength();
    result += aEnd.mPosition - aEnd.mBuffer->DataStart();
  }
  return result;
}

/**
 * nsScannerSubstring
 */

nsScannerSubstring::nsScannerSubstring()
    : mStart(nullptr, nullptr),
      mEnd(nullptr, nullptr),
      mBufferList(nullptr),
      mLength(0),
      mIsDirty(true) {}

nsScannerSubstring::nsScannerSubstring(const nsAString& s)
    : mBufferList(nullptr), mIsDirty(true) {
  Rebind(s);
}

nsScannerSubstring::~nsScannerSubstring() {
  release_ownership_of_buffer_list();
}

int32_t nsScannerSubstring::CountChar(char16_t c) const {
  /*
    re-write this to use a counting sink
   */

  size_type result = 0;
  size_type lengthToExamine = Length();

  nsScannerIterator iter;
  for (BeginReading(iter);;) {
    int32_t lengthToExamineInThisFragment = iter.size_forward();
    const char16_t* fromBegin = iter.get();
    result += size_type(
        NS_COUNT(fromBegin, fromBegin + lengthToExamineInThisFragment, c));
    if (!(lengthToExamine -= lengthToExamineInThisFragment)) return result;
    iter.advance(lengthToExamineInThisFragment);
  }
  // never reached; quiets warnings
  return 0;
}

void nsScannerSubstring::Rebind(const nsScannerSubstring& aString,
                                const nsScannerIterator& aStart,
                                const nsScannerIterator& aEnd) {
  // allow for the case where &aString == this

  aString.acquire_ownership_of_buffer_list();
  release_ownership_of_buffer_list();

  mStart = aStart;
  mEnd = aEnd;
  mBufferList = aString.mBufferList;
  mLength = Distance(aStart, aEnd);
  mIsDirty = true;
}

void nsScannerSubstring::Rebind(const nsAString& aString) {
  release_ownership_of_buffer_list();

  mBufferList = new nsScannerBufferList(AllocBufferFromString(aString));
  mIsDirty = true;

  init_range_from_buffer_list();
  acquire_ownership_of_buffer_list();
}

const nsAString& nsScannerSubstring::AsString() const {
  if (mIsDirty) {
    nsScannerSubstring* mutable_this = const_cast<nsScannerSubstring*>(this);

    if (mStart.mBuffer == mEnd.mBuffer) {
      // We only have a single fragment to deal with, so just return it
      // as a substring.
      mutable_this->mFlattenedRep.Rebind(mStart.mPosition, mEnd.mPosition);
    } else {
      // Otherwise, we need to copy the data into a flattened buffer.
      nsScannerIterator start, end;
      CopyUnicodeTo(BeginReading(start), EndReading(end),
                    mutable_this->mFlattenedRep);
    }

    mutable_this->mIsDirty = false;
  }

  return mFlattenedRep;
}

nsScannerIterator& nsScannerSubstring::BeginReading(
    nsScannerIterator& iter) const {
  iter.mOwner = this;

  iter.mFragment.mBuffer = mStart.mBuffer;
  iter.mFragment.mFragmentStart = mStart.mPosition;
  if (mStart.mBuffer == mEnd.mBuffer)
    iter.mFragment.mFragmentEnd = mEnd.mPosition;
  else
    iter.mFragment.mFragmentEnd = mStart.mBuffer->DataEnd();

  iter.mPosition = mStart.mPosition;
  iter.normalize_forward();
  return iter;
}

nsScannerIterator& nsScannerSubstring::EndReading(
    nsScannerIterator& iter) const {
  iter.mOwner = this;

  iter.mFragment.mBuffer = mEnd.mBuffer;
  iter.mFragment.mFragmentEnd = mEnd.mPosition;
  if (mStart.mBuffer == mEnd.mBuffer)
    iter.mFragment.mFragmentStart = mStart.mPosition;
  else
    iter.mFragment.mFragmentStart = mEnd.mBuffer->DataStart();

  iter.mPosition = mEnd.mPosition;
  // must not |normalize_backward| as that would likely invalidate tests like
  // |while ( first != last )|
  return iter;
}

bool nsScannerSubstring::GetNextFragment(nsScannerFragment& frag) const {
  // check to see if we are at the end of the buffer list
  if (frag.mBuffer == mEnd.mBuffer) return false;

  frag.mBuffer = frag.mBuffer->getNext();

  if (frag.mBuffer == mStart.mBuffer)
    frag.mFragmentStart = mStart.mPosition;
  else
    frag.mFragmentStart = frag.mBuffer->DataStart();

  if (frag.mBuffer == mEnd.mBuffer)
    frag.mFragmentEnd = mEnd.mPosition;
  else
    frag.mFragmentEnd = frag.mBuffer->DataEnd();

  return true;
}

bool nsScannerSubstring::GetPrevFragment(nsScannerFragment& frag) const {
  // check to see if we are at the beginning of the buffer list
  if (frag.mBuffer == mStart.mBuffer) return false;

  frag.mBuffer = frag.mBuffer->getPrevious();

  if (frag.mBuffer == mStart.mBuffer)
    frag.mFragmentStart = mStart.mPosition;
  else
    frag.mFragmentStart = frag.mBuffer->DataStart();

  if (frag.mBuffer == mEnd.mBuffer)
    frag.mFragmentEnd = mEnd.mPosition;
  else
    frag.mFragmentEnd = frag.mBuffer->DataEnd();

  return true;
}

/**
 * nsScannerString
 */

nsScannerString::nsScannerString(Buffer* aBuf) {
  mBufferList = new nsScannerBufferList(aBuf);

  init_range_from_buffer_list();
  acquire_ownership_of_buffer_list();
}

void nsScannerString::AppendBuffer(Buffer* aBuf) {
  mBufferList->Append(aBuf);
  mLength += aBuf->DataLength();

  mEnd.mBuffer = aBuf;
  mEnd.mPosition = aBuf->DataEnd();

  mIsDirty = true;
}

void nsScannerString::DiscardPrefix(const nsScannerIterator& aIter) {
  Position old_start(mStart);
  mStart = aIter;
  mLength -= Position::Distance(old_start, mStart);

  mStart.mBuffer->IncrementUsageCount();
  old_start.mBuffer->DecrementUsageCount();

  mBufferList->DiscardUnreferencedPrefix(old_start.mBuffer);

  mIsDirty = true;
}

void nsScannerString::UngetReadable(const nsAString& aReadable,
                                    const nsScannerIterator& aInsertPoint)
/*
 * Warning: this routine manipulates the shared buffer list in an
 * unexpected way.  The original design did not really allow for
 * insertions, but this call promises that if called for a point after the
 * end of all extant token strings, that no token string or the work string
 * will be invalidated.
 *
 * This routine is protected because it is the responsibility of the
 * derived class to keep those promises.
 */
{
  Position insertPos(aInsertPoint);

  mBufferList->SplitBuffer(insertPos);
  // splitting to the right keeps the work string and any extant token
  // pointing to and holding a reference count on the same buffer

  Buffer* new_buffer = AllocBufferFromString(aReadable);
  // make a new buffer with all the data to insert...
  // ALERT: we may have empty space to re-use in the split buffer,
  // measure the cost of this and decide if we should do the work to fill
  // it

  Buffer* buffer_to_split = insertPos.mBuffer;
  mBufferList->InsertAfter(new_buffer, buffer_to_split);
  mLength += aReadable.Length();

  mEnd.mBuffer = mBufferList->Tail();
  mEnd.mPosition = mEnd.mBuffer->DataEnd();

  mIsDirty = true;
}

/**
 * nsScannerSharedSubstring
 */

void nsScannerSharedSubstring::Rebind(const nsScannerIterator& aStart,
                                      const nsScannerIterator& aEnd) {
  // If the start and end positions are inside the same buffer, we must
  // acquire ownership of the buffer.  If not, we can optimize by not holding
  // onto it.

  Buffer* buffer = const_cast<Buffer*>(aStart.buffer());
  bool sameBuffer = buffer == aEnd.buffer();

  nsScannerBufferList* bufferList;

  if (sameBuffer) {
    bufferList = aStart.mOwner->mBufferList;
    bufferList->AddRef();
    buffer->IncrementUsageCount();
  }

  if (mBufferList) ReleaseBuffer();

  if (sameBuffer) {
    mBuffer = buffer;
    mBufferList = bufferList;
    mString.Rebind(aStart.mPosition, aEnd.mPosition);
  } else {
    mBuffer = nullptr;
    mBufferList = nullptr;
    CopyUnicodeTo(aStart, aEnd, mString);
  }
}

void nsScannerSharedSubstring::ReleaseBuffer() {
  NS_ASSERTION(mBufferList, "Should only be called with non-null mBufferList");
  mBuffer->DecrementUsageCount();
  mBufferList->DiscardUnreferencedPrefix(mBuffer);
  mBufferList->Release();
}

void nsScannerSharedSubstring::MakeMutable() {
  nsString temp(mString);  // this will force a copy of the data
  mString.Assign(temp);    // mString will now share the just-allocated buffer

  ReleaseBuffer();

  mBuffer = nullptr;
  mBufferList = nullptr;
}

/**
 * utils -- based on code from nsReadableUtils.cpp
 */

// private helper function
static inline nsAString::iterator& copy_multifragment_string(
    nsScannerIterator& first, const nsScannerIterator& last,
    nsAString::iterator& result) {
  typedef nsCharSourceTraits<nsScannerIterator> source_traits;
  typedef nsCharSinkTraits<nsAString::iterator> sink_traits;

  while (first != last) {
    uint32_t distance = source_traits::readable_distance(first, last);
    sink_traits::write(result, source_traits::read(first), distance);
    NS_ASSERTION(distance > 0,
                 "|copy_multifragment_string| will never terminate");
    source_traits::advance(first, distance);
  }

  return result;
}

bool CopyUnicodeTo(const nsScannerIterator& aSrcStart,
                   const nsScannerIterator& aSrcEnd, nsAString& aDest) {
  mozilla::CheckedInt<nsAString::size_type> distance(
      Distance(aSrcStart, aSrcEnd));
  if (!distance.isValid()) {
    return false;  // overflow detected
  }

  if (!aDest.SetLength(distance.value(), mozilla::fallible)) {
    aDest.Truncate();
    return false;  // out of memory
  }
  auto writer = aDest.BeginWriting();
  nsScannerIterator fromBegin(aSrcStart);

  copy_multifragment_string(fromBegin, aSrcEnd, writer);
  return true;
}

bool AppendUnicodeTo(const nsScannerIterator& aSrcStart,
                     const nsScannerIterator& aSrcEnd,
                     nsScannerSharedSubstring& aDest) {
  // Check whether we can just create a dependent string.
  if (aDest.str().IsEmpty()) {
    // We can just make |aDest| point to the buffer.
    // This will take care of copying if the buffer spans fragments.
    aDest.Rebind(aSrcStart, aSrcEnd);
    return true;
  }
  // The dest string is not empty, so it can't be a dependent substring.
  return AppendUnicodeTo(aSrcStart, aSrcEnd, aDest.writable());
}

bool AppendUnicodeTo(const nsScannerIterator& aSrcStart,
                     const nsScannerIterator& aSrcEnd, nsAString& aDest) {
  const nsAString::size_type oldLength = aDest.Length();
  CheckedInt<nsAString::size_type> newLen(Distance(aSrcStart, aSrcEnd));
  newLen += oldLength;
  if (!newLen.isValid()) {
    return false;  // overflow detected
  }

  if (!aDest.SetLength(newLen.value(), mozilla::fallible))
    return false;  // out of memory
  auto writer = aDest.BeginWriting();
  std::advance(writer, oldLength);
  nsScannerIterator fromBegin(aSrcStart);

  copy_multifragment_string(fromBegin, aSrcEnd, writer);
  return true;
}

bool FindCharInReadable(char16_t aChar, nsScannerIterator& aSearchStart,
                        const nsScannerIterator& aSearchEnd) {
  while (aSearchStart != aSearchEnd) {
    int32_t fragmentLength;
    if (SameFragment(aSearchStart, aSearchEnd))
      fragmentLength = aSearchEnd.get() - aSearchStart.get();
    else
      fragmentLength = aSearchStart.size_forward();

    const char16_t* charFoundAt =
        nsCharTraits<char16_t>::find(aSearchStart.get(), fragmentLength, aChar);
    if (charFoundAt) {
      aSearchStart.advance(charFoundAt - aSearchStart.get());
      return true;
    }

    aSearchStart.advance(fragmentLength);
  }

  return false;
}

bool FindInReadable(const nsAString& aPattern, nsScannerIterator& aSearchStart,
                    nsScannerIterator& aSearchEnd,
                    const nsStringComparator& compare) {
  bool found_it = false;

  // only bother searching at all if we're given a non-empty range to search
  if (aSearchStart != aSearchEnd) {
    nsAString::const_iterator aPatternStart, aPatternEnd;
    aPattern.BeginReading(aPatternStart);
    aPattern.EndReading(aPatternEnd);

    // outer loop keeps searching till we find it or run out of string to search
    while (!found_it) {
      // fast inner loop (that's what it's called, not what it is) looks for a
      // potential match
      while (aSearchStart != aSearchEnd &&
             compare(aPatternStart.get(), aSearchStart.get(), 1, 1))
        ++aSearchStart;

      // if we broke out of the `fast' loop because we're out of string ...
      // we're done: no match
      if (aSearchStart == aSearchEnd) break;

      // otherwise, we're at a potential match, let's see if we really hit one
      nsAString::const_iterator testPattern(aPatternStart);
      nsScannerIterator testSearch(aSearchStart);

      // slow inner loop verifies the potential match (found by the `fast' loop)
      // at the current position
      for (;;) {
        // we already compared the first character in the outer loop,
        //  so we'll advance before the next comparison
        ++testPattern;
        ++testSearch;

        // if we verified all the way to the end of the pattern, then we found
        // it!
        if (testPattern == aPatternEnd) {
          found_it = true;
          aSearchEnd = testSearch;  // return the exact found range through the
                                    // parameters
          break;
        }

        // if we got to end of the string we're searching before we hit the end
        // of the
        //  pattern, we'll never find what we're looking for
        if (testSearch == aSearchEnd) {
          aSearchStart = aSearchEnd;
          break;
        }

        // else if we mismatched ... it's time to advance to the next search
        // position
        //  and get back into the `fast' loop
        if (compare(testPattern.get(), testSearch.get(), 1, 1)) {
          ++aSearchStart;
          break;
        }
      }
    }
  }

  return found_it;
}

/**
 * This implementation is simple, but does too much work.
 * It searches the entire string from left to right, and returns the last match
 * found, if any. This implementation will be replaced when I get
 * |reverse_iterator|s working.
 */
bool RFindInReadable(const nsAString& aPattern, nsScannerIterator& aSearchStart,
                     nsScannerIterator& aSearchEnd,
                     const nsStringComparator& aComparator) {
  bool found_it = false;

  nsScannerIterator savedSearchEnd(aSearchEnd);
  nsScannerIterator searchStart(aSearchStart), searchEnd(aSearchEnd);

  while (searchStart != searchEnd) {
    if (FindInReadable(aPattern, searchStart, searchEnd, aComparator)) {
      found_it = true;

      // this is the best match so far, so remember it
      aSearchStart = searchStart;
      aSearchEnd = searchEnd;

      // ...and get ready to search some more
      //  (it's tempting to set |searchStart=searchEnd| ... but that misses
      //  overlapping patterns)
      ++searchStart;
      searchEnd = savedSearchEnd;
    }
  }

  // if we never found it, return an empty range
  if (!found_it) aSearchStart = aSearchEnd;

  return found_it;
}
