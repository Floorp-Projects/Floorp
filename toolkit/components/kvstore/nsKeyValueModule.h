/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsKeyValueModule_h
#define nsKeyValueModule_h

#include "nsID.h"

extern "C" {
// Implemented in Rust.
nsresult nsKeyValueServiceConstructor(REFNSIID aIID, void** aResult);
}  // extern "C"

#endif  // defined nsKeyValueModule_h
