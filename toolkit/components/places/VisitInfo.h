/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_places_VisitInfo_h__
#define mozilla_places_VisitInfo_h__

#include "mozIAsyncHistory.h"
#include "nsAutoPtr.h"
#include "mozilla/Attributes.h"

class nsIURI;

namespace mozilla {
namespace places {

class VisitInfo MOZ_FINAL : public mozIVisitInfo
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_MOZIVISITINFO

  VisitInfo(int64_t aVisitId, PRTime aVisitDate, uint32_t aTransitionType,
            already_AddRefed<nsIURI> aReferrer);

private:
  const int64_t mVisitId;
  const PRTime mVisitDate;
  const uint32_t mTransitionType;
  nsCOMPtr<nsIURI> mReferrer;
};

} // namespace places
} // namespace mozilla

#endif // mozilla_places_VisitInfo_h__
