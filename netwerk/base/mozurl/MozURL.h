/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozURL_h__
#define mozURL_h__

#include "mozilla/net/MozURL_ffi.h"
#include "mozilla/RefPtr.h"
#include "mozilla/Result.h"

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
//
// Implementor Note: This type is only a holder for methods in C++, and does not
// reflect the actual layout of the type.
class MozURL final {
 public:
  static nsresult Init(MozURL** aURL, const nsACString& aSpec,
                       const MozURL* aBaseURL = nullptr) {
    return mozurl_new(aURL, &aSpec, aBaseURL);
  }

  nsDependentCSubstring Spec() const { return mozurl_spec(this); }
  nsDependentCSubstring Scheme() const { return mozurl_scheme(this); }
  nsDependentCSubstring Username() const { return mozurl_username(this); }
  nsDependentCSubstring Password() const { return mozurl_password(this); }
  // Will return the hostname of URL. If the hostname is an IPv6 address,
  // it will be enclosed in square brackets, such as `[::1]`
  nsDependentCSubstring Host() const { return mozurl_host(this); }
  // Will return the port number, if specified, or -1
  int32_t Port() const { return mozurl_port(this); }
  int32_t RealPort() const { return mozurl_real_port(this); }
  // If the URL's port number is equal to the default port, will only return the
  // hostname, otherwise it will return a string of the form `{host}:{port}`
  // See: https://url.spec.whatwg.org/#default-port
  nsDependentCSubstring HostPort() const { return mozurl_host_port(this); }
  nsDependentCSubstring FilePath() const { return mozurl_filepath(this); }
  nsDependentCSubstring Path() const { return mozurl_path(this); }
  nsDependentCSubstring Query() const { return mozurl_query(this); }
  bool HasQuery() const { return mozurl_has_query(this); }
  nsDependentCSubstring Ref() const { return mozurl_fragment(this); }
  bool HasFragment() const { return mozurl_has_fragment(this); }
  nsDependentCSubstring Directory() const { return mozurl_directory(this); }
  nsDependentCSubstring PrePath() const { return mozurl_prepath(this); }
  nsDependentCSubstring SpecNoRef() const { return mozurl_spec_no_ref(this); }

  // This matches the definition of origins and base domains in nsIPrincipal for
  // almost all URIs (some rare file:// URIs don't match and it would be hard to
  // fix them). It definitely matches nsIPrincipal for URIs used in quota
  // manager and there are checks in quota manager and its clients that prevent
  // different definitions (see QuotaManager::IsPrincipalInfoValid).
  // See also TestMozURL.cpp which enumerates a huge pile of URIs and checks
  // that origin and base domain definitions are in sync.
  void Origin(nsACString& aOrigin) const { mozurl_origin(this, &aOrigin); }
  nsresult BaseDomain(nsACString& aBaseDomain) const {
    return mozurl_base_domain(this, &aBaseDomain);
  }
  bool CannotBeABase() { return mozurl_cannot_be_a_base(this); }

  nsresult GetCommonBase(const MozURL* aOther, MozURL** aCommon) const {
    return mozurl_common_base(this, aOther, aCommon);
  }
  nsresult GetRelative(const MozURL* aOther, nsACString* aRelative) const {
    return mozurl_relative(this, aOther, aRelative);
  }

  size_t SizeOf() { return mozurl_sizeof(this); }

  class Mutator {
   public:
    // Calling this method will result in the creation of a new MozURL that
    // adopts the mutator's mURL.
    // If any of the setters failed with an error code, that error code will be
    // returned here. It will also return an error code if Finalize is called
    // more than once on the Mutator.
    nsresult Finalize(MozURL** aURL) {
      nsresult rv = GetStatus();
      if (NS_SUCCEEDED(rv)) {
        mURL.forget(aURL);
      } else {
        *aURL = nullptr;
      }
      return rv;
    }

