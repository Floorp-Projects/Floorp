// Copyright 2022 The Chromium Authors.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>
#include <winternl.h>

#include <cstring>
#include <memory>
#include <utility>
#include <vector>

#include "common/utils_win.h"

#include "client_win.h"

namespace content_analysis {
namespace sdk {

const DWORD kBufferSize = 4096;

// Use the same default timeout value (50ms) as CreateNamedPipeA(), expressed
// in 100ns intervals.
constexpr LONGLONG kDefaultTimeout = 500000;

// The following #defines and struct are copied from the official Microsoft
// Windows Driver Kit headers because they are not available in the official
// Microsoft Windows user mode SDK headers.

#define FSCTL_PIPE_WAIT 0x110018
#define STATUS_SUCCESS ((NTSTATUS)0)
#define STATUS_PIPE_NOT_AVAILABLE ((NTSTATUS)0xc00000ac)
#define STATUS_IO_TIMEOUT ((NTSTATUS)0xc00000b5)

typedef struct _FILE_PIPE_WAIT_FOR_BUFFER {
  LARGE_INTEGER Timeout;
  ULONG NameLength;
  BOOLEAN TimeoutSpecified;
  WCHAR Name[1];
} FILE_PIPE_WAIT_FOR_BUFFER, *PFILE_PIPE_WAIT_FOR_BUFFER;

namespace {

using NtCreateFileFn = decltype(&::NtCreateFile);

NtCreateFileFn GetNtCreateFileFn() {
  static NtCreateFileFn fnNtCreateFile = []() {
    NtCreateFileFn fn = nullptr;
    HMODULE h = LoadLibraryA("NtDll.dll");
    if (h != nullptr) {
      fn = reinterpret_cast<NtCreateFileFn>(GetProcAddress(h, "NtCreateFile"));
      FreeLibrary(h);
    }
    return fn;
  }();

  return fnNtCreateFile;
}


using NtFsControlFileFn = NTSTATUS (NTAPI *)(
    HANDLE FileHandle,
    HANDLE Event,
    PIO_APC_ROUTINE ApcRoutine,
    PVOID ApcContext,
    PIO_STATUS_BLOCK IoStatusBlock,
    ULONG IoControlCode,
    PVOID InputBuffer,
    ULONG InputBufferLength,
    PVOID OutputBuffer,
    ULONG OutputBufferLength);

NtFsControlFileFn GetNtFsControlFileFn() {
  static NtFsControlFileFn fnNtFsControlFile = []() {
    NtFsControlFileFn fn = nullptr;
    HMODULE h = LoadLibraryA("NtDll.dll");
    if (h != nullptr) {
      fn = reinterpret_cast<NtFsControlFileFn>(GetProcAddress(h, "NtFsControlFile"));
      FreeLibrary(h);
    }
    return fn;
  }();

  return fnNtFsControlFile;
}

NTSTATUS WaitForPipeAvailability(const UNICODE_STRING& path) {
  NtCreateFileFn fnNtCreateFile = GetNtCreateFileFn();
  if (fnNtCreateFile == nullptr) {
    return false;
  }
  NtFsControlFileFn fnNtFsControlFile = GetNtFsControlFileFn();
  if (fnNtFsControlFile == nullptr) {
    return false;
  }

  // Build the device name.  This is the initial part of `path` which is
  // assumed to start with the string `kPipePrefixForClient`.  The `Length`
  // field is measured in bytes, not characters, and does not include the null
  // terminator.  It's important that the device name ends with a trailing
  // backslash.
  size_t device_name_char_length = std::strlen(internal::kPipePrefixForClient);
  UNICODE_STRING device_name;
  device_name.Buffer = path.Buffer;
  device_name.Length = device_name_char_length * sizeof(wchar_t);
  device_name.MaximumLength = device_name.Length;

  // Build the pipe name.  This is the remaining part of `path` after the device
  // name.
  UNICODE_STRING pipe_name;
  pipe_name.Buffer = path.Buffer + device_name_char_length;
  pipe_name.Length = path.Length - device_name.Length;
  pipe_name.MaximumLength = pipe_name.Length;

  // Build the ioctl input buffer.  This buffer is the size of
  // FILE_PIPE_WAIT_FOR_BUFFER plus the length of the pipe name.  Since
  // FILE_PIPE_WAIT_FOR_BUFFER includes one WCHAR this includes space for
  // the terminating null character of the name which wcsncpy() copies.
  size_t buffer_size = sizeof(FILE_PIPE_WAIT_FOR_BUFFER) + pipe_name.Length;
  std::vector<char> buffer(buffer_size);
  FILE_PIPE_WAIT_FOR_BUFFER* wait_buffer =
      reinterpret_cast<FILE_PIPE_WAIT_FOR_BUFFER*>(buffer.data());
  wait_buffer->Timeout.QuadPart = kDefaultTimeout;
  wait_buffer->NameLength = pipe_name.Length;
  wait_buffer->TimeoutSpecified = TRUE;
  std::wcsncpy(wait_buffer->Name, pipe_name.Buffer, wait_buffer->NameLength /
      sizeof(wchar_t));

  OBJECT_ATTRIBUTES attr;
  InitializeObjectAttributes(&attr, &device_name, OBJ_CASE_INSENSITIVE, nullptr,
                             nullptr);

  IO_STATUS_BLOCK io;
  HANDLE h = INVALID_HANDLE_VALUE;
  NTSTATUS sts = fnNtCreateFile(&h, GENERIC_READ | GENERIC_WRITE | SYNCHRONIZE,
      &attr, &io, /*AllocationSize=*/nullptr, FILE_ATTRIBUTE_NORMAL,
      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, FILE_OPEN,
      FILE_SYNCHRONOUS_IO_NONALERT, /*EaBuffer=*/nullptr, /*EaLength=*/0);
  if (sts != STATUS_SUCCESS) {
    return false;
  }

  IO_STATUS_BLOCK io2;
  sts = fnNtFsControlFile(h, /*Event=*/nullptr, /*ApcRoutine=*/nullptr,
      /*ApcContext*/nullptr, &io2, FSCTL_PIPE_WAIT, buffer.data(),
      buffer.size(), nullptr, 0);
  CloseHandle(h);
  return sts;
}

// Reads the next message from the pipe and returns a buffer of chars.
// This function is synchronous.
std::vector<char> ReadNextMessageFromPipe(
    HANDLE pipe,
    OVERLAPPED* overlapped) {
  DWORD err = ERROR_SUCCESS;
  std::vector<char> buffer(kBufferSize);
  char* p = buffer.data();
  int final_size = 0;
  while (true) {
    DWORD read;

    // Even though the pipe is opened for overlapped IO, the read operation
    // could still completely synchronously.  For example, a server's response
    // message could already be available in the pipe's internal buffer.
    // If ReadFile() does complete synchronously, TRUE is returned.  In this
    // case update the final size and exit the loop.
    if (ReadFile(pipe, p, kBufferSize, &read, overlapped)) {
      final_size += read;
      break;
    } else {
      // Reaching here means that ReadFile() will either complete async or
      // an error has occurred.  The former case is detected if the error code
      // is "IO pending", in which case GetOverlappedResult() is called to wait
      // for the IO to complete.  If that function returns TRUE then the read
      // operation completed successfully and the code simply updates the final
      // size and exits the loop.
      err = GetLastError();
      if (err == ERROR_IO_PENDING) {
        if (GetOverlappedResult(pipe, overlapped, &read, /*wait=*/TRUE)) {
          final_size += read;
          break;
        } else {
          err = GetLastError();
        }
      }

      // Reaching here means an error has occurred.  One error is recoverable:
      // "more data".  For any other type of error break out of the loop.
      if (err != ERROR_MORE_DATA) {
        final_size = 0;
        break;
      }

      // Reaching here means the error is "more data", that is, the buffer
      // specified in ReadFile() was too small to contain the entire response
      // message from the server. ReadFile() has placed the start of the
      // message in the specified buffer but ReadFile() needs to be called
      // again to read the remaining part.
      //
      // The buffer size is increased and the current pointer into the buffer
      // `p` is adjusted so that when the loop re-runs, it calls ReadFile()
      // with the correct point in the buffer.  It's possible that this loop
      // might have to run many times if the response message is rather large.
      buffer.resize(buffer.size() + kBufferSize);
      p = buffer.data() + buffer.size() - kBufferSize;
    }
  }

  buffer.resize(final_size);
  return buffer;
}

// Writes a string to the pipe. Returns true if successful, false otherwise.
// This function is synchronous.
bool WriteMessageToPipe(
    HANDLE pipe,
    const std::string& message,
    OVERLAPPED* overlapped) {
  if (message.empty())
    return false;

  // Even though the pipe is opened for overlapped IO, the write operation
  // could still completely synchronously.  If it does, TRUE is returned.
  // In this case the function is done.
  bool ok = WriteFile(pipe, message.data(), message.size(), nullptr, overlapped);
  if (!ok) {
    // Reaching here means that WriteFile() will either complete async or
    // an error has occurred.  The former case is detected if the error code
    // is "IO pending", in which case GetOverlappedResult() is called to wait
    // for the IO to complete.  Whether the operation completes sync or async,
    // return true if the operation succeeded and false otherwise.
    DWORD err = GetLastError();
    if (err == ERROR_IO_PENDING) {
      DWORD written;
      ok = GetOverlappedResult(pipe, overlapped, &written, /*wait=*/TRUE);
    }
  }

  return ok;
}

}  // namespace

// static
std::unique_ptr<Client> Client::Create(Config config) {
  int rc;
  auto client = std::make_unique<ClientWin>(std::move(config), &rc);
  return rc == 0 ? std::move(client) : nullptr;
}

ClientWin::ClientWin(Config config, int* rc) : ClientBase(std::move(config)) {
  *rc = -1;

  std::string pipename =
    internal::GetPipeNameForClient(configuration().name,
                                   configuration().user_specific);
  if (!pipename.empty()) {
    unsigned long pid = 0;
    if (ConnectToPipe(pipename, &hPipe_) == ERROR_SUCCESS &&
        GetNamedPipeServerProcessId(hPipe_, &pid)) {
      agent_info().pid = pid;

      // Getting the process path is best effort.
      *rc = 0;
      std::string binary_path;
      if (internal::GetProcessPath(pid, &binary_path)) {
        agent_info().binary_path = std::move(binary_path);
      }
    }
  }

  if (*rc != 0) {
    Shutdown();
  }
}

ClientWin::~ClientWin() {
  Shutdown();
}

int ClientWin::Send(ContentAnalysisRequest request,
                    ContentAnalysisResponse* response) {
  ChromeToAgent chrome_to_agent;
  *chrome_to_agent.mutable_request() = std::move(request);

  internal::ScopedOverlapped overlapped;
  if (!overlapped.is_valid()) {
    return -1;
  }

  bool success = WriteMessageToPipe(hPipe_,
                                    chrome_to_agent.SerializeAsString(),
                                    overlapped);
  if (success) {
    std::vector<char> buffer = ReadNextMessageFromPipe(hPipe_, overlapped);
    AgentToChrome agent_to_chrome;
    success = buffer.size() > 0 &&
        agent_to_chrome.ParseFromArray(buffer.data(), buffer.size());
    if (success) {
      *response = std::move(*agent_to_chrome.mutable_response());
    }
  }

  return success ? 0 : -1;
}

int ClientWin::Acknowledge(const ContentAnalysisAcknowledgement& ack) {
  // TODO: could avoid a copy by changing argument to be
  // `ContentAnalysisAcknowledgement ack` and then using std::move() below and
  // at call site.
  ChromeToAgent chrome_to_agent;
  *chrome_to_agent.mutable_ack() = ack;

  internal::ScopedOverlapped overlapped;
  if (!overlapped.is_valid()) {
    return -1;
  }

  return WriteMessageToPipe(hPipe_, chrome_to_agent.SerializeAsString(), overlapped)
      ? 0 : -1;
}

int ClientWin::CancelRequests(const ContentAnalysisCancelRequests& cancel) {
  // TODO: could avoid a copy by changing argument to be
  // `ContentAnalysisCancelRequests cancel` and then using std::move() below and
  // at call site.
  ChromeToAgent chrome_to_agent;
  *chrome_to_agent.mutable_cancel() = cancel;

  internal::ScopedOverlapped overlapped;
  if (!overlapped.is_valid()) {
    return -1;
  }

  return WriteMessageToPipe(hPipe_, chrome_to_agent.SerializeAsString(), overlapped)
      ? 0 : -1;
}

// static
DWORD ClientWin::ConnectToPipe(const std::string& pipename, HANDLE* handle) {
  // Get pointers to the Ntxxx functions.  This is required to use absolute
  // pipe names from the Windows NT Object Manager's namespace. This protects
  // against the "\\.\pipe" symlink being redirected.

  NtCreateFileFn fnNtCreateFile = GetNtCreateFileFn();
  if (fnNtCreateFile == nullptr) {
    return ERROR_INVALID_FUNCTION;
  }

  // Convert the path to a wchar_t string.  Pass pipename.size() as the
  // `cbMultiByte` argument instead of -1 since the terminating null should not
  // be counted.  NtCreateFile() does not expect the object name to be
  // terminated.  Note that `buffer` and hence `name` created from it are both
  // unterminated strings.
  int wlen = MultiByteToWideChar(CP_ACP, 0, pipename.c_str(), pipename.size(),
                                 nullptr, 0);
  if (wlen == 0) {
    return GetLastError();
  }
  std::vector<wchar_t> buffer(wlen);
  MultiByteToWideChar(CP_ACP, 0, pipename.c_str(), pipename.size(),
                      buffer.data(), wlen);

  UNICODE_STRING name;
  name.Buffer = buffer.data();
  name.Length = wlen * sizeof(wchar_t);  // Length in bytes, not characters.
  name.MaximumLength = name.Length;

  OBJECT_ATTRIBUTES attr;
  InitializeObjectAttributes(&attr, &name, OBJ_CASE_INSENSITIVE, nullptr,
                             nullptr);

  // Open the named pipe for overlapped IO, i.e. do not specify either of the
  // FILE_SYNCHRONOUS_IO_xxxALERT in the creation option flags.  If the pipe
  // is not opened for overlapped IO, then the Send() method will block if
  // called from different threads since only one read or write operation would
  // be allowed at a time.
  IO_STATUS_BLOCK io;
  HANDLE h = INVALID_HANDLE_VALUE;
  NTSTATUS sts = STATUS_IO_TIMEOUT;
  while (sts == STATUS_IO_TIMEOUT) {
    sts = fnNtCreateFile(&h, GENERIC_READ | GENERIC_WRITE |
        SYNCHRONIZE, &attr, &io, /*AllocationSize=*/nullptr,
        FILE_ATTRIBUTE_NORMAL, /*ShareAccess=*/0, FILE_OPEN,
        FILE_NON_DIRECTORY_FILE,
        /*EaBuffer=*/nullptr, /*EaLength=*/0);
    if (sts != STATUS_SUCCESS) {
      if (sts != STATUS_PIPE_NOT_AVAILABLE) {
        break;
      }

      sts = WaitForPipeAvailability(name);
      if (sts != STATUS_SUCCESS && sts != STATUS_IO_TIMEOUT) {
        break;
      }
    }
  }

  if (sts != STATUS_SUCCESS) {
    return ERROR_PIPE_NOT_CONNECTED;
  }

  // Change to message read mode to match server side.  Max connection count
  // and timeout must be null if client and server are on the same machine.
  DWORD mode = PIPE_READMODE_MESSAGE;
  if (!SetNamedPipeHandleState(h, &mode,
                               /*maxCollectionCount=*/nullptr,
                               /*connectionTimeout=*/nullptr)) {
    DWORD err = GetLastError();
    CloseHandle(h);
    return err;
  }

  *handle = h;
  return ERROR_SUCCESS;
}

void ClientWin::Shutdown() {
  if (hPipe_ != INVALID_HANDLE_VALUE) {
    // TODO: This trips the LateWriteObserver.  We could move this earlier
    // (before the LateWriteObserver is created) or just remove it, although
    // the later could mean an ACK message is not processed by the agent
    // in time.
    // FlushFileBuffers(hPipe_);
    CloseHandle(hPipe_);
    hPipe_ = INVALID_HANDLE_VALUE;
  }
}

}  // namespace sdk
}  // namespace content_analysis