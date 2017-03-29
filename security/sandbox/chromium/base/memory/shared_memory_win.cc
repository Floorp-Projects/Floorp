// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/shared_memory.h"

#include <aclapi.h>
#include <stddef.h>
#include <stdint.h>

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"

namespace {

// Errors that can occur during Shared Memory construction.
// These match tools/metrics/histograms/histograms.xml.
// This enum is append-only.
enum CreateError {
  SUCCESS = 0,
  SIZE_ZERO = 1,
  SIZE_TOO_LARGE = 2,
  INITIALIZE_ACL_FAILURE = 3,
  INITIALIZE_SECURITY_DESC_FAILURE = 4,
  SET_SECURITY_DESC_FAILURE = 5,
  CREATE_FILE_MAPPING_FAILURE = 6,
  REDUCE_PERMISSIONS_FAILURE = 7,
  ALREADY_EXISTS = 8,
  CREATE_ERROR_LAST = ALREADY_EXISTS
};

// Emits UMA metrics about encountered errors. Pass zero (0) for |winerror|
// if there is no associated Windows error.
void LogError(CreateError error, DWORD winerror) {
  UMA_HISTOGRAM_ENUMERATION("SharedMemory.CreateError", error,
                            CREATE_ERROR_LAST + 1);
  static_assert(ERROR_SUCCESS == 0, "Windows error code changed!");
  if (winerror != ERROR_SUCCESS)
    UMA_HISTOGRAM_SPARSE_SLOWLY("SharedMemory.CreateWinError", winerror);
}

typedef enum _SECTION_INFORMATION_CLASS {
  SectionBasicInformation,
} SECTION_INFORMATION_CLASS;

typedef struct _SECTION_BASIC_INFORMATION {
  PVOID BaseAddress;
  ULONG Attributes;
  LARGE_INTEGER Size;
} SECTION_BASIC_INFORMATION, *PSECTION_BASIC_INFORMATION;

typedef ULONG(__stdcall* NtQuerySectionType)(
    HANDLE SectionHandle,
    SECTION_INFORMATION_CLASS SectionInformationClass,
    PVOID SectionInformation,
    ULONG SectionInformationLength,
    PULONG ResultLength);

// Returns the length of the memory section starting at the supplied address.
size_t GetMemorySectionSize(void* address) {
  MEMORY_BASIC_INFORMATION memory_info;
  if (!::VirtualQuery(address, &memory_info, sizeof(memory_info)))
    return 0;
  return memory_info.RegionSize - (static_cast<char*>(address) -
         static_cast<char*>(memory_info.AllocationBase));
}

// Checks if the section object is safe to map. At the moment this just means
// it's not an image section.
bool IsSectionSafeToMap(HANDLE handle) {
  static NtQuerySectionType nt_query_section_func;
  if (!nt_query_section_func) {
    nt_query_section_func = reinterpret_cast<NtQuerySectionType>(
        ::GetProcAddress(::GetModuleHandle(L"ntdll.dll"), "NtQuerySection"));
    DCHECK(nt_query_section_func);
  }

  // The handle must have SECTION_QUERY access for this to succeed.
  SECTION_BASIC_INFORMATION basic_information = {};
  ULONG status =
      nt_query_section_func(handle, SectionBasicInformation, &basic_information,
                            sizeof(basic_information), nullptr);
  if (status)
    return false;
  return (basic_information.Attributes & SEC_IMAGE) != SEC_IMAGE;
}

// Returns a HANDLE on success and |nullptr| on failure.
// This function is similar to CreateFileMapping, but removes the permissions
// WRITE_DAC, WRITE_OWNER, READ_CONTROL, and DELETE.
//
// A newly created file mapping has two sets of permissions. It has access
// control permissions (WRITE_DAC, WRITE_OWNER, READ_CONTROL, and DELETE) and
// file permissions (FILE_MAP_READ, FILE_MAP_WRITE, etc.). ::DuplicateHandle()
// with the parameter DUPLICATE_SAME_ACCESS copies both sets of permissions.
//
// The Chrome sandbox prevents HANDLEs with the WRITE_DAC permission from being
// duplicated into unprivileged processes. But the only way to copy file
// permissions is with the parameter DUPLICATE_SAME_ACCESS. This means that
// there is no way for a privileged process to duplicate a file mapping into an
// unprivileged process while maintaining the previous file permissions.
//
// By removing all access control permissions of a file mapping immediately
// after creation, ::DuplicateHandle() effectively only copies the file
// permissions.
HANDLE CreateFileMappingWithReducedPermissions(SECURITY_ATTRIBUTES* sa,
                                               size_t rounded_size,
                                               LPCWSTR name) {
  HANDLE h = CreateFileMapping(INVALID_HANDLE_VALUE, sa, PAGE_READWRITE, 0,
                               static_cast<DWORD>(rounded_size), name);
  if (!h) {
    LogError(CREATE_FILE_MAPPING_FAILURE, GetLastError());
    return nullptr;
  }

  HANDLE h2;
  BOOL success = ::DuplicateHandle(
      GetCurrentProcess(), h, GetCurrentProcess(), &h2,
      FILE_MAP_READ | FILE_MAP_WRITE | SECTION_QUERY, FALSE, 0);
  BOOL rv = ::CloseHandle(h);
  DCHECK(rv);

  if (!success) {
    LogError(REDUCE_PERMISSIONS_FAILURE, GetLastError());
    return nullptr;
  }

  return h2;
}

}  // namespace.

