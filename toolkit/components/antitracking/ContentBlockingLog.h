/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ContentBlockingLog_h
#define mozilla_ContentBlockingLog_h

#include "mozilla/ContentBlockingNotifier.h"
#include "mozilla/JSONWriter.h"
#include "mozilla/Maybe.h"
#include "mozilla/StaticPrefs_browser.h"
#include "mozilla/Tuple.h"
#include "mozilla/UniquePtr.h"
#include "nsIWebProgressListener.h"
#include "nsReadableUtils.h"
#include "nsTArray.h"
#include "nsWindowSizes.h"

class nsIPrincipal;

namespace mozilla {

class ContentBlockingLog final {
  typedef ContentBlockingNotifier::StorageAccessPermissionGrantedReason
      StorageAccessPermissionGrantedReason;

  struct LogEntry {
    uint32_t mType;
    uint32_t mRepeatCount;
    bool mBlocked;
    Maybe<ContentBlockingNotifier::StorageAccessPermissionGrantedReason>
        mReason;
    nsTArray<nsCString> mTrackingFullHashes;
  };

  struct OriginDataEntry {
    OriginDataEntry()
        : mHasLevel1TrackingContentLoaded(false),
          mHasLevel2TrackingContentLoaded(false) {}

    bool mHasLevel1TrackingContentLoaded;
    bool mHasLevel2TrackingContentLoaded;
    Maybe<bool> mHasCookiesLoaded;
    Maybe<bool> mHasTrackerCookiesLoaded;
    Maybe<bool> mHasSocialTrackerCookiesLoaded;
    nsTArray<LogEntry> mLogs;
  };

  struct OriginEntry {
    OriginEntry() { mData = MakeUnique<OriginDataEntry>(); }

    nsCString mOrigin;
    UniquePtr<OriginDataEntry> mData;
  };

  typedef nsTArray<OriginEntry> OriginDataTable;

  struct StringWriteFunc : public JSONWriteFunc {
    nsACString&
        mBuffer;  // The lifetime of the struct must be bound to the buffer
    explicit StringWriteFunc(nsACString& aBuffer) : mBuffer(aBuffer) {}

    void Write(const char* aStr) override { mBuffer.Append(aStr); }
    void Write(const char* aStr, size_t aLen) override {
      mBuffer.Append(aStr, aLen);
    }
  };

  struct Comparator {
   public:
    bool Equals(const OriginDataTable::elem_type& aLeft,
                const OriginDataTable::elem_type& aRight) const {
      return aLeft.mOrigin.Equals(aRight.mOrigin);
    }

    bool Equals(const OriginDataTable::elem_type& aLeft,
                const nsACString& aRight) const {
      return aLeft.mOrigin.Equals(aRight);
    }
  };

 public:
  static const nsLiteralCString kDummyOriginHash;

  ContentBlockingLog() = default;
  ~ContentBlockingLog() = default;

  // Record the log in the parent process. This should be called only in the
  // parent process and will replace the RecordLog below after we remove the
  // ContentBlockingLog from content processes.
  Maybe<uint32_t> RecordLogParent(
      const nsACString& aOrigin, uint32_t aType, bool aBlocked,
      const Maybe<
          ContentBlockingNotifier::StorageAccessPermissionGrantedReason>&
          aReason,
      const nsTArray<nsCString>& aTrackingFullHashes);

  void RecordLog(
      const nsACString& aOrigin, uint32_t aType, bool aBlocked,
      const Maybe<
          ContentBlockingNotifier::StorageAccessPermissionGrantedReason>&
          aReason,
      const nsTArray<nsCString>& aTrackingFullHashes) {
    RecordLogInternal(aOrigin, aType, aBlocked, aReason, aTrackingFullHashes);
  }

  void ReportOrigins();
  void ReportLog(nsIPrincipal* aFirstPartyPrincipal);

  nsAutoCString Stringify() {
    nsAutoCString buffer;

    JSONWriter w(MakeUnique<StringWriteFunc>(buffer));
    w.Start();

    for (const OriginEntry& entry : mLog) {
      if (!entry.mData) {
        continue;
      }

      w.StartArrayProperty(entry.mOrigin.get(), w.SingleLineStyle);

      StringifyCustomFields(entry, w);
      for (const LogEntry& item : entry.mData->mLogs) {
        w.StartArrayElement(w.SingleLineStyle);
        {
          w.IntElement(item.mType);
          w.BoolElement(item.mBlocked);
          w.IntElement(item.mRepeatCount);
          if (item.mReason.isSome()) {
            w.IntElement(item.mReason.value());
          }
        }
        w.EndArray();
      }
      w.EndArray();
    }

    w.End();

    return buffer;
  }

