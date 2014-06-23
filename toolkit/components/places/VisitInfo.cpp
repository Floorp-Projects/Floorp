/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VisitInfo.h"
#include "nsIURI.h"

namespace mozilla {
namespace places {

////////////////////////////////////////////////////////////////////////////////
//// VisitInfo

VisitInfo::VisitInfo(int64_t aVisitId,
                     PRTime aVisitDate,
                     uint32_t aTransitionType,
                     already_AddRefed<nsIURI> aReferrer)
: mVisitId(aVisitId)
, mVisitDate(aVisitDate)
, mTransitionType(aTransitionType)
, mReferrer(aReferrer)
{
}

VisitInfo::~VisitInfo()
{
}

////////////////////////////////////////////////////////////////////////////////
//// mozIVisitInfo

NS_IMETHODIMP
VisitInfo::GetVisitId(int64_t* _visitId)
{
  *_visitId = mVisitId;
  return NS_OK;
}

NS_IMETHODIMP
VisitInfo::GetVisitDate(PRTime* _visitDate)
{
  *_visitDate = mVisitDate;
  return NS_OK;
}

NS_IMETHODIMP
VisitInfo::GetTransitionType(uint32_t* _transitionType)
{
  *_transitionType = mTransitionType;
  return NS_OK;
}

NS_IMETHODIMP
VisitInfo::GetReferrerURI(nsIURI** _referrer)
{
  NS_IF_ADDREF(*_referrer = mReferrer);
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
//// nsISupports

NS_IMPL_ISUPPORTS(
  VisitInfo
, mozIVisitInfo
)

} // namespace places
} // namespace mozilla
