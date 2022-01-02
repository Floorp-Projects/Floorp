/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/** @file
 * Helper functions for (de)serializing objects to/from ASCII strings.
 */

#ifndef NSSERIALIZATIONHELPER_H_
#define NSSERIALIZATIONHELPER_H_

#include "nsStringFwd.h"
#include "nsISerializationHelper.h"
#include "mozilla/Attributes.h"

class nsISerializable;

/**
 * Serialize an object to an ASCII string.
 */
nsresult NS_SerializeToString(nsISerializable* obj, nsACString& str);

/**
 * Deserialize an object.
 */
nsresult NS_DeserializeObject(const nsACString& str, nsISupports** obj);

class nsSerializationHelper final : public nsISerializationHelper {
  ~nsSerializationHelper() = default;

  NS_DECL_ISUPPORTS
  NS_DECL_NSISERIALIZATIONHELPER
};

#endif
