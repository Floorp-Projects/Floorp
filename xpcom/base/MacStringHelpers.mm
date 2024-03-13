/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MacStringHelpers.h"
#include "nsObjCExceptions.h"

#include "mozilla/IntegerTypeTraits.h"
#include <limits>

namespace mozilla {

void CopyNSStringToXPCOMString(const NSString* aFrom, nsAString& aTo) {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  if (!aFrom) {
    aTo.Truncate();
    return;
  }

  NSUInteger len = [aFrom length];
  if (len > std::numeric_limits<nsAString::size_type>::max()) {
    aTo.AllocFailed(std::numeric_limits<nsAString::size_type>::max());
  }

  aTo.SetLength(len);
  [aFrom getCharacters:reinterpret_cast<unichar*>(aTo.BeginWriting())
                 range:NSMakeRange(0, len)];

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

NSString* XPCOMStringToNSString(const nsAString& aFrom) {
  if (aFrom.IsEmpty()) {
    return [NSString string];
  }
  return [NSString stringWithCharacters:reinterpret_cast<const unichar*>(
                                            aFrom.BeginReading())
                                 length:aFrom.Length()];
}

NSString* XPCOMStringToNSString(const nsACString& aFrom) {
  if (aFrom.IsEmpty()) {
    return [NSString string];
  }
  return [[[NSString alloc] initWithBytes:aFrom.BeginReading()
                                   length:aFrom.Length()
                                 encoding:NSUTF8StringEncoding] autorelease];
}

}  // namespace mozilla
