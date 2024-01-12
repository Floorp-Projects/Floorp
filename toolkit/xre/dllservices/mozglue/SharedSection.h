/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_glue_SharedSection_h
#define mozilla_glue_SharedSection_h

#include <winternl.h>
#include "nscore.h"
#include "mozilla/Maybe.h"
#include "mozilla/Vector.h"
#include "mozilla/WindowsDllBlocklistInfo.h"

namespace mozilla::nt {

// This interface provides a way to access winlauncher's shared section
// through DllServices.
struct NS_NO_VTABLE SharedSection {
  // Returns the recorded dependent modules. A return value of Nothing()
  // indicates an error. (for example, if the launcher process isn't active)
  virtual Maybe<Vector<const wchar_t*>> GetDependentModules() = 0;
  // Returns a span of the whole space for dynamic blocklist entries.
  // Use IsValidDynamicBlocklistEntry() to determine the end of the list.
  virtual Span<const DllBlockInfoT<UNICODE_STRING>> GetDynamicBlocklist() = 0;
};

}  // namespace mozilla::nt

#endif  // mozilla_glue_SharedSection_h
