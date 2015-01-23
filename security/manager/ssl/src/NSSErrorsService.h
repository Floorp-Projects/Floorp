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

class NSSErrorsService MOZ_FINAL : public nsINSSErrorsService
{
  NS_DECL_ISUPPORTS
  NS_DECL_NSINSSERRORSSERVICE

public:
  nsresult Init();

private:
  // For XPCOM implementations that are not a base class for some other
  // class, it is good practice to make the destructor non-virtual and
  // private.  Then the only way to delete the object is via Release.
#ifdef _MSC_VER
  // C4265: Class has virtual members but destructor is not virtual
  __pragma(warning(disable:4265))
#endif
  ~NSSErrorsService();

  nsCOMPtr<nsIStringBundle> mPIPNSSBundle;
  nsCOMPtr<nsIStringBundle> mNSSErrorsBundle;
};

bool IsNSSErrorCode(PRErrorCode code);
nsresult GetXPCOMFromNSSError(PRErrorCode code);
bool ErrorIsOverridable(PRErrorCode code);

} // psm
} // mozilla

#define NS_NSSERRORSSERVICE_CID \
  { 0x9ef18451, 0xa157, 0x4d17, { 0x81, 0x32, 0x47, 0xaf, 0xef, 0x21, 0x36, 0x89 } }

#endif // NSSErrorsService_h
