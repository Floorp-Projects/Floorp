/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_places_PlaceInfo_h__
#define mozilla_places_PlaceInfo_h__

#include "mozIAsyncHistory.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nsAutoPtr.h"
#include "mozilla/Attributes.h"

class nsIURI;
class mozIVisitInfo;

namespace mozilla {
namespace places {


class PlaceInfo final : public mozIPlaceInfo
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_MOZIPLACEINFO

  typedef nsTArray< nsCOMPtr<mozIVisitInfo> > VisitsArray;

  PlaceInfo(int64_t aId, const nsCString& aGUID, already_AddRefed<nsIURI> aURI,
            const nsString& aTitle, int64_t aFrecency);
  PlaceInfo(int64_t aId, const nsCString& aGUID, already_AddRefed<nsIURI> aURI,
            const nsString& aTitle, int64_t aFrecency,
            const VisitsArray& aVisits);

private:
  ~PlaceInfo() {}

  const int64_t mId;
  const nsCString mGUID;
  nsCOMPtr<nsIURI> mURI;
  const nsString mTitle;
  const int64_t mFrecency;
  const VisitsArray mVisits;
  bool mVisitsAvailable;
};

} // namespace places
} // namespace mozilla

#endif // mozilla_places_PlaceInfo_h__
