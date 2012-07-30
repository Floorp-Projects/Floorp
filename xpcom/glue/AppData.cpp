/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/AppData.h"
#include "nsXULAppAPI.h"
#include "nsINIParser.h"
#include "nsIFile.h"
#include "nsCRTGlue.h"
#include "nsAutoPtr.h"

namespace mozilla {

void
SetAllocatedString(const char *&str, const char *newvalue)
{
  NS_Free(const_cast<char*>(str));
  if (newvalue) {
    str = NS_strdup(newvalue);
  }
  else {
    str = nullptr;
  }
}

void
SetAllocatedString(const char *&str, const nsACString &newvalue)
{
  NS_Free(const_cast<char*>(str));
  if (newvalue.IsEmpty()) {
    str = nullptr;
  }
  else {
    str = ToNewCString(newvalue);
  }
}

ScopedAppData::ScopedAppData(const nsXREAppData* aAppData)
{
  Zero();

  this->size = aAppData->size;

  SetAllocatedString(this->vendor, aAppData->vendor);
  SetAllocatedString(this->name, aAppData->name);
  SetAllocatedString(this->version, aAppData->version);
  SetAllocatedString(this->buildID, aAppData->buildID);
  SetAllocatedString(this->ID, aAppData->ID);
  SetAllocatedString(this->copyright, aAppData->copyright);
  SetAllocatedString(this->profile, aAppData->profile);
  SetStrongPtr(this->directory, aAppData->directory);
  this->flags = aAppData->flags;

  if (aAppData->size > offsetof(nsXREAppData, xreDirectory)) {
    SetStrongPtr(this->xreDirectory, aAppData->xreDirectory);
    SetAllocatedString(this->minVersion, aAppData->minVersion);
    SetAllocatedString(this->maxVersion, aAppData->maxVersion);
  }

  if (aAppData->size > offsetof(nsXREAppData, crashReporterURL)) {
    SetAllocatedString(this->crashReporterURL, aAppData->crashReporterURL);
  }

  if (aAppData->size > offsetof(nsXREAppData, UAName)) {
    SetAllocatedString(this->UAName, aAppData->UAName);
  }
}

ScopedAppData::~ScopedAppData()
{
  SetAllocatedString(this->vendor, nullptr);
  SetAllocatedString(this->name, nullptr);
  SetAllocatedString(this->version, nullptr);
  SetAllocatedString(this->buildID, nullptr);
  SetAllocatedString(this->ID, nullptr);
  SetAllocatedString(this->copyright, nullptr);
  SetAllocatedString(this->profile, nullptr);

  NS_IF_RELEASE(this->directory);

  SetStrongPtr(this->xreDirectory, (nsIFile*) nullptr);
  SetAllocatedString(this->minVersion, nullptr);
  SetAllocatedString(this->maxVersion, nullptr);

  SetAllocatedString(this->crashReporterURL, nullptr);
  SetAllocatedString(this->UAName, nullptr);
}

} // namespace mozilla
