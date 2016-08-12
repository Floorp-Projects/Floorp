/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
// See
// https://wiki.mozilla.org/Security/Features/Application_Reputation_Design_Doc
// for a description of Chrome's implementation of this feature.
#include "ApplicationReputation.h"
#include "csd.pb.h"

#include "nsIArray.h"
#include "nsIApplicationReputation.h"
#include "nsIChannel.h"
#include "nsIHttpChannel.h"
#include "nsIIOService.h"
#include "nsIPrefService.h"
#include "nsISimpleEnumerator.h"
#include "nsIStreamListener.h"
#include "nsIStringStream.h"
#include "nsITimer.h"
#include "nsIUploadChannel2.h"
#include "nsIURI.h"
#include "nsIURL.h"
#include "nsIUrlClassifierDBService.h"
#include "nsIX509Cert.h"
#include "nsIX509CertDB.h"
#include "nsIX509CertList.h"

#include "mozilla/BasePrincipal.h"
#include "mozilla/ErrorNames.h"
#include "mozilla/LoadContext.h"
#include "mozilla/Preferences.h"
#include "mozilla/Services.h"
#include "mozilla/Telemetry.h"
#include "mozilla/TimeStamp.h"

#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsDebug.h"
#include "nsError.h"
#include "nsNetCID.h"
#include "nsReadableUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nsThreadUtils.h"
#include "nsXPCOMStrings.h"

#include "nsIContentPolicy.h"
#include "nsILoadInfo.h"
#include "nsContentUtils.h"

using mozilla::BasePrincipal;
using mozilla::PrincipalOriginAttributes;
using mozilla::Preferences;
using mozilla::TimeStamp;
using mozilla::Telemetry::Accumulate;
using safe_browsing::ClientDownloadRequest;
using safe_browsing::ClientDownloadRequest_CertificateChain;
using safe_browsing::ClientDownloadRequest_Resource;
using safe_browsing::ClientDownloadRequest_SignatureInfo;

// Preferences that we need to initialize the query.
#define PREF_SB_APP_REP_URL "browser.safebrowsing.downloads.remote.url"
#define PREF_SB_MALWARE_ENABLED "browser.safebrowsing.malware.enabled"
#define PREF_SB_DOWNLOADS_ENABLED "browser.safebrowsing.downloads.enabled"
#define PREF_SB_DOWNLOADS_REMOTE_ENABLED "browser.safebrowsing.downloads.remote.enabled"
#define PREF_SB_DOWNLOADS_REMOTE_TIMEOUT "browser.safebrowsing.downloads.remote.timeout_ms"
#define PREF_GENERAL_LOCALE "general.useragent.locale"
#define PREF_DOWNLOAD_BLOCK_TABLE "urlclassifier.downloadBlockTable"
#define PREF_DOWNLOAD_ALLOW_TABLE "urlclassifier.downloadAllowTable"

// Preferences that are needed to action the verdict.
#define PREF_BLOCK_DANGEROUS            "browser.safebrowsing.downloads.remote.block_dangerous"
#define PREF_BLOCK_DANGEROUS_HOST       "browser.safebrowsing.downloads.remote.block_dangerous_host"
#define PREF_BLOCK_POTENTIALLY_UNWANTED "browser.safebrowsing.downloads.remote.block_potentially_unwanted"
#define PREF_BLOCK_UNCOMMON             "browser.safebrowsing.downloads.remote.block_uncommon"

// MOZ_LOG=ApplicationReputation:5
mozilla::LazyLogModule ApplicationReputationService::prlog("ApplicationReputation");
#define LOG(args) MOZ_LOG(ApplicationReputationService::prlog, mozilla::LogLevel::Debug, args)
#define LOG_ENABLED() MOZ_LOG_TEST(ApplicationReputationService::prlog, mozilla::LogLevel::Debug)

class PendingDBLookup;

// A single use class private to ApplicationReputationService encapsulating an
// nsIApplicationReputationQuery and an nsIApplicationReputationCallback. Once
// created by ApplicationReputationService, it is guaranteed to call mCallback.
// This class is private to ApplicationReputationService.
class PendingLookup final : public nsIStreamListener,
                            public nsITimerCallback
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSITIMERCALLBACK

  // Constructor and destructor.
  PendingLookup(nsIApplicationReputationQuery* aQuery,
                nsIApplicationReputationCallback* aCallback);

  // Start the lookup. The lookup may have 2 parts: local and remote. In the
  // local lookup, PendingDBLookups are created to query the local allow and
  // blocklists for various URIs associated with this downloaded file. In the
  // event that no results are found, a remote lookup is sent to the Application
  // Reputation server.
  nsresult StartLookup();

private:
  ~PendingLookup();

  friend class PendingDBLookup;

  // Telemetry states.
  // Status of the remote response (valid or not).
  enum SERVER_RESPONSE_TYPES {
    SERVER_RESPONSE_VALID = 0,
    SERVER_RESPONSE_FAILED = 1,
    SERVER_RESPONSE_INVALID = 2,
  };

  // Number of blocklist and allowlist hits we have seen.
  uint32_t mBlocklistCount;
  uint32_t mAllowlistCount;

  // The query containing metadata about the downloaded file.
  nsCOMPtr<nsIApplicationReputationQuery> mQuery;

  // The callback with which to report the verdict.
  nsCOMPtr<nsIApplicationReputationCallback> mCallback;

  // An array of strings created from certificate information used to whitelist
  // the downloaded file.
  nsTArray<nsCString> mAllowlistSpecs;
  // The source URI of the download, the referrer and possibly any redirects.
  nsTArray<nsCString> mAnylistSpecs;

  // When we started this query
  TimeStamp mStartTime;

  // The channel used to talk to the remote lookup server
  nsCOMPtr<nsIChannel> mChannel;

  // Timer to abort this lookup if it takes too long
  nsCOMPtr<nsITimer> mTimeoutTimer;

  // A protocol buffer for storing things we need in the remote request. We
  // store the resource chain (redirect information) as well as signature
  // information extracted using the Windows Authenticode API, if the binary is
  // signed.
  ClientDownloadRequest mRequest;

  // The response from the application reputation query. This is read in chunks
  // as part of our nsIStreamListener implementation and may contain embedded
  // NULLs.
  nsCString mResponse;

  // Returns true if the file is likely to be binary.
  bool IsBinaryFile();

  // Returns the type of download binary for the file.
  ClientDownloadRequest::DownloadType GetDownloadType(const nsAString& aFilename);

  // Clean up and call the callback. PendingLookup must not be used after this
  // function is called.
  nsresult OnComplete(bool shouldBlock, nsresult rv,
    uint32_t verdict = nsIApplicationReputationService::VERDICT_SAFE);

  // Wrapper function for nsIStreamListener.onStopRequest to make it easy to
  // guarantee calling the callback
  nsresult OnStopRequestInternal(nsIRequest *aRequest,
                                 nsISupports *aContext,
                                 nsresult aResult,
                                 bool* aShouldBlock,
                                 uint32_t* aVerdict);

  // Strip url parameters, fragments, and user@pass fields from the URI spec
  // using nsIURL. If aURI is not an nsIURL, returns the original nsIURI.spec.
  nsresult GetStrippedSpec(nsIURI* aUri, nsACString& spec);

  // Escape '/' and '%' in certificate attribute values.
  nsCString EscapeCertificateAttribute(const nsACString& aAttribute);

  // Escape ':' in fingerprint values.
  nsCString EscapeFingerprint(const nsACString& aAttribute);

  // Generate whitelist strings for the given certificate pair from the same
  // certificate chain.
  nsresult GenerateWhitelistStringsForPair(
    nsIX509Cert* certificate, nsIX509Cert* issuer);

  // Generate whitelist strings for the given certificate chain, which starts
  // with the signer and may go all the way to the root cert.
  nsresult GenerateWhitelistStringsForChain(
    const ClientDownloadRequest_CertificateChain& aChain);

  // For signed binaries, generate strings of the form:
  // http://sb-ssl.google.com/safebrowsing/csd/certificate/
  //   <issuer_cert_sha1_fingerprint>[/CN=<cn>][/O=<org>][/OU=<unit>]
  // for each (cert, issuer) pair in each chain of certificates that is
  // associated with the binary.
  nsresult GenerateWhitelistStrings();

  // Parse the XPCOM certificate lists and stick them into the protocol buffer
  // version.
  nsresult ParseCertificates(nsIArray* aSigArray);

  // Adds the redirects to mAnylistSpecs to be looked up.
  nsresult AddRedirects(nsIArray* aRedirects);

  // Helper function to ensure that we call PendingLookup::LookupNext or
  // PendingLookup::OnComplete.
  nsresult DoLookupInternal();

  // Looks up all the URIs that may be responsible for allowlisting or
  // blocklisting the downloaded file. These URIs may include whitelist strings
  // generated by certificates verifying the binary as well as the target URI
  // from which the file was downloaded.
  nsresult LookupNext();

  // Sends a query to the remote application reputation service. Returns NS_OK
  // on success.
  nsresult SendRemoteQuery();

  // Helper function to ensure that we always call the callback.
  nsresult SendRemoteQueryInternal();
};

