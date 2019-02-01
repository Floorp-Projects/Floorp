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

#include "http_symbol_supplier.h"

#include <algorithm>

#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

#include <errno.h>

#include "google_breakpad/processor/code_module.h"
#include "google_breakpad/processor/system_info.h"
#include "processor/logging.h"
#include "processor/pathname_stripper.h"

#ifdef _WIN32
#  include <direct.h>
#  include "zlib.h"
#else
#  include <curl/curl.h>
#endif

namespace breakpad_extra {

using google_breakpad::CodeModule;
using google_breakpad::PathnameStripper;
using google_breakpad::SystemInfo;

static bool file_exists(const string& file_name) {
  struct stat sb;
  return stat(file_name.c_str(), &sb) == 0;
}

static string dirname(const string& path) {
  size_t i = path.rfind('/');
  if (i == string::npos) {
    return path;
  }
  return path.substr(0, i);
}

#ifdef _WIN32
#  define mkdir_port(d) _mkdir(d)
#else
#  define mkdir_port(d) mkdir(d, 0755)
#endif

static bool mkdirs(const string& file) {
  vector<string> dirs;
  string dir = dirname(file);
  while (!file_exists(dir)) {
    dirs.push_back(dir);
    string new_dir = dirname(dir);
    if (new_dir == dir || dir.empty()) {
      break;
    }
    dir = new_dir;
  }
  for (auto d = dirs.rbegin(); d != dirs.rend(); ++d) {
    if (mkdir_port(d->c_str()) != 0) {
      BPLOG(ERROR) << "Error creating " << *d << ": " << errno;
      return false;
    }
  }
  return true;
}

static vector<string> vector_from(const string& front,
                                  const vector<string>& rest) {
  vector<string> vec(1, front);
  std::copy(rest.begin(), rest.end(), std::back_inserter(vec));
  return vec;
}

HTTPSymbolSupplier::HTTPSymbolSupplier(const vector<string>& server_urls,
                                       const string& cache_path,
                                       const vector<string>& local_paths,
                                       const string& tmp_path)
    : SimpleSymbolSupplier(vector_from(cache_path, local_paths)),
      server_urls_(server_urls),
      cache_path_(cache_path),
      tmp_path_(tmp_path) {
#ifdef _WIN32
  session_ = InternetOpenW(L"Breakpad/1.0", INTERNET_OPEN_TYPE_PRECONFIG,
                           nullptr, nullptr, 0);
  if (!session_) {
    BPLOG(INFO) << "HTTPSymbolSupplier: InternetOpenW: Error: "
                << GetLastError();
  }
#else
  session_ = curl_easy_init();
#endif
  for (auto i = server_urls_.begin(); i < server_urls_.end(); ++i) {
    if (*(i->end() - 1) != '/') {
      i->push_back('/');
    }
  }
  // Remove any trailing slash on tmp_path.
  if (!tmp_path_.empty() && *(tmp_path_.end() - 1) == '/') {
    tmp_path_.erase(tmp_path_.end() - 1);
  }
}

HTTPSymbolSupplier::~HTTPSymbolSupplier() {
#ifdef _WIN32
  InternetCloseHandle(session_);
#else
  curl_easy_cleanup(session_);
#endif
}

void HTTPSymbolSupplier::StoreSymbolStats(const CodeModule* module,
                                          const SymbolStats& stats) {
  const auto& key =
      std::make_pair(module->debug_file(), module->debug_identifier());
  if (symbol_stats_.find(key) == symbol_stats_.end()) {
    symbol_stats_[key] = stats;
  }
}

void HTTPSymbolSupplier::StoreCacheHit(const CodeModule* module) {
  SymbolStats stats = {true, 0.0f};
  StoreSymbolStats(module, stats);
}

void HTTPSymbolSupplier::StoreCacheMiss(const CodeModule* module,
                                        float fetch_time) {
  SymbolStats stats = {false, fetch_time};
  StoreSymbolStats(module, stats);
}

SymbolSupplier::SymbolResult HTTPSymbolSupplier::GetSymbolFile(
    const CodeModule* module, const SystemInfo* system_info,
    string* symbol_file) {
  SymbolSupplier::SymbolResult res =
      SimpleSymbolSupplier::GetSymbolFile(module, system_info, symbol_file);
  if (res != SymbolSupplier::NOT_FOUND) {
    StoreCacheHit(module);
    return res;
  }

  if (!FetchSymbolFile(module, system_info)) {
    return SymbolSupplier::NOT_FOUND;
  }

  return SimpleSymbolSupplier::GetSymbolFile(module, system_info, symbol_file);
}

SymbolSupplier::SymbolResult HTTPSymbolSupplier::GetSymbolFile(
    const CodeModule* module, const SystemInfo* system_info,
    string* symbol_file, string* symbol_data) {
  SymbolSupplier::SymbolResult res = SimpleSymbolSupplier::GetSymbolFile(
      module, system_info, symbol_file, symbol_data);
  if (res != SymbolSupplier::NOT_FOUND) {
    StoreCacheHit(module);
    return res;
  }

  if (!FetchSymbolFile(module, system_info)) {
    return SymbolSupplier::NOT_FOUND;
  }

  return SimpleSymbolSupplier::GetSymbolFile(module, system_info, symbol_file,
                                             symbol_data);
}

SymbolSupplier::SymbolResult HTTPSymbolSupplier::GetCStringSymbolData(
    const CodeModule* module, const SystemInfo* system_info,
    string* symbol_file, char** symbol_data, size_t* size) {
  SymbolSupplier::SymbolResult res = SimpleSymbolSupplier::GetCStringSymbolData(
      module, system_info, symbol_file, symbol_data, size);
  if (res != SymbolSupplier::NOT_FOUND) {
    StoreCacheHit(module);
    return res;
  }

  if (!FetchSymbolFile(module, system_info)) {
    return SymbolSupplier::NOT_FOUND;
  }

  return SimpleSymbolSupplier::GetCStringSymbolData(
      module, system_info, symbol_file, symbol_data, size);
}

namespace {
string JoinPath(const string& path, const string& sub) {
  if (path[path.length() - 1] == '/') {
    return path + sub;
  }
  return path + "/" + sub;
}

#ifdef _WIN32
string URLEncode(HINTERNET session, const string& url) {
  string out(url.length() * 3, '\0');
  DWORD length = out.length();
  ;
  if (InternetCanonicalizeUrlA(url.c_str(), &out[0], &length, 0)) {
    out.resize(length);
    return out;
  }
  return url;
}

string JoinURL(HINTERNET session, const string& url, const string& sub) {
  return url + "/" + URLEncode(session, sub);
}

bool FetchURLToFile(HINTERNET session, const string& url, const string& file,
                    const string& tmp_path, float* fetch_time) {
  *fetch_time = 0.0f;

  URL_COMPONENTSA comps = {};
  comps.dwStructSize = sizeof(URL_COMPONENTSA);
  comps.dwHostNameLength = static_cast<DWORD>(-1);
  comps.dwSchemeLength = static_cast<DWORD>(-1);
  comps.dwUrlPathLength = static_cast<DWORD>(-1);
  comps.dwExtraInfoLength = static_cast<DWORD>(-1);

  if (!InternetCrackUrlA(url.c_str(), 0, 0, &comps)) {
    BPLOG(INFO) << "HTTPSymbolSupplier: InternetCrackUrlA: Error: "
                << GetLastError();
    return false;
  }

  DWORD start = GetTickCount();
  string host(comps.lpszHostName, comps.dwHostNameLength);
  string path(comps.lpszUrlPath, comps.dwUrlPathLength);
  HINTERNET conn = InternetConnectA(session, host.c_str(), comps.nPort, nullptr,
                                    nullptr, INTERNET_SERVICE_HTTP, 0, 0);
  if (!conn) {
    BPLOG(INFO) << "HTTPSymbolSupplier: HttpOpenRequest: Error: "
                << GetLastError();
    return false;
  }

  HINTERNET req = HttpOpenRequestA(conn, "GET", path.c_str(), nullptr, nullptr,
                                   nullptr, INTERNET_FLAG_NO_COOKIES, 0);
  if (!req) {
    BPLOG(INFO) << "HTTPSymbolSupplier: HttpSendRequest: Error: "
                << GetLastError();
    InternetCloseHandle(conn);
    return false;
  }

  DWORD status = 0;
  DWORD size = sizeof(status);
  if (!HttpSendRequest(req, nullptr, 0, nullptr, 0)) {
    BPLOG(INFO) << "HTTPSymbolSupplier: HttpSendRequest: Error: "
                << GetLastError();
    InternetCloseHandle(req);
    InternetCloseHandle(conn);
    return false;
  }

  if (!HttpQueryInfo(req, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER,
                     &status, &size, nullptr)) {
    BPLOG(INFO) << "HTTPSymbolSupplier: HttpQueryInfo: Error: "
                << GetLastError();
    InternetCloseHandle(req);
    InternetCloseHandle(conn);
    return false;
  }

  bool do_ungzip = false;
  // See if the content is gzipped and we need to decompress it.
  char encoding[32];
  DWORD encoding_size = sizeof(encoding);
  if (HttpQueryInfo(req, HTTP_QUERY_CONTENT_ENCODING, encoding, &encoding_size,
                    nullptr) &&
      strcmp(encoding, "gzip") == 0) {
    do_ungzip = true;
    BPLOG(INFO) << "HTTPSymbolSupplier: need to manually un-gzip";
  }

  bool success = false;
  if (status == 200) {
    DWORD bytes = 0;
    string tempfile(MAX_PATH, '\0');
    if (GetTempFileNameA(tmp_path.c_str(), "sym", 1, &tempfile[0]) != 0) {
      tempfile.resize(strlen(tempfile.c_str()));
      BPLOG(INFO) << "HTTPSymbolSupplier: symbol exists, saving to "
                  << tempfile;
      FILE* f = fopen(tempfile.c_str(), "wb");
      while (InternetQueryDataAvailable(req, &bytes, 0, 0) && bytes > 0) {
        vector<uint8_t> data(bytes);
        DWORD downloaded = 0;
        if (InternetReadFile(req, &data[0], bytes, &downloaded)) {
          fwrite(&data[0], downloaded, 1, f);
        }
      }
      fclose(f);
      if (do_ungzip) {
        string gzfile = tempfile + ".gz";
        MoveFileA(tempfile.c_str(), gzfile.c_str());
        uint8_t buffer[4096];
        gzFile g = gzopen(gzfile.c_str(), "r");
        FILE* f = fopen(tempfile.c_str(), "w");
        if (g && f) {
          while (true) {
            int bytes_read = gzread(g, buffer, sizeof(buffer));
            if (bytes_read > 0) {
              fwrite(buffer, bytes_read, 1, f);
            } else {
              if (bytes_read == 0) {
                success = true;
              }
              break;
            }
          }
        }
        if (g) {
          gzclose(g);
        }
        if (f) {
          fclose(f);
        }
        if (!success) {
          BPLOG(INFO) << "HTTPSymbolSupplier: failed to decompress " << file;
        }
      } else {
        success = true;
      }

      *fetch_time = GetTickCount() - start;

      if (success) {
        success = mkdirs(file);
        if (!success) {
          BPLOG(INFO) << "HTTPSymbolSupplier: failed to create directories "
                      << "for " << file;
        } else {
          success = MoveFileA(tempfile.c_str(), file.c_str());
          if (!success) {
            BPLOG(INFO) << "HTTPSymbolSupplier: failed to rename file";
            unlink(tempfile.c_str());
          }
        }
      }
    }
  } else {
    BPLOG(INFO) << "HTTPSymbolSupplier: HTTP response code: " << status;
  }

  InternetCloseHandle(req);
  InternetCloseHandle(conn);
  return success;
}

#else  // !_WIN32
string URLEncode(CURL* curl, const string& url) {
  char* escaped_url_raw =
      curl_easy_escape(curl, const_cast<char*>(url.c_str()), url.length());
  if (not escaped_url_raw) {
    BPLOG(INFO) << "HTTPSymbolSupplier: couldn't escape URL: " << url;
    return "";
  }
  string escaped_url(escaped_url_raw);
  curl_free(escaped_url_raw);
  return escaped_url;
}

string JoinURL(CURL* curl, const string& url, const string& sub) {
  return url + "/" + URLEncode(curl, sub);
}

bool FetchURLToFile(CURL* curl, const string& url, const string& file,
                    const string& tmp_path, float* fetch_time) {
  *fetch_time = 0.0f;

  string tempfile = JoinPath(tmp_path, "symbolXXXXXX");
  int fd = mkstemp(&tempfile[0]);
  if (fd == -1) {
    return false;
  }
  FILE* f = fdopen(fd, "w");

  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_ENCODING, "");
  curl_easy_setopt(curl, CURLOPT_STDERR, stderr);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, f);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);

  struct timeval t1, t2;
  gettimeofday(&t1, nullptr);
  bool result = false;
  long retcode = -1;
  if (curl_easy_perform(curl) != 0) {
    BPLOG(INFO) << "HTTPSymbolSupplier: curl_easy_perform failed";
  } else if (curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &retcode) != 0) {
    BPLOG(INFO) << "HTTPSymbolSupplier: curl_easy_getinfo failed";
  } else if (retcode != 200) {
    BPLOG(INFO) << "HTTPSymbolSupplier: HTTP response code: " << retcode;
  } else {
    BPLOG(INFO) << "HTTPSymbolSupplier: symbol exists, saving to " << file;
    result = true;
  }
  gettimeofday(&t2, nullptr);
  *fetch_time =
      (t2.tv_sec - t1.tv_sec) * 1000.0 + (t2.tv_usec - t1.tv_usec) / 1000.0;
  fclose(f);
  close(fd);

  if (result) {
    result = mkdirs(file);
    if (!result) {
      BPLOG(INFO) << "HTTPSymbolSupplier: failed to create directories for "
                  << file;
    }
  }
  if (result) {
    result = 0 == rename(tempfile.c_str(), file.c_str());
    if (!result) {
      int e = errno;
      BPLOG(INFO) << "HTTPSymbolSupplier: failed to rename file, errno=" << e;
    }
  }

  if (!result) {
    unlink(tempfile.c_str());
  }

  return result;
}
#endif
}  // namespace

