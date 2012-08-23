/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ManifestParser_h
#define ManifestParser_h

#include "nsComponentManager.h"
#include "nsChromeRegistry.h"
#include "mozilla/FileLocation.h"

class nsIFile;

void ParseManifest(NSLocationType type, mozilla::FileLocation &file,
                   char* buf, bool aChromeOnly);

void LogMessage(const char* aMsg, ...);

void LogMessageWithContext(mozilla::FileLocation &aFile,
                           uint32_t aLineNumber, const char* aMsg, ...);

#endif // ManifestParser_h
