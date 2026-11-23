#ifndef FLOORP_PRIVATE_PROFILE_TOKEN_H
#define FLOORP_PRIVATE_PROFILE_TOKEN_H

#include <memory>
#include <string>
#include <string_view>

namespace floorp::profile_token {
// One profile owner. No secret is stored on this object after Write returns.
// Paths are derived from a trusted Gecko ProfD, never from token/client input.
// The native implementation pins the root/ancestor handles, rejects reparses
// and hard links, and installs an owner+SYSTEM protected DACL before any secret.
class PrivateProfileToken final {
 public:
  PrivateProfileToken();
  ~PrivateProfileToken();
  PrivateProfileToken(const PrivateProfileToken&) = delete;
  PrivateProfileToken& operator=(const PrivateProfileToken&) = delete;
  bool Prepare(const std::wstring& profilePath);
  bool Write(std::string_view token);
  bool Secure();
  bool Remove();
  bool Ready() const;
  // True only when the last Remove was blocked by Windows sharing semantics.
  bool CleanupBlockedByReader() const;
 private:
  struct Impl;
  std::unique_ptr<Impl> mImpl;
};
}  // namespace floorp::profile_token
#endif
