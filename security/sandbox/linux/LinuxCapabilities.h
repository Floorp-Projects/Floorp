/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_LinuxCapabilities_h
#define mozilla_LinuxCapabilities_h

#include <linux/capability.h>
#include <stdint.h>

#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/PodOperations.h"

// This class is a relatively simple interface to manipulating the
// capabilities of a Linux process/thread; see the capabilities(7) man
// page for background information.

// Unfortunately, Android's kernel headers omit some definitions
// needed for the low-level capability interface.  They're part of the
// stable syscall ABI, so it's safe to include them here.
#ifndef _LINUX_CAPABILITY_VERSION_3
#define _LINUX_CAPABILITY_VERSION_3  0x20080522
#define _LINUX_CAPABILITY_U32S_3     2
#endif
#ifndef CAP_TO_INDEX
#define CAP_TO_INDEX(x)     ((x) >> 5)
#define CAP_TO_MASK(x)      (1 << ((x) & 31))
#endif

namespace mozilla {

class LinuxCapabilities final
{
public:
  // A class to represent a bit within the capability sets as an lvalue.
  class BitRef {
    __u32& mWord;
    __u32 mMask;
    friend class LinuxCapabilities;
    BitRef(__u32& aWord, uint32_t aMask) : mWord(aWord), mMask(aMask) { }
    BitRef(const BitRef& aBit) : mWord(aBit.mWord), mMask(aBit.mMask) { }
  public:
    operator bool() const {
      return mWord & mMask;
    }
    BitRef& operator=(bool aSetTo) {
      if (aSetTo) {
        mWord |= mMask;
      } else {
        mWord &= mMask;
      }
      return *this;
    }
  };

  // The default value is the empty set.
  LinuxCapabilities() { PodArrayZero(mBits); }

  // Get the current thread's capability sets and assign them to this
  // object.  Returns whether it succeeded and sets errno on failure.
  // Shouldn't fail unless the kernel is very old.
  bool GetCurrent();

  // Try to set the current thread's capability sets to those
  // specified in this object.  Returns whether it succeeded and sets
  // errno on failure.
  bool SetCurrentRaw() const;

  // The capability model requires that the permitted set always be a
  // superset of the effective and inheritable sets.  This method
  // expands the permitted set as needed and then sets the current
  // thread's capabilities, as described above.
  bool SetCurrent() {
    Normalize();
    return SetCurrentRaw();
  }

  void Normalize() {
    for (size_t i = 0; i < _LINUX_CAPABILITY_U32S_3; ++i) {
      mBits[i].permitted |= mBits[i].effective | mBits[i].inheritable;
    }
  }

  // These three methods expose individual bits in the three
  // capability sets as objects that can be used as bool lvalues.
  // The argument is the capability number, as defined in
  // the <linux/capability.h> header.
  BitRef Effective(unsigned aCap)
  {
    return GenericBitRef(&__user_cap_data_struct::effective, aCap);
  }

  BitRef Permitted(unsigned aCap)
  {
    return GenericBitRef(&__user_cap_data_struct::permitted, aCap);
  }

  BitRef Inheritable(unsigned aCap)
  {
    return GenericBitRef(&__user_cap_data_struct::inheritable, aCap);
  }

private:
  __user_cap_data_struct mBits[_LINUX_CAPABILITY_U32S_3];

  BitRef GenericBitRef(__u32 __user_cap_data_struct::* aField, unsigned aCap)
  {
    // Please don't pass untrusted data as the capability number.
    MOZ_ASSERT(CAP_TO_INDEX(aCap) < _LINUX_CAPABILITY_U32S_3);
    return BitRef(mBits[CAP_TO_INDEX(aCap)].*aField, CAP_TO_MASK(aCap));
  }
};

} // namespace mozilla

#endif // mozilla_LinuxCapabilities_h
