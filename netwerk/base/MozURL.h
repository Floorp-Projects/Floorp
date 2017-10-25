/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozURL_h__
#define mozURL_h__

#include "rust-url-capi/src/rust-url-capi.h"
#include "mozilla/UniquePtr.h"

namespace mozilla {
namespace net {

// This class provides a thread-safe, immutable URL parser.
// As long as there is RefPtr to the object, you may use it on any thread.
// The constructor is private. One can instantiate the object by
// calling the Init() method as such:
//
// RefPtr<MozURL> url;
// nsAutoCString href("http://example.com/path?query#ref");
// nsresult rv = MozURL::Init(getter_AddRefs(url), href);
// if (NS_SUCCEEDED(rv)) { /* use url */ }
//
// When changing the URL is needed, you need to call the Mutate() method.
// This gives you a Mutator object, on which you can perform setter operations.
// Calling Finalize() on the Mutator will result in a new MozURL and a status
// code. If any of the setter operations failed, it will be reflected in the
// status code, and a null MozURL.
//
// Note: In the case of a domain name containing non-ascii characters,
// GetSpec and GetHostname will return the IDNA(punycode) version of the host.
// Also note that for now, MozURL only supports the UTF-8 charset.
class MozURL final
{
public:
  static nsresult Init(MozURL** aURL, const nsACString& aSpec,
                       const MozURL* aBaseURL = nullptr);

  nsresult GetScheme(nsACString& aScheme);
  nsresult GetSpec(nsACString& aSpec);
  nsresult GetUsername(nsACString& aUser);
  nsresult GetPassword(nsACString& aPassword);
  // Will return the hostname of URL. If the hostname is an IPv6 address,
  // it will be enclosed in square brackets, such as `[::1]`
  nsresult GetHostname(nsACString& aHost);
  // If the URL's port number is equal to the default port, will only return the
  // hostname, otherwise it will return a string of the form `{host}:{port}`
  // See: https://url.spec.whatwg.org/#default-port
  nsresult GetHostPort(nsACString& aHostPort);
  // Will return the port number, if specified, or -1
  nsresult GetPort(int32_t* aPort);
  nsresult GetFilePath(nsACString& aPath);
  nsresult GetQuery(nsACString& aQuery);
  nsresult GetRef(nsACString& aRef);

private:
  explicit MozURL(rusturl* rawPtr)
    : mURL(rawPtr)
  {
  }
  virtual ~MozURL() {}
  struct FreeRustURL
  {
    void operator()(rusturl* aPtr) { rusturl_free(aPtr); }
  };
  mozilla::UniquePtr<rusturl, FreeRustURL> mURL;

public:
  class MOZ_STACK_CLASS Mutator
  {
  public:
    // Calling this method will result in the creation of a new MozURL that
    // adopts the mutator's mURL.
    // If any of the setters failed with an error code, that error code will be
    // returned here. It will also return an error code if Finalize is called
    // more than once on the Mutator.
    nsresult Finalize(MozURL** aURL);

    // These setter methods will return a reference to `this` so that you may
    // chain setter operations as such:
    //
    // RefPtr<MozURL> url2;
    // nsresult rv = url->Mutate().SetHostname(NS_LITERAL_CSTRING("newhost"))
    //                            .SetFilePath(NS_LITERAL_CSTRING("new/file/path"))
    //                            .Finalize(getter_AddRefs(url2));
    // if (NS_SUCCEEDED(rv)) { /* use url2 */ }
    Mutator& SetScheme(const nsACString& aScheme);
    Mutator& SetUsername(const nsACString& aUser);
    Mutator& SetPassword(const nsACString& aPassword);
    Mutator& SetHostname(const nsACString& aHost);
    Mutator& SetHostPort(const nsACString& aHostPort);
    Mutator& SetFilePath(const nsACString& aPath);
    Mutator& SetQuery(const nsACString& aQuery);
    Mutator& SetRef(const nsACString& aRef);
    Mutator& SetPort(int32_t aPort);

    // This method returns the status code of the setter operations.
    // If any of the setters failed, it will return the code of the first error
    // that occured. If none of the setters failed, it will return NS_OK.
    // This method is useful to avoid doing expensive operations when the result
    // would not be used because an error occurred. For example:
    //
    // RefPtr<MozURL> url2;
    // MozURL::Mutator mut = url->Mutate();
    // mut.SetScheme("!@#$"); // this would fail
    // if (NS_SUCCEDED(mut.GetStatus())) {
    //   nsAutoCString host(ExpensiveComputing());
    //   rv = mut.SetHostname(host).Finalize(getter_AddRefs(url2));
    // }
    // if (NS_SUCCEEDED(rv)) { /* use url2 */ }
    nsresult GetStatus() { return mStatus; }
  private:
    explicit Mutator(MozURL* url)
      : mURL(rusturl_clone(url->mURL.get()))
      , mFinalized(false)
      , mStatus(NS_OK)
    {
    }
    mozilla::UniquePtr<rusturl, FreeRustURL> mURL;
    bool mFinalized;
    nsresult mStatus;
    friend class MozURL;
  };

  Mutator Mutate() { return Mutator(this); }

// These are used to avoid inheriting from nsISupports
public:
  NS_IMETHOD_(MozExternalRefCountType) AddRef(void);
  NS_IMETHOD_(MozExternalRefCountType) Release(void);
  typedef mozilla::TrueType HasThreadSafeRefCnt;
protected:
  ::mozilla::ThreadSafeAutoRefCnt mRefCnt;
  NS_DECL_OWNINGTHREAD
};

} // namespace net
} // namespace mozilla

#endif // mozURL_h__
