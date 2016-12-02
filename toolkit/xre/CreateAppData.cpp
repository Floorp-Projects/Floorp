/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsXULAppAPI.h"
#include "nsINIParser.h"
#include "nsIFile.h"
#include "nsAutoPtr.h"
#include "mozilla/XREAppData.h"

using namespace mozilla;

static void
ReadString(nsINIParser &parser, const char* section,
           const char* key, XREAppData::CharPtr& result)
{
  nsCString str;
  nsresult rv = parser.GetString(section, key, str);
  if (NS_SUCCEEDED(rv)) {
    result = str.get();
  }
}

struct ReadFlag {
  const char *section;
  const char *key;
  uint32_t flag;
};

static void
ReadFlag(nsINIParser &parser, const char* section,
         const char* key, uint32_t flag, uint32_t& result)
{
  char buf[6]; // large enough to hold "false"
  nsresult rv = parser.GetString(section, key, buf, sizeof(buf));
  if (NS_SUCCEEDED(rv) || rv == NS_ERROR_LOSS_OF_SIGNIFICANT_DATA) {
    if (buf[0] == '1' || buf[0] == 't' || buf[0] == 'T') {
      result |= flag;
    }
    if (buf[0] == '0' || buf[0] == 'f' || buf[0] == 'F') {
      result &= ~flag;
    }
  }
}

nsresult
XRE_ParseAppData(nsIFile* aINIFile, XREAppData& aAppData)
{
  NS_ENSURE_ARG(aINIFile);

  nsresult rv;

  nsINIParser parser;
  rv = parser.Init(aINIFile);
  if (NS_FAILED(rv))
    return rv;

  ReadString(parser, "App", "Vendor", aAppData.vendor);
  ReadString(parser, "App", "Name", aAppData.name),
  ReadString(parser, "App", "RemotingName", aAppData.remotingName);
  ReadString(parser, "App", "Version", aAppData.version);
  ReadString(parser, "App", "BuildID", aAppData.buildID);
  ReadString(parser, "App", "ID", aAppData.ID);
  ReadString(parser, "App", "Copyright", aAppData.copyright);
  ReadString(parser, "App", "Profile", aAppData.profile);
  ReadString(parser, "Gecko", "MinVersion", aAppData.minVersion);
  ReadString(parser, "Gecko", "MaxVersion", aAppData.maxVersion);
  ReadString(parser, "Crash Reporter", "ServerURL", aAppData.crashReporterURL);
  ReadString(parser, "App", "UAName", aAppData.UAName);
  ReadFlag(parser, "XRE", "EnableProfileMigrator",
           NS_XRE_ENABLE_PROFILE_MIGRATOR, aAppData.flags);
  ReadFlag(parser, "Crash Reporter", "Enabled",
           NS_XRE_ENABLE_CRASH_REPORTER, aAppData.flags);

  return NS_OK;
}
