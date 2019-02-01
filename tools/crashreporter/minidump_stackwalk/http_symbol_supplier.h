// Copyright (c) 2011 The Mozilla Foundation
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
//     * Neither the name of The Mozilla Foundation nor the names of its
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

// A SymbolSupplier that can fetch symbols via HTTP from a symbol server
// serving them at Microsoft symbol server-compatible paths.

#include <map>
#include <set>
#include <string>

#include "processor/simple_symbol_supplier.h"

#ifdef _WIN32
#  include <windows.h>
#  include <wininet.h>
#else
typedef void CURL;
#endif

namespace google_breakpad {
class CodeModule;
struct SystemInfo;
}  // namespace google_breakpad

namespace breakpad_extra {

using google_breakpad::CodeModule;
using google_breakpad::SimpleSymbolSupplier;
using google_breakpad::SymbolSupplier;
using google_breakpad::SystemInfo;
using std::string;
using std::vector;

class HTTPSymbolSupplier : public SimpleSymbolSupplier {
 public:
  // Construct an HTTPSymbolSupplier.
  // |server_urls| contains URLs to query for symbols.
  // |cache_path| is a directory in which to store downloaded symbols.
  // |local_paths| are directories to query for symbols before checking URLs.
  HTTPSymbolSupplier(const vector<string>& server_urls,
                     const string& cache_path,
                     const vector<string>& local_paths, const string& tmp_path);
  virtual ~HTTPSymbolSupplier();

  // Returns the path to the symbol file for the given module.  See the
  // description above.
  virtual SymbolSupplier::SymbolResult GetSymbolFile(
      const CodeModule* module, const SystemInfo* system_info,
      string* symbol_file);

  virtual SymbolSupplier::SymbolResult GetSymbolFile(
      const CodeModule* module, const SystemInfo* system_info,
      string* symbol_file, string* symbol_data);

  // Allocates data buffer on heap and writes symbol data into buffer.
  // Symbol supplier ALWAYS takes ownership of the data buffer.
  virtual SymbolSupplier::SymbolResult GetCStringSymbolData(
      const CodeModule* module, const SystemInfo* system_info,
      string* symbol_file, char** symbol_data, size_t* size);

  struct SymbolStats {
    // true if the symbol file was already cached on disk,
    // false if a HTTP request was made to fetch it.
    bool was_cached_on_disk;
    // If was_cached_on_disk is false, the time in milliseconds
    // that the full HTTP request to fetch the symbol file took.
    float fetch_time_ms;
  };

  // Get stats on symbols for a module.
  // Returns true if stats were found, false if not.
  bool GetStats(const CodeModule* module, SymbolStats* stats) const;

 private:
  bool FetchSymbolFile(const CodeModule* module, const SystemInfo* system_info);

  bool SymbolWasError(const CodeModule* module, const SystemInfo* system_info);
  void StoreCacheHit(const CodeModule* Module);
  void StoreCacheMiss(const CodeModule* module, float fetch_time);
  void StoreSymbolStats(const CodeModule* module, const SymbolStats& stats);

  vector<string> server_urls_;
  string cache_path_;
  string tmp_path_;
  std::set<std::pair<string, string>> error_symbols_;
  std::map<std::pair<string, string>, SymbolStats> symbol_stats_;
#ifdef _WIN32
  HINTERNET session_;
#else
  CURL* session_;
#endif
};

}  // namespace breakpad_extra
