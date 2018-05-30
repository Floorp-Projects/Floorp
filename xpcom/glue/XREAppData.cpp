/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/XREAppData.h"
#include "nsCRTGlue.h"

namespace mozilla {

XREAppData&
XREAppData::operator=(const StaticXREAppData& aOther)
{
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

  return *this;
}

XREAppData&
XREAppData::operator=(const XREAppData& aOther)
{
  directory = aOther.directory;
  vendor = aOther.vendor;
  name = aOther.name;
  remotingName = aOther.remotingName;
  version = aOther.version;
  buildID = aOther.buildID;
  ID = aOther.ID;
  copyright = aOther.copyright;
  flags = aOther.flags;
  xreDirectory = aOther.xreDirectory;
  minVersion = aOther.minVersion;
  maxVersion = aOther.maxVersion;
  crashReporterURL = aOther.crashReporterURL;
  profile = aOther.profile;
  UAName = aOther.UAName;
  sourceURL = aOther.sourceURL;
#if defined(XP_WIN) && defined(MOZ_SANDBOX)
  sandboxBrokerServices = aOther.sandboxBrokerServices;
  sandboxPermissionsService = aOther.sandboxPermissionsService;
#endif
  return *this;
}

} // namespace mozilla
