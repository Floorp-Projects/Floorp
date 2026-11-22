// SPDX-License-Identifier: MPL-2.0
#ifndef FLOORP_PRIVATE_PROFILE_TOKEN_SERVICE_H
#define FLOORP_PRIVATE_PROFILE_TOKEN_SERVICE_H
#include "nsIFloorpPrivateProfileToken.h"
#include <memory>

namespace floorp::profile_token {
class PrivateProfileToken;
class PrivateProfileTokenService final : public nsIFloorpPrivateProfileToken {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIFLOORPPRIVATEPROFILETOKEN
  PrivateProfileTokenService();
 private:
  ~PrivateProfileTokenService();
  std::unique_ptr<PrivateProfileToken> mStorage;
};
}
#endif
