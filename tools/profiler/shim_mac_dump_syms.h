/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Read debug info from |obj_file| and park it in a Module, returned
// via |module|.  Caller owns the Module and is responsible for
// deallocating it.  Note that |debug_dirs| is ignored.
bool ReadSymbolData(const string& obj_file,
                    const std::vector<string> &debug_dirs,
                    SymbolData symbol_data,
                    google_breakpad::Module** module);
