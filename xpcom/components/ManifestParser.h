/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ManifestParser_h
#define ManifestParser_h

#include "nsComponentManager.h"
#include "nsChromeRegistry.h"
#include "mozilla/Attributes.h"
#include "mozilla/FileLocation.h"

void ParseManifest(NSLocationType aType, mozilla::FileLocation& aFile,
                   char* aBuf, bool aChromeOnly);

void LogMessage(const char* aMsg, ...) MOZ_FORMAT_PRINTF(1, 2);

void LogMessageWithContext(mozilla::FileLocation& aFile, uint32_t aLineNumber,
                           const char* aMsg, ...) MOZ_FORMAT_PRINTF(3, 4);

#endif  // ManifestParser_h
