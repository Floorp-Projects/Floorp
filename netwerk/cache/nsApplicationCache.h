/* vim:set ts=2 sw=2 sts=2 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

class nsApplicationCache : public nsIApplicationCache
                         , public nsSupportsWeakReference
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIAPPLICATIONCACHE

  nsApplicationCache(nsOfflineCacheDevice *device,
                     const nsACString &group,
                     const nsACString &clientID);

  nsApplicationCache();

  void MarkInvalid();

private:
  virtual ~nsApplicationCache();

  RefPtr<nsOfflineCacheDevice> mDevice;
  nsCString mGroup;
  nsCString mClientID;
  bool mValid;
};

