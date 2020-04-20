/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsID_h__
#define nsID_h__

#include <string.h>

#include "nscore.h"

#define NSID_LENGTH 39

/**
 * A "unique identifier". This is modeled after OSF DCE UUIDs.
 */

struct nsID {
  /**
   * @name Identifier values
   */

  //@{
  uint32_t m0;
  uint16_t m1;
  uint16_t m2;
  uint8_t m3[8];
  //@}

  /**
   * @name Methods
   */

  //@{
  /**
   * Ensures everything is zeroed out.
   */
  void Clear();

  /**
   * Equivalency method. Compares this nsID with another.
   * @return <b>true</b> if they are the same, <b>false</b> if not.
   */

  inline bool Equals(const nsID& aOther) const {
    // At the time of this writing, modern compilers (namely Clang) inline this
    // memcmp call into a single SIMD operation on x86/x86-64 architectures (as
    // long as we compare to zero rather than returning the full integer return
    // value), which is measurably more efficient to any manual comparisons we
    // can do directly.
#if defined(__x86_64__) || defined(__i386__)
    return !memcmp(this, &aOther, sizeof *this);
#else
    // However, on ARM architectures, compilers still tend to generate a direct
    // memcmp call, which we'd like to avoid.
    return (((uint32_t*)&m0)[0] == ((uint32_t*)&aOther.m0)[0]) &&
           (((uint32_t*)&m0)[1] == ((uint32_t*)&aOther.m0)[1]) &&
           (((uint32_t*)&m0)[2] == ((uint32_t*)&aOther.m0)[2]) &&
           (((uint32_t*)&m0)[3] == ((uint32_t*)&aOther.m0)[3]);
#endif
  }

  inline bool operator==(const nsID& aOther) const { return Equals(aOther); }

  /**
   * nsID Parsing method. Turns a {xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx}
   * string into an nsID
   */
  bool Parse(const char* aIDStr);

#ifndef XPCOM_GLUE_AVOID_NSPR
  /**
   * nsID string encoder. Returns an allocated string in
   * {xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx} format. Caller should free string.
   * YOU SHOULD ONLY USE THIS IF YOU CANNOT USE ToProvidedString() BELOW.
   */
  char* ToString() const;

  /**
   * nsID string encoder. Builds a string in
   * {xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx} format, into a char[NSID_LENGTH]
   * buffer provided by the caller (for instance, on the stack).
   */
  void ToProvidedString(char (&aDest)[NSID_LENGTH]) const;

#endif  // XPCOM_GLUE_AVOID_NSPR

  // Infallibly duplicate an nsID. Must be freed with free().
  nsID* Clone() const;

  //@}
};

#ifndef XPCOM_GLUE_AVOID_NSPR
/**
 * A stack helper class to convert a nsID to a string.  Useful
 * for printing nsIDs.  For example:
 *   nsID aID = ...;
 *   printf("%s", nsIDToCString(aID).get());
 */
class nsIDToCString {
 public:
  explicit nsIDToCString(const nsID& aID) {
    aID.ToProvidedString(mStringBytes);
  }

  const char* get() const { return mStringBytes; }

 protected:
  char mStringBytes[NSID_LENGTH];
};
#endif

/*
 * Class IDs
 */

typedef nsID nsCID;

// Define an CID
#define NS_DEFINE_CID(_name, _cidspec) const nsCID _name = _cidspec

#define NS_DEFINE_NAMED_CID(_name) static const nsCID k##_name = _name

#define REFNSCID const nsCID&

/**
 * An "interface id" which can be used to uniquely identify a given
 * interface.
 */

typedef nsID nsIID;

/**
 * A macro shorthand for <tt>const nsIID&<tt>
 */

#define REFNSIID const nsIID&

/**
 * Define an IID
 * obsolete - do not use this macro
 */

#define NS_DEFINE_IID(_name, _iidspec) const nsIID _name = _iidspec

/**
 * A macro to build the static const IID accessor method. The Dummy
 * template parameter only exists so that the kIID symbol will be linked
 * properly (weak symbol on linux, gnu_linkonce on mac, multiple-definitions
 * merged on windows). Dummy should always be instantiated as "void".
 */

#define NS_DECLARE_STATIC_IID_ACCESSOR(the_iid) \
  template <typename T, typename U>             \
  struct COMTypeInfo;

#define NS_DEFINE_STATIC_IID_ACCESSOR(the_interface, the_iid)                \
  template <typename T>                                                      \
  struct the_interface::COMTypeInfo<the_interface, T> {                      \
    static const nsIID kIID NS_HIDDEN;                                       \
  };                                                                         \
  template <typename T>                                                      \
  const nsIID the_interface::COMTypeInfo<the_interface, T>::kIID NS_HIDDEN = \
      the_iid;

/**
 * A macro to build the static const CID accessor method
 */

#define NS_DEFINE_STATIC_CID_ACCESSOR(the_cid) \
  static const nsID& GetCID() {                \
    static const nsID cid = the_cid;           \
    return cid;                                \
  }

#define NS_GET_IID(T) (T::COMTypeInfo<T, void>::kIID)
#define NS_GET_TEMPLATE_IID(T) (T::template COMTypeInfo<T, void>::kIID)

#endif