  bool HasBlockedAnyOfType(uint32_t aType) const {
    // Note: nothing inside this loop should return false, the goal for the
    // loop is to scan the log to see if we find a matching entry, and if so
    // we would return true, otherwise in the end of the function outside of
    // the loop we take the common `return false;` statement.
    for (const OriginEntry& entry : mLog) {
      if (!entry.mData) {
        continue;
      }

      if (aType ==
          nsIWebProgressListener::STATE_LOADED_LEVEL_1_TRACKING_CONTENT) {
        if (entry.mData->mHasLevel1TrackingContentLoaded) {
          return true;
        }
      } else if (aType == nsIWebProgressListener::
                              STATE_LOADED_LEVEL_2_TRACKING_CONTENT) {
        if (entry.mData->mHasLevel2TrackingContentLoaded) {
          return true;
        }
      } else if (aType == nsIWebProgressListener::STATE_COOKIES_LOADED) {
        if (entry.mData->mHasCookiesLoaded.isSome() &&
            entry.mData->mHasCookiesLoaded.value()) {
          return true;
        }
      } else if (aType ==
                 nsIWebProgressListener::STATE_COOKIES_LOADED_TRACKER) {
        if (entry.mData->mHasTrackerCookiesLoaded.isSome() &&
            entry.mData->mHasTrackerCookiesLoaded.value()) {
          return true;
        }
      } else if (aType ==
                 nsIWebProgressListener::STATE_COOKIES_LOADED_SOCIALTRACKER) {
        if (entry.mData->mHasSocialTrackerCookiesLoaded.isSome() &&
            entry.mData->mHasSocialTrackerCookiesLoaded.value()) {
          return true;
        }
      } else {
        for (const auto& item : entry.mData->mLogs) {
          if (((item.mType & aType) != 0) && item.mBlocked) {
            return true;
          }
        }
      }
    }
    return false;
  }

  void AddSizeOfExcludingThis(nsWindowSizes& aSizes) const {
    aSizes.mDOMOtherSize +=
        mLog.ShallowSizeOfExcludingThis(aSizes.mState.mMallocSizeOf);

    // Now add the sizes of each origin log queue.
    for (const OriginEntry& entry : mLog) {
      if (entry.mData) {
        aSizes.mDOMOtherSize += aSizes.mState.mMallocSizeOf(entry.mData.get()) +
                                entry.mData->mLogs.ShallowSizeOfExcludingThis(
                                    aSizes.mState.mMallocSizeOf);
      }
    }
  }

  uint32_t GetContentBlockingEventsInLog() {
    uint32_t events = 0;

    // We iterate the whole log to produce the overview of blocked events.
    for (const OriginEntry& entry : mLog) {
      if (!entry.mData) {
        continue;
      }

      if (entry.mData->mHasLevel1TrackingContentLoaded) {
        events |= nsIWebProgressListener::STATE_LOADED_LEVEL_1_TRACKING_CONTENT;
      }

      if (entry.mData->mHasLevel2TrackingContentLoaded) {
        events |= nsIWebProgressListener::STATE_LOADED_LEVEL_2_TRACKING_CONTENT;
      }

      if (entry.mData->mHasCookiesLoaded.isSome() &&
          entry.mData->mHasCookiesLoaded.value()) {
        events |= nsIWebProgressListener::STATE_COOKIES_LOADED;
      }

      if (entry.mData->mHasTrackerCookiesLoaded.isSome() &&
          entry.mData->mHasTrackerCookiesLoaded.value()) {
        events |= nsIWebProgressListener::STATE_COOKIES_LOADED_TRACKER;
      }

      if (entry.mData->mHasSocialTrackerCookiesLoaded.isSome() &&
          entry.mData->mHasSocialTrackerCookiesLoaded.value()) {
        events |= nsIWebProgressListener::STATE_COOKIES_LOADED_SOCIALTRACKER;
      }

      for (const auto& item : entry.mData->mLogs) {
        if (item.mBlocked) {
          events |= item.mType;
        }
      }
    }

    return events;
  }

