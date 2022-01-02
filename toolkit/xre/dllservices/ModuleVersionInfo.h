/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ModuleVersionInfo_h
#define mozilla_ModuleVersionInfo_h

#include <windows.h>
#include "nsString.h"

namespace mozilla {

// Obtains basic version info from a module image's version info resource.
class ModuleVersionInfo {
 public:
  // We favor English(US) for these fields, otherwise we take the first
  // translation provided in the version resource.
  nsString mCompanyName;
  nsString mProductName;
  nsString mLegalCopyright;
  nsString mFileDescription;

  // Represents an A.B.C.D style version number, internally stored as a uint64_t
  class VersionNumber {
    uint64_t mVersion64 = 0;

   public:
    VersionNumber() = default;

    VersionNumber(DWORD aMostSig, DWORD aLeastSig)
        : mVersion64((uint64_t)aMostSig << 32 | aLeastSig) {}

    uint16_t A() const {
      return (uint16_t)((mVersion64 & 0xffff000000000000) >> 48);
    }

    uint16_t B() const {
      return (uint16_t)((mVersion64 & 0x0000ffff00000000) >> 32);
    }

    uint16_t C() const {
      return (uint16_t)((mVersion64 & 0x00000000ffff0000) >> 16);
    }

    uint16_t D() const { return (uint16_t)(mVersion64 & 0x000000000000ffff); }

    uint64_t Version64() const { return mVersion64; }

    bool operator==(const VersionNumber& aOther) const {
      return mVersion64 == aOther.mVersion64;
    }

    nsCString ToString() const {
      nsCString ret;
      ret.AppendPrintf("%d.%d.%d.%d", (int)A(), (int)B(), (int)C(), (int)D());
      return ret;
    }
  };

  VersionNumber mFileVersion;
  VersionNumber mProductVersion;

  // Returns false if it has no version resource or has no fixed version info.
  bool GetFromImage(const nsAString& aPath);
};

}  // namespace mozilla

#endif  // mozilla_ModuleVersionInfo_h