namespace base {

SharedMemory::SharedMemory()
    : external_section_(false),
      mapped_size_(0),
      memory_(NULL),
      read_only_(false),
      requested_size_(0) {}

SharedMemory::SharedMemory(const std::wstring& name)
    : external_section_(false),
      name_(name),
      mapped_size_(0),
      memory_(NULL),
      read_only_(false),
      requested_size_(0) {}

SharedMemory::SharedMemory(const SharedMemoryHandle& handle, bool read_only)
    : external_section_(true),
      mapped_size_(0),
      memory_(NULL),
      read_only_(read_only),
      requested_size_(0) {
  DCHECK(!handle.IsValid() || handle.BelongsToCurrentProcess());
  mapped_file_.Set(handle.GetHandle());
}

SharedMemory::~SharedMemory() {
  Unmap();
  Close();
}

// static
bool SharedMemory::IsHandleValid(const SharedMemoryHandle& handle) {
  return handle.IsValid();
}

// static
SharedMemoryHandle SharedMemory::NULLHandle() {
  return SharedMemoryHandle();
}

// static
void SharedMemory::CloseHandle(const SharedMemoryHandle& handle) {
  handle.Close();
}

// static
size_t SharedMemory::GetHandleLimit() {
  // Rounded down from value reported here:
  // http://blogs.technet.com/b/markrussinovich/archive/2009/09/29/3283844.aspx
  return static_cast<size_t>(1 << 23);
}

// static
SharedMemoryHandle SharedMemory::DuplicateHandle(
    const SharedMemoryHandle& handle) {
  DCHECK(handle.BelongsToCurrentProcess());
  HANDLE duped_handle;
  ProcessHandle process = GetCurrentProcess();
  BOOL success =
      ::DuplicateHandle(process, handle.GetHandle(), process, &duped_handle, 0,
                        FALSE, DUPLICATE_SAME_ACCESS);
  if (success) {
    base::SharedMemoryHandle handle(duped_handle, GetCurrentProcId());
    handle.SetOwnershipPassesToIPC(true);
    return handle;
  }
  return SharedMemoryHandle();
}

bool SharedMemory::CreateAndMapAnonymous(size_t size) {
  return CreateAnonymous(size) && Map(size);
}

bool SharedMemory::Create(const SharedMemoryCreateOptions& options) {
  // TODO(bsy,sehr): crbug.com/210609 NaCl forces us to round up 64k here,
  // wasting 32k per mapping on average.
  static const size_t kSectionMask = 65536 - 1;
  DCHECK(!options.executable);
  DCHECK(!mapped_file_.Get());
  if (options.size == 0) {
    LogError(SIZE_ZERO, 0);
    return false;
  }

  // Check maximum accounting for overflow.
  if (options.size >
      static_cast<size_t>(std::numeric_limits<int>::max()) - kSectionMask) {
    LogError(SIZE_TOO_LARGE, 0);
    return false;
  }

  size_t rounded_size = (options.size + kSectionMask) & ~kSectionMask;
  name_ = options.name_deprecated ?
      ASCIIToUTF16(*options.name_deprecated) : L"";
  SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, FALSE };
  SECURITY_DESCRIPTOR sd;
  ACL dacl;

  if (name_.empty()) {
    // Add an empty DACL to enforce anonymous read-only sections.
    sa.lpSecurityDescriptor = &sd;
    if (!InitializeAcl(&dacl, sizeof(dacl), ACL_REVISION)) {
      LogError(INITIALIZE_ACL_FAILURE, GetLastError());
      return false;
    }
    if (!InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION)) {
      LogError(INITIALIZE_SECURITY_DESC_FAILURE, GetLastError());
      return false;
    }
    if (!SetSecurityDescriptorDacl(&sd, TRUE, &dacl, FALSE)) {
      LogError(SET_SECURITY_DESC_FAILURE, GetLastError());
      return false;
    }