 private:
  void RecordLogInternal(
      const nsACString& aOrigin, uint32_t aType, bool aBlocked,
      const Maybe<
          ContentBlockingNotifier::StorageAccessPermissionGrantedReason>&
          aReason = Nothing(),
      const nsTArray<nsCString>& aTrackingFullHashes = nsTArray<nsCString>()) {
    DebugOnly<bool> isCookiesBlockedTracker =
        aType == nsIWebProgressListener::STATE_COOKIES_BLOCKED_TRACKER ||
        aType == nsIWebProgressListener::STATE_COOKIES_BLOCKED_SOCIALTRACKER;
    MOZ_ASSERT_IF(aBlocked, aReason.isNothing());
    MOZ_ASSERT_IF(!isCookiesBlockedTracker, aReason.isNothing());
    MOZ_ASSERT_IF(isCookiesBlockedTracker && !aBlocked, aReason.isSome());

    if (aOrigin.IsVoid()) {
      return;
    }
    auto index = mLog.IndexOf(aOrigin, 0, Comparator());
    if (index != OriginDataTable::NoIndex) {
      OriginEntry& entry = mLog[index];
      if (!entry.mData) {
        return;
      }

      if (RecordLogEntryInCustomField(aType, entry, aBlocked)) {
        return;
      }
      if (!entry.mData->mLogs.IsEmpty()) {
        auto& last = entry.mData->mLogs.LastElement();
        if (last.mType == aType && last.mBlocked == aBlocked) {
          ++last.mRepeatCount;
          // Don't record recorded events.  This helps compress our log.
          // We don't care about if the the reason is the same, just keep the
          // first one.
          // Note: {aReason, aTrackingFullHashes} are not compared here and we
          // simply keep the first for the reason, and merge hashes to make sure
          // they can be correctly recorded.
          for (const auto& hash : aTrackingFullHashes) {
            if (!last.mTrackingFullHashes.Contains(hash)) {
              last.mTrackingFullHashes.AppendElement(hash);
            }
          }
          return;
        }
      }
      if (entry.mData->mLogs.Length() ==
          std::max(1u,
                   StaticPrefs::browser_contentblocking_originlog_length())) {
        // Cap the size at the maximum length adjustable by the pref
        entry.mData->mLogs.RemoveElementAt(0);
      }
      entry.mData->mLogs.AppendElement(
          LogEntry{aType, 1u, aBlocked, aReason, aTrackingFullHashes.Clone()});
      return;
    }

    // The entry has not been found.

    OriginEntry* entry = mLog.AppendElement();
    if (NS_WARN_IF(!entry || !entry->mData)) {
      return;
    }

    entry->mOrigin = aOrigin;

    if (aType ==
        nsIWebProgressListener::STATE_LOADED_LEVEL_1_TRACKING_CONTENT) {
      entry->mData->mHasLevel1TrackingContentLoaded = aBlocked;
    } else if (aType ==
               nsIWebProgressListener::STATE_LOADED_LEVEL_2_TRACKING_CONTENT) {
      entry->mData->mHasLevel2TrackingContentLoaded = aBlocked;
    } else if (aType == nsIWebProgressListener::STATE_COOKIES_LOADED) {
      MOZ_ASSERT(entry->mData->mHasCookiesLoaded.isNothing());
      entry->mData->mHasCookiesLoaded.emplace(aBlocked);
    } else if (aType == nsIWebProgressListener::STATE_COOKIES_LOADED_TRACKER) {
      MOZ_ASSERT(entry->mData->mHasTrackerCookiesLoaded.isNothing());
      entry->mData->mHasTrackerCookiesLoaded.emplace(aBlocked);
    } else if (aType ==
               nsIWebProgressListener::STATE_COOKIES_LOADED_SOCIALTRACKER) {
      MOZ_ASSERT(entry->mData->mHasSocialTrackerCookiesLoaded.isNothing());
      entry->mData->mHasSocialTrackerCookiesLoaded.emplace(aBlocked);
    } else {
      entry->mData->mLogs.AppendElement(
          LogEntry{aType, 1u, aBlocked, aReason, aTrackingFullHashes.Clone()});
    }
  }

