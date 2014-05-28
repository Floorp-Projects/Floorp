/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NSSErrorsService_h
#define NSSErrorsService_h

#include "nsINSSErrorsService.h"

#include "mozilla/Attributes.h"
#include "nsCOMPtr.h"
#include "nsIStringBundle.h"
#include "prerror.h"

namespace mozilla {
namespace psm {

enum PSMErrorCodes {
  PSM_ERROR_KEY_PINNING_FAILURE = (nsINSSErrorsService::PSM_ERROR_BASE + 0)
};

void RegisterPSMErrorTable();

class NSSErrorsService MOZ_FINAL : public nsINSSErrorsService
{
  NS_DECL_ISUPPORTS
  NS_DECL_NSINSSERRORSSERVICE

public:
  nsresult Init();

private:
  nsCOMPtr<nsIStringBundle> mPIPNSSBundle;
  nsCOMPtr<nsIStringBundle> mNSSErrorsBundle;
};

bool IsNSSErrorCode(PRErrorCode code);
nsresult GetXPCOMFromNSSError(PRErrorCode code);

} // psm
} // mozilla

#define NS_NSSERRORSSERVICE_CID \
  { 0x9ef18451, 0xa157, 0x4d17, { 0x81, 0x32, 0x47, 0xaf, 0xef, 0x21, 0x36, 0x89 } }

#endif // NSSErrorsService_h
