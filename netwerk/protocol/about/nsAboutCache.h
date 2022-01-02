/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsAboutCache_h__
#define nsAboutCache_h__

#include "nsIAboutModule.h"
#include "nsICacheStorageVisitor.h"
#include "nsICacheStorage.h"

#include "nsString.h"
#include "nsIChannel.h"
#include "nsIOutputStream.h"
#include "nsILoadContextInfo.h"

#include "nsCOMPtr.h"
#include "nsTArray.h"

#define NS_FORWARD_SAFE_NSICHANNEL_SUBSET(_to)                                 \
  NS_IMETHOD GetOriginalURI(nsIURI** aOriginalURI) override {                  \
    return !(_to) ? NS_ERROR_NULL_POINTER                                      \
                  : (_to)->GetOriginalURI(aOriginalURI);                       \
  }                                                                            \
  NS_IMETHOD SetOriginalURI(nsIURI* aOriginalURI) override {                   \
    return !(_to) ? NS_ERROR_NULL_POINTER                                      \
                  : (_to)->SetOriginalURI(aOriginalURI);                       \
  }                                                                            \
  NS_IMETHOD GetURI(nsIURI** aURI) override {                                  \
    return !(_to) ? NS_ERROR_NULL_POINTER : (_to)->GetURI(aURI);               \
  }                                                                            \
  NS_IMETHOD GetOwner(nsISupports** aOwner) override {                         \
    return !(_to) ? NS_ERROR_NULL_POINTER : (_to)->GetOwner(aOwner);           \
  }                                                                            \
  NS_IMETHOD SetOwner(nsISupports* aOwner) override {                          \
    return !(_to) ? NS_ERROR_NULL_POINTER : (_to)->SetOwner(aOwner);           \
  }                                                                            \
  NS_IMETHOD GetNotificationCallbacks(                                         \
      nsIInterfaceRequestor** aNotificationCallbacks) override {               \
    return !(_to) ? NS_ERROR_NULL_POINTER                                      \
                  : (_to)->GetNotificationCallbacks(aNotificationCallbacks);   \
  }                                                                            \
  NS_IMETHOD SetNotificationCallbacks(                                         \
      nsIInterfaceRequestor* aNotificationCallbacks) override {                \
    return !(_to) ? NS_ERROR_NULL_POINTER                                      \
                  : (_to)->SetNotificationCallbacks(aNotificationCallbacks);   \
  }                                                                            \
  NS_IMETHOD GetSecurityInfo(nsISupports** aSecurityInfo) override {           \
    return !(_to) ? NS_ERROR_NULL_POINTER                                      \
                  : (_to)->GetSecurityInfo(aSecurityInfo);                     \
  }                                                                            \
  NS_IMETHOD GetContentType(nsACString& aContentType) override {               \
    return !(_to) ? NS_ERROR_NULL_POINTER                                      \
                  : (_to)->GetContentType(aContentType);                       \
  }                                                                            \
  NS_IMETHOD SetContentType(const nsACString& aContentType) override {         \
    return !(_to) ? NS_ERROR_NULL_POINTER                                      \
                  : (_to)->SetContentType(aContentType);                       \
  }                                                                            \
  NS_IMETHOD GetContentCharset(nsACString& aContentCharset) override {         \
    return !(_to) ? NS_ERROR_NULL_POINTER                                      \
                  : (_to)->GetContentCharset(aContentCharset);                 \
  }                                                                            \
  NS_IMETHOD SetContentCharset(const nsACString& aContentCharset) override {   \
    return !(_to) ? NS_ERROR_NULL_POINTER                                      \
                  : (_to)->SetContentCharset(aContentCharset);                 \
  }                                                                            \
  NS_IMETHOD GetContentLength(int64_t* aContentLength) override {              \
    return !(_to) ? NS_ERROR_NULL_POINTER                                      \
                  : (_to)->GetContentLength(aContentLength);                   \
  }                                                                            \
  NS_IMETHOD SetContentLength(int64_t aContentLength) override {               \
    return !(_to) ? NS_ERROR_NULL_POINTER                                      \
                  : (_to)->SetContentLength(aContentLength);                   \
  }                                                                            \
  NS_IMETHOD GetContentDisposition(uint32_t* aContentDisposition) override {   \
    return !(_to) ? NS_ERROR_NULL_POINTER                                      \
                  : (_to)->GetContentDisposition(aContentDisposition);         \
  }                                                                            \
  NS_IMETHOD SetContentDisposition(uint32_t aContentDisposition) override {    \
    return !(_to) ? NS_ERROR_NULL_POINTER                                      \
                  : (_to)->SetContentDisposition(aContentDisposition);         \
  }                                                                            \
  NS_IMETHOD GetContentDispositionFilename(                                    \
      nsAString& aContentDispositionFilename) override {                       \
    return !(_to) ? NS_ERROR_NULL_POINTER                                      \
                  : (_to)->GetContentDispositionFilename(                      \
                        aContentDispositionFilename);                          \
  }                                                                            \
  NS_IMETHOD SetContentDispositionFilename(                                    \
      const nsAString& aContentDispositionFilename) override {                 \
    return !(_to) ? NS_ERROR_NULL_POINTER                                      \
                  : (_to)->SetContentDispositionFilename(                      \
                        aContentDispositionFilename);                          \
  }                                                                            \
  NS_IMETHOD GetContentDispositionHeader(                                      \
      nsACString& aContentDispositionHeader) override {                        \
    return !(_to) ? NS_ERROR_NULL_POINTER                                      \
                  : (_to)->GetContentDispositionHeader(                        \
                        aContentDispositionHeader);                            \
  }                                                                            \
  NS_IMETHOD GetLoadInfo(nsILoadInfo** aLoadInfo) override {                   \
    return !(_to) ? NS_ERROR_NULL_POINTER : (_to)->GetLoadInfo(aLoadInfo);     \
  }                                                                            \
  NS_IMETHOD SetLoadInfo(nsILoadInfo* aLoadInfo) override {                    \
    return !(_to) ? NS_ERROR_NULL_POINTER : (_to)->SetLoadInfo(aLoadInfo);     \
  }                                                                            \
  NS_IMETHOD GetIsDocument(bool* aIsDocument) override {                       \
    return !(_to) ? NS_ERROR_NULL_POINTER : (_to)->GetIsDocument(aIsDocument); \
  }                                                                            \
  NS_IMETHOD GetCanceled(bool* aCanceled) override {                           \
    return !(_to) ? NS_ERROR_NULL_POINTER : (_to)->GetCanceled(aCanceled);     \
  };

class nsAboutCache final : public nsIAboutModule {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIABOUTMODULE

  nsAboutCache() = default;

  [[nodiscard]] static nsresult Create(nsISupports* aOuter, REFNSIID aIID,
                                       void** aResult);

  [[nodiscard]] static nsresult GetStorage(nsACString const& storageName,
                                           nsILoadContextInfo* loadInfo,
                                           nsICacheStorage** storage);

 protected:
  virtual ~nsAboutCache() = default;

  class Channel final : public nsIChannel, public nsICacheStorageVisitor {
    NS_DECL_ISUPPORTS
    NS_DECL_NSICACHESTORAGEVISITOR
    NS_FORWARD_SAFE_NSIREQUEST(mChannel)
    NS_FORWARD_SAFE_NSICHANNEL_SUBSET(mChannel)
    NS_IMETHOD AsyncOpen(nsIStreamListener* aListener) override;
    NS_IMETHOD Open(nsIInputStream** _retval) override;

   private:
    virtual ~Channel() = default;

   public:
    [[nodiscard]] nsresult Init(nsIURI* aURI, nsILoadInfo* aLoadInfo);
    [[nodiscard]] nsresult ParseURI(nsIURI* uri, nsACString& storage);

    // Finds a next storage we wish to visit (we use this method
    // even there is a specified storage name, which is the only
    // one in the list then.)  Posts FireVisitStorage() when found.
    [[nodiscard]] nsresult VisitNextStorage();
    // Helper method that calls VisitStorage() for the current storage.
    // When it fails, OnCacheEntryVisitCompleted is simulated to close
    // the output stream and thus the about:cache channel.
    void FireVisitStorage();
    // Kiks the visit cycle for the given storage, names can be:
    // "disk", "memory", "appcache"
    // Note: any newly added storage type has to be manually handled here.
    [[nodiscard]] nsresult VisitStorage(nsACString const& storageName);

    // Writes content of mBuffer to mStream and truncates
    // the buffer.  It may fail when the input stream is closed by canceling
    // the input stream channel.  It can be used to stop the cache iteration
    // process.
    [[nodiscard]] nsresult FlushBuffer();

    // Whether we are showing overview status of all available
    // storages.
    bool mOverview = false;

    // Flag initially false, that indicates the entries header has
    // been added to the output HTML.
    bool mEntriesHeaderAdded = false;

    // Cancelation flag
    bool mCancel = false;

    // The list of all storage names we want to visit
    nsTArray<nsCString> mStorageList;
    nsCString mStorageName;
    nsCOMPtr<nsICacheStorage> mStorage;

    // Output data buffering and streaming output
    nsCString mBuffer;
    nsCOMPtr<nsIOutputStream> mStream;

    // The input stream channel, the one that actually does the job
    nsCOMPtr<nsIChannel> mChannel;
  };
};

#define NS_ABOUT_CACHE_MODULE_CID                    \
  { /* 9158c470-86e4-11d4-9be2-00e09872a416 */       \
    0x9158c470, 0x86e4, 0x11d4, {                    \
      0x9b, 0xe2, 0x00, 0xe0, 0x98, 0x72, 0xa4, 0x16 \
    }                                                \
  }

#endif  // nsAboutCache_h__
