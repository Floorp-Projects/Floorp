/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsCommandLineServiceMac_h_
#define nsCommandLineServiceMac_h_

#include "nscore.h"

namespace CommandLineServiceMac {
void SetupMacCommandLine(int& argc, char**& argv, bool forRestart);

}  // namespace CommandLineServiceMac

#endif  // nsCommandLineServiceMac_h_
