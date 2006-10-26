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

// ExceptionHandler can write a minidump file when an exception occurs,
// or when WriteMinidump() is called explicitly by your program.
//
// To have the exception handler write minidumps when an uncaught exception
// (crash) occurs, you should create an instance early in the execution
// of your program, and keep it around for the entire time you want to
// have crash handling active (typically, until shutdown).
//
// If you want to write minidumps without installing the exception handler,
// you can create an ExceptionHandler with install_handler set to false,
// then call WriteMinidump.  You can also use this technique if you want to
// use different minidump callbacks for different call sites.
//
// In either case, a callback function is called when a minidump is written,
// which receives the unqiue id of the minidump.  The caller can use this
// id to collect and write additional application state, and to launch an
// external crash-reporting application.
//
// It is important that creation and destruction of ExceptionHandler objects
// be nested cleanly, when using install_handler = true.
// Avoid the following pattern:
//   ExceptionHandler *e = new ExceptionHandler(...);
//   ExceptionHandler *f = new ExceptionHandler(...);
//   delete e;
// This will put the exception filter stack into an inconsistent state.
//
// To use this library in your project, you will need to link against
// ole32.lib.

#ifndef CLIENT_WINDOWS_HANDLER_EXCEPTION_HANDLER_H__
#define CLIENT_WINDOWS_HANDLER_EXCEPTION_HANDLER_H__

#include <Windows.h>
#include <DbgHelp.h>

#pragma warning( push )
// disable exception handler warnings
#pragma warning( disable : 4530 ) 
#include <string>

namespace google_airbag {

using std::wstring;

class ExceptionHandler {
 public:
  // A callback function to run after the minidump has been written.
  // minidump_id is a unique id for the dump, so the minidump
  // file is <dump_path>\<minidump_id>.dmp.  succeeded indicates whether
  // a minidump file was successfully written.
  typedef void (*MinidumpCallback)(const wstring &minidump_id,
                                   void *context, bool succeeded);

  // Creates a new ExceptionHandler instance to handle writing minidumps.
  // Minidump files will be written to dump_path, and the optional callback
  // is called after writing the dump file, as described above.
  // If install_handler is true, then a minidump will be written whenever
  // an unhandled exception occurs.  If it is false, minidumps will only
  // be written when WriteMinidump is called.
  ExceptionHandler(const wstring &dump_path, MinidumpCallback callback,
                   void *callback_context, bool install_handler);
  ~ExceptionHandler();

  // Get and set the minidump path
  wstring dump_path() const { return dump_path_; }
  void set_dump_path(const wstring &dump_path) { dump_path_ = dump_path; }

  // Writes a minidump immediately.  This can be used to capture the
  // execution state independently of a crash.  Returns true on success.
  bool WriteMinidump();

  // Convenience form of WriteMinidump which does not require an
  // ExceptionHandler instance.
  static bool WriteMinidump(const wstring &dump_path,
                            MinidumpCallback callback, void *callback_context);

 private:
  // Function pointer type for MiniDumpWriteDump, which is looked up
  // dynamically.
  typedef BOOL (WINAPI *MiniDumpWriteDump_type)(
      HANDLE hProcess,
      DWORD dwPid,
      HANDLE hFile,
      MINIDUMP_TYPE DumpType,
      CONST PMINIDUMP_EXCEPTION_INFORMATION ExceptionParam,
      CONST PMINIDUMP_USER_STREAM_INFORMATION UserStreamParam,
      CONST PMINIDUMP_CALLBACK_INFORMATION CallbackParam);

  // This function does the actual writing of a minidump.
  bool WriteMinidumpWithException(EXCEPTION_POINTERS *exinfo);

  // Called when an unhandled exception occurs.
  static LONG WINAPI HandleException(EXCEPTION_POINTERS *exinfo);

  // Generates a new ID and stores it in next_minidump_id_.
  void UpdateNextID();

  MinidumpCallback callback_;
  void *callback_context_;

  wstring dump_path_;
  wstring next_minidump_id_;

  HMODULE dbghelp_module_;
  MiniDumpWriteDump_type minidump_write_dump_;

  ExceptionHandler *previous_handler_;  // current_handler_ before us
  LPTOP_LEVEL_EXCEPTION_FILTER previous_filter_;

  // the currently-installed ExceptionHandler, of which there can be only 1
  static ExceptionHandler *current_handler_;

  // disallow copy ctor and operator=
  explicit ExceptionHandler(const ExceptionHandler &);
  void operator=(const ExceptionHandler &);
};

}  // namespace google_airbag

#pragma warning( pop )
#endif  // CLIENT_WINDOWS_HANDLER_EXCEPTION_HANDLER_H__
