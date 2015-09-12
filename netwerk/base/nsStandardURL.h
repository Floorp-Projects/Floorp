/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsStandardURL_h__
#define nsStandardURL_h__

#include "nsString.h"
#include "nsISerializable.h"
#include "nsIFileURL.h"
#include "nsIStandardURL.h"
#include "nsIUnicodeEncoder.h"
#include "nsIObserver.h"
#include "nsCOMPtr.h"
#include "nsURLHelper.h"
#include "nsIClassInfo.h"
#include "nsISizeOf.h"
#include "prclist.h"
#include "mozilla/Attributes.h"
#include "mozilla/MemoryReporting.h"
#include "nsIIPCSerializableURI.h"
#include "nsISensitiveInfoHiddenURI.h"

#ifdef NS_BUILD_REFCNT_LOGGING
#define DEBUG_DUMP_URLS_AT_SHUTDOWN
#endif

class nsIBinaryInputStream;
class nsIBinaryOutputStream;
class nsIIDNService;
class nsIPrefBranch;
class nsIFile;
class nsIURLParser;

//-----------------------------------------------------------------------------
// standard URL implementation
//-----------------------------------------------------------------------------

class nsStandardURL : public nsIFileURL
                    , public nsIStandardURL
                    , public nsISerializable
                    , public nsIClassInfo
                    , public nsISizeOf
                    , public nsIIPCSerializableURI
                    , public nsISensitiveInfoHiddenURI
{
protected:
    virtual ~nsStandardURL();

public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIURI
    NS_DECL_NSIURL
    NS_DECL_NSIFILEURL
    NS_DECL_NSISTANDARDURL
    NS_DECL_NSISERIALIZABLE
    NS_DECL_NSICLASSINFO
    NS_DECL_NSIMUTABLE
    NS_DECL_NSIIPCSERIALIZABLEURI
    NS_DECL_NSISENSITIVEINFOHIDDENURI

    // nsISizeOf
    virtual size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const override;
    virtual size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const override;

    explicit nsStandardURL(bool aSupportsFileURL = false, bool aTrackURL = true);

    static void InitGlobalObjects();
    static void ShutdownGlobalObjects();

public: /* internal -- HPUX compiler can't handle this being private */
    //
    // location and length of an url segment relative to mSpec
    //
    struct URLSegment
    {
        uint32_t mPos;
        int32_t  mLen;

        URLSegment() : mPos(0), mLen(-1) {}
        URLSegment(uint32_t pos, int32_t len) : mPos(pos), mLen(len) {}
        void Reset() { mPos = 0; mLen = -1; }
        // Merge another segment following this one to it if they're contiguous
        // Assumes we have something like "foo;bar" where this object is 'foo' and right
        // is 'bar'.
        void Merge(const nsCString &spec, const char separator, const URLSegment &right) {
            if (mLen >= 0 && 
                *(spec.get() + mPos + mLen) == separator &&
                mPos + mLen + 1 == right.mPos) {
                mLen += 1 + right.mLen;
            }
        }
    };

    //
    // Pref observer
    //
    class nsPrefObserver final : public nsIObserver
    {
        ~nsPrefObserver() {}

    public:
        NS_DECL_ISUPPORTS
        NS_DECL_NSIOBSERVER

        nsPrefObserver() { }
    };
    friend class nsPrefObserver;

    //
    // URL segment encoder : performs charset conversion and URL escaping.
    //
    class nsSegmentEncoder
    {
    public:
        explicit nsSegmentEncoder(const char *charset);

        // Encode the given segment if necessary, and return the length of
        // the encoded segment.  The encoded segment is appended to |buf|
        // if and only if encoding is required.
        int32_t EncodeSegmentCount(const char *str,
                                   const URLSegment &segment,
                                   int16_t mask,
                                   nsAFlatCString &buf,
                                   bool& appended,
                                   uint32_t extraLen = 0);
         
        // Encode the given string if necessary, and return a reference to
        // the encoded string.  Returns a reference to |buf| if encoding
        // is required.  Otherwise, a reference to |str| is returned.
        const nsACString &EncodeSegment(const nsASingleFragmentCString &str,
                                        int16_t mask,
                                        nsAFlatCString &buf);
    private:
        bool InitUnicodeEncoder();
        
        const char* mCharset;  // Caller should keep this alive for
                               // the life of the segment encoder
        nsCOMPtr<nsIUnicodeEncoder> mEncoder;
    };
    friend class nsSegmentEncoder;

protected:
    // enum used in a few places to specify how .ref attribute should be handled
    enum RefHandlingEnum {
        eIgnoreRef,
        eHonorRef
    };

    // Helper to share code between Equals and EqualsExceptRef
    // NOTE: *not* virtual, because no one needs to override this so far...
    nsresult EqualsInternal(nsIURI* unknownOther,
                            RefHandlingEnum refHandlingMode,
                            bool* result);

    virtual nsStandardURL* StartClone();

    // Helper to share code between Clone methods.
    nsresult CloneInternal(RefHandlingEnum aRefHandlingMode,
                           nsIURI** aClone);
    // Helper method that copies member variables from the source StandardURL
    // if copyCached = true, it will also copy mFile and mHostA
    nsresult CopyMembers(nsStandardURL * source, RefHandlingEnum mode,
                         bool copyCached = false);

    // Helper for subclass implementation of GetFile().  Subclasses that map
    // URIs to files in a special way should implement this method.  It should
    // ensure that our mFile is initialized, if it's possible.
    // returns NS_ERROR_NO_INTERFACE if the url does not map to a file
    virtual nsresult EnsureFile();

private:
    int32_t  Port() { return mPort == -1 ? mDefaultPort : mPort; }

    void     Clear();
    void     InvalidateCache(bool invalidateCachedFile = true);

    bool     ValidIPv6orHostname(const char *host);
    bool     NormalizeIDN(const nsCSubstring &host, nsCString &result);
    void     CoalescePath(netCoalesceFlags coalesceFlag, char *path);

    uint32_t AppendSegmentToBuf(char *, uint32_t, const char *, URLSegment &, const nsCString *esc=nullptr, bool useEsc = false);
    uint32_t AppendToBuf(char *, uint32_t, const char *, uint32_t);

    nsresult BuildNormalizedSpec(const char *spec);

    bool     SegmentIs(const URLSegment &s1, const char *val, bool ignoreCase = false);
    bool     SegmentIs(const char* spec, const URLSegment &s1, const char *val, bool ignoreCase = false);
    bool     SegmentIs(const URLSegment &s1, const char *val, const URLSegment &s2, bool ignoreCase = false);

    int32_t  ReplaceSegment(uint32_t pos, uint32_t len, const char *val, uint32_t valLen);
    int32_t  ReplaceSegment(uint32_t pos, uint32_t len, const nsACString &val);

    nsresult ParseURL(const char *spec, int32_t specLen);
    nsresult ParsePath(const char *spec, uint32_t pathPos, int32_t pathLen = -1);

    char    *AppendToSubstring(uint32_t pos, int32_t len, const char *tail);

    // dependent substring helpers
    const nsDependentCSubstring Segment(uint32_t pos, int32_t len); // see below
    const nsDependentCSubstring Segment(const URLSegment &s) { return Segment(s.mPos, s.mLen); }

    // dependent substring getters
    const nsDependentCSubstring Prepath();  // see below
    const nsDependentCSubstring Scheme()    { return Segment(mScheme); }
    const nsDependentCSubstring Userpass(bool includeDelim = false); // see below
    const nsDependentCSubstring Username()  { return Segment(mUsername); }
    const nsDependentCSubstring Password()  { return Segment(mPassword); }
    const nsDependentCSubstring Hostport(); // see below
    const nsDependentCSubstring Host();     // see below
    const nsDependentCSubstring Path()      { return Segment(mPath); }
    const nsDependentCSubstring Filepath()  { return Segment(mFilepath); }
    const nsDependentCSubstring Directory() { return Segment(mDirectory); }
    const nsDependentCSubstring Filename(); // see below
    const nsDependentCSubstring Basename()  { return Segment(mBasename); }
    const nsDependentCSubstring Extension() { return Segment(mExtension); }
    const nsDependentCSubstring Query()     { return Segment(mQuery); }
    const nsDependentCSubstring Ref()       { return Segment(mRef); }

    // shift the URLSegments to the right by diff
    void ShiftFromAuthority(int32_t diff) { mAuthority.mPos += diff; ShiftFromUsername(diff); }
    void ShiftFromUsername(int32_t diff)  { mUsername.mPos += diff; ShiftFromPassword(diff); }
    void ShiftFromPassword(int32_t diff)  { mPassword.mPos += diff; ShiftFromHost(diff); }
    void ShiftFromHost(int32_t diff)      { mHost.mPos += diff; ShiftFromPath(diff); }
    void ShiftFromPath(int32_t diff)      { mPath.mPos += diff; ShiftFromFilepath(diff); }
    void ShiftFromFilepath(int32_t diff)  { mFilepath.mPos += diff; ShiftFromDirectory(diff); }
    void ShiftFromDirectory(int32_t diff) { mDirectory.mPos += diff; ShiftFromBasename(diff); }
    void ShiftFromBasename(int32_t diff)  { mBasename.mPos += diff; ShiftFromExtension(diff); }
    void ShiftFromExtension(int32_t diff) { mExtension.mPos += diff; ShiftFromQuery(diff); }
    void ShiftFromQuery(int32_t diff)     { mQuery.mPos += diff; ShiftFromRef(diff); }
    void ShiftFromRef(int32_t diff)       { mRef.mPos += diff; }

    // fastload helper functions
    nsresult ReadSegment(nsIBinaryInputStream *, URLSegment &);
    nsresult WriteSegment(nsIBinaryOutputStream *, const URLSegment &);

    static void PrefsChanged(nsIPrefBranch *prefs, const char *pref);

    void FindHostLimit(nsACString::const_iterator& aStart,
                       nsACString::const_iterator& aEnd);

    // mSpec contains the normalized version of the URL spec (UTF-8 encoded).
    nsCString mSpec;
    int32_t   mDefaultPort;
    int32_t   mPort;

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

    nsCString              mOriginCharset;
    nsCOMPtr<nsIURLParser> mParser;

    // mFile is protected so subclasses can access it directly
protected:
    nsCOMPtr<nsIFile>      mFile;  // cached result for nsIFileURL::GetFile
    
private:
    char                  *mHostA; // cached result for nsIURI::GetHostA

    enum {
        eEncoding_Unknown,
        eEncoding_ASCII,
        eEncoding_UTF8
    };

    uint32_t mHostEncoding    : 2; // eEncoding_xxx
    uint32_t mSpecEncoding    : 2; // eEncoding_xxx
    uint32_t mURLType         : 2; // nsIStandardURL::URLTYPE_xxx
    uint32_t mMutable         : 1; // nsIStandardURL::mutable
    uint32_t mSupportsFileURL : 1; // QI to nsIFileURL?

    // global objects.  don't use COMPtr as its destructor will cause a
    // coredump if we leak it.
    static nsIIDNService               *gIDN;
    static char                         gHostLimitDigits[];
    static bool                         gInitialized;
    static bool                         gEscapeUTF8;
    static bool                         gAlwaysEncodeInUTF8;
    static bool                         gEncodeQueryInUTF8;

public:
#ifdef DEBUG_DUMP_URLS_AT_SHUTDOWN
    PRCList mDebugCList;
    void PrintSpec() const { printf("  %s\n", mSpec.get()); }
#endif
};

