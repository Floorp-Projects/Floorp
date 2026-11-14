// SPDX-License-Identifier: MPL-2.0
#include "PrivateProfileToken.h"
#include <windows.h>
#include <aclapi.h>
#include <sddl.h>
#include <winioctl.h>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

using floorp::profile_token::PrivateProfileToken;
namespace fs = std::filesystem;
namespace {
const char* currentCase = "initialization";
unsigned assertions = 0;
void Require(bool condition, const char* name) {
  ++assertions;
  if (!condition) throw std::runtime_error(std::string(currentCase) + ": " + name);
}
struct Temp final {
  fs::path root;
  Temp() {
    wchar_t path[32768]{};
    Require(GetTempPathW(32768, path) > 0, "temporary root");
    root = fs::path(path) / (L"FloorpPrivateToken-" + std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(GetTickCount64()));
    Require(fs::create_directory(root), "isolated test directory");
  }
  ~Temp() { std::error_code ignored; fs::remove_all(root, ignored); }
  fs::path Profile(const wchar_t* name) { const auto path = root / name; Require(fs::create_directory(path), "create test profile"); return path; }
};
fs::path Token(const fs::path& profile) { return profile / L"floorp-os-server" / L"os-server-token"; }
void Put(const fs::path& path, const std::string& text) { std::ofstream file(path, std::ios::binary); file << text; Require(file.good(), "write synthetic fixture"); }
std::string Read(const fs::path& path) { std::ifstream file(path, std::ios::binary); if (!file.good()) std::cerr << "Native reader error code " << GetLastError() << " errno " << errno << std::endl; Require(file.good(), "ordinary CRT reader opens advertised file"); return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()}; }

