// Copyright (c) 2009, Google Inc.             -*- mode: c++ -*-
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// module.h: defines google_breakpad::Module, for writing breakpad symbol files

#ifndef COMMON_LINUX_MODULE_H__
#define COMMON_LINUX_MODULE_H__

#include <map>
#include <string>
#include <vector>
#include <cstdio>

#include "google_breakpad/common/breakpad_types.h"

namespace google_breakpad {

using std::string;
using std::vector;
using std::map;

// A Module represents the contents of a module, and supports methods
// for adding information produced by parsing STABS or DWARF data
// --- possibly both from the same file --- and then writing out the
// unified contents as a Breakpad-format symbol file.
class Module {
 public:
  // The type of addresses and sizes in a symbol table.
  typedef u_int64_t Address;
  struct File;
  struct Function;
  struct Line;

  // Addresses appearing in File, Function, and Line structures are
  // absolute, not relative to the the module's load address.  That
  // is, if the module were loaded at its nominal load address, the
  // addresses would be correct.

  // A source file.
  struct File {
    // The name of the source file.
    string name_;

    // The file's source id.  The Write member function clears this
    // field and assigns source ids a fresh, so any value placed here
    // before calling Write will be lost.
    int source_id_;
  };

  // A function.
  struct Function {
    // For sorting by address.  (Not style-guide compliant, but it's
    // stupid not to put this in the struct.)
    static bool CompareByAddress(const Function *x, const Function *y) {
      return x->address_ < y->address_;
    }

    // The function's name.
    string name_;
    
    // The start address and length of the function's code.
    Address address_, size_;

    // The function's parameter size.
    Address parameter_size_;

    // Source lines belonging to this function, sorted by increasing
    // address.
    vector<Line> lines_;
  };

  // A source line.
  struct Line {
    // For sorting by address.  (Not style-guide compliant, but it's
    // stupid not to put this in the struct.)
    static bool CompareByAddress(const Module::Line &x, const Module::Line &y) {
      return x.address_ < y.address_;
    }

    Address address_, size_;    // The address and size of the line's code.
    File *file_;                // The source file.
    int number_;                // The source line number.
  };
    
  // Create a new module with the given name, operating system,
  // architecture, and ID string.
  Module(const string &name, const string &os, const string &architecture, 
         const string &id);
  ~Module();

  // Set the module's load address to LOAD_ADDRESS; addresses given
  // for functions and lines will be written to the Breakpad symbol
  // file as offsets from this address.  Construction initializes this
  // module's load address to zero: addresses written to the symbol
  // file will be the same as they appear in the File and Line
  // structures.
  void SetLoadAddress(Address load_address);

  // Add FUNCTION to the module.
  // Destroying this module frees all Function objects that have been
  // added with this function.
  void AddFunction(Function *function);

  // Add all the functions in [BEGIN,END) to the module.
  // Destroying this module frees all Function objects that have been
  // added with this function.
  void AddFunctions(vector<Function *>::iterator begin,
                    vector<Function *>::iterator end);

  // If this module has a file named NAME, return a pointer to it.  If
  // it has none, then create one and return a pointer to the new
  // file.  Destroying this module frees all File objects that have
  // been created using this function, or with Insert.
  File *FindFile(const string &name);
  File *FindFile(const char *name);

  // Write this module to STREAM in the breakpad symbol format.
  // Return true if all goes well, or false if an error occurs.  This
  // method writes out:
  // - a header based on the values given to the constructor,
  // - the source files added via FindFile, and finally
  // - the functions added via AddFunctions, each with its lines.
  // Addresses in the output are all relative to the load address
  // established by SetLoadAddress.
  bool Write(FILE *stream);

private:

  // Find those files in this module that are actually referred to by
  // functions' line number data, and assign them source id numbers.
  // Set the source id numbers for all other files --- unused by the
  // source line data --- to -1.  We do this before writing out the
  // symbol file, at which point we omit any unused files.
  void AssignSourceIds();

  // Report an error that has occurred writing the symbol file, using
  // errno to find the appropriate cause.  Return false.
  static bool ReportError();

  // Module header entries.
  string name_, os_, architecture_, id_;

  // The module's nominal load address.  Addresses for functions and
  // lines are absolute, assuming the module is loaded at this
  // address.
  Address load_address_;

  // Relation for maps whose keys are strings shared with some other
  // structure.
  struct CompareStringPtrs {
    bool operator()(const string *x, const string *y) { return *x < *y; };
  };

  // A map from filenames to File structures.  The map's keys are
  // pointers to the Files' names.
  typedef map<const string *, File *, CompareStringPtrs> FileByNameMap;

  // The module owns all the files and functions that have been added
  // to it; destroying the module frees the Files and Functions these
  // point to.
  FileByNameMap files_;                 // This module's source files.  
  vector<Function *> functions_;        // This module's functions.
};

} // namespace google_breakpad

#endif  // COMMON_LINUX_MODULE_H__
