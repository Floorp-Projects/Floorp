/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ProfilerMarker_h
#define ProfilerMarker_h

#include "mozilla/UniquePtrExtensions.h"

#include "ProfilerMarkerPayload.h"

template<typename T>
class ProfilerLinkedList;
class SpliceableJSONWriter;
class UniqueStacks;

class ProfilerMarker
{
  friend class ProfilerLinkedList<ProfilerMarker>;

public:
  explicit ProfilerMarker(const char* aMarkerName,
                          ProfilerMarkerPayload* aPayload = nullptr,
                          double aTime = 0)
    : mMarkerName(strdup(aMarkerName))
    , mPayload(aPayload)
    , mTime(aTime)
  {}

  void SetGeneration(uint32_t aGenID) { mGenID = aGenID; }

  bool HasExpired(uint32_t aGenID) const { return mGenID + 2 <= aGenID; }

  double GetTime() const { return mTime; }

  void StreamJSON(SpliceableJSONWriter& aWriter,
                  const mozilla::TimeStamp& aStartTime,
                  UniqueStacks& aUniqueStacks) const
  {
    // Schema:
    //   [name, time, data]

    aWriter.StartArrayElement();
    {
      aUniqueStacks.mUniqueStrings.WriteElement(aWriter, mMarkerName.get());
      aWriter.DoubleElement(mTime);
      // TODO: Store the callsite for this marker if available:
      // if have location data
      //   b.NameValue(marker, "location", ...);
      if (mPayload) {
        aWriter.StartObjectElement();
        {
          mPayload->StreamPayload(aWriter, aStartTime, aUniqueStacks);
        }
        aWriter.EndObject();
      }
    }
    aWriter.EndArray();
  }

private:
  mozilla::UniqueFreePtr<char> mMarkerName;
  mozilla::UniquePtr<ProfilerMarkerPayload> mPayload;
  ProfilerMarker* mNext;
  double mTime;
  uint32_t mGenID;
};

template<typename T>
class ProfilerLinkedList
{
public:
  ProfilerLinkedList()
    : mHead(nullptr)
    , mTail(nullptr)
  {}

  void insert(T* aElem)
  {
    if (!mTail) {
      mHead = aElem;
      mTail = aElem;
    } else {
      mTail->mNext = aElem;
      mTail = aElem;
    }
    aElem->mNext = nullptr;
  }

  T* popHead()
  {
    if (!mHead) {
      MOZ_ASSERT(false);
      return nullptr;
    }

    T* head = mHead;

    mHead = head->mNext;
    if (!mHead) {
      mTail = nullptr;
    }

    return head;
  }

  const T* peek() {
    return mHead;
  }

private:
  T* mHead;
  T* mTail;
};

typedef ProfilerLinkedList<ProfilerMarker> ProfilerMarkerLinkedList;

template<typename T>
class ProfilerSignalSafeLinkedList
{
public:
  ProfilerSignalSafeLinkedList()
    : mSignalLock(false)
  {}

  ~ProfilerSignalSafeLinkedList()
  {
    if (mSignalLock) {
      // Some thread is modifying the list. We should only be released on that
      // thread.
      abort();
    }

    while (mList.peek()) {
      delete mList.popHead();
    }
  }

  // Insert an item into the list. Must only be called from the owning thread.
  // Must not be called while the list from accessList() is being accessed.
  // In the profiler, we ensure that by interrupting the profiled thread
  // (which is the one that owns this list and calls insert() on it) until
  // we're done reading the list from the signal handler.
  void insert(T* aElement)
  {
    MOZ_ASSERT(aElement);

    mSignalLock = true;

    mList.insert(aElement);

    mSignalLock = false;
  }

  // Called within signal, from any thread, possibly while insert() is in the
  // middle of modifying the list (on the owning thread). Will return null if
  // that is the case.
  // Function must be reentrant.
  ProfilerLinkedList<T>* accessList()
  {
    return mSignalLock ? nullptr : &mList;
  }

private:
  ProfilerLinkedList<T> mList;

  // If this is set, then it's not safe to read the list because its contents
  // are being changed.
  mozilla::Atomic<bool> mSignalLock;
};

#endif  // ProfilerMarker_h
