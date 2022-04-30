/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __ClassOfService_h__
#define __ClassOfService_h__

#include "ipc/IPCMessageUtils.h"

namespace mozilla::net {

class ClassOfService {
 public:
  ClassOfService() : mClassFlags(0), mIncremental(false) {}
  ClassOfService(unsigned long flags, bool incremental)
      : mClassFlags(flags), mIncremental(incremental) {}

  // class flags (priority)
  unsigned long Flags() const { return mClassFlags; }
  void SetFlags(unsigned long flags) { mClassFlags = flags; }

  // incremental flags
  bool Incremental() const { return mIncremental; }
  void SetIncremental(bool incremental) { mIncremental = incremental; }

 private:
  unsigned long mClassFlags;
  bool mIncremental;
  friend IPC::ParamTraits<mozilla::net::ClassOfService>;
  friend bool operator==(const ClassOfService& lhs, const ClassOfService& rhs);
  friend bool operator!=(const ClassOfService& lhs, const ClassOfService& rhs);
};

inline bool operator==(const ClassOfService& lhs, const ClassOfService& rhs) {
  return lhs.mClassFlags == rhs.mClassFlags &&
         lhs.mIncremental == rhs.mIncremental;
}

inline bool operator!=(const ClassOfService& lhs, const ClassOfService& rhs) {
  return !(lhs == rhs);
}

}  // namespace mozilla::net

namespace IPC {
template <>
struct ParamTraits<mozilla::net::ClassOfService> {
  typedef mozilla::net::ClassOfService paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.mClassFlags);
    WriteParam(aWriter, aParam.mIncremental);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    if (!ReadParam(aReader, &aResult->mClassFlags) ||
        !ReadParam(aReader, &aResult->mIncremental))
      return false;

    return true;
  }
};

}  // namespace IPC

#endif
