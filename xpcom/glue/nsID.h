/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsID_h__
#define nsID_h__

#include <string.h>

#ifndef nscore_h___
#include "nscore.h"
#endif

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
   * Equivalency method. Compares this nsID with another.
   * @return <b>true</b> if they are the same, <b>false</b> if not.
   */

  inline bool Equals(const nsID& other) const {
    // Unfortunately memcmp isn't faster than this.
    return
      ((((uint32_t*) &m0)[0] == ((uint32_t*) &other.m0)[0]) &&
       (((uint32_t*) &m0)[1] == ((uint32_t*) &other.m0)[1]) &&
       (((uint32_t*) &m0)[2] == ((uint32_t*) &other.m0)[2]) &&
       (((uint32_t*) &m0)[3] == ((uint32_t*) &other.m0)[3]));
  }

  /**
   * nsID Parsing method. Turns a {xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx}
   * string into an nsID
   */
  NS_COM_GLUE bool Parse(const char *aIDStr);

#ifndef XPCOM_GLUE_AVOID_NSPR
  /**
   * nsID string encoder. Returns an allocated string in 
   * {xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx} format. Caller should free string.
   * YOU SHOULD ONLY USE THIS IF YOU CANNOT USE ToProvidedString() BELOW.
   */
  NS_COM_GLUE char* ToString() const;

  /**
   * nsID string encoder. Builds a string in 
   * {xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx} format, into a char[NSID_LENGTH]
   * buffer provided by the caller (for instance, on the stack).
   */
  NS_COM_GLUE void ToProvidedString(char (&dest)[NSID_LENGTH]) const;

#endif // XPCOM_GLUE_AVOID_NSPR

  //@}
};

/*
 * Class IDs
 */

typedef nsID nsCID;

// Define an CID
#define NS_DEFINE_CID(_name, _cidspec) \
  const nsCID _name = _cidspec

#define NS_DEFINE_NAMED_CID(_name) \
  static nsCID k##_name = _name

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
 
#define NS_DEFINE_IID(_name, _iidspec) \
  const nsIID _name = _iidspec

/**
 * A macro to build the static const IID accessor method. The Dummy
 * template parameter only exists so that the kIID symbol will be linked
 * properly (weak symbol on linux, gnu_linkonce on mac, multiple-definitions
 * merged on windows). Dummy should always be instantiated as "int".
 */

#define NS_DECLARE_STATIC_IID_ACCESSOR(the_iid)                         \
  template <class Dummy>                                                \
  struct COMTypeInfo                                                    \
  {                                                                     \
    static const nsIID kIID NS_HIDDEN;                                  \
  };                                                                    \
  static const nsIID& GetIID() {return COMTypeInfo<int>::kIID;}

#define NS_DEFINE_STATIC_IID_ACCESSOR(the_interface, the_iid)           \
  template <class Dummy>                                                \
  const nsIID the_interface::COMTypeInfo<Dummy>::kIID NS_HIDDEN = the_iid;

/**
 * A macro to build the static const CID accessor method
 */

#define NS_DEFINE_STATIC_CID_ACCESSOR(the_cid) \
  static const nsID& GetCID() {static const nsID cid = the_cid; return cid;}

#define NS_GET_IID(T) (::T::COMTypeInfo<int>::kIID)
#define NS_GET_TEMPLATE_IID(T) (T::template COMTypeInfo<int>::kIID)

#endif