    // These setter methods will return a reference to `this` so that you may
    // chain setter operations as such:
    //
    // RefPtr<MozURL> url2;
    // nsresult rv = url->Mutate().SetHostname("newhost"_ns)
    //                            .SetFilePath("new/file/path"_ns)
    //                            .Finalize(getter_AddRefs(url2));
    // if (NS_SUCCEEDED(rv)) { /* use url2 */ }
    Mutator& SetScheme(const nsACString& aScheme) {
      if (NS_SUCCEEDED(GetStatus())) {
        mStatus = mozurl_set_scheme(mURL, &aScheme);
      }
      return *this;
    }
    Mutator& SetUsername(const nsACString& aUser) {
      if (NS_SUCCEEDED(GetStatus())) {
        mStatus = mozurl_set_username(mURL, &aUser);
      }
      return *this;
    }
    Mutator& SetPassword(const nsACString& aPassword) {
      if (NS_SUCCEEDED(GetStatus())) {
        mStatus = mozurl_set_password(mURL, &aPassword);
      }
      return *this;
    }
    Mutator& SetHostname(const nsACString& aHost) {
      if (NS_SUCCEEDED(GetStatus())) {
        mStatus = mozurl_set_hostname(mURL, &aHost);
      }
      return *this;
    }
    Mutator& SetHostPort(const nsACString& aHostPort) {
      if (NS_SUCCEEDED(GetStatus())) {
        mStatus = mozurl_set_host_port(mURL, &aHostPort);
      }
      return *this;
    }
    Mutator& SetFilePath(const nsACString& aPath) {
      if (NS_SUCCEEDED(GetStatus())) {
        mStatus = mozurl_set_pathname(mURL, &aPath);
      }
      return *this;
    }
    Mutator& SetQuery(const nsACString& aQuery) {
      if (NS_SUCCEEDED(GetStatus())) {
        mStatus = mozurl_set_query(mURL, &aQuery);
      }
      return *this;
    }
    Mutator& SetRef(const nsACString& aRef) {
      if (NS_SUCCEEDED(GetStatus())) {
        mStatus = mozurl_set_fragment(mURL, &aRef);
      }
      return *this;
    }
    Mutator& SetPort(int32_t aPort) {
      if (NS_SUCCEEDED(GetStatus())) {
        mStatus = mozurl_set_port_no(mURL, aPort);
      }
      return *this;
    }

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
    nsresult GetStatus() { return mURL ? mStatus : NS_ERROR_NOT_AVAILABLE; }

    static Result<Mutator, nsresult> FromSpec(
        const nsACString& aSpec, const MozURL* aBaseURL = nullptr) {
      Mutator m = Mutator(aSpec, aBaseURL);
      if (m.mURL) {
        MOZ_ASSERT(NS_SUCCEEDED(m.mStatus));
        return m;
      }

      MOZ_ASSERT(NS_FAILED(m.mStatus));
      return Err(m.mStatus);
    }

   private:
    explicit Mutator(MozURL* aUrl) : mStatus(NS_OK) {
      mozurl_clone(aUrl, getter_AddRefs(mURL));
    }

    // This is only used to create a mutator from a string without cloning it
    // so we avoid a pointless copy in FromSpec. It is important that we
    // check the value of mURL afterwards.
    explicit Mutator(const nsACString& aSpec,
                     const MozURL* aBaseURL = nullptr) {
      mStatus = mozurl_new(getter_AddRefs(mURL), &aSpec, aBaseURL);
    }
    RefPtr<MozURL> mURL;
    nsresult mStatus;
    friend class MozURL;
  };

  Mutator Mutate() { return Mutator(this); }

  // AddRef and Release are non-virtual on this type, and always call into rust.
  void AddRef() { mozurl_addref(this); }
  void Release() { mozurl_release(this); }

 private:
  // Make it a compile time error for C++ code to ever create, destruct, or copy
  // MozURL objects. All of these operations will be performed by rust.
  MozURL();  /* never defined */
  ~MozURL(); /* never defined */
  MozURL(const MozURL&) = delete;
  MozURL& operator=(const MozURL&) = delete;
};

}  // namespace net
}  // namespace mozilla

#endif  // mozURL_h__
