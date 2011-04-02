/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is VisitInfo object.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Shawn Wilsher <me@shawnwilsher.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "VisitInfo.h"
#include "nsIURI.h"

namespace mozilla {
namespace places {

////////////////////////////////////////////////////////////////////////////////
//// VisitInfo

VisitInfo::VisitInfo(PRInt64 aVisitId,
                     PRTime aVisitDate,
                     PRUint32 aTransitionType,
                     already_AddRefed<nsIURI> aReferrer,
                     PRInt64 aSessionId)
: mVisitId(aVisitId)
, mVisitDate(aVisitDate)
, mTransitionType(aTransitionType)
, mReferrer(aReferrer)
, mSessionId(aSessionId)
{
}

////////////////////////////////////////////////////////////////////////////////
//// mozIVisitInfo

NS_IMETHODIMP
VisitInfo::GetVisitId(PRInt64* _visitId)
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
VisitInfo::GetTransitionType(PRUint32* _transitionType)
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

NS_IMETHODIMP
VisitInfo::GetSessionId(PRInt64* _sessionId)
{
  *_sessionId = mSessionId;
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
//// nsISupports

NS_IMPL_ISUPPORTS1(
  VisitInfo
, mozIVisitInfo
)

} // namespace places
} // namespace mozilla