bool HTTPSymbolSupplier::FetchSymbolFile(const CodeModule* module,
                                         const SystemInfo* system_info) {
  if (!session_) {
    return false;
  }
  // Copied from simple_symbol_supplier.cc
  string debug_file_name = PathnameStripper::File(module->debug_file());
  if (debug_file_name.empty()) {
    return false;
  }
  string path = debug_file_name;
  string url = URLEncode(session_, debug_file_name);

  // Append the identifier as a directory name.
  string identifier = module->debug_identifier();
  if (identifier.empty()) {
    return false;
  }
  path = JoinPath(path, identifier);
  url = JoinURL(session_, url, identifier);

  // See if we already attempted to fetch this symbol file.
  if (SymbolWasError(module, system_info)) {
    return false;
  }

  // Transform the debug file name into one ending in .sym.  If the existing
  // name ends in .pdb, strip the .pdb.  Otherwise, add .sym to the non-.pdb
  // name.
  string debug_file_extension;
  if (debug_file_name.size() > 4) {
    debug_file_extension = debug_file_name.substr(debug_file_name.size() - 4);
  }
  std::transform(debug_file_extension.begin(), debug_file_extension.end(),
                 debug_file_extension.begin(), tolower);
  if (debug_file_extension == ".pdb") {
    debug_file_name = debug_file_name.substr(0, debug_file_name.size() - 4);
  }

  debug_file_name += ".sym";
  path = JoinPath(path, debug_file_name);
  url = JoinURL(session_, url, debug_file_name);

  string full_path = JoinPath(cache_path_, path);

  bool result = false;
  for (auto server_url = server_urls_.begin(); server_url < server_urls_.end();
       ++server_url) {
    string full_url = *server_url + url;
    float fetch_time;
    BPLOG(INFO) << "HTTPSymbolSupplier: querying " << full_url;
    if (FetchURLToFile(session_, full_url, full_path, tmp_path_, &fetch_time)) {
      StoreCacheMiss(module, fetch_time);
      result = true;
      break;
    }
  }
  if (!result) {
    error_symbols_.insert(
        std::make_pair(module->debug_file(), module->debug_identifier()));
  }
  return result;
}

bool HTTPSymbolSupplier::GetStats(const CodeModule* module,
                                  SymbolStats* stats) const {
  const auto& found = symbol_stats_.find(
      std::make_pair(module->debug_file(), module->debug_identifier()));
  if (found == symbol_stats_.end()) {
    return false;
  }

  *stats = found->second;
  return true;
}

bool HTTPSymbolSupplier::SymbolWasError(const CodeModule* module,
                                        const SystemInfo* system_info) {
  return error_symbols_.find(std::make_pair(module->debug_file(),
                                            module->debug_identifier())) !=
         error_symbols_.end();
}

}  // namespace breakpad_extra