    // Windows ignores DACLs on certain unnamed objects (like shared sections).
    // So, we generate a random name when we need to enforce read-only.
    uint64_t rand_values[4];
    RandBytes(&rand_values, sizeof(rand_values));
    name_ = StringPrintf(L"CrSharedMem_%016llx%016llx%016llx%016llx",
                         rand_values[0], rand_values[1],
                         rand_values[2], rand_values[3]);
  }
  mapped_file_.Set(CreateFileMappingWithReducedPermissions(
      &sa, rounded_size, name_.empty() ? nullptr : name_.c_str()));
  if (!mapped_file_.IsValid()) {
    // The error is logged within CreateFileMappingWithReducedPermissions().
    return false;
  }

  requested_size_ = options.size;

  // Check if the shared memory pre-exists.
  if (GetLastError() == ERROR_ALREADY_EXISTS) {
    // If the file already existed, set requested_size_ to 0 to show that
    // we don't know the size.
    requested_size_ = 0;
    external_section_ = true;
    if (!options.open_existing_deprecated) {
      Close();
      // From "if" above: GetLastError() == ERROR_ALREADY_EXISTS.
      LogError(ALREADY_EXISTS, ERROR_ALREADY_EXISTS);
      return false;
    }
  }

  LogError(SUCCESS, ERROR_SUCCESS);
  return true;
}

bool SharedMemory::Delete(const std::string& name) {
  // intentionally empty -- there is nothing for us to do on Windows.
  return true;
}

bool SharedMemory::Open(const std::string& name, bool read_only) {
  DCHECK(!mapped_file_.Get());
  DWORD access = FILE_MAP_READ | SECTION_QUERY;
  if (!read_only)
    access |= FILE_MAP_WRITE;
  name_ = ASCIIToUTF16(name);
  read_only_ = read_only;
  mapped_file_.Set(
      OpenFileMapping(access, false, name_.empty() ? nullptr : name_.c_str()));
  if (!mapped_file_.IsValid())
    return false;
  // If a name specified assume it's an external section.
  if (!name_.empty())
    external_section_ = true;
  // Note: size_ is not set in this case.
  return true;
}

bool SharedMemory::MapAt(off_t offset, size_t bytes) {
  if (!mapped_file_.Get())
    return false;

  if (bytes > static_cast<size_t>(std::numeric_limits<int>::max()))
    return false;

  if (memory_)
    return false;

  if (external_section_ && !IsSectionSafeToMap(mapped_file_.Get()))
    return false;

  memory_ = MapViewOfFile(
      mapped_file_.Get(),
      read_only_ ? FILE_MAP_READ : FILE_MAP_READ | FILE_MAP_WRITE,
      static_cast<uint64_t>(offset) >> 32, static_cast<DWORD>(offset), bytes);
  if (memory_ != NULL) {
    DCHECK_EQ(0U, reinterpret_cast<uintptr_t>(memory_) &
        (SharedMemory::MAP_MINIMUM_ALIGNMENT - 1));
    mapped_size_ = GetMemorySectionSize(memory_);
    return true;
  }
  return false;
}

bool SharedMemory::Unmap() {
  if (memory_ == NULL)
    return false;

  UnmapViewOfFile(memory_);
  memory_ = NULL;
  return true;
}

bool SharedMemory::ShareToProcessCommon(ProcessHandle process,
                                        SharedMemoryHandle* new_handle,
                                        bool close_self,
                                        ShareMode share_mode) {
  *new_handle = SharedMemoryHandle();
  DWORD access = FILE_MAP_READ | SECTION_QUERY;
  DWORD options = 0;
  HANDLE mapped_file = mapped_file_.Get();
  HANDLE result;
  if (share_mode == SHARE_CURRENT_MODE && !read_only_)
    access |= FILE_MAP_WRITE;
  if (close_self) {
    // DUPLICATE_CLOSE_SOURCE causes DuplicateHandle to close mapped_file.
    options = DUPLICATE_CLOSE_SOURCE;
    HANDLE detached_handle = mapped_file_.Take();
    DCHECK_EQ(detached_handle, mapped_file);
    Unmap();
  }

  if (process == GetCurrentProcess() && close_self) {
    *new_handle = SharedMemoryHandle(mapped_file, base::GetCurrentProcId());
    return true;
  }

  if (!::DuplicateHandle(GetCurrentProcess(), mapped_file, process, &result,
                         access, FALSE, options)) {
    return false;
  }
  *new_handle = SharedMemoryHandle(result, base::GetProcId(process));
  new_handle->SetOwnershipPassesToIPC(true);
  return true;
}


void SharedMemory::Close() {
  mapped_file_.Close();
}

SharedMemoryHandle SharedMemory::handle() const {
  return SharedMemoryHandle(mapped_file_.Get(), base::GetCurrentProcId());
}

SharedMemoryHandle SharedMemory::TakeHandle() {
  SharedMemoryHandle handle(mapped_file_.Take(), base::GetCurrentProcId());
  handle.SetOwnershipPassesToIPC(true);
  memory_ = nullptr;
  mapped_size_ = 0;
  return handle;
}

}  // namespace base