// A single-use class for looking up a single URI in the safebrowsing DB. This
// class is private to PendingLookup.
class PendingDBLookup final : public nsIUrlClassifierCallback
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIURLCLASSIFIERCALLBACK

  // Constructor and destructor
  explicit PendingDBLookup(PendingLookup* aPendingLookup);

  // Look up the given URI in the safebrowsing DBs, optionally on both the allow
  // list and the blocklist. If there is a match, call
  // PendingLookup::OnComplete. Otherwise, call PendingLookup::LookupNext.
  nsresult LookupSpec(const nsACString& aSpec, bool aAllowlistOnly);

private:
  ~PendingDBLookup();

  // The download appeared on the allowlist, blocklist, or no list (and thus
  // could trigger a remote query.
  enum LIST_TYPES {
    ALLOW_LIST = 0,
    BLOCK_LIST = 1,
    NO_LIST = 2,
  };

  nsCString mSpec;
  bool mAllowlistOnly;
  RefPtr<PendingLookup> mPendingLookup;
  nsresult LookupSpecInternal(const nsACString& aSpec);
};

NS_IMPL_ISUPPORTS(PendingDBLookup,
                  nsIUrlClassifierCallback)

PendingDBLookup::PendingDBLookup(PendingLookup* aPendingLookup) :
  mAllowlistOnly(false),
  mPendingLookup(aPendingLookup)
{
  LOG(("Created pending DB lookup [this = %p]", this));
}

PendingDBLookup::~PendingDBLookup()
{
  LOG(("Destroying pending DB lookup [this = %p]", this));
  mPendingLookup = nullptr;
}

nsresult
PendingDBLookup::LookupSpec(const nsACString& aSpec,
                            bool aAllowlistOnly)
{
  LOG(("Checking principal %s [this=%p]", aSpec.Data(), this));
  mSpec = aSpec;
  mAllowlistOnly = aAllowlistOnly;
  nsresult rv = LookupSpecInternal(aSpec);
  if (NS_FAILED(rv)) {
    LOG(("Error in LookupSpecInternal"));
    return mPendingLookup->OnComplete(false, NS_OK);
  }
  // LookupSpecInternal has called nsIUrlClassifierCallback.lookup, which is
  // guaranteed to call HandleEvent.
  return rv;
}

nsresult
PendingDBLookup::LookupSpecInternal(const nsACString& aSpec)
{
  nsresult rv;

  nsCOMPtr<nsIURI> uri;
  nsCOMPtr<nsIIOService> ios = do_GetService(NS_IOSERVICE_CONTRACTID, &rv);
  rv = ios->NewURI(aSpec, nullptr, nullptr, getter_AddRefs(uri));
  NS_ENSURE_SUCCESS(rv, rv);

  PrincipalOriginAttributes attrs;
  nsCOMPtr<nsIPrincipal> principal =
    BasePrincipal::CreateCodebasePrincipal(uri, attrs);
  if (!principal) {
    return NS_ERROR_FAILURE;
  }

  // Check local lists to see if the URI has already been whitelisted or
  // blacklisted.
  LOG(("Checking DB service for principal %s [this = %p]", mSpec.get(), this));
  nsCOMPtr<nsIUrlClassifierDBService> dbService =
    do_GetService(NS_URLCLASSIFIERDBSERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString tables;
  nsAutoCString allowlist;
  Preferences::GetCString(PREF_DOWNLOAD_ALLOW_TABLE, &allowlist);
  if (!allowlist.IsEmpty()) {
    tables.Append(allowlist);
  }
  nsAutoCString blocklist;
  Preferences::GetCString(PREF_DOWNLOAD_BLOCK_TABLE, &blocklist);
  if (!mAllowlistOnly && !blocklist.IsEmpty()) {
    tables.Append(',');
    tables.Append(blocklist);
  }
  return dbService->Lookup(principal, tables, this);
}

NS_IMETHODIMP
PendingDBLookup::HandleEvent(const nsACString& tables)
{
  // HandleEvent is guaranteed to call either:
  // 1) PendingLookup::OnComplete if the URL matches the blocklist, or
  // 2) PendingLookup::LookupNext if the URL does not match the blocklist.
  // Blocklisting trumps allowlisting.
  nsAutoCString blockList;
  Preferences::GetCString(PREF_DOWNLOAD_BLOCK_TABLE, &blockList);
  if (!mAllowlistOnly && FindInReadable(blockList, tables)) {
    mPendingLookup->mBlocklistCount++;
    Accumulate(mozilla::Telemetry::APPLICATION_REPUTATION_LOCAL, BLOCK_LIST);
    LOG(("Found principal %s on blocklist [this = %p]", mSpec.get(), this));
    return mPendingLookup->OnComplete(true, NS_OK,
      nsIApplicationReputationService::VERDICT_DANGEROUS);
  }

  nsAutoCString allowList;
  Preferences::GetCString(PREF_DOWNLOAD_ALLOW_TABLE, &allowList);
  if (FindInReadable(allowList, tables)) {
    mPendingLookup->mAllowlistCount++;
    Accumulate(mozilla::Telemetry::APPLICATION_REPUTATION_LOCAL, ALLOW_LIST);
    LOG(("Found principal %s on allowlist [this = %p]", mSpec.get(), this));
    // Don't call onComplete, since blocklisting trumps allowlisting
  } else {
    LOG(("Didn't find principal %s on any list [this = %p]", mSpec.get(),
         this));
    Accumulate(mozilla::Telemetry::APPLICATION_REPUTATION_LOCAL, NO_LIST);
  }
  return mPendingLookup->LookupNext();
}

NS_IMPL_ISUPPORTS(PendingLookup,
                  nsIStreamListener,
                  nsIRequestObserver)

PendingLookup::PendingLookup(nsIApplicationReputationQuery* aQuery,
                             nsIApplicationReputationCallback* aCallback) :
  mBlocklistCount(0),
  mAllowlistCount(0),
  mQuery(aQuery),
  mCallback(aCallback)
{
  LOG(("Created pending lookup [this = %p]", this));
}

PendingLookup::~PendingLookup()
{
  LOG(("Destroying pending lookup [this = %p]", this));
}

bool
PendingLookup::IsBinaryFile()
{
  nsString fileName;
  nsresult rv = mQuery->GetSuggestedFileName(fileName);
  if (NS_FAILED(rv)) {
    LOG(("No suggested filename [this = %p]", this));
    return false;
  }
  LOG(("Suggested filename: %s [this = %p]",
       NS_ConvertUTF16toUTF8(fileName).get(), this));
  return
    // Extracted from the "File Type Policies" Chrome extension
    //StringEndsWith(fileName, NS_LITERAL_STRING(".001")) ||
    //StringEndsWith(fileName, NS_LITERAL_STRING(".7z")) ||
    //StringEndsWith(fileName, NS_LITERAL_STRING(".ace")) ||
    StringEndsWith(fileName, NS_LITERAL_STRING(".action")) || // Mac script
    StringEndsWith(fileName, NS_LITERAL_STRING(".ad")) || // AppleDouble encoded file
    StringEndsWith(fileName, NS_LITERAL_STRING(".ade")) || // MS Access
    StringEndsWith(fileName, NS_LITERAL_STRING(".adp")) || // MS Access
    StringEndsWith(fileName, NS_LITERAL_STRING(".apk")) || // Android package
    StringEndsWith(fileName, NS_LITERAL_STRING(".app")) || // Mac app
    //StringEndsWith(fileName, NS_LITERAL_STRING(".application")) ||
    StringEndsWith(fileName, NS_LITERAL_STRING(".appref-ms")) || // Windows
    //StringEndsWith(fileName, NS_LITERAL_STRING(".arc")) ||
    //StringEndsWith(fileName, NS_LITERAL_STRING(".arj")) ||
    StringEndsWith(fileName, NS_LITERAL_STRING(".as")) || // Adobe Flash
    StringEndsWith(fileName, NS_LITERAL_STRING(".asp")) || // Windows Server script
    StringEndsWith(fileName, NS_LITERAL_STRING(".asx")) || // Windows Media Player
    //StringEndsWith(fileName, NS_LITERAL_STRING(".b64")) ||
    //StringEndsWith(fileName, NS_LITERAL_STRING(".balz")) ||
    StringEndsWith(fileName, NS_LITERAL_STRING(".bas")) || // Basic script
    StringEndsWith(fileName, NS_LITERAL_STRING(".bash")) || // Linux shell
    StringEndsWith(fileName, NS_LITERAL_STRING(".bat")) || // Windows shell
    //StringEndsWith(fileName, NS_LITERAL_STRING(".bhx")) ||
    StringEndsWith(fileName, NS_LITERAL_STRING(".bin")) || // Generic binary
    //StringEndsWith(fileName, NS_LITERAL_STRING(".bz")) || // Linux archive (bzip)
    StringEndsWith(fileName, NS_LITERAL_STRING(".bz2")) || // Linux archive (bzip2)
    StringEndsWith(fileName, NS_LITERAL_STRING(".bzip2")) || // Linux archive (bzip2)
    StringEndsWith(fileName, NS_LITERAL_STRING(".cab")) || // Windows archive
    //StringEndsWith(fileName, NS_LITERAL_STRING(".cdr")) || // Audio CD image
    //StringEndsWith(fileName, NS_LITERAL_STRING(".cfg")) ||
    StringEndsWith(fileName, NS_LITERAL_STRING(".chi")) || // Windows Help
    StringEndsWith(fileName, NS_LITERAL_STRING(".chm")) || // Windows Help
    StringEndsWith(fileName, NS_LITERAL_STRING(".class")) || // Java
    StringEndsWith(fileName, NS_LITERAL_STRING(".cmd")) || // Windows executable
    StringEndsWith(fileName, NS_LITERAL_STRING(".com")) || // Windows executable
    StringEndsWith(fileName, NS_LITERAL_STRING(".command")) || // Mac script
    //StringEndsWith(fileName, NS_LITERAL_STRING(".cpgz")) ||
    //StringEndsWith(fileName, NS_LITERAL_STRING(".cpio")) ||
    StringEndsWith(fileName, NS_LITERAL_STRING(".cpl")) || // Windows
    StringEndsWith(fileName, NS_LITERAL_STRING(".crt")) || // Windows
    StringEndsWith(fileName, NS_LITERAL_STRING(".crx")) || // Chrome extensions
    StringEndsWith(fileName, NS_LITERAL_STRING(".csh")) || // Linux shell
    StringEndsWith(fileName, NS_LITERAL_STRING(".dart")) || // Dart script
    StringEndsWith(fileName, NS_LITERAL_STRING(".dc42")) || // Apple DiskCopy Image
    StringEndsWith(fileName, NS_LITERAL_STRING(".deb")) || // Linux package
    StringEndsWith(fileName, NS_LITERAL_STRING(".dex")) || // MS Excel
    StringEndsWith(fileName, NS_LITERAL_STRING(".diskcopy42")) || // Apple DiskCopy Image
    StringEndsWith(fileName, NS_LITERAL_STRING(".dll")) || // Windows
    StringEndsWith(fileName, NS_LITERAL_STRING(".dmg")) || // Mac disk image
    StringEndsWith(fileName, NS_LITERAL_STRING(".dmgpart")) || // Mac disk image
    //StringEndsWith(fileName, NS_LITERAL_STRING(".docb")) ||
    StringEndsWith(fileName, NS_LITERAL_STRING(".docm")) || // MS Word
    StringEndsWith(fileName, NS_LITERAL_STRING(".docx")) || // MS Word
    StringEndsWith(fileName, NS_LITERAL_STRING(".dotm")) || // MS Word
    //StringEndsWith(fileName, NS_LITERAL_STRING(".dott")) ||
    StringEndsWith(fileName, NS_LITERAL_STRING(".drv")) || // Windows driver
    //StringEndsWith(fileName, NS_LITERAL_STRING(".dvdr")) || // DVD image
    StringEndsWith(fileName, NS_LITERAL_STRING(".efi")) || // Firmware
    StringEndsWith(fileName, NS_LITERAL_STRING(".eml")) || // Email
    StringEndsWith(fileName, NS_LITERAL_STRING(".exe")) || // Windows executable
    //StringEndsWith(fileName, NS_LITERAL_STRING(".fat")) ||
    StringEndsWith(fileName, NS_LITERAL_STRING(".fon")) || // Windows font
    //StringEndsWith(fileName, NS_LITERAL_STRING(".fxp")) ||
    StringEndsWith(fileName, NS_LITERAL_STRING(".gadget")) || // Windows
    StringEndsWith(fileName, NS_LITERAL_STRING(".grp")) || // Windows
    StringEndsWith(fileName, NS_LITERAL_STRING(".gz")) || // Linux archive (gzip)
    StringEndsWith(fileName, NS_LITERAL_STRING(".gzip")) || // Linux archive (gzip)
    StringEndsWith(fileName, NS_LITERAL_STRING(".hfs")) || // Mac disk image
    StringEndsWith(fileName, NS_LITERAL_STRING(".hlp")) || // Windows Help
    StringEndsWith(fileName, NS_LITERAL_STRING(".hqx")) || // Mac archive
    StringEndsWith(fileName, NS_LITERAL_STRING(".hta")) || // HTML program file
    StringEndsWith(fileName, NS_LITERAL_STRING(".htt")) || // MS HTML template
    StringEndsWith(fileName, NS_LITERAL_STRING(".img")) || // Generic image
    StringEndsWith(fileName, NS_LITERAL_STRING(".imgpart")) || // Mac disk image
    StringEndsWith(fileName, NS_LITERAL_STRING(".inf")) || // Windows installer
    //StringEndsWith(fileName, NS_LITERAL_STRING(".ini")) ||
    StringEndsWith(fileName, NS_LITERAL_STRING(".ins")) || // InstallShield
    StringEndsWith(fileName, NS_LITERAL_STRING(".inx")) || // InstallShield
    StringEndsWith(fileName, NS_LITERAL_STRING(".iso")) || // CD image
    //StringEndsWith(fileName, NS_LITERAL_STRING(".isp")) ||
    StringEndsWith(fileName, NS_LITERAL_STRING(".isu")) || // InstallShield
    StringEndsWith(fileName, NS_LITERAL_STRING(".jar")) || // Java
    StringEndsWith(fileName, NS_LITERAL_STRING(".jnlp")) || // Java
    StringEndsWith(fileName, NS_LITERAL_STRING(".job")) || // Windows
    StringEndsWith(fileName, NS_LITERAL_STRING(".js")) || // JavaScript script
    StringEndsWith(fileName, NS_LITERAL_STRING(".jse")) || // JScript
    StringEndsWith(fileName, NS_LITERAL_STRING(".ksh")) || // Linux shell
    //StringEndsWith(fileName, NS_LITERAL_STRING(".lha")) ||
    StringEndsWith(fileName, NS_LITERAL_STRING(".lnk")) || // Windows
    //StringEndsWith(fileName, NS_LITERAL_STRING(".local")) ||
    //StringEndsWith(fileName, NS_LITERAL_STRING(".lpaq1")) ||
    //StringEndsWith(fileName, NS_LITERAL_STRING(".lpaq5")) ||
    //StringEndsWith(fileName, NS_LITERAL_STRING(".lpaq8")) ||
    //StringEndsWith(fileName, NS_LITERAL_STRING(".lzh")) ||
    //StringEndsWith(fileName, NS_LITERAL_STRING(".lzma")) ||
    StringEndsWith(fileName, NS_LITERAL_STRING(".mad")) || // MS Access
    StringEndsWith(fileName, NS_LITERAL_STRING(".maf")) || // MS Access
    StringEndsWith(fileName, NS_LITERAL_STRING(".mag")) || // MS Access
    StringEndsWith(fileName, NS_LITERAL_STRING(".mam")) || // MS Access
    //StringEndsWith(fileName, NS_LITERAL_STRING(".manifest")) ||
    StringEndsWith(fileName, NS_LITERAL_STRING(".maq")) || // MS Access
    StringEndsWith(fileName, NS_LITERAL_STRING(".mar")) || // MS Access
    StringEndsWith(fileName, NS_LITERAL_STRING(".mas")) || // MS Access
    StringEndsWith(fileName, NS_LITERAL_STRING(".mat")) || // MS Access
    //StringEndsWith(fileName, NS_LITERAL_STRING(".mau")) ||
    StringEndsWith(fileName, NS_LITERAL_STRING(".mav")) || // MS Access
    StringEndsWith(fileName, NS_LITERAL_STRING(".maw")) || // MS Access
    StringEndsWith(fileName, NS_LITERAL_STRING(".mda")) || // MS Access
    StringEndsWith(fileName, NS_LITERAL_STRING(".mdb")) || // MS Access
    StringEndsWith(fileName, NS_LITERAL_STRING(".mde")) || // MS Access
    StringEndsWith(fileName, NS_LITERAL_STRING(".mdt")) || // MS Access
    StringEndsWith(fileName, NS_LITERAL_STRING(".mdw")) || // MS Access
    StringEndsWith(fileName, NS_LITERAL_STRING(".mdz")) || // MS Access
    StringEndsWith(fileName, NS_LITERAL_STRING(".mht")) || // MS HTML
    StringEndsWith(fileName, NS_LITERAL_STRING(".mhtml")) || // MS HTML
    StringEndsWith(fileName, NS_LITERAL_STRING(".mim")) || // MS Mail
    StringEndsWith(fileName, NS_LITERAL_STRING(".mmc")) || // MS Office
    //StringEndsWith(fileName, NS_LITERAL_STRING(".mof")) ||
    //StringEndsWith(fileName, NS_LITERAL_STRING(".mpkg")) ||
    StringEndsWith(fileName, NS_LITERAL_STRING(".msc")) || // MS Windows
    StringEndsWith(fileName, NS_LITERAL_STRING(".msg")) || // Email
    StringEndsWith(fileName, NS_LITERAL_STRING(".msh")) || // Windows shell
    StringEndsWith(fileName, NS_LITERAL_STRING(".msh1")) || // Windows shell
    StringEndsWith(fileName, NS_LITERAL_STRING(".msh1xml")) || // Windows shell
    StringEndsWith(fileName, NS_LITERAL_STRING(".msh2")) || // Windows shell
    StringEndsWith(fileName, NS_LITERAL_STRING(".msh2xml")) || // Windows shell
    StringEndsWith(fileName, NS_LITERAL_STRING(".mshxml")) || // Windows
    StringEndsWith(fileName, NS_LITERAL_STRING(".msi")) || // Windows installer
    StringEndsWith(fileName, NS_LITERAL_STRING(".msp")) || // Windows installer
    StringEndsWith(fileName, NS_LITERAL_STRING(".mst")) || // Windows installer
    StringEndsWith(fileName, NS_LITERAL_STRING(".ndif")) || // Mac disk image
    //StringEndsWith(fileName, NS_LITERAL_STRING(".ntfs")) || // 7z
    StringEndsWith(fileName, NS_LITERAL_STRING(".ocx")) || // ActiveX
    StringEndsWith(fileName, NS_LITERAL_STRING(".ops")) || // MS Office
    StringEndsWith(fileName, NS_LITERAL_STRING(".osx")) || // Mac PowerPC executable
    StringEndsWith(fileName, NS_LITERAL_STRING(".out")) || // Linux binary
    StringEndsWith(fileName, NS_LITERAL_STRING(".paf")) || // PortableApps package
    //StringEndsWith(fileName, NS_LITERAL_STRING(".paq8f")) ||
    //StringEndsWith(fileName, NS_LITERAL_STRING(".paq8jd")) ||
    //StringEndsWith(fileName, NS_LITERAL_STRING(".paq8l")) ||
    //StringEndsWith(fileName, NS_LITERAL_STRING(".paq8o")) ||
    StringEndsWith(fileName, NS_LITERAL_STRING(".partial")) || // Downloads
    //StringEndsWith(fileName, NS_LITERAL_STRING(".pax")) ||
    //StringEndsWith(fileName, NS_LITERAL_STRING(".pcd")) || // Kodak Picture CD
    StringEndsWith(fileName, NS_LITERAL_STRING(".pdf")) || // Adobe Acrobat
    //StringEndsWith(fileName, NS_LITERAL_STRING(".pea")) ||
    StringEndsWith(fileName, NS_LITERAL_STRING(".pet")) || // Linux package
    StringEndsWith(fileName, NS_LITERAL_STRING(".pif")) || // Windows
    StringEndsWith(fileName, NS_LITERAL_STRING(".pkg")) || // Mac installer
    StringEndsWith(fileName, NS_LITERAL_STRING(".pl")) || // Perl script
    //StringEndsWith(fileName, NS_LITERAL_STRING(".plg")) ||
    StringEndsWith(fileName, NS_LITERAL_STRING(".potx")) || // MS PowerPoint
    StringEndsWith(fileName, NS_LITERAL_STRING(".ppam")) || // MS PowerPoint
    StringEndsWith(fileName, NS_LITERAL_STRING(".ppsx")) || // MS PowerPoint
    StringEndsWith(fileName, NS_LITERAL_STRING(".pptm")) || // MS PowerPoint
    StringEndsWith(fileName, NS_LITERAL_STRING(".pptx")) || // MS PowerPoint
    StringEndsWith(fileName, NS_LITERAL_STRING(".prf")) || // MS Outlook
    //StringEndsWith(fileName, NS_LITERAL_STRING(".prg")) ||
    StringEndsWith(fileName, NS_LITERAL_STRING(".ps1")) || // Windows shell
    StringEndsWith(fileName, NS_LITERAL_STRING(".ps1xml")) || // Windows shell
    StringEndsWith(fileName, NS_LITERAL_STRING(".ps2")) || // Windows shell
    StringEndsWith(fileName, NS_LITERAL_STRING(".ps2xml")) || // Windows shell
    StringEndsWith(fileName, NS_LITERAL_STRING(".psc1")) || // Windows shell
    StringEndsWith(fileName, NS_LITERAL_STRING(".psc2")) || // Windows shell
    StringEndsWith(fileName, NS_LITERAL_STRING(".pst")) || // MS Outlook
    StringEndsWith(fileName, NS_LITERAL_STRING(".pup")) || // Linux package
    StringEndsWith(fileName, NS_LITERAL_STRING(".py")) || // Python script
    StringEndsWith(fileName, NS_LITERAL_STRING(".pyc")) || // Python binary
    StringEndsWith(fileName, NS_LITERAL_STRING(".pyw")) || // Python GUI
    //StringEndsWith(fileName, NS_LITERAL_STRING(".quad")) ||
    //StringEndsWith(fileName, NS_LITERAL_STRING(".r00")) ||
    //StringEndsWith(fileName, NS_LITERAL_STRING(".r01")) ||
    //StringEndsWith(fileName, NS_LITERAL_STRING(".r02")) ||
    //StringEndsWith(fileName, NS_LITERAL_STRING(".r03")) ||
    //StringEndsWith(fileName, NS_LITERAL_STRING(".r04")) ||
    //StringEndsWith(fileName, NS_LITERAL_STRING(".r05")) ||
    //StringEndsWith(fileName, NS_LITERAL_STRING(".r06")) ||
    //StringEndsWith(fileName, NS_LITERAL_STRING(".r07")) ||
    //StringEndsWith(fileName, NS_LITERAL_STRING(".r08")) ||
    //StringEndsWith(fileName, NS_LITERAL_STRING(".r09")) ||
    //StringEndsWith(fileName, NS_LITERAL_STRING(".r10")) ||
    //StringEndsWith(fileName, NS_LITERAL_STRING(".r11")) ||
    //StringEndsWith(fileName, NS_LITERAL_STRING(".r12")) ||
    //StringEndsWith(fileName, NS_LITERAL_STRING(".r13")) ||
    //StringEndsWith(fileName, NS_LITERAL_STRING(".r14")) ||
    //StringEndsWith(fileName, NS_LITERAL_STRING(".r15")) ||
    //StringEndsWith(fileName, NS_LITERAL_STRING(".r16")) ||
    //StringEndsWith(fileName, NS_LITERAL_STRING(".r17")) ||
    //StringEndsWith(fileName, NS_LITERAL_STRING(".r18")) ||
    //StringEndsWith(fileName, NS_LITERAL_STRING(".r19")) ||
    //StringEndsWith(fileName, NS_LITERAL_STRING(".r20")) ||
    //StringEndsWith(fileName, NS_LITERAL_STRING(".r21")) ||
    //StringEndsWith(fileName, NS_LITERAL_STRING(".r22")) ||
    //StringEndsWith(fileName, NS_LITERAL_STRING(".r23")) ||
    //StringEndsWith(fileName, NS_LITERAL_STRING(".r24")) ||
    //StringEndsWith(fileName, NS_LITERAL_STRING(".r25")) ||
    //StringEndsWith(fileName, NS_LITERAL_STRING(".r26")) ||
    //StringEndsWith(fileName, NS_LITERAL_STRING(".r27")) ||
    //StringEndsWith(fileName, NS_LITERAL_STRING(".r28")) ||
    //StringEndsWith(fileName, NS_LITERAL_STRING(".r29")) ||
    //StringEndsWith(fileName, NS_LITERAL_STRING(".rar")) ||
    StringEndsWith(fileName, NS_LITERAL_STRING(".rb")) || // Ruby script
    StringEndsWith(fileName, NS_LITERAL_STRING(".reg")) || // Windows
    StringEndsWith(fileName, NS_LITERAL_STRING(".rels")) || // MS Office
    StringEndsWith(fileName, NS_LITERAL_STRING(".rgs")) || // InstallShield
    StringEndsWith(fileName, NS_LITERAL_STRING(".rpm")) || // Linux package
    StringEndsWith(fileName, NS_LITERAL_STRING(".rtf")) || // MS Office
    StringEndsWith(fileName, NS_LITERAL_STRING(".run")) || // Linux shell
    StringEndsWith(fileName, NS_LITERAL_STRING(".scf")) || // Windows shell
    StringEndsWith(fileName, NS_LITERAL_STRING(".scr")) || // Windows
    StringEndsWith(fileName, NS_LITERAL_STRING(".sct")) || // Windows shell
    StringEndsWith(fileName, NS_LITERAL_STRING(".search-ms")) || // Windows
    StringEndsWith(fileName, NS_LITERAL_STRING(".sh")) || // Linux shell
    StringEndsWith(fileName, NS_LITERAL_STRING(".shar")) || // Linux shell
    StringEndsWith(fileName, NS_LITERAL_STRING(".shb")) || // Windows
    StringEndsWith(fileName, NS_LITERAL_STRING(".shs")) || // Windows shell
    StringEndsWith(fileName, NS_LITERAL_STRING(".sldm")) || // MS PowerPoint
    StringEndsWith(fileName, NS_LITERAL_STRING(".sldx")) || // MS PowerPoint
    StringEndsWith(fileName, NS_LITERAL_STRING(".slp")) || // Linux package
    StringEndsWith(fileName, NS_LITERAL_STRING(".smi")) || // Mac disk image
    StringEndsWith(fileName, NS_LITERAL_STRING(".sparsebundle")) || // Mac disk image
    StringEndsWith(fileName, NS_LITERAL_STRING(".sparseimage")) || // Mac disk image
    StringEndsWith(fileName, NS_LITERAL_STRING(".spl")) || // Windows
    //StringEndsWith(fileName, NS_LITERAL_STRING(".squashfs")) ||
    StringEndsWith(fileName, NS_LITERAL_STRING(".svg")) ||
    StringEndsWith(fileName, NS_LITERAL_STRING(".swf")) || // Adobe Flash
    StringEndsWith(fileName, NS_LITERAL_STRING(".swm")) || // Windows Imaging
    StringEndsWith(fileName, NS_LITERAL_STRING(".sys")) || // Windows
    StringEndsWith(fileName, NS_LITERAL_STRING(".tar")) || // Linux archive
    StringEndsWith(fileName, NS_LITERAL_STRING(".taz")) || // Linux archive (bzip2)
    StringEndsWith(fileName, NS_LITERAL_STRING(".tbz")) || // Linux archive (bzip2)
    StringEndsWith(fileName, NS_LITERAL_STRING(".tbz2")) || // Linux archive (bzip2)
    StringEndsWith(fileName, NS_LITERAL_STRING(".tcsh")) || // Linux shell
    StringEndsWith(fileName, NS_LITERAL_STRING(".tgz")) || // Linux archive (gzip)
    //StringEndsWith(fileName, NS_LITERAL_STRING(".toast")) ||
    //StringEndsWith(fileName, NS_LITERAL_STRING(".torrent")) || // Bittorrent
    StringEndsWith(fileName, NS_LITERAL_STRING(".tpz")) || // Linux archive (gzip)
    StringEndsWith(fileName, NS_LITERAL_STRING(".txz")) || // Linux archive (xz)
    StringEndsWith(fileName, NS_LITERAL_STRING(".tz")) || // Linux archive (gzip)
    //StringEndsWith(fileName, NS_LITERAL_STRING(".u3p")) ||
    StringEndsWith(fileName, NS_LITERAL_STRING(".udf")) || // MS Excel
    StringEndsWith(fileName, NS_LITERAL_STRING(".udif")) || // Mac disk image
    StringEndsWith(fileName, NS_LITERAL_STRING(".url")) || // Windows
    //StringEndsWith(fileName, NS_LITERAL_STRING(".uu")) ||
    //StringEndsWith(fileName, NS_LITERAL_STRING(".uue")) ||
    StringEndsWith(fileName, NS_LITERAL_STRING(".vb")) || // Visual Basic script
    StringEndsWith(fileName, NS_LITERAL_STRING(".vbe")) || // Visual Basic script
    StringEndsWith(fileName, NS_LITERAL_STRING(".vbs")) || // Visual Basic script
    StringEndsWith(fileName, NS_LITERAL_STRING(".vbscript")) || // Visual Basic script
    StringEndsWith(fileName, NS_LITERAL_STRING(".vhd")) || // Windows virtual hard drive
    StringEndsWith(fileName, NS_LITERAL_STRING(".vhdx")) || // Windows virtual hard drive
    StringEndsWith(fileName, NS_LITERAL_STRING(".vmdk")) || // VMware virtual disk
    StringEndsWith(fileName, NS_LITERAL_STRING(".vsd")) || // MS Visio
    StringEndsWith(fileName, NS_LITERAL_STRING(".vsmacros")) || // MS Visual Studio
    StringEndsWith(fileName, NS_LITERAL_STRING(".vss")) || // MS Visio
    StringEndsWith(fileName, NS_LITERAL_STRING(".vst")) || // MS Visio
    StringEndsWith(fileName, NS_LITERAL_STRING(".vsw")) || // MS Visio
    StringEndsWith(fileName, NS_LITERAL_STRING(".website")) ||  // MSIE
    StringEndsWith(fileName, NS_LITERAL_STRING(".wim")) || // Windows Imaging
    StringEndsWith(fileName, NS_LITERAL_STRING(".workflow")) || // Mac Automator
    //StringEndsWith(fileName, NS_LITERAL_STRING(".wrc")) || // FreeArc archive
    StringEndsWith(fileName, NS_LITERAL_STRING(".ws")) || // Windows script
    StringEndsWith(fileName, NS_LITERAL_STRING(".wsc")) || // Windows script
    StringEndsWith(fileName, NS_LITERAL_STRING(".wsf")) || // Windows script
    StringEndsWith(fileName, NS_LITERAL_STRING(".wsh")) || // Windows script
    StringEndsWith(fileName, NS_LITERAL_STRING(".xar")) || // MS Excel
    StringEndsWith(fileName, NS_LITERAL_STRING(".xbap")) || // MS Silverlight
    //StringEndsWith(fileName, NS_LITERAL_STRING(".xip")) ||
    StringEndsWith(fileName, NS_LITERAL_STRING(".xlsm")) || // MS Excel
    StringEndsWith(fileName, NS_LITERAL_STRING(".xlsx")) || // MS Excel
    StringEndsWith(fileName, NS_LITERAL_STRING(".xltm")) || // MS Excel
    StringEndsWith(fileName, NS_LITERAL_STRING(".xltx")) || // MS Excel
    StringEndsWith(fileName, NS_LITERAL_STRING(".xml")) ||
    StringEndsWith(fileName, NS_LITERAL_STRING(".xnk")) || // MS Exchange
    StringEndsWith(fileName, NS_LITERAL_STRING(".xrm-ms")) || // Windows
    StringEndsWith(fileName, NS_LITERAL_STRING(".xsl")) || // XML Stylesheet
    //StringEndsWith(fileName, NS_LITERAL_STRING(".xxe")) ||
    StringEndsWith(fileName, NS_LITERAL_STRING(".xz")) || // Linux archive (xz)
    StringEndsWith(fileName, NS_LITERAL_STRING(".z")) || // InstallShield
#ifdef XP_WIN // disable on Mac/Linux, see 1167493
    StringEndsWith(fileName, NS_LITERAL_STRING(".zip")) || // Generic archive
#endif
    StringEndsWith(fileName, NS_LITERAL_STRING(".zipx")); // WinZip
    //StringEndsWith(fileName, NS_LITERAL_STRING(".zpaq"));
}

ClientDownloadRequest::DownloadType
PendingLookup::GetDownloadType(const nsAString& aFilename) {
  MOZ_ASSERT(IsBinaryFile());

  // From https://cs.chromium.org/chromium/src/chrome/common/safe_browsing/download_protection_util.cc?l=17
  if (StringEndsWith(aFilename, NS_LITERAL_STRING(".zip"))) {
    return ClientDownloadRequest::ZIPPED_EXECUTABLE;
  } else if (StringEndsWith(aFilename, NS_LITERAL_STRING(".apk"))) {
    return ClientDownloadRequest::ANDROID_APK;
  } else if (StringEndsWith(aFilename, NS_LITERAL_STRING(".app")) ||
             StringEndsWith(aFilename, NS_LITERAL_STRING(".cdr")) ||
             StringEndsWith(aFilename, NS_LITERAL_STRING(".dart")) ||
             StringEndsWith(aFilename, NS_LITERAL_STRING(".dc42")) ||
             StringEndsWith(aFilename, NS_LITERAL_STRING(".diskcopy42")) ||
             StringEndsWith(aFilename, NS_LITERAL_STRING(".dmg")) ||
             StringEndsWith(aFilename, NS_LITERAL_STRING(".dmgpart")) ||
             StringEndsWith(aFilename, NS_LITERAL_STRING(".dvdr")) ||
             StringEndsWith(aFilename, NS_LITERAL_STRING(".img")) ||
             StringEndsWith(aFilename, NS_LITERAL_STRING(".imgpart")) ||
             StringEndsWith(aFilename, NS_LITERAL_STRING(".iso")) ||
             StringEndsWith(aFilename, NS_LITERAL_STRING(".mpkg")) ||
             StringEndsWith(aFilename, NS_LITERAL_STRING(".ndif")) ||
             StringEndsWith(aFilename, NS_LITERAL_STRING(".osx")) ||
             StringEndsWith(aFilename, NS_LITERAL_STRING(".pkg")) ||
             StringEndsWith(aFilename, NS_LITERAL_STRING(".smi")) ||
             StringEndsWith(aFilename, NS_LITERAL_STRING(".sparsebundle")) ||
             StringEndsWith(aFilename, NS_LITERAL_STRING(".sparseimage")) ||
             StringEndsWith(aFilename, NS_LITERAL_STRING(".toast")) ||
             StringEndsWith(aFilename, NS_LITERAL_STRING(".udif"))) {
    return ClientDownloadRequest::MAC_EXECUTABLE;
  }

  return ClientDownloadRequest::WIN_EXECUTABLE; // default to Windows binaries
}

nsresult
PendingLookup::LookupNext()
{
  // We must call LookupNext or SendRemoteQuery upon return.
  // Look up all of the URLs that could allow or block this download.
  // Blocklist first.
  if (mBlocklistCount > 0) {
    return OnComplete(true, NS_OK,
                      nsIApplicationReputationService::VERDICT_DANGEROUS);
  }
  int index = mAnylistSpecs.Length() - 1;
  nsCString spec;
  if (index >= 0) {
    // Check the source URI, referrer and redirect chain.
    spec = mAnylistSpecs[index];
    mAnylistSpecs.RemoveElementAt(index);
    RefPtr<PendingDBLookup> lookup(new PendingDBLookup(this));
    return lookup->LookupSpec(spec, false);
  }
  // If any of mAnylistSpecs matched the blocklist, go ahead and block.
  if (mBlocklistCount > 0) {
    return OnComplete(true, NS_OK,
                      nsIApplicationReputationService::VERDICT_DANGEROUS);
  }
  // If any of mAnylistSpecs matched the allowlist, go ahead and pass.
  if (mAllowlistCount > 0) {
    return OnComplete(false, NS_OK);
  }
  // Only binary signatures remain.
  index = mAllowlistSpecs.Length() - 1;
  if (index >= 0) {
    spec = mAllowlistSpecs[index];
    LOG(("PendingLookup::LookupNext: checking %s on allowlist", spec.get()));
    mAllowlistSpecs.RemoveElementAt(index);
    RefPtr<PendingDBLookup> lookup(new PendingDBLookup(this));
    return lookup->LookupSpec(spec, true);
  }
  // There are no more URIs to check against local list. If the file is
  // not eligible for remote lookup, bail.
  if (!IsBinaryFile()) {
    LOG(("Not eligible for remote lookups [this=%x]", this));
    return OnComplete(false, NS_OK);
  }
  nsresult rv = SendRemoteQuery();
  if (NS_FAILED(rv)) {
    return OnComplete(false, rv);
  }
  return NS_OK;
}

nsCString
PendingLookup::EscapeCertificateAttribute(const nsACString& aAttribute)
{
  // Escape '/' because it's a field separator, and '%' because Chrome does
  nsCString escaped;
  escaped.SetCapacity(aAttribute.Length());
  for (unsigned int i = 0; i < aAttribute.Length(); ++i) {
    if (aAttribute.Data()[i] == '%') {
      escaped.AppendLiteral("%25");
    } else if (aAttribute.Data()[i] == '/') {
      escaped.AppendLiteral("%2F");
    } else if (aAttribute.Data()[i] == ' ') {
      escaped.AppendLiteral("%20");
    } else {
      escaped.Append(aAttribute.Data()[i]);
    }
  }
  return escaped;
}

nsCString
PendingLookup::EscapeFingerprint(const nsACString& aFingerprint)
{
  // Google's fingerprint doesn't have colons
  nsCString escaped;
  escaped.SetCapacity(aFingerprint.Length());
  for (unsigned int i = 0; i < aFingerprint.Length(); ++i) {
    if (aFingerprint.Data()[i] != ':') {
      escaped.Append(aFingerprint.Data()[i]);
    }
  }
  return escaped;
}

nsresult
PendingLookup::GenerateWhitelistStringsForPair(
  nsIX509Cert* certificate,
  nsIX509Cert* issuer)
{
  // The whitelist paths have format:
  // http://sb-ssl.google.com/safebrowsing/csd/certificate/<issuer_cert_fingerprint>[/CN=<cn>][/O=<org>][/OU=<unit>]
  // Any of CN, O, or OU may be omitted from the whitelist entry. Unfortunately
  // this is not publicly documented, but the Chrome implementation can be found
  // here:
  // https://code.google.com/p/chromium/codesearch#search/&q=GetCertificateWhitelistStrings
  nsCString whitelistString(
    "http://sb-ssl.google.com/safebrowsing/csd/certificate/");

  nsString fingerprint;
  nsresult rv = issuer->GetSha1Fingerprint(fingerprint);
  NS_ENSURE_SUCCESS(rv, rv);
  whitelistString.Append(
    EscapeFingerprint(NS_ConvertUTF16toUTF8(fingerprint)));

  nsString commonName;
  rv = certificate->GetCommonName(commonName);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!commonName.IsEmpty()) {
    whitelistString.AppendLiteral("/CN=");
    whitelistString.Append(
      EscapeCertificateAttribute(NS_ConvertUTF16toUTF8(commonName)));
  }

  nsString organization;
  rv = certificate->GetOrganization(organization);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!organization.IsEmpty()) {
    whitelistString.AppendLiteral("/O=");
    whitelistString.Append(
      EscapeCertificateAttribute(NS_ConvertUTF16toUTF8(organization)));
  }

  nsString organizationalUnit;
  rv = certificate->GetOrganizationalUnit(organizationalUnit);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!organizationalUnit.IsEmpty()) {
    whitelistString.AppendLiteral("/OU=");
    whitelistString.Append(
      EscapeCertificateAttribute(NS_ConvertUTF16toUTF8(organizationalUnit)));
  }
  LOG(("Whitelisting %s", whitelistString.get()));

  mAllowlistSpecs.AppendElement(whitelistString);
  return NS_OK;
}

