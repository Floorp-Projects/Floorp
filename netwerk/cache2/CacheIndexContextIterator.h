/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CacheIndexContextIterator__h__
#define CacheIndexContextIterator__h__

#include "CacheIndexIterator.h"

class nsILoadContextInfo;

namespace mozilla {
namespace net {

class CacheIndexContextIterator : public CacheIndexIterator
{
public:
  CacheIndexContextIterator(CacheIndex *aIndex, bool aAddNew,
                            nsILoadContextInfo *aInfo);
  virtual ~CacheIndexContextIterator();

private:
  virtual void AddRecord(CacheIndexRecord *aRecord);
  virtual void AddRecords(const nsTArray<CacheIndexRecord *> &aRecords);

  nsCOMPtr<nsILoadContextInfo> mInfo;
};

} // namespace net
} // namespace mozilla

#endif
