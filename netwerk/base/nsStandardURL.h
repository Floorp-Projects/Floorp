/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsStandardURL_h__
#define nsStandardURL_h__

#include <bitset>

#include "nsString.h"
#include "nsISerializable.h"
#include "nsIFileURL.h"
#include "nsIStandardURL.h"
#include "mozilla/Encoding.h"
#include "nsCOMPtr.h"
#include "nsURLHelper.h"
#include "nsISizeOf.h"
#include "mozilla/Attributes.h"
#include "mozilla/LinkedList.h"
#include "mozilla/MemoryReporting.h"
#include "nsISensitiveInfoHiddenURI.h"
#include "nsIURIMutator.h"

#ifdef NS_BUILD_REFCNT_LOGGING
#  define DEBUG_DUMP_URLS_AT_SHUTDOWN
#endif

class nsIBinaryInputStream;
class nsIBinaryOutputStream;
class nsIIDNService;
class nsIPrefBranch;
class nsIFile;
class nsIURLParser;

namespace mozilla {
class Encoding;
namespace net {

template <typename T>
class URLSegmentNumber {
  T mData{0};
  bool mParity{false};

 public:
  URLSegmentNumber() = default;
  explicit URLSegmentNumber(T data) : mData(data) {
    mParity = CalculateParity();
  }
  bool operator==(URLSegmentNumber value) const { return mData == value.mData; }
  bool operator!=(URLSegmentNumber value) const { return mData != value.mData; }
  bool operator>(URLSegmentNumber value) const { return mData > value.mData; }
  URLSegmentNumber operator+(int32_t value) const {
    return URLSegmentNumber(mData + value);
  }
  URLSegmentNumber operator+(uint32_t value) const {
    return URLSegmentNumber(mData + value);
  }
  URLSegmentNumber operator-(int32_t value) const {
    return URLSegmentNumber(mData - value);
  }
  URLSegmentNumber operator-(uint32_t value) const {
    return URLSegmentNumber(mData - value);
  }
  URLSegmentNumber operator+=(URLSegmentNumber value) {
    mData += value.mData;
    mParity = CalculateParity();
    return *this;
  }
  URLSegmentNumber operator+=(T value) {
    mData += value;
    mParity = CalculateParity();
    return *this;
  }
  URLSegmentNumber operator-=(URLSegmentNumber value) {
    mData -= value.mData;
    mParity = CalculateParity();
    return *this;
  }
  URLSegmentNumber operator-=(T value) {
    mData -= value;
    mParity = CalculateParity();
    return *this;
  }
  operator T() const { return mData; }
  URLSegmentNumber& operator=(T value) {
    mData = value;
    mParity = CalculateParity();
    return *this;
  }
  URLSegmentNumber& operator++() {
    ++mData;
    mParity = CalculateParity();
    return *this;
  }
  URLSegmentNumber operator++(int) {
    URLSegmentNumber value = *this;
    *this += 1;
    return value;
  }
  bool CalculateParity() const {
    std::bitset<32> bits((uint32_t)mData);
    return bits.count() % 2 == 0 ? false : true;
  }
  bool Parity() const { return mParity; }
};

//-----------------------------------------------------------------------------
// standard URL implementation
//-----------------------------------------------------------------------------

class nsStandardURL : public nsIFileURL,
                      public nsIStandardURL,
                      public nsISerializable,
                      public nsISizeOf,
                      public nsISensitiveInfoHiddenURI
#ifdef DEBUG_DUMP_URLS_AT_SHUTDOWN
    ,
                      public LinkedListElement<nsStandardURL>
#endif
{
 protected:
  virtual ~nsStandardURL();
  explicit nsStandardURL(bool aSupportsFileURL = false, bool aTrackURL = true);

 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIURI
  NS_DECL_NSIURL
  NS_DECL_NSIFILEURL
  NS_DECL_NSISTANDARDURL
  NS_DECL_NSISERIALIZABLE
  NS_DECL_NSISENSITIVEINFOHIDDENURI

  // nsISizeOf
  virtual size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const override;
  virtual size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const override;

  static void InitGlobalObjects();
  static void ShutdownGlobalObjects();

 public: /* internal -- HPUX compiler can't handle this being private */
  //
  // location and length of an url segment relative to mSpec
  //
  struct URLSegment {
#ifdef EARLY_BETA_OR_EARLIER
    URLSegmentNumber<uint32_t> mPos{0};
    URLSegmentNumber<int32_t> mLen{-1};
#else
    uint32_t mPos{0};
    int32_t mLen{-1};
#endif

    URLSegment() = default;
    URLSegment(uint32_t pos, int32_t len) : mPos(pos), mLen(len) {}
    URLSegment(const URLSegment& aCopy) = default;
    void Reset() {
      mPos = 0;
      mLen = -1;
    }
    // Merge another segment following this one to it if they're contiguous
    // Assumes we have something like "foo;bar" where this object is 'foo' and
    // right is 'bar'.
    void Merge(const nsCString& spec, const char separator,
               const URLSegment& right) {
      if (mLen >= 0 && *(spec.get() + mPos + mLen) == separator &&
          mPos + mLen + 1 == right.mPos) {
        mLen += 1 + right.mLen;
      }
    }
  };

  //
  // URL segment encoder : performs charset conversion and URL escaping.
  //
  class nsSegmentEncoder {
   public:
    explicit nsSegmentEncoder(const Encoding* encoding = nullptr);

    // Encode the given segment if necessary, and return the length of
    // the encoded segment.  The encoded segment is appended to |aOut|
    // if and only if encoding is required.
    int32_t EncodeSegmentCount(const char* str, const URLSegment& aSeg,
                               int16_t mask, nsCString& aOut, bool& appended,
                               uint32_t extraLen = 0);

    // Encode the given string if necessary, and return a reference to
    // the encoded string.  Returns a reference to |result| if encoding
    // is required.  Otherwise, a reference to |str| is returned.
    const nsACString& EncodeSegment(const nsACString& str, int16_t mask,
                                    nsCString& result);

   private:
    const Encoding* mEncoding;
  };
  friend class nsSegmentEncoder;

  static nsresult NormalizeIPv4(const nsACString& host, nsCString& result);

 protected:
  // enum used in a few places to specify how .ref attribute should be handled
  enum RefHandlingEnum { eIgnoreRef, eHonorRef, eReplaceRef };

  // Helper to share code between Equals and EqualsExceptRef
  // NOTE: *not* virtual, because no one needs to override this so far...
  nsresult EqualsInternal(nsIURI* unknownOther, RefHandlingEnum refHandlingMode,
                          bool* result);

  virtual nsStandardURL* StartClone();

  // Helper to share code between Clone methods.
  nsresult CloneInternal(RefHandlingEnum aRefHandlingMode,
                         const nsACString& aNewRef, nsIURI** aClone);
  // Helper method that copies member variables from the source StandardURL
  // if copyCached = true, it will also copy mFile and mDisplayHost
  nsresult CopyMembers(nsStandardURL* source, RefHandlingEnum mode,
                       const nsACString& newRef, bool copyCached = false);

  // Helper for subclass implementation of GetFile().  Subclasses that map
  // URIs to files in a special way should implement this method.  It should
  // ensure that our mFile is initialized, if it's possible.
  // returns NS_ERROR_NO_INTERFACE if the url does not map to a file
  virtual nsresult EnsureFile();

  virtual nsresult Clone(nsIURI** aURI);
  virtual nsresult SetSpecInternal(const nsACString& input);
  virtual nsresult SetScheme(const nsACString& input);
  virtual nsresult SetUserPass(const nsACString& input);
  virtual nsresult SetUsername(const nsACString& input);
  virtual nsresult SetPassword(const nsACString& input);
  virtual nsresult SetHostPort(const nsACString& aValue);
  virtual nsresult SetHost(const nsACString& input);
  virtual nsresult SetPort(int32_t port);
  virtual nsresult SetPathQueryRef(const nsACString& input);
  virtual nsresult SetRef(const nsACString& input);
  virtual nsresult SetFilePath(const nsACString& input);
  virtual nsresult SetQuery(const nsACString& input);
  virtual nsresult SetQueryWithEncoding(const nsACString& input,
                                        const Encoding* encoding);
  bool Deserialize(const mozilla::ipc::URIParams&);
  nsresult ReadPrivate(nsIObjectInputStream* stream);

 private:
  nsresult Init(uint32_t urlType, int32_t defaultPort, const nsACString& spec,
                const char* charset, nsIURI* baseURI);
  nsresult SetDefaultPort(int32_t aNewDefaultPort);
  nsresult SetFile(nsIFile* file);

  nsresult SetFileNameInternal(const nsACString& input);
  nsresult SetFileBaseNameInternal(const nsACString& input);
  nsresult SetFileExtensionInternal(const nsACString& input);

  int32_t Port() { return mPort == -1 ? mDefaultPort : mPort; }

  void ReplacePortInSpec(int32_t aNewPort);
  void Clear();
  void InvalidateCache(bool invalidateCachedFile = true);

  bool ValidIPv6orHostname(const char* host, uint32_t length);
  static bool IsValidOfBase(unsigned char c, const uint32_t base);
  nsresult NormalizeIDN(const nsCString& host, nsCString& result);
  nsresult CheckIfHostIsAscii();
  void CoalescePath(netCoalesceFlags coalesceFlag, char* path);

  uint32_t AppendSegmentToBuf(char*, uint32_t, const char*,
                              const URLSegment& input, URLSegment& output,
                              const nsCString* esc = nullptr,
                              bool useEsc = false, int32_t* diff = nullptr);
  uint32_t AppendToBuf(char*, uint32_t, const char*, uint32_t);

  nsresult BuildNormalizedSpec(const char* spec, const Encoding* encoding);
  nsresult SetSpecWithEncoding(const nsACString& input,
                               const Encoding* encoding);

  bool SegmentIs(const URLSegment& seg, const char* val,
                 bool ignoreCase = false);
  bool SegmentIs(const char* spec, const URLSegment& seg, const char* val,
                 bool ignoreCase = false);
  bool SegmentIs(const URLSegment& seg1, const char* val,
                 const URLSegment& seg2, bool ignoreCase = false);

  int32_t ReplaceSegment(uint32_t pos, uint32_t len, const char* val,
                         uint32_t valLen);
  int32_t ReplaceSegment(uint32_t pos, uint32_t len, const nsACString& val);

  nsresult ParseURL(const char* spec, int32_t specLen);
  nsresult ParsePath(const char* spec, uint32_t pathPos, int32_t pathLen = -1);

  char* AppendToSubstring(uint32_t pos, int32_t len, const char* tail);

  // dependent substring helpers
  nsDependentCSubstring Segment(uint32_t pos, int32_t len);  // see below
  nsDependentCSubstring Segment(const URLSegment& s) {
    return Segment(s.mPos, s.mLen);
  }

  // dependent substring getters
  nsDependentCSubstring Prepath();  // see below
  nsDependentCSubstring Scheme() { return Segment(mScheme); }
  nsDependentCSubstring Userpass(bool includeDelim = false);  // see below
  nsDependentCSubstring Username() { return Segment(mUsername); }
  nsDependentCSubstring Password() { return Segment(mPassword); }
  nsDependentCSubstring Hostport();  // see below
  nsDependentCSubstring Host();      // see below
  nsDependentCSubstring Path() { return Segment(mPath); }
  nsDependentCSubstring Filepath() { return Segment(mFilepath); }
  nsDependentCSubstring Directory() { return Segment(mDirectory); }
  nsDependentCSubstring Filename();  // see below
  nsDependentCSubstring Basename() { return Segment(mBasename); }
  nsDependentCSubstring Extension() { return Segment(mExtension); }
  nsDependentCSubstring Query() { return Segment(mQuery); }
  nsDependentCSubstring Ref() { return Segment(mRef); }

  // shift the URLSegments to the right by diff
  void ShiftFromAuthority(int32_t diff);
  void ShiftFromUsername(int32_t diff);
  void ShiftFromPassword(int32_t diff);
  void ShiftFromHost(int32_t diff);
  void ShiftFromPath(int32_t diff);
  void ShiftFromFilepath(int32_t diff);
  void ShiftFromDirectory(int32_t diff);
  void ShiftFromBasename(int32_t diff);
  void ShiftFromExtension(int32_t diff);
  void ShiftFromQuery(int32_t diff);
  void ShiftFromRef(int32_t diff);

  // fastload helper functions
  nsresult ReadSegment(nsIBinaryInputStream*, URLSegment&);
  nsresult WriteSegment(nsIBinaryOutputStream*, const URLSegment&);

  void FindHostLimit(nsACString::const_iterator& aStart,
                     nsACString::const_iterator& aEnd);

  // Asserts that the URL has sane values
  void SanityCheck();

  // Checks if the URL has a valid representation.
  bool IsValid();

  // mSpec contains the normalized version of the URL spec (UTF-8 encoded).
  nsCString mSpec;
  int32_t mDefaultPort{-1};
  int32_t mPort{-1};

  // url parts (relative to mSpec)
  URLSegment mScheme;
  URLSegment mAuthority;
  URLSegment mUsername;
  URLSegment mPassword;
  URLSegment mHost;
  URLSegment mPath;
  URLSegment mFilepath;
  URLSegment mDirectory;
  URLSegment mBasename;
  URLSegment mExtension;
  URLSegment mQuery;
  URLSegment mRef;

  nsCOMPtr<nsIURLParser> mParser;

  // mFile is protected so subclasses can access it directly
 protected:
  nsCOMPtr<nsIFile> mFile;  // cached result for nsIFileURL::GetFile

 private:
  // cached result for nsIURI::GetDisplayHost
  nsCString mDisplayHost;

  enum { eEncoding_Unknown, eEncoding_ASCII, eEncoding_UTF8 };

  uint32_t mURLType : 2;          // nsIStandardURL::URLTYPE_xxx
  uint32_t mSupportsFileURL : 1;  // QI to nsIFileURL?
  uint32_t mCheckedIfHostA : 1;   // If set to true, it means either that
                                  // mDisplayHost has a been initialized, or
                                  // that the hostname is not punycode

  // global objects.
  static StaticRefPtr<nsIIDNService> gIDN;
  static const char gHostLimitDigits[];

 public:
#ifdef DEBUG_DUMP_URLS_AT_SHUTDOWN
  void PrintSpec() const { printf("  %s\n", mSpec.get()); }
#endif

 public:
  // We make this implementation a template so that we can avoid writing
  // the same code for SubstitutingURL (which extends nsStandardURL)
  template <class T>
  class TemplatedMutator : public nsIURIMutator,
                           public BaseURIMutator<T>,
                           public nsIStandardURLMutator,
                           public nsIURLMutator,
                           public nsIFileURLMutator,
                           public nsISerializable {
    NS_FORWARD_SAFE_NSIURISETTERS_RET(BaseURIMutator<T>::mURI)

    [[nodiscard]] NS_IMETHOD Deserialize(
        const mozilla::ipc::URIParams& aParams) override {
      return BaseURIMutator<T>::InitFromIPCParams(aParams);
    }

    NS_IMETHOD
    Write(nsIObjectOutputStream* aOutputStream) override {
      MOZ_ASSERT_UNREACHABLE("Use nsIURIMutator.read() instead");
      return NS_ERROR_NOT_IMPLEMENTED;
    }

    [[nodiscard]] NS_IMETHOD Read(nsIObjectInputStream* aStream) override {
      return BaseURIMutator<T>::InitFromInputStream(aStream);
    }

    [[nodiscard]] NS_IMETHOD Finalize(nsIURI** aURI) override {
      BaseURIMutator<T>::mURI.forget(aURI);
      return NS_OK;
    }

    [[nodiscard]] NS_IMETHOD SetSpec(const nsACString& aSpec,
                                     nsIURIMutator** aMutator) override {
      if (aMutator) {
        nsCOMPtr<nsIURIMutator> mutator = this;
        mutator.forget(aMutator);
      }
      return BaseURIMutator<T>::InitFromSpec(aSpec);
    }

    [[nodiscard]] NS_IMETHOD Init(uint32_t aURLType, int32_t aDefaultPort,
                                  const nsACString& aSpec, const char* aCharset,
                                  nsIURI* aBaseURI,
                                  nsIURIMutator** aMutator) override {
      if (aMutator) {
        nsCOMPtr<nsIURIMutator> mutator = this;
        mutator.forget(aMutator);
      }
      RefPtr<T> uri;
      if (BaseURIMutator<T>::mURI) {
        // We don't need a new URI object if we already have one
        BaseURIMutator<T>::mURI.swap(uri);
      } else {
        uri = Create();
      }
      nsresult rv =
          uri->Init(aURLType, aDefaultPort, aSpec, aCharset, aBaseURI);
      if (NS_FAILED(rv)) {
        return rv;
      }
      BaseURIMutator<T>::mURI = std::move(uri);
      return NS_OK;
    }

    [[nodiscard]] NS_IMETHODIMP SetDefaultPort(
        int32_t aNewDefaultPort, nsIURIMutator** aMutator) override {
      if (!BaseURIMutator<T>::mURI) {
        return NS_ERROR_NULL_POINTER;
      }
      if (aMutator) {
        nsCOMPtr<nsIURIMutator> mutator = this;
        mutator.forget(aMutator);
      }
      return BaseURIMutator<T>::mURI->SetDefaultPort(aNewDefaultPort);
    }

    [[nodiscard]] NS_IMETHOD SetFileName(const nsACString& aFileName,
                                         nsIURIMutator** aMutator) override {
      if (!BaseURIMutator<T>::mURI) {
        return NS_ERROR_NULL_POINTER;
      }
      if (aMutator) {
        nsCOMPtr<nsIURIMutator> mutator = this;
        mutator.forget(aMutator);
      }
      return BaseURIMutator<T>::mURI->SetFileNameInternal(aFileName);
    }

    [[nodiscard]] NS_IMETHOD SetFileBaseName(
        const nsACString& aFileBaseName, nsIURIMutator** aMutator) override {
      if (!BaseURIMutator<T>::mURI) {
        return NS_ERROR_NULL_POINTER;
      }
      if (aMutator) {
        nsCOMPtr<nsIURIMutator> mutator = this;
        mutator.forget(aMutator);
      }
      return BaseURIMutator<T>::mURI->SetFileBaseNameInternal(aFileBaseName);
    }

    [[nodiscard]] NS_IMETHOD SetFileExtension(
        const nsACString& aFileExtension, nsIURIMutator** aMutator) override {
      if (!BaseURIMutator<T>::mURI) {
        return NS_ERROR_NULL_POINTER;
      }
      if (aMutator) {
        nsCOMPtr<nsIURIMutator> mutator = this;
        mutator.forget(aMutator);
      }
      return BaseURIMutator<T>::mURI->SetFileExtensionInternal(aFileExtension);
    }

    T* Create() override { return new T(mMarkedFileURL); }

    [[nodiscard]] NS_IMETHOD MarkFileURL() override {
      mMarkedFileURL = true;
      return NS_OK;
    }

    [[nodiscard]] NS_IMETHOD SetFile(nsIFile* aFile) override {
      RefPtr<T> uri;
      if (BaseURIMutator<T>::mURI) {
        // We don't need a new URI object if we already have one
        BaseURIMutator<T>::mURI.swap(uri);
      } else {
        uri = new T(/* aSupportsFileURL = */ true);
      }

      nsresult rv = uri->SetFile(aFile);
      if (NS_FAILED(rv)) {
        return rv;
      }
      BaseURIMutator<T>::mURI.swap(uri);
      return NS_OK;
    }

    explicit TemplatedMutator() = default;

   private:
    virtual ~TemplatedMutator() = default;

    bool mMarkedFileURL = false;

    friend T;
  };

  class Mutator final : public TemplatedMutator<nsStandardURL> {
    NS_DECL_ISUPPORTS
   public:
    explicit Mutator() = default;

   private:
    virtual ~Mutator() = default;
  };

  friend BaseURIMutator<nsStandardURL>;
};

#define NS_THIS_STANDARDURL_IMPL_CID                 \
  { /* b8e3e97b-1ccd-4b45-af5a-79596770f5d7 */       \
    0xb8e3e97b, 0x1ccd, 0x4b45, {                    \
      0xaf, 0x5a, 0x79, 0x59, 0x67, 0x70, 0xf5, 0xd7 \
    }                                                \
  }

//-----------------------------------------------------------------------------
// Dependent substring getters
//-----------------------------------------------------------------------------

inline nsDependentCSubstring nsStandardURL::Segment(uint32_t pos, int32_t len) {
  if (len < 0) {
    pos = 0;
    len = 0;
  }
  return Substring(mSpec, pos, uint32_t(len));
}

inline nsDependentCSubstring nsStandardURL::Prepath() {
  uint32_t len = 0;
  if (mAuthority.mLen >= 0) len = mAuthority.mPos + mAuthority.mLen;
  return Substring(mSpec, 0, len);
}

inline nsDependentCSubstring nsStandardURL::Userpass(bool includeDelim) {
  uint32_t pos = 0, len = 0;
  if (mUsername.mLen > 0 || mPassword.mLen > 0) {
    if (mUsername.mLen > 0) {
      pos = mUsername.mPos;
      len = mUsername.mLen;
      if (mPassword.mLen >= 0) {
        len += (mPassword.mLen + 1);
      }
    } else {
      pos = mPassword.mPos - 1;
      len = mPassword.mLen + 1;
    }

    if (includeDelim) len++;
  }
  return Substring(mSpec, pos, len);
}

inline nsDependentCSubstring nsStandardURL::Hostport() {
  uint32_t pos = 0, len = 0;
  if (mAuthority.mLen > 0) {
    pos = mHost.mPos;
    len = mAuthority.mPos + mAuthority.mLen - pos;
  }
  return Substring(mSpec, pos, len);
}

inline nsDependentCSubstring nsStandardURL::Host() {
  uint32_t pos = 0, len = 0;
  if (mHost.mLen > 0) {
    pos = mHost.mPos;
    len = mHost.mLen;
    if (mSpec.CharAt(pos) == '[' && mSpec.CharAt(pos + len - 1) == ']') {
      pos++;
      len -= 2;
    }
  }
  return Substring(mSpec, pos, len);
}

inline nsDependentCSubstring nsStandardURL::Filename() {
  uint32_t pos = 0, len = 0;
  // if there is no basename, then there can be no extension
  if (mBasename.mLen > 0) {
    pos = mBasename.mPos;
    len = mBasename.mLen;
    if (mExtension.mLen >= 0) len += (mExtension.mLen + 1);
  }
  return Substring(mSpec, pos, len);
}

}  // namespace net
}  // namespace mozilla

#endif  // nsStandardURL_h__
