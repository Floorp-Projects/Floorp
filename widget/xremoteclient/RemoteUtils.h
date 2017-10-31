/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef RemoteUtils_h__
#define RemoteUtils_h__

char*
ConstructCommandLine(int32_t argc, char **argv,
                     const char* aDesktopStartupID,
                     int *aCommandLineLength);

#endif // RemoteUtils_h__