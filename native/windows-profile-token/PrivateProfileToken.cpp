// SPDX-License-Identifier: MPL-2.0
#include "PrivateProfileToken.h"

#ifdef MOZILLA_INTERNAL_API
#  include "mozilla/WindowsHWrapper.h"
#else
#  include <windows.h>
#endif
#include <aclapi.h>
#include <sddl.h>
#include <winternl.h>
#include <cstring>
#include <utility>
#include <vector>

namespace floorp::profile_token {
namespace {
constexpr ULONG kDontReparse = 0x1000;
constexpr ULONG kDirectory = 0x1;
constexpr ULONG kSynchronous = 0x20;
constexpr ULONG kNonDirectory = 0x40;
constexpr ULONG kOpenReparsePoint = 0x00200000;
constexpr ULONG kOpen = 1;
constexpr ULONG kOpenIf = 3;
constexpr NTSTATUS kNameMissing = static_cast<NTSTATUS>(0xc0000034UL);
constexpr NTSTATUS kPathMissing = static_cast<NTSTATUS>(0xc000003aUL);
constexpr NTSTATUS kSharingViolation = static_cast<NTSTATUS>(0xc0000043UL);
constexpr wchar_t kDirectoryName[] = L"floorp-os-server";
constexpr wchar_t kFileName[] = L"os-server-token";

class Handle final {
 public:
  explicit Handle(HANDLE value = INVALID_HANDLE_VALUE) : mValue(value) {}
  ~Handle() { Reset(); }
  Handle(const Handle&) = delete;
  Handle& operator=(const Handle&) = delete;
  Handle(Handle&& other) noexcept : mValue(other.mValue) { other.mValue = INVALID_HANDLE_VALUE; }
  Handle& operator=(Handle&& other) noexcept {
    if (this != &other) { Reset(); mValue = other.mValue; other.mValue = INVALID_HANDLE_VALUE; }
    return *this;
  }
  explicit operator bool() const { return mValue != nullptr && mValue != INVALID_HANDLE_VALUE; }
  HANDLE Get() const { return mValue; }
  void Reset() { if (*this) CloseHandle(mValue); mValue = INVALID_HANDLE_VALUE; }
 private:
  HANDLE mValue;
};
struct LocalMemory final {
  HLOCAL value = nullptr;
  ~LocalMemory() { if (value) LocalFree(value); }
};
using NtCreate = NTSTATUS(NTAPI*)(PHANDLE, ACCESS_MASK, POBJECT_ATTRIBUTES,
    PIO_STATUS_BLOCK, PLARGE_INTEGER, ULONG, ULONG, ULONG, ULONG, PVOID, ULONG);

Handle Relative(HANDLE parent, const std::wstring& name, ACCESS_MASK access,
                bool directory, ULONG disposition = kOpen,
                PSECURITY_DESCRIPTOR security = nullptr, NTSTATUS* result = nullptr,
                ULONG share = FILE_SHARE_READ) {
  if (name.empty() || name.size() > 255 || name.find_first_of(L"\\/:\0", 0, 4) != std::wstring::npos) return Handle();
  const auto create = reinterpret_cast<NtCreate>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "NtCreateFile"));
  if (!create) return Handle();
  UNICODE_STRING unicode{};
  unicode.Buffer = const_cast<PWSTR>(name.data());
  unicode.Length = static_cast<USHORT>(name.size() * sizeof(wchar_t));
  unicode.MaximumLength = unicode.Length;
  OBJECT_ATTRIBUTES attributes{};
  attributes.Length = sizeof(attributes);
  attributes.RootDirectory = parent;
  attributes.ObjectName = &unicode;
  attributes.Attributes = OBJ_CASE_INSENSITIVE | kDontReparse;
  attributes.SecurityDescriptor = security;
  IO_STATUS_BLOCK io{};
  HANDLE file = INVALID_HANDLE_VALUE;
  const NTSTATUS status = create(&file, access | SYNCHRONIZE, &attributes, &io,
      nullptr, FILE_ATTRIBUTE_NORMAL, directory ? FILE_SHARE_READ | FILE_SHARE_WRITE : share, disposition,
      kOpenReparsePoint | kSynchronous | (directory ? kDirectory : kNonDirectory),
      nullptr, 0);
  if (result) *result = status;
  return status >= 0 ? Handle(file) : Handle();
}

bool Node(HANDLE handle, bool directory) {
  FILE_ATTRIBUTE_TAG_INFO attributes{};
  if (GetFileType(handle) != FILE_TYPE_DISK ||
      !GetFileInformationByHandleEx(handle, FileAttributeTagInfo, &attributes, sizeof(attributes)) ||
      (attributes.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) ||
      !!(attributes.FileAttributes & FILE_ATTRIBUTE_DIRECTORY) != directory) return false;
  if (!directory) {
    FILE_STANDARD_INFO standard{};
    if (!GetFileInformationByHandleEx(handle, FileStandardInfo, &standard, sizeof(standard)) || standard.NumberOfLinks != 1) return false;
  }
  return true;
}
bool Identity(HANDLE handle, FILE_ID_INFO& identity) {
  return GetFileInformationByHandleEx(handle, FileIdInfo, &identity, sizeof(identity)) != FALSE;
}
bool Same(const FILE_ID_INFO& left, const FILE_ID_INFO& right) {
  return left.VolumeSerialNumber == right.VolumeSerialNumber &&
      std::memcmp(left.FileId.Identifier, right.FileId.Identifier, sizeof(left.FileId.Identifier)) == 0;
}

bool Components(const std::wstring& path, std::vector<std::wstring>& parts) {
  if (path.size() < 4 || path.size() > 32760 ||
      !((path[0] >= L'A' && path[0] <= L'Z') || (path[0] >= L'a' && path[0] <= L'z')) ||
      path[1] != L':' || path[2] != L'\\') return false;
  size_t start = 3;
  while (start < path.size()) {
    const size_t end = path.find(L'\\', start);
    const std::wstring component = path.substr(start, end == std::wstring::npos ? end : end - start);
    if (component.empty() || component.size() > 255 || component == L"." || component == L".." ||
        component.back() == L'.' || component.back() == L' ') return false;
    for (wchar_t character : component) if (character < 32 || character == 127 || character == L'/' || character == L':') return false;
    parts.push_back(component);
    if (parts.size() > 128) return false;
    if (end == std::wstring::npos) break;
    start = end + 1;
    if (start == path.size()) return false;
  }
  return !parts.empty();
}

bool ValidToken(std::string_view token) {
  if (token.empty() || token.size() > 4096) return false;
  unsigned padding = 0;
  for (unsigned char character : token) {
    if (character == '=') { if (++padding > 2) return false; continue; }
    if (padding || !((character >= 'A' && character <= 'Z') || (character >= 'a' && character <= 'z') ||
        (character >= '0' && character <= '9') || character == '.' || character == '_' || character == '~' ||
        character == '+' || character == '/' || character == '-')) return false;
  }
  return padding < token.size();
}
}  // namespace

struct PrivateProfileToken::Impl final {
  std::vector<Handle> ancestors;
  Handle directory;
  Handle anchor;
  FILE_ID_INFO identity{};
  std::vector<BYTE> sid;
  LocalMemory descriptor;
  std::wstring profile;
  bool ready = false;
  bool cleanupBlockedByReader = false;

  bool Security() {
    HANDLE raw = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &raw)) return false;
    Handle processToken(raw);
    DWORD bytes = 0;
    GetTokenInformation(raw, TokenUser, nullptr, 0, &bytes);
    if (!bytes || bytes > 65536) return false;
    std::vector<BYTE> user(bytes);
    if (!GetTokenInformation(raw, TokenUser, user.data(), bytes, &bytes)) return false;
    const PSID current = reinterpret_cast<TOKEN_USER*>(user.data())->User.Sid;
    if (!IsValidSid(current)) return false;
    sid.resize(GetLengthSid(current));
    if (!CopySid(static_cast<DWORD>(sid.size()), sid.data(), current)) return false;
    LPWSTR sidText = nullptr;
    if (!ConvertSidToStringSidW(sid.data(), &sidText)) return false;
    LocalMemory text;
    text.value = sidText;
    const std::wstring sddl = L"O:" + std::wstring(sidText) + L"D:P(A;;FA;;;" + sidText + L")(A;;FA;;;SY)";
    PSECURITY_DESCRIPTOR security = nullptr;
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl.c_str(), SDDL_REVISION_1, &security, nullptr)) return false;
    descriptor.value = security;
    return true;
  }
  bool Private(HANDLE handle) const {
    PSECURITY_DESCRIPTOR raw = nullptr;
    PSID owner = nullptr;
    PACL acl = nullptr;
    if (GetSecurityInfo(handle, SE_FILE_OBJECT, OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION,
                        &owner, nullptr, &acl, nullptr, &raw) != ERROR_SUCCESS) return false;
    LocalMemory security;
    security.value = raw;
    SECURITY_DESCRIPTOR_CONTROL control = 0;
    DWORD revision = 0;
    if (!owner || !EqualSid(owner, const_cast<BYTE*>(sid.data())) || !acl ||
        !GetSecurityDescriptorControl(raw, &control, &revision) || !(control & SE_DACL_PROTECTED) ||
        acl->AceCount == 0 || acl->AceCount > 2) return false;
    BYTE system[SECURITY_MAX_SID_SIZE]{};
    DWORD size = sizeof(system);
    if (!CreateWellKnownSid(WinLocalSystemSid, nullptr, system, &size)) return false;
    bool userSeen = false, systemSeen = false;
    for (DWORD index = 0; index < acl->AceCount; ++index) {
      void* rawAce = nullptr;
      if (!GetAce(acl, index, &rawAce)) return false;
      const auto* ace = static_cast<ACCESS_ALLOWED_ACE*>(rawAce);
      if (ace->Header.AceType != ACCESS_ALLOWED_ACE_TYPE || ace->Header.AceFlags || ace->Mask != FILE_ALL_ACCESS) return false;
      PSID subject = const_cast<DWORD*>(&ace->SidStart);
      if (EqualSid(subject, const_cast<BYTE*>(sid.data()))) userSeen = true;
      else if (EqualSid(subject, system)) systemSeen = true;
      else return false;
      if (EqualSid(subject, system)) systemSeen = true;
    }
    return userSeen && systemSeen;
  }
  bool Owner(HANDLE handle) const {
    PSECURITY_DESCRIPTOR raw = nullptr;
    PSID owner = nullptr;
    if (GetSecurityInfo(handle, SE_FILE_OBJECT, OWNER_SECURITY_INFORMATION, &owner,
        nullptr, nullptr, nullptr, &raw) != ERROR_SUCCESS) return false;
    LocalMemory security;
    security.value = raw;
    return owner && EqualSid(owner, const_cast<BYTE*>(sid.data()));
  }
  bool Tighten(HANDLE handle) const {
    PSECURITY_DESCRIPTOR old = nullptr;
    PSID owner = nullptr;
    if (GetSecurityInfo(handle, SE_FILE_OBJECT, OWNER_SECURITY_INFORMATION, &owner,
                        nullptr, nullptr, nullptr, &old) != ERROR_SUCCESS) return false;
    LocalMemory existing;
    existing.value = old;
    if (!owner || !EqualSid(owner, const_cast<BYTE*>(sid.data()))) return false;
    // SetSecurityInfo is intentionally NOT used here: its Win32 inheritance
    // propagation can rewrite ACLs of preexisting children, including a hard
    // link to a foreign file. Apply the descriptor to this opened object only.
    if (!SetKernelObjectSecurity(handle,
        DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION,
        descriptor.value)) return false;
    return Private(handle);
  }
  bool PinsSafe() const {
    for (const auto& ancestor : ancestors) if (!Node(ancestor.Get(), true)) return false;
    return directory && Node(directory.Get(), true);
  }
  Handle Owned(ACCESS_MASK access, NTSTATUS* status = nullptr) const {
    Handle file = Relative(directory.Get(), kFileName, access | FILE_READ_ATTRIBUTES | READ_CONTROL, false, kOpen, nullptr, status);
    FILE_ID_INFO observed{};
    if (!file || !Node(file.Get(), false) || !Identity(file.Get(), observed) || !Same(identity, observed)) return Handle();
    return file;
  }
};

PrivateProfileToken::PrivateProfileToken() : mImpl(std::make_unique<Impl>()) {}
PrivateProfileToken::~PrivateProfileToken() = default;
bool PrivateProfileToken::Ready() const { return mImpl->ready; }
bool PrivateProfileToken::CleanupBlockedByReader() const { return mImpl->cleanupBlockedByReader; }

bool PrivateProfileToken::Prepare(const std::wstring& profilePath) {
  if (mImpl->ready) return mImpl->profile == profilePath && Secure();
  std::vector<std::wstring> parts;
  if (!Components(profilePath, parts) ||
      GetDriveTypeW(profilePath.substr(0, 3).c_str()) != DRIVE_FIXED) return false;
  auto candidate = std::make_unique<Impl>();
  if (!candidate->Security()) return false;
  std::wstring root = L"\\\\?\\" + profilePath.substr(0, 3);
  Handle drive(CreateFileW(root.c_str(), FILE_READ_ATTRIBUTES | FILE_TRAVERSE | SYNCHRONIZE,
      FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
      FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
  if (!drive || !Node(drive.Get(), true)) return false;
  candidate->ancestors.push_back(std::move(drive));
  for (const auto& part : parts) {
    Handle next = Relative(candidate->ancestors.back().Get(), part, FILE_READ_ATTRIBUTES | FILE_TRAVERSE, true);
    if (!next || !Node(next.Get(), true)) return false;
    candidate->ancestors.push_back(std::move(next));
  }
  candidate->directory = Relative(candidate->ancestors.back().Get(), kDirectoryName,
      FILE_READ_ATTRIBUTES | FILE_TRAVERSE | READ_CONTROL | WRITE_DAC | FILE_ADD_FILE,
      true, kOpenIf, candidate->descriptor.value);
  if (!candidate->directory || !Node(candidate->directory.Get(), true) ||
      !candidate->Owner(candidate->directory.Get())) return false;
  Handle file = Relative(candidate->directory.Get(), kFileName,
      FILE_GENERIC_WRITE | FILE_READ_ATTRIBUTES | READ_CONTROL | WRITE_DAC,
      false, kOpenIf, candidate->descriptor.value);
  // Validate the leaf (especially hard-link count) before touching existing
  // security. Neither file contents nor any outside-linked ACL may be changed
  // just because preparation ultimately rejects an unsafe location.
  if (!file || !Node(file.Get(), false) || !candidate->Owner(file.Get()) ||
      !candidate->Tighten(candidate->directory.Get()) ||
      !candidate->Tighten(file.Get()) || !Identity(file.Get(), candidate->identity)) return false;
  // The identity anchor permits ordinary CRT/Python readers. Holding DELETE
  // access indefinitely would force every reader to share FILE_SHARE_DELETE.
  // Later operations reopen the single fixed leaf relative to the pinned
  // directory, deny concurrent mutation, and compare against this live object.
  candidate->anchor = Relative(candidate->directory.Get(), kFileName,
      FILE_READ_ATTRIBUTES | READ_CONTROL, false, kOpen, nullptr, nullptr,
      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE);
  FILE_ID_INFO anchored{};
  if (!candidate->anchor || !Identity(candidate->anchor.Get(), anchored) ||
      !Same(candidate->identity, anchored)) return false;
  candidate->profile = profilePath;
  candidate->ready = true;
  mImpl = std::move(candidate);
  return true;
}

bool PrivateProfileToken::Write(std::string_view token) {
  if (!mImpl->ready || !ValidToken(token) || !mImpl->PinsSafe() ||
      !mImpl->Tighten(mImpl->directory.Get())) return false;
  Handle file = mImpl->Owned(FILE_GENERIC_WRITE | WRITE_DAC);
  if (!file || !mImpl->Tighten(file.Get())) return false;
  LARGE_INTEGER zero{};
  if (!SetFilePointerEx(file.Get(), zero, nullptr, FILE_BEGIN)) return false;
  size_t offset = 0;
  while (offset < token.size()) {
    DWORD written = 0;
    if (!WriteFile(file.Get(), token.data() + offset, static_cast<DWORD>(token.size() - offset), &written, nullptr) || !written) return false;
    offset += written;
  }
  return SetEndOfFile(file.Get()) && FlushFileBuffers(file.Get());
}
bool PrivateProfileToken::Secure() {
  if (!mImpl->ready || !mImpl->PinsSafe() || !mImpl->Private(mImpl->directory.Get())) return false;
  Handle file = mImpl->Owned(FILE_READ_ATTRIBUTES | READ_CONTROL);
  return file && mImpl->Private(file.Get());
}
bool PrivateProfileToken::Remove() {
  mImpl->cleanupBlockedByReader = false;
  if (!mImpl->ready) return true;
  if (!mImpl->PinsSafe()) return false;
  NTSTATUS status = 0;
  Handle file = mImpl->Owned(DELETE | FILE_READ_ATTRIBUTES | READ_CONTROL, &status);
  if (!file) {
    if (status != kNameMissing && status != kPathMissing) {
      mImpl->cleanupBlockedByReader = status == kSharingViolation;
      return false;
    }
  } else {
    if (!mImpl->Private(file.Get()) || !mImpl->Private(mImpl->directory.Get())) return false;
    FILE_DISPOSITION_INFO disposition{TRUE};
    if (!SetFileInformationByHandle(file.Get(), FileDispositionInfo, &disposition, sizeof(disposition))) return false;
  }
  mImpl = std::make_unique<Impl>();
  return true;
}
}  // namespace floorp::profile_token
