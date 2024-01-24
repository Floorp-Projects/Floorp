/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// The Gecko Profiler is an always-on profiler that takes fast and low overhead
// samples of the program execution using only userspace functionality for
// portability. The goal of this module is to provide performance data in a
// generic cross-platform way without requiring custom tools or kernel support.
//
// Samples are collected to form a timeline with optional timeline event
// (markers) used for filtering. The samples include both native stacks and
// platform-independent "label stack" frames.

#ifndef ProfileAdditionalInformation_h
#define ProfileAdditionalInformation_h

#include "shared-libraries.h"
#include "js/Value.h"
#include "nsString.h"

namespace IPC {
class MessageReader;
class MessageWriter;
template <typename T>
struct ParamTraits;
}  // namespace IPC

namespace mozilla {
// This structure contains additional information gathered while generating the
// profile json and iterating the buffer.
struct ProfileGenerationAdditionalInformation {
  ProfileGenerationAdditionalInformation() = default;
  explicit ProfileGenerationAdditionalInformation(
      const SharedLibraryInfo&& aSharedLibraries)
      : mSharedLibraries(aSharedLibraries) {}

  size_t SizeOf() const { return mSharedLibraries.SizeOf(); }

  void Append(ProfileGenerationAdditionalInformation&& aOther) {
    mSharedLibraries.AddAllSharedLibraries(aOther.mSharedLibraries);
  }

  void FinishGathering() { mSharedLibraries.DeduplicateEntries(); }

  void ToJSValue(JSContext* aCx, JS::MutableHandle<JS::Value> aRetVal) const;

  SharedLibraryInfo mSharedLibraries;
};

struct ProfileAndAdditionalInformation {
  ProfileAndAdditionalInformation() = default;
  explicit ProfileAndAdditionalInformation(const nsCString&& aProfile)
      : mProfile(aProfile) {}

  ProfileAndAdditionalInformation(
      const nsCString&& aProfile,
      const ProfileGenerationAdditionalInformation&& aAdditionalInformation)
      : mProfile(aProfile),
        mAdditionalInformation(Some(aAdditionalInformation)) {}

  size_t SizeOf() const {
    size_t size = mProfile.Length();
    if (mAdditionalInformation.isSome()) {
      size += mAdditionalInformation->SizeOf();
    }
    return size;
  }

  nsCString mProfile;
  Maybe<ProfileGenerationAdditionalInformation> mAdditionalInformation;
};
}  // namespace mozilla

namespace IPC {
template <>
struct ParamTraits<mozilla::ProfileGenerationAdditionalInformation> {
  typedef mozilla::ProfileGenerationAdditionalInformation paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam);
  static bool Read(MessageReader* aReader, paramType* aResult);
};
}  // namespace IPC

#endif  // ProfileAdditionalInformation_h