nsresult
PendingLookup::GenerateWhitelistStringsForChain(
  const safe_browsing::ClientDownloadRequest_CertificateChain& aChain)
{
  // We need a signing certificate and an issuer to construct a whitelist
  // entry.
  if (aChain.element_size() < 2) {
    return NS_OK;
  }

  // Get the signer.
  nsresult rv;
  nsCOMPtr<nsIX509CertDB> certDB = do_GetService(NS_X509CERTDB_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIX509Cert> signer;
  rv = certDB->ConstructX509(
    const_cast<char *>(aChain.element(0).certificate().data()),
    aChain.element(0).certificate().size(), getter_AddRefs(signer));
  NS_ENSURE_SUCCESS(rv, rv);

  for (int i = 1; i < aChain.element_size(); ++i) {
    // Get the issuer.
    nsCOMPtr<nsIX509Cert> issuer;
    rv = certDB->ConstructX509(
      const_cast<char *>(aChain.element(i).certificate().data()),
      aChain.element(i).certificate().size(), getter_AddRefs(issuer));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = GenerateWhitelistStringsForPair(signer, issuer);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  return NS_OK;
}

nsresult
PendingLookup::GenerateWhitelistStrings()
{
  for (int i = 0; i < mRequest.signature().certificate_chain_size(); ++i) {
    nsresult rv = GenerateWhitelistStringsForChain(
      mRequest.signature().certificate_chain(i));
    NS_ENSURE_SUCCESS(rv, rv);
  }
  return NS_OK;
}

nsresult
PendingLookup::AddRedirects(nsIArray* aRedirects)
{
  uint32_t length = 0;
  aRedirects->GetLength(&length);
  LOG(("ApplicationReputation: Got %u redirects", length));
  nsCOMPtr<nsISimpleEnumerator> iter;
  nsresult rv = aRedirects->Enumerate(getter_AddRefs(iter));
  NS_ENSURE_SUCCESS(rv, rv);

  bool hasMoreRedirects = false;
  rv = iter->HasMoreElements(&hasMoreRedirects);
  NS_ENSURE_SUCCESS(rv, rv);

  while (hasMoreRedirects) {
    nsCOMPtr<nsISupports> supports;
    rv = iter->GetNext(getter_AddRefs(supports));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIPrincipal> principal = do_QueryInterface(supports, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIURI> uri;
    rv = principal->GetURI(getter_AddRefs(uri));
    NS_ENSURE_SUCCESS(rv, rv);

    // Add the spec to our list of local lookups. The most recent redirect is
    // the last element.
    nsCString spec;
    rv = GetStrippedSpec(uri, spec);
    NS_ENSURE_SUCCESS(rv, rv);
    mAnylistSpecs.AppendElement(spec);
    LOG(("ApplicationReputation: Appending redirect %s\n", spec.get()));

    // Store the redirect information in the remote request.
    ClientDownloadRequest_Resource* resource = mRequest.add_resources();
    resource->set_url(spec.get());
    resource->set_type(ClientDownloadRequest::DOWNLOAD_REDIRECT);

    rv = iter->HasMoreElements(&hasMoreRedirects);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  return NS_OK;
}

nsresult
PendingLookup::StartLookup()
{
  mStartTime = TimeStamp::Now();
  nsresult rv = DoLookupInternal();
  if (NS_FAILED(rv)) {
    return OnComplete(false, NS_OK);
  }
  return rv;
}

nsresult
PendingLookup::GetStrippedSpec(nsIURI* aUri, nsACString& escaped)
{
  // If aURI is not an nsIURL, we do not want to check the lists or send a
  // remote query.
  nsresult rv;
  nsCOMPtr<nsIURL> url = do_QueryInterface(aUri, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = url->GetScheme(escaped);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCString temp;
  rv = url->GetHostPort(temp);
  NS_ENSURE_SUCCESS(rv, rv);

  escaped.Append("://");
  escaped.Append(temp);

  rv = url->GetFilePath(temp);
  NS_ENSURE_SUCCESS(rv, rv);

  // nsIUrl.filePath starts with '/'
  escaped.Append(temp);

  return NS_OK;
}

nsresult
PendingLookup::DoLookupInternal()
{
  // We want to check the target URI, its referrer, and associated redirects
  // against the local lists.
  nsCOMPtr<nsIURI> uri;
  nsresult rv = mQuery->GetSourceURI(getter_AddRefs(uri));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCString sourceSpec;
  rv = GetStrippedSpec(uri, sourceSpec);
  NS_ENSURE_SUCCESS(rv, rv);

  mAnylistSpecs.AppendElement(sourceSpec);

  ClientDownloadRequest_Resource* resource = mRequest.add_resources();
  resource->set_url(sourceSpec.get());
  resource->set_type(ClientDownloadRequest::DOWNLOAD_URL);

  nsCOMPtr<nsIURI> referrer = nullptr;
  rv = mQuery->GetReferrerURI(getter_AddRefs(referrer));
  if (referrer) {
    nsCString referrerSpec;
    rv = GetStrippedSpec(referrer, referrerSpec);
    NS_ENSURE_SUCCESS(rv, rv);
    mAnylistSpecs.AppendElement(referrerSpec);
    resource->set_referrer(referrerSpec.get());
  }
  nsCOMPtr<nsIArray> redirects;
  rv = mQuery->GetRedirects(getter_AddRefs(redirects));
  if (redirects) {
    AddRedirects(redirects);
  } else {
    LOG(("ApplicationReputation: Got no redirects [this=%p]", this));
  }

  // Extract the signature and parse certificates so we can use it to check
  // whitelists.
  nsCOMPtr<nsIArray> sigArray;
  rv = mQuery->GetSignatureInfo(getter_AddRefs(sigArray));
  NS_ENSURE_SUCCESS(rv, rv);

  if (sigArray) {
    rv = ParseCertificates(sigArray);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  rv = GenerateWhitelistStrings();
  NS_ENSURE_SUCCESS(rv, rv);

  // Start the call chain.
  return LookupNext();
}

nsresult
PendingLookup::OnComplete(bool shouldBlock, nsresult rv, uint32_t verdict)
{
  MOZ_ASSERT(!shouldBlock ||
             verdict != nsIApplicationReputationService::VERDICT_SAFE);

  if (NS_FAILED(rv)) {
    nsAutoCString errorName;
    mozilla::GetErrorName(rv, errorName);
    LOG(("Failed sending remote query for application reputation "
         "[rv = %s, this = %p]", errorName.get(), this));
  }

  if (mTimeoutTimer) {
    mTimeoutTimer->Cancel();
    mTimeoutTimer = nullptr;
  }

  Accumulate(mozilla::Telemetry::APPLICATION_REPUTATION_SHOULD_BLOCK,
    shouldBlock);
  double t = (TimeStamp::Now() - mStartTime).ToMilliseconds();
  LOG(("Application Reputation verdict is %lu, obtained in %f ms [this = %p]",
       verdict, t, this));
  if (shouldBlock) {
    LOG(("Application Reputation check failed, blocking bad binary [this = %p]",
        this));
  } else {
    LOG(("Application Reputation check passed [this = %p]", this));
  }
  nsresult res = mCallback->OnComplete(shouldBlock, rv, verdict);
  return res;
}

nsresult
PendingLookup::ParseCertificates(nsIArray* aSigArray)
{
  // If we haven't been set for any reason, bail.
  NS_ENSURE_ARG_POINTER(aSigArray);

  // Binaries may be signed by multiple chains of certificates. If there are no
  // chains, the binary is unsigned (or we were unable to extract signature
  // information on a non-Windows platform)
  nsCOMPtr<nsISimpleEnumerator> chains;
  nsresult rv = aSigArray->Enumerate(getter_AddRefs(chains));
  NS_ENSURE_SUCCESS(rv, rv);

  bool hasMoreChains = false;
  rv = chains->HasMoreElements(&hasMoreChains);
  NS_ENSURE_SUCCESS(rv, rv);

  while (hasMoreChains) {
    nsCOMPtr<nsISupports> chainSupports;
    rv = chains->GetNext(getter_AddRefs(chainSupports));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIX509CertList> certList = do_QueryInterface(chainSupports, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    safe_browsing::ClientDownloadRequest_CertificateChain* certChain =
      mRequest.mutable_signature()->add_certificate_chain();
    nsCOMPtr<nsISimpleEnumerator> chainElt;
    rv = certList->GetEnumerator(getter_AddRefs(chainElt));
    NS_ENSURE_SUCCESS(rv, rv);

    // Each chain may have multiple certificates.
    bool hasMoreCerts = false;
    rv = chainElt->HasMoreElements(&hasMoreCerts);
    while (hasMoreCerts) {
      nsCOMPtr<nsISupports> certSupports;
      rv = chainElt->GetNext(getter_AddRefs(certSupports));
      NS_ENSURE_SUCCESS(rv, rv);

      nsCOMPtr<nsIX509Cert> cert = do_QueryInterface(certSupports, &rv);
      NS_ENSURE_SUCCESS(rv, rv);

      uint8_t* data = nullptr;
      uint32_t len = 0;
      rv = cert->GetRawDER(&len, &data);
      NS_ENSURE_SUCCESS(rv, rv);

      // Add this certificate to the protobuf to send remotely.
      certChain->add_element()->set_certificate(data, len);
      free(data);

      rv = chainElt->HasMoreElements(&hasMoreCerts);
      NS_ENSURE_SUCCESS(rv, rv);
    }
    rv = chains->HasMoreElements(&hasMoreChains);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  if (mRequest.signature().certificate_chain_size() > 0) {
    mRequest.mutable_signature()->set_trusted(true);
  }
  return NS_OK;
}

nsresult
PendingLookup::SendRemoteQuery()
{
  nsresult rv = SendRemoteQueryInternal();
  if (NS_FAILED(rv)) {
    return OnComplete(false, rv);
  }
  // SendRemoteQueryInternal has fired off the query and we call OnComplete in
  // the nsIStreamListener.onStopRequest.
  return rv;
}

nsresult
PendingLookup::SendRemoteQueryInternal()
{
  // If we aren't supposed to do remote lookups, bail.
  if (!Preferences::GetBool(PREF_SB_DOWNLOADS_REMOTE_ENABLED, false)) {
    LOG(("Remote lookups are disabled [this = %p]", this));
    return NS_ERROR_NOT_AVAILABLE;
  }
  // If the remote lookup URL is empty or absent, bail.
  nsCString serviceUrl;
  NS_ENSURE_SUCCESS(Preferences::GetCString(PREF_SB_APP_REP_URL, &serviceUrl),
                    NS_ERROR_NOT_AVAILABLE);
  if (serviceUrl.IsEmpty()) {
    LOG(("Remote lookup URL is empty [this = %p]", this));
    return NS_ERROR_NOT_AVAILABLE;
  }

  // If the blocklist or allowlist is empty (so we couldn't do local lookups),
  // bail
  {
    nsAutoCString table;
    NS_ENSURE_SUCCESS(Preferences::GetCString(PREF_DOWNLOAD_BLOCK_TABLE,
                                              &table),
                      NS_ERROR_NOT_AVAILABLE);
    if (table.IsEmpty()) {
      LOG(("Blocklist is empty [this = %p]", this));
      return NS_ERROR_NOT_AVAILABLE;
    }
  }
#ifdef XP_WIN
  // The allowlist is only needed to do signature verification on Windows
  {
    nsAutoCString table;
    NS_ENSURE_SUCCESS(Preferences::GetCString(PREF_DOWNLOAD_ALLOW_TABLE,
                                              &table),
                      NS_ERROR_NOT_AVAILABLE);
    if (table.IsEmpty()) {
      LOG(("Allowlist is empty [this = %p]", this));
      return NS_ERROR_NOT_AVAILABLE;
    }
  }
#endif

  LOG(("Sending remote query for application reputation [this = %p]",
       this));
  // We did not find a local result, so fire off the query to the
  // application reputation service.
  nsCOMPtr<nsIURI> uri;
  nsresult rv;
  rv = mQuery->GetSourceURI(getter_AddRefs(uri));
  NS_ENSURE_SUCCESS(rv, rv);
  nsCString spec;
  rv = GetStrippedSpec(uri, spec);
  NS_ENSURE_SUCCESS(rv, rv);
  mRequest.set_url(spec.get());

  uint32_t fileSize;
  rv = mQuery->GetFileSize(&fileSize);
  NS_ENSURE_SUCCESS(rv, rv);
  mRequest.set_length(fileSize);
  // We have no way of knowing whether or not a user initiated the
  // download. Set it to true to lessen the chance of false positives.
  mRequest.set_user_initiated(true);

  nsCString locale;
  NS_ENSURE_SUCCESS(Preferences::GetCString(PREF_GENERAL_LOCALE, &locale),
                    NS_ERROR_NOT_AVAILABLE);
  mRequest.set_locale(locale.get());
  nsCString sha256Hash;
  rv = mQuery->GetSha256Hash(sha256Hash);
  NS_ENSURE_SUCCESS(rv, rv);
  mRequest.mutable_digests()->set_sha256(sha256Hash.Data());
  nsString fileName;
  rv = mQuery->GetSuggestedFileName(fileName);
  NS_ENSURE_SUCCESS(rv, rv);
  mRequest.set_file_basename(NS_ConvertUTF16toUTF8(fileName).get());
  mRequest.set_download_type(GetDownloadType(fileName));

  if (mRequest.signature().trusted()) {
    LOG(("Got signed binary for remote application reputation check "
         "[this = %p]", this));
  } else {
    LOG(("Got unsigned binary for remote application reputation check "
         "[this = %p]", this));
  }

  // Serialize the protocol buffer to a string. This can only fail if we are
  // out of memory, or if the protocol buffer req is missing required fields
  // (only the URL for now).
  std::string serialized;
  if (!mRequest.SerializeToString(&serialized)) {
    return NS_ERROR_UNEXPECTED;
  }
  LOG(("Serialized protocol buffer [this = %p]: (length=%d) %s", this,
       serialized.length(), serialized.c_str()));

  // Set the input stream to the serialized protocol buffer
  nsCOMPtr<nsIStringInputStream> sstream =
    do_CreateInstance("@mozilla.org/io/string-input-stream;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = sstream->SetData(serialized.c_str(), serialized.length());
  NS_ENSURE_SUCCESS(rv, rv);

  // Set up the channel to transmit the request to the service.
  nsCOMPtr<nsIIOService> ios = do_GetService(NS_IOSERVICE_CONTRACTID, &rv);
  rv = ios->NewChannel2(serviceUrl,
                        nullptr,
                        nullptr,
                        nullptr, // aLoadingNode
                        nsContentUtils::GetSystemPrincipal(),
                        nullptr, // aTriggeringPrincipal
                        nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_DATA_IS_NULL,
                        nsIContentPolicy::TYPE_OTHER,
                        getter_AddRefs(mChannel));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsILoadInfo> loadInfo = mChannel->GetLoadInfo();
  if (loadInfo) {
    loadInfo->SetOriginAttributes(
      mozilla::NeckoOriginAttributes(NECKO_SAFEBROWSING_APP_ID, false));
  }

  nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(mChannel, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  // Upload the protobuf to the application reputation service.
  nsCOMPtr<nsIUploadChannel2> uploadChannel = do_QueryInterface(mChannel, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = uploadChannel->ExplicitSetUploadStream(sstream,
    NS_LITERAL_CSTRING("application/octet-stream"), serialized.size(),
    NS_LITERAL_CSTRING("POST"), false);
  NS_ENSURE_SUCCESS(rv, rv);

  // Set the Safebrowsing cookie jar, so that the regular Google cookie is not
  // sent with this request. See bug 897516.
  nsCOMPtr<nsIInterfaceRequestor> loadContext =
    new mozilla::LoadContext(NECKO_SAFEBROWSING_APP_ID);
  rv = mChannel->SetNotificationCallbacks(loadContext);
  NS_ENSURE_SUCCESS(rv, rv);

  uint32_t timeoutMs = Preferences::GetUint(PREF_SB_DOWNLOADS_REMOTE_TIMEOUT, 10000);
  mTimeoutTimer = do_CreateInstance(NS_TIMER_CONTRACTID);
  mTimeoutTimer->InitWithCallback(this, timeoutMs, nsITimer::TYPE_ONE_SHOT);

  rv = mChannel->AsyncOpen2(this);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
PendingLookup::Notify(nsITimer* aTimer)
{
  LOG(("Remote lookup timed out [this = %p]", this));
  MOZ_ASSERT(aTimer == mTimeoutTimer);
  Accumulate(mozilla::Telemetry::APPLICATION_REPUTATION_REMOTE_LOOKUP_TIMEOUT,
    true);
  mChannel->Cancel(NS_ERROR_NET_TIMEOUT);
  mTimeoutTimer->Cancel();
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
//// nsIStreamListener
static nsresult
AppendSegmentToString(nsIInputStream* inputStream,
                      void *closure,
                      const char *rawSegment,
                      uint32_t toOffset,
                      uint32_t count,
                      uint32_t *writeCount) {
  nsAutoCString* decodedData = static_cast<nsAutoCString*>(closure);
  decodedData->Append(rawSegment, count);
  *writeCount = count;
  return NS_OK;
}

NS_IMETHODIMP
PendingLookup::OnDataAvailable(nsIRequest *aRequest,
                               nsISupports *aContext,
                               nsIInputStream *aStream,
                               uint64_t offset,
                               uint32_t count) {
  uint32_t read;
  return aStream->ReadSegments(AppendSegmentToString, &mResponse, count, &read);
}

NS_IMETHODIMP
PendingLookup::OnStartRequest(nsIRequest *aRequest,
                              nsISupports *aContext) {
  return NS_OK;
}

NS_IMETHODIMP
PendingLookup::OnStopRequest(nsIRequest *aRequest,
                             nsISupports *aContext,
                             nsresult aResult) {
  NS_ENSURE_STATE(mCallback);

  bool shouldBlock = false;
  uint32_t verdict = nsIApplicationReputationService::VERDICT_SAFE;
  Accumulate(mozilla::Telemetry::APPLICATION_REPUTATION_REMOTE_LOOKUP_TIMEOUT,
    false);

  nsresult rv = OnStopRequestInternal(aRequest, aContext, aResult,
                                      &shouldBlock, &verdict);
  OnComplete(shouldBlock, rv, verdict);
  return rv;
}

nsresult
PendingLookup::OnStopRequestInternal(nsIRequest *aRequest,
                                     nsISupports *aContext,
                                     nsresult aResult,
                                     bool* aShouldBlock,
                                     uint32_t* aVerdict) {
  if (NS_FAILED(aResult)) {
    Accumulate(mozilla::Telemetry::APPLICATION_REPUTATION_SERVER,
      SERVER_RESPONSE_FAILED);
    return aResult;
  }

  *aShouldBlock = false;
  *aVerdict = nsIApplicationReputationService::VERDICT_SAFE;
  nsresult rv;
  nsCOMPtr<nsIHttpChannel> channel = do_QueryInterface(aRequest, &rv);
  if (NS_FAILED(rv)) {
    Accumulate(mozilla::Telemetry::APPLICATION_REPUTATION_SERVER,
      SERVER_RESPONSE_FAILED);
    return rv;
  }

  uint32_t status = 0;
  rv = channel->GetResponseStatus(&status);
  if (NS_FAILED(rv)) {
    Accumulate(mozilla::Telemetry::APPLICATION_REPUTATION_SERVER,
      SERVER_RESPONSE_FAILED);
    return rv;
  }

  if (status != 200) {
    Accumulate(mozilla::Telemetry::APPLICATION_REPUTATION_SERVER,
      SERVER_RESPONSE_FAILED);
    return NS_ERROR_NOT_AVAILABLE;
  }

  std::string buf(mResponse.Data(), mResponse.Length());
  safe_browsing::ClientDownloadResponse response;
  if (!response.ParseFromString(buf)) {
    LOG(("Invalid protocol buffer response [this = %p]: %s", this, buf.c_str()));
    Accumulate(mozilla::Telemetry::APPLICATION_REPUTATION_SERVER,
                                   SERVER_RESPONSE_INVALID);
    return NS_ERROR_CANNOT_CONVERT_DATA;
  }

  Accumulate(mozilla::Telemetry::APPLICATION_REPUTATION_SERVER,
    SERVER_RESPONSE_VALID);
  // Clamp responses 0-7, we only know about 0-4 for now.
  Accumulate(mozilla::Telemetry::APPLICATION_REPUTATION_SERVER_VERDICT,
    std::min<uint32_t>(response.verdict(), 7));
  switch(response.verdict()) {
    case safe_browsing::ClientDownloadResponse::DANGEROUS:
      *aShouldBlock = Preferences::GetBool(PREF_BLOCK_DANGEROUS, true);
      *aVerdict = nsIApplicationReputationService::VERDICT_DANGEROUS;
      break;
    case safe_browsing::ClientDownloadResponse::DANGEROUS_HOST:
      *aShouldBlock = Preferences::GetBool(PREF_BLOCK_DANGEROUS_HOST, true);
      *aVerdict = nsIApplicationReputationService::VERDICT_DANGEROUS_HOST;
      break;
    case safe_browsing::ClientDownloadResponse::POTENTIALLY_UNWANTED:
      *aShouldBlock = Preferences::GetBool(PREF_BLOCK_POTENTIALLY_UNWANTED, false);
      *aVerdict = nsIApplicationReputationService::VERDICT_POTENTIALLY_UNWANTED;
      break;
    case safe_browsing::ClientDownloadResponse::UNCOMMON:
      *aShouldBlock = Preferences::GetBool(PREF_BLOCK_UNCOMMON, false);
      *aVerdict = nsIApplicationReputationService::VERDICT_UNCOMMON;
      break;
    default:
      // Treat everything else as safe
      break;
  }

  return NS_OK;
}

NS_IMPL_ISUPPORTS(ApplicationReputationService,
                  nsIApplicationReputationService)

ApplicationReputationService*
  ApplicationReputationService::gApplicationReputationService = nullptr;

ApplicationReputationService*
ApplicationReputationService::GetSingleton()
{
  if (gApplicationReputationService) {
    NS_ADDREF(gApplicationReputationService);
    return gApplicationReputationService;
  }

  // We're not initialized yet.
  gApplicationReputationService = new ApplicationReputationService();
  if (gApplicationReputationService) {
    NS_ADDREF(gApplicationReputationService);
  }

  return gApplicationReputationService;
}

ApplicationReputationService::ApplicationReputationService()
{
  LOG(("Application reputation service started up"));
}

ApplicationReputationService::~ApplicationReputationService() {
  LOG(("Application reputation service shutting down"));
}

NS_IMETHODIMP
ApplicationReputationService::QueryReputation(
    nsIApplicationReputationQuery* aQuery,
    nsIApplicationReputationCallback* aCallback) {
  LOG(("Starting application reputation check [query=%p]", aQuery));
  NS_ENSURE_ARG_POINTER(aQuery);
  NS_ENSURE_ARG_POINTER(aCallback);

  Accumulate(mozilla::Telemetry::APPLICATION_REPUTATION_COUNT, true);
  nsresult rv = QueryReputationInternal(aQuery, aCallback);
  if (NS_FAILED(rv)) {
    Accumulate(mozilla::Telemetry::APPLICATION_REPUTATION_SHOULD_BLOCK,
      false);
    aCallback->OnComplete(false, rv,
                          nsIApplicationReputationService::VERDICT_SAFE);
  }
  return NS_OK;
}

nsresult ApplicationReputationService::QueryReputationInternal(
  nsIApplicationReputationQuery* aQuery,
  nsIApplicationReputationCallback* aCallback) {
  nsresult rv;
  // If malware checks aren't enabled, don't query application reputation.
  if (!Preferences::GetBool(PREF_SB_MALWARE_ENABLED, false)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  if (!Preferences::GetBool(PREF_SB_DOWNLOADS_ENABLED, false)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsCOMPtr<nsIURI> uri;
  rv = aQuery->GetSourceURI(getter_AddRefs(uri));
  NS_ENSURE_SUCCESS(rv, rv);
  // Bail if the URI hasn't been set.
  NS_ENSURE_STATE(uri);

  // Create a new pending lookup and start the call chain.
  RefPtr<PendingLookup> lookup(new PendingLookup(aQuery, aCallback));
  NS_ENSURE_STATE(lookup);

  return lookup->StartLookup();
}
