/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ContentBlockingLog_h
#define mozilla_ContentBlockingLog_h

#include "mozilla/ContentBlockingNotifier.h"
#include "mozilla/JSONStringWriteFuncs.h"
#include "mozilla/Maybe.h"
#include "mozilla/StaticPrefs_browser.h"

#include "mozilla/UniquePtr.h"
#include "nsIWebProgressListener.h"
#include "nsReadableUtils.h"
#include "nsTArray.h"
#include "nsWindowSizes.h"

class nsIPrincipal;

namespace mozilla {

class nsRFPService;

class ContentBlockingLog final {
  typedef ContentBlockingNotifier::StorageAccessPermissionGrantedReason
      StorageAccessPermissionGrantedReason;

 protected:
  struct LogEntry {
    uint32_t mType;
    uint32_t mRepeatCount;
    bool mBlocked;
    Maybe<ContentBlockingNotifier::StorageAccessPermissionGrantedReason>
        mReason;
    nsTArray<nsCString> mTrackingFullHashes;
    Maybe<ContentBlockingNotifier::CanvasFingerprinter> mCanvasFingerprinter;
    Maybe<bool> mCanvasFingerprinterKnownText;
  };

  struct OriginDataEntry {
    OriginDataEntry()
        : mHasLevel1TrackingContentLoaded(false),
          mHasLevel2TrackingContentLoaded(false),
          mHasSuspiciousFingerprintingActivity(false) {}

    bool mHasLevel1TrackingContentLoaded;
    bool mHasLevel2TrackingContentLoaded;
    bool mHasSuspiciousFingerprintingActivity;
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

  friend class nsRFPService;

  typedef nsTArray<OriginEntry> OriginDataTable;

  struct Comparator {
   public:
    bool Equals(const OriginDataTable::value_type& aLeft,
                const OriginDataTable::value_type& aRight) const {
      return aLeft.mOrigin.Equals(aRight.mOrigin);
    }

    bool Equals(const OriginDataTable::value_type& aLeft,
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
      const nsTArray<nsCString>& aTrackingFullHashes,
      const Maybe<ContentBlockingNotifier::CanvasFingerprinter>&
          aCanvasFingerprinter,
      const Maybe<bool> aCanvasFingerprinterKnownText);

  void RecordLog(
      const nsACString& aOrigin, uint32_t aType, bool aBlocked,
      const Maybe<
          ContentBlockingNotifier::StorageAccessPermissionGrantedReason>&
          aReason,
      const nsTArray<nsCString>& aTrackingFullHashes) {
    RecordLogInternal(aOrigin, aType, aBlocked, aReason, aTrackingFullHashes);
  }

  void ReportLog(nsIPrincipal* aFirstPartyPrincipal);
  void ReportCanvasFingerprintingLog(nsIPrincipal* aFirstPartyPrincipal);
  void ReportFontFingerprintingLog(nsIPrincipal* aFirstPartyPrincipal);
  void ReportEmailTrackingLog(nsIPrincipal* aFirstPartyPrincipal);

  nsAutoCString Stringify() {
    nsAutoCString buffer;

    JSONStringRefWriteFunc js(buffer);
    JSONWriter w(js);
    w.Start();

    for (const OriginEntry& entry : mLog) {
      if (!entry.mData) {
        continue;
      }

      w.StartArrayProperty(entry.mOrigin, w.SingleLineStyle);

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
    aSizes.mDOMSizes.mDOMOtherSize +=
        mLog.ShallowSizeOfExcludingThis(aSizes.mState.mMallocSizeOf);

    // Now add the sizes of each origin log queue.
    for (const OriginEntry& entry : mLog) {
      if (entry.mData) {
        aSizes.mDOMSizes.mDOMOtherSize +=
            aSizes.mState.mMallocSizeOf(entry.mData.get()) +
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

      if (entry.mData->mHasSuspiciousFingerprintingActivity) {
        events |=
            nsIWebProgressListener::STATE_BLOCKED_SUSPICIOUS_FINGERPRINTING;
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
  OriginEntry* RecordLogInternal(
      const nsACString& aOrigin, uint32_t aType, bool aBlocked,
      const Maybe<
          ContentBlockingNotifier::StorageAccessPermissionGrantedReason>&
          aReason = Nothing(),
      const nsTArray<nsCString>& aTrackingFullHashes = nsTArray<nsCString>(),
      const Maybe<ContentBlockingNotifier::CanvasFingerprinter>&
          aCanvasFingerprinter = Nothing(),
      const Maybe<bool> aCanvasFingerprinterKnownText = Nothing());

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
    if (aEntry.mData->mHasSuspiciousFingerprintingActivity) {
      aWriter.StartArrayElement(aWriter.SingleLineStyle);
      {
        aWriter.IntElement(
            nsIWebProgressListener::STATE_BLOCKED_SUSPICIOUS_FINGERPRINTING);
        aWriter.BoolElement(true);  // blocked
        aWriter.IntElement(1);      // repeat count
      }
      aWriter.EndArray();
    }
  }

 private:
  OriginDataTable mLog;
};

}  // namespace mozilla

#endif
