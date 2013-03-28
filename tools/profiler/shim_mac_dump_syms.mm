/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "common/mac/dump_syms.h"
#include "shim_mac_dump_syms.h"

bool ReadSymbolData(const string& obj_file,
                    const std::vector<string> &debug_dirs,
                    SymbolData symbol_data,
                    google_breakpad::Module** module)
{
  google_breakpad::DumpSymbols ds(symbol_data);

  NSString* obj_file_ns = [NSString stringWithUTF8String:obj_file.c_str()];
  // TODO: remember to [obj_file_ns release] this at the exit points

  if (!ds.Read(obj_file_ns))
    return false;

  return ds.ReadSymbolData(module);
}
