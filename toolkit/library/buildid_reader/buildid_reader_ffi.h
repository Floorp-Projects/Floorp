/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef buildid_reader_ffi_h
#define buildid_reader_ffi_h

#include "nsString.h"

extern "C" {

nsresult read_toolkit_buildid_from_file(const nsAString* fname,
                                        const nsCString* nname,
                                        nsCString* rv_build_id);

}  // extern "C"

#endif  // buildid_reader_ffi_h
