/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsXULAppAPI.h"
#include "nsINIParser.h"
#include "nsIFile.h"
#include "nsAutoPtr.h"
#include "mozilla/AppData.h"

using namespace mozilla;

nsresult
XRE_CreateAppData(nsIFile* aINIFile, nsXREAppData **aAppData)
{
  NS_ENSURE_ARG(aINIFile && aAppData);

  nsAutoPtr<ScopedAppData> data(new ScopedAppData());
  if (!data)
    return NS_ERROR_OUT_OF_MEMORY;

  nsresult rv = XRE_ParseAppData(aINIFile, data);
  if (NS_FAILED(rv))
    return rv;

  if (!data->directory) {
    nsCOMPtr<nsIFile> appDir;
    rv = aINIFile->GetParent(getter_AddRefs(appDir));
    if (NS_FAILED(rv))
      return rv;

    appDir.forget(&data->directory);
  }

  *aAppData = data.forget();
  return NS_OK;
}

struct ReadString {
  const char *section;
  const char *key;
  const char **buffer;
};

static void
ReadStrings(nsINIParser &parser, const ReadString *reads)
{
  nsresult rv;
  nsCString str;

  while (reads->section) {
    rv = parser.GetString(reads->section, reads->key, str);
    if (NS_SUCCEEDED(rv)) {
      SetAllocatedString(*reads->buffer, str);
    }

    ++reads;
  }
}

struct ReadFlag {
  const char *section;
  const char *key;
  uint32_t flag;
};

static void
ReadFlags(nsINIParser &parser, const ReadFlag *reads, uint32_t *buffer)
{
  nsresult rv;
  char buf[6]; // large enough to hold "false"

  while (reads->section) {
    rv = parser.GetString(reads->section, reads->key, buf, sizeof(buf));
    if (NS_SUCCEEDED(rv) || rv == NS_ERROR_LOSS_OF_SIGNIFICANT_DATA) {
      if (buf[0] == '1' || buf[0] == 't' || buf[0] == 'T') {
        *buffer |= reads->flag;
      }
      if (buf[0] == '0' || buf[0] == 'f' || buf[0] == 'F') {
        *buffer &= ~reads->flag;
      }
    }

    ++reads;
  }
}

nsresult
XRE_ParseAppData(nsIFile* aINIFile, nsXREAppData *aAppData)
{
  NS_ENSURE_ARG(aINIFile && aAppData);

  nsresult rv;

  nsINIParser parser;
  rv = parser.Init(aINIFile);
  if (NS_FAILED(rv))
    return rv;

  nsCString str;

  ReadString strings[] = {
    { "App", "Vendor",    &aAppData->vendor },
    { "App", "Name",      &aAppData->name },
    { "App", "Version",   &aAppData->version },
    { "App", "BuildID",   &aAppData->buildID },
    { "App", "ID",        &aAppData->ID },
    { "App", "Copyright", &aAppData->copyright },
    { "App", "Profile",   &aAppData->profile },
    { nullptr }
  };
  ReadStrings(parser, strings);

  ReadFlag flags[] = {
    { "XRE", "EnableProfileMigrator", NS_XRE_ENABLE_PROFILE_MIGRATOR },
    { nullptr }
  };
  ReadFlags(parser, flags, &aAppData->flags);

  if (aAppData->size > offsetof(nsXREAppData, xreDirectory)) {
    ReadString strings2[] = {
      { "Gecko", "MinVersion", &aAppData->minVersion },
      { "Gecko", "MaxVersion", &aAppData->maxVersion },
      { nullptr }
    };
    ReadStrings(parser, strings2);
  }

  if (aAppData->size > offsetof(nsXREAppData, crashReporterURL)) {
    ReadString strings3[] = {
      { "Crash Reporter", "ServerURL", &aAppData->crashReporterURL },
      { nullptr }
    };
    ReadStrings(parser, strings3);
    ReadFlag flags2[] = {
      { "Crash Reporter", "Enabled", NS_XRE_ENABLE_CRASH_REPORTER },
      { nullptr }
    };
    ReadFlags(parser, flags2, &aAppData->flags);
  }

  if (aAppData->size > offsetof(nsXREAppData, UAName)) {
    ReadString strings4[] = {
      { "App", "UAName",    &aAppData->UAName },
      { nullptr }
    };
    ReadStrings(parser, strings4);
  }

  return NS_OK;
}

void
XRE_FreeAppData(nsXREAppData *aAppData)
{
  if (!aAppData) {
    NS_ERROR("Invalid arg");
    return;
  }

  ScopedAppData* sad = static_cast<ScopedAppData*>(aAppData);
  delete sad;
}