  bool RecordLogEntryInCustomField(uint32_t aType, OriginEntry& aEntry,
                                   bool aBlocked) {
    if (aType ==
        nsIWebProgressListener::STATE_LOADED_LEVEL_1_TRACKING_CONTENT) {
      aEntry.mData->mHasLevel1TrackingContentLoaded = aBlocked;
      return true;
    }
    if (aType ==
        nsIWebProgressListener::STATE_LOADED_LEVEL_2_TRACKING_CONTENT) {
      aEntry.mData->mHasLevel2TrackingContentLoaded = aBlocked;
      return true;
    }
    if (aType == nsIWebProgressListener::STATE_COOKIES_LOADED) {
      if (aEntry.mData->mHasCookiesLoaded.isSome()) {
        aEntry.mData->mHasCookiesLoaded.ref() = aBlocked;
      } else {
        aEntry.mData->mHasCookiesLoaded.emplace(aBlocked);
      }
      return true;
    }
    if (aType == nsIWebProgressListener::STATE_COOKIES_LOADED_TRACKER) {
      if (aEntry.mData->mHasTrackerCookiesLoaded.isSome()) {
        aEntry.mData->mHasTrackerCookiesLoaded.ref() = aBlocked;
      } else {
        aEntry.mData->mHasTrackerCookiesLoaded.emplace(aBlocked);
      }
      return true;
    }
    if (aType == nsIWebProgressListener::STATE_COOKIES_LOADED_SOCIALTRACKER) {
      if (aEntry.mData->mHasSocialTrackerCookiesLoaded.isSome()) {
        aEntry.mData->mHasSocialTrackerCookiesLoaded.ref() = aBlocked;
      } else {
        aEntry.mData->mHasSocialTrackerCookiesLoaded.emplace(aBlocked);
      }
      return true;
    }
    return false;
  }

  void StringifyCustomFields(const OriginEntry& aEntry, JSONWriter& aWriter) {
    if (aEntry.mData->mHasLevel1TrackingContentLoaded) {
      aWriter.StartArrayElement(aWriter.SingleLineStyle);
      {
        aWriter.IntElement(
            nsIWebProgressListener::STATE_LOADED_LEVEL_1_TRACKING_CONTENT);
        aWriter.BoolElement(true);  // blocked
        aWriter.IntElement(1);      // repeat count
      }
      aWriter.EndArray();
    }
    if (aEntry.mData->mHasLevel2TrackingContentLoaded) {
      aWriter.StartArrayElement(aWriter.SingleLineStyle);
      {
        aWriter.IntElement(
            nsIWebProgressListener::STATE_LOADED_LEVEL_2_TRACKING_CONTENT);
        aWriter.BoolElement(true);  // blocked
        aWriter.IntElement(1);      // repeat count
      }
      aWriter.EndArray();
    }
    if (aEntry.mData->mHasCookiesLoaded.isSome()) {
      aWriter.StartArrayElement(aWriter.SingleLineStyle);
      {
        aWriter.IntElement(nsIWebProgressListener::STATE_COOKIES_LOADED);
        aWriter.BoolElement(
            aEntry.mData->mHasCookiesLoaded.value());  // blocked
        aWriter.IntElement(1);                         // repeat count
      }
      aWriter.EndArray();
    }
    if (aEntry.mData->mHasTrackerCookiesLoaded.isSome()) {
      aWriter.StartArrayElement(aWriter.SingleLineStyle);
      {
        aWriter.IntElement(
            nsIWebProgressListener::STATE_COOKIES_LOADED_TRACKER);
        aWriter.BoolElement(
            aEntry.mData->mHasTrackerCookiesLoaded.value());  // blocked
        aWriter.IntElement(1);                                // repeat count
      }
      aWriter.EndArray();
    }
    if (aEntry.mData->mHasSocialTrackerCookiesLoaded.isSome()) {
      aWriter.StartArrayElement(aWriter.SingleLineStyle);
      {
        aWriter.IntElement(
            nsIWebProgressListener::STATE_COOKIES_LOADED_SOCIALTRACKER);
        aWriter.BoolElement(
            aEntry.mData->mHasSocialTrackerCookiesLoaded.value());  // blocked
        aWriter.IntElement(1);  // repeat count
      }
      aWriter.EndArray();
    }
  }

 private:
  OriginDataTable mLog;
};

}  // namespace mozilla

#endif