void AssertPrivate(const fs::path& path) {
  PSECURITY_DESCRIPTOR security = nullptr;
  PSID owner = nullptr;
  PACL acl = nullptr;
  Require(GetNamedSecurityInfoW(const_cast<LPWSTR>(path.c_str()), SE_FILE_OBJECT,
      OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION, &owner, nullptr, &acl, nullptr, &security) == ERROR_SUCCESS, "inspect actual native DACL");
  SECURITY_DESCRIPTOR_CONTROL control{};
  DWORD revision = 0;
  bool good = owner && acl && acl->AceCount == 2 &&
      GetSecurityDescriptorControl(security, &control, &revision) && (control & SE_DACL_PROTECTED);
  BYTE system[SECURITY_MAX_SID_SIZE]{};
  DWORD bytes = sizeof(system);
  good = good && CreateWellKnownSid(WinLocalSystemSid, nullptr, system, &bytes);
  bool user = false, kernel = false;
  if (good) for (DWORD index = 0; index < acl->AceCount; ++index) {
    void* raw = nullptr;
    if (!GetAce(acl, index, &raw)) { good = false; break; }
    const auto* ace = static_cast<ACCESS_ALLOWED_ACE*>(raw);
    if (ace->Header.AceType != ACCESS_ALLOWED_ACE_TYPE || ace->Header.AceFlags || ace->Mask != FILE_ALL_ACCESS) { good = false; break; }
    PSID subject = const_cast<DWORD*>(&ace->SidStart);
    if (EqualSid(subject, owner)) user = true;
    else if (EqualSid(subject, system)) kernel = true;
    else { good = false; break; }
  }
  if (security) LocalFree(security);
  Require(good && user && kernel, "protected DACL grants only owner and SYSTEM, no inherited ACE");
}
void PublicFixture(const fs::path& path) {
  PSECURITY_DESCRIPTOR descriptor = nullptr;
  Require(ConvertStringSecurityDescriptorToSecurityDescriptorW(L"D:P(A;;FA;;;WD)", SDDL_REVISION_1, &descriptor, nullptr) != FALSE, "synthetic public DACL");
  BOOL present = FALSE, inherited = FALSE;
  PACL acl = nullptr;
  const bool loaded = GetSecurityDescriptorDacl(descriptor, &present, &acl, &inherited) != FALSE;
  const DWORD error = loaded ? SetNamedSecurityInfoW(const_cast<LPWSTR>(path.c_str()), SE_FILE_OBJECT,
      DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION, nullptr, nullptr, acl, nullptr) : ERROR_INVALID_SECURITY_DESCR;
  LocalFree(descriptor);
  Require(error == ERROR_SUCCESS && present, "set public synthetic fixture DACL");
}
void Junction(const fs::path& location, const fs::path& target) {
  Require(fs::create_directory(location), "empty junction fixture");
  HANDLE directory = CreateFileW(location.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
      nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
  Require(directory != INVALID_HANDLE_VALUE, "open junction fixture");
  struct MountPoint {
    ULONG tag;
    USHORT length, reserved, subOffset, subLength, printOffset, printLength;
    wchar_t buffer[16384];
  } data{};
  const std::wstring print = target.wstring();
  const std::wstring substitute = L"\\??\\" + print;
  Require(substitute.size() + print.size() + 2 < 16384, "bounded junction fixture");
  data.tag = IO_REPARSE_TAG_MOUNT_POINT;
  data.subLength = static_cast<USHORT>(substitute.size() * sizeof(wchar_t));
  data.printOffset = static_cast<USHORT>((substitute.size() + 1) * sizeof(wchar_t));
  data.printLength = static_cast<USHORT>(print.size() * sizeof(wchar_t));
  std::memcpy(data.buffer, substitute.c_str(), (substitute.size() + 1) * sizeof(wchar_t));
  std::memcpy(reinterpret_cast<BYTE*>(data.buffer) + data.printOffset, print.c_str(), (print.size() + 1) * sizeof(wchar_t));
  data.length = static_cast<USHORT>(8 + (substitute.size() + print.size() + 2) * sizeof(wchar_t));
  DWORD returned = 0;
  const BOOL result = DeviceIoControl(directory, FSCTL_SET_REPARSE_POINT, &data,
      static_cast<DWORD>(8 + data.length), nullptr, 0, &returned, nullptr);
  CloseHandle(directory);
  Require(result != FALSE, "create real unprivileged directory junction");
}

void NativeContracts() {
  Temp temp;
  currentCase = "native publication/rotation/CRT reader";
  const auto normal = temp.Profile(L"normal");
  {
    PrivateProfileToken owner;
    Require(owner.Prepare(normal.wstring()), "prepare native owner");
    AssertPrivate(normal / L"floorp-os-server");
    AssertPrivate(Token(normal));
    Require(owner.Write("SYNTHETIC-native-long-token"), "native write");
    Put(normal / L"browser-unrelated.txt", "ordinary profile IO remains usable");
    Require(Read(normal / L"browser-unrelated.txt") == "ordinary profile IO remains usable", "directory pins do not block normal browser profile IO");
    Require(Read(Token(normal)) == "SYNTHETIC-native-long-token", "CRT reads while identity anchor remains held");
    Require(owner.Write("short"), "rotation");
    Require(Read(Token(normal)) == "short", "rotation truncates prior bytes");
    Require(owner.Secure(), "native security validation");
    Require(!owner.Write(std::string(4097, 'x')) && !owner.Write("bad\r\nvalue") && !owner.Write("="), "invalid token rejected");
    Require(Read(Token(normal)) == "short", "invalid input does not change secret bytes");
    Require(!MoveFileExW(normal.c_str(), (temp.root / L"moved").c_str(), 0), "pinned profile cannot be renamed during publication");
    Require(!MoveFileExW((normal / L"floorp-os-server").c_str(), (normal / L"replaced").c_str(), 0), "pinned owned directory cannot be renamed");
    Require(owner.Remove(), "delete exact owned object");
    Require(!fs::exists(Token(normal)), "owned advert removed");
    Require(owner.Remove(), "cleanup idempotent");
  }
  {
    PrivateProfileToken first;
    Require(first.Prepare(normal.wstring()) && first.Write("retained"), "normal stop fixture");
  }
  Require(Read(Token(normal)) == "retained", "normal destruction retains compatible advertisement");
  {
    PrivateProfileToken next;
    Require(next.Prepare(normal.wstring()) && next.Write("restart") && next.Remove(), "fresh owner can adopt private retained leaf");
  }
  currentCase = "external leaf deletion and pending handle cleanup";
  const auto missing = temp.Profile(L"externally-removed-leaf");
  {
    PrivateProfileToken owner;
    Require(owner.Prepare(missing.wstring()) && owner.Write("SYNTHETIC-before-delete"), "prepare external removal fixture");
    Require(DeleteFileW(Token(missing).c_str()) != FALSE, "same-user removes advertised name while identity anchor lives");
    Require(owner.Remove(), "externally removed leaf cleanup is idempotent even while delete-pending");
    Require(!owner.Ready() && !fs::exists(Token(missing)), "pending deletion retires only captured owner");
  }
  currentCase = "existing owner public ACL";
  const auto publicProfile = temp.Profile(L"public-fixture");
  fs::create_directory(publicProfile / L"floorp-os-server");
  Put(Token(publicProfile), "SYNTHETIC-old-public-token");
  PublicFixture(publicProfile / L"floorp-os-server");
  PublicFixture(Token(publicProfile));
  {
    PrivateProfileToken owner;
    Require(owner.Prepare(publicProfile.wstring()), "tighten existing owner fixture before write");
    AssertPrivate(publicProfile / L"floorp-os-server");
    AssertPrivate(Token(publicProfile));
    Require(owner.Write("private-new") && owner.Remove(), "write only after actual DACL tightened");
  }
  currentCase = "native junction ancestors";
  const auto foreign = temp.Profile(L"foreign");
  Put(foreign / L"sentinel", "FOREIGN-UNCHANGED");
  const auto junctionProfile = temp.root / L"junction-profile";
  Junction(junctionProfile, foreign);
  {
    PrivateProfileToken owner;
    Require(!owner.Prepare(junctionProfile.wstring()), "reparse profile ancestor rejected");
    Require(!fs::exists(foreign / L"floorp-os-server"), "ancestor junction never receives secret");
  }
  const auto junctionChild = temp.Profile(L"junction-child");
  Junction(junctionChild / L"floorp-os-server", foreign);
  {
    PrivateProfileToken owner;
    Require(!owner.Prepare(junctionChild.wstring()), "reparse dedicated directory rejected");
  }
  currentCase = "native file symlink refusal";
  const auto symlinkProfile = temp.Profile(L"file-symlink-leaf");
  fs::create_directory(symlinkProfile / L"floorp-os-server");
  Require(CreateSymbolicLinkW(Token(symlinkProfile).c_str(), (foreign / L"sentinel").c_str(),
      SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE) != FALSE, "real file symlink fixture must be available");
  {
    PrivateProfileToken owner;
    Require(!owner.Prepare(symlinkProfile.wstring()), "file symlink leaf rejected before ACL or secret mutation");
    Require(Read(foreign / L"sentinel") == "FOREIGN-UNCHANGED", "file symlink target ACL and content preserved");
  }
  currentCase = "native hardlink refusal";
  const auto hardProfile = temp.Profile(L"hardlink");
  fs::create_directory(hardProfile / L"floorp-os-server");
  Require(CreateHardLinkW(Token(hardProfile).c_str(), (foreign / L"sentinel").c_str(), nullptr) != FALSE, "create real hardlink fixture");
  {
    PrivateProfileToken owner;
    Require(!owner.Prepare(hardProfile.wstring()), "multiply linked token leaf rejected before write");
    Require(Read(foreign / L"sentinel") == "FOREIGN-UNCHANGED", "hardlink target never overwritten");
  }
  currentCase = "foreign leaf identity swap";
  const auto siblingProfile = temp.Profile(L"foreign-sibling-acl");
  fs::create_directory(siblingProfile / L"floorp-os-server");
  Require(CreateHardLinkW((siblingProfile / L"floorp-os-server" / L"foreign-sibling").c_str(),
      (foreign / L"sentinel").c_str(), nullptr) != FALSE, "create unrelated linked sibling");
  {
    PrivateProfileToken owner;
    Require(owner.Prepare(siblingProfile.wstring()) && owner.Write("SYNTHETIC-sibling") && owner.Remove(),
        "prepare/write/remove never propagates DACL changes into unrelated siblings");
    Require(Read(foreign / L"sentinel") == "FOREIGN-UNCHANGED", "foreign sibling content and read ACL preserved");
  }
  const auto swap = temp.Profile(L"foreign-leaf-swap");
  {
    PrivateProfileToken owner;
    Require(owner.Prepare(swap.wstring()) && owner.Write("SYNTHETIC-owned"), "prepare identity fixture");
    Require(MoveFileExW(Token(swap).c_str(), (swap / L"captured-old").c_str(), 0) != FALSE, "controlled same-user leaf substitution");
    Put(Token(swap), "FOREIGN-REPLACEMENT");
    Require(!owner.Write("must-not-overwrite") && !owner.Remove(), "identity mismatch refuses overwrite and foreign cleanup");
    Require(Read(Token(swap)) == "FOREIGN-REPLACEMENT", "foreign replacement preserved");
  }
  for (const auto* path : {L"relative", L"C:\\bad\\..\\profile", L"C:\\bad:stream", L"\\\\server\\share\\profile", L"C:\\bad\\profile ", L"C:\\bad\\profile."}) {
    PrivateProfileToken owner;
    Require(!owner.Prepare(path), "unsafe profile spelling rejected");
  }
  Require(Read(foreign / L"sentinel") == "FOREIGN-UNCHANGED", "all hostile native cases preserve foreign data");
}
}  // namespace
int main() {
  try { NativeContracts(); std::cout << "Windows native private-profile contracts passed: " << assertions << " assertions\n"; return 0; }
  catch (const std::exception& error) { std::cerr << "Windows native contract failed: " << error.what() << "\n"; return 1; }
}
