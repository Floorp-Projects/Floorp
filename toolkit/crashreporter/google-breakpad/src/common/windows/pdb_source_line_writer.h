// Copyright (c) 2006, Google Inc.
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

// PDBSourceLineWriter uses a pdb file produced by Visual C++ to output
// a line/address map for use with SourceLineResolver.

#ifndef _PDB_SOURCE_LINE_WRITER_H__
#define _PDB_SOURCE_LINE_WRITER_H__

#include <atlcomcli.h>

#include <string>

struct IDiaEnumLineNumbers;
struct IDiaSession;
struct IDiaSymbol;

namespace google_airbag {

using std::wstring;

class PDBSourceLineWriter {
 public:
  enum FileFormat {
    PDB_FILE,  // a .pdb file containing debug symbols
    EXE_FILE,  // a .exe or .dll file
  };

  explicit PDBSourceLineWriter();
  ~PDBSourceLineWriter();

  // Opens the given file.  For executable files, the corresponding pdb
  // file must be available; Open will be if it is not.
  // If there is already a pdb file open, it is automatically closed.
  // Returns true on success.
  bool Open(const wstring &file, FileFormat format);

  // Locates the pdb file for the given executable (exe or dll) file,
  // and opens it.  If there is already a pdb file open, it is automatically
  // closed.  Returns true on success.
  bool OpenExecutable(const wstring &exe_file);

  // Writes a map file from the current pdb file to the given file stream.
  // Returns true on success.
  bool WriteMap(FILE *map_file);

  // Closes the current pdb file and its associated resources.
  void Close();

  // Returns the GUID for the module, as a string,
  // e.g. "11111111-2222-3333-4444-555555555555".
  wstring GetModuleGUID();

 private:
  // Outputs the line/address pairs for each line in the enumerator.
  // Returns true on success.
  bool PrintLines(IDiaEnumLineNumbers *lines);

  // Outputs a function address and name, followed by its source line list.
  // Returns true on success.
  bool PrintFunction(IDiaSymbol *function);

  // Outputs all functions as described above.  Returns true on success.
  bool PrintFunctions();

  // Outputs all of the source files in the session's pdb file.
  // Returns true on success.
  bool PrintSourceFiles();

  // Outputs all of the frame information necessary to construct stack
  // backtraces in the absence of frame pointers.  Returns true on success.
  bool PrintFrameData();

  // Outputs a single public symbol address and name, if the symbol corresponds
  // to a code address.  Returns true on success.  If symbol is does not
  // correspond to code, returns true without outputting anything.
  bool PrintCodePublicSymbol(IDiaSymbol *symbol);

  // Returns the function name for a symbol.  If possible, the name is
  // undecorated.  If the symbol's decorated form indicates the size of
  // parameters on the stack, this information is returned in stack_param_size.
  // Returns true on success.  If the symbol doesn't encode parameter size
  // information, stack_param_size is set to -1.
  static bool GetSymbolFunctionName(IDiaSymbol *function, BSTR *name,
                                    int *stack_param_size);

  // Returns the number of bytes of stack space used for a function's
  // parameters.  function must have the tag SymTagFunction.  In the event of
  // a failure, returns 0, which is also a valid number of bytes.
  static int GetFunctionStackParamSize(IDiaSymbol *function);

  // The session for the currently-open pdb file.
  CComPtr<IDiaSession> session_;

  // The current output file for this WriteMap invocation.
  FILE *output_;

  // Disallow copy ctor and operator=
  PDBSourceLineWriter(const PDBSourceLineWriter&);
  void operator=(const PDBSourceLineWriter&);
};

}  // namespace google_airbag

#endif  // _PDB_SOURCE_LINE_WRITER_H__
