/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/XREAppData.h"
#include "nsCRTGlue.h"

namespace mozilla {

XREAppData& XREAppData::operator=(const StaticXREAppData& aOther) {
  vendor = aOther.vendor;
  name = aOther.name;
  remotingName = aOther.remotingName;
  version = aOther.version;
  buildID = aOther.buildID;
  ID = aOther.ID;
  copyright = aOther.copyright;
  flags = aOther.flags;
  minVersion = aOther.minVersion;
  maxVersion = aOther.maxVersion;
  crashReporterURL = aOther.crashReporterURL;
  profile = aOther.profile;
  UAName = aOther.UAName;
  sourceURL = aOther.sourceURL;
  updateURL = aOther.updateURL;

  return *this;
}

XREAppData& XREAppData::operator=(const XREAppData& aOther) = default;

void XREAppData::SanitizeNameForDBus(nsACString& aName) {
  auto IsValidDBusNameChar = [](char aChar) {
    return IsAsciiAlpha(aChar) || IsAsciiDigit(aChar) || aChar == '_';
  };

  // D-Bus names can contain only [a-z][A-Z][0-9]_, so we replace all characters
  // that aren't in that range with underscores.
  char* cur = aName.BeginWriting();
  char* end = aName.EndWriting();
  for (; cur != end; cur++) {
    if (!IsValidDBusNameChar(*cur)) {
      *cur = '_';
    }
  }
}

void XREAppData::GetDBusAppName(nsACString& aName) const {
  const char* env = getenv("MOZ_DBUS_APP_NAME");
  if (env) {
    aName.Assign(env);
  } else {
    aName.Assign(name);
    ToLowerCase(aName);
    SanitizeNameForDBus(aName);
  }
}

}  // namespace mozilla