#define NS_THIS_STANDARDURL_IMPL_CID                 \
{ /* b8e3e97b-1ccd-4b45-af5a-79596770f5d7 */         \
    0xb8e3e97b,                                      \
    0x1ccd,                                          \
    0x4b45,                                          \
    {0xaf, 0x5a, 0x79, 0x59, 0x67, 0x70, 0xf5, 0xd7} \
}

//-----------------------------------------------------------------------------
// Dependent substring getters
//-----------------------------------------------------------------------------

inline const nsDependentCSubstring
nsStandardURL::Segment(uint32_t pos, int32_t len)
{
    if (len < 0) {
        pos = 0;
        len = 0;
    }
    return Substring(mSpec, pos, uint32_t(len));
}

inline const nsDependentCSubstring
nsStandardURL::Prepath()
{
    uint32_t len = 0;
    if (mAuthority.mLen >= 0)
        len = mAuthority.mPos + mAuthority.mLen;
    return Substring(mSpec, 0, len);
}

inline const nsDependentCSubstring
nsStandardURL::Userpass(bool includeDelim)
{
    uint32_t pos=0, len=0;
    // if there is no username, then there can be no password
    if (mUsername.mLen > 0) {
        pos = mUsername.mPos;
        len = mUsername.mLen;
        if (mPassword.mLen >= 0)
            len += (mPassword.mLen + 1);
        if (includeDelim)
            len++;
    }
    return Substring(mSpec, pos, len);
}

inline const nsDependentCSubstring
nsStandardURL::Hostport()
{
    uint32_t pos=0, len=0;
    if (mAuthority.mLen > 0) {
        pos = mHost.mPos;
        len = mAuthority.mPos + mAuthority.mLen - pos;
    }
    return Substring(mSpec, pos, len);
}

inline const nsDependentCSubstring
nsStandardURL::Host()
{
    uint32_t pos=0, len=0;
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

inline const nsDependentCSubstring
nsStandardURL::Filename()
{
    uint32_t pos=0, len=0;
    // if there is no basename, then there can be no extension
    if (mBasename.mLen > 0) {
        pos = mBasename.mPos;
        len = mBasename.mLen;
        if (mExtension.mLen >= 0)
            len += (mExtension.mLen + 1);
    }
    return Substring(mSpec, pos, len);
}

#endif // nsStandardURL_h__
