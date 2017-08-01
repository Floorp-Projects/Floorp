/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
// See
// https://wiki.mozilla.org/Security/Features/Application_Reputation_Design_Doc
// for a description of Chrome's implementation of this feature.
#include "ApplicationReputation.h"
#include "chrome/common/safe_browsing/csd.pb.h"

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

#include "mozilla/ArrayUtils.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/ErrorNames.h"
#include "mozilla/LoadContext.h"
#include "mozilla/Preferences.h"
#include "mozilla/Services.h"
#include "mozilla/Telemetry.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/intl/LocaleService.h"

#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsDebug.h"
#include "nsDependentSubstring.h"
#include "nsError.h"
#include "nsNetCID.h"
#include "nsReadableUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nsThreadUtils.h"

#include "nsIContentPolicy.h"
#include "nsICryptoHash.h"
#include "nsILoadInfo.h"
#include "nsContentUtils.h"
#include "nsWeakReference.h"
#include "nsIRedirectHistoryEntry.h"

using mozilla::ArrayLength;
using mozilla::BasePrincipal;
using mozilla::OriginAttributes;
using mozilla::Preferences;
using mozilla::TimeStamp;
using mozilla::Telemetry::Accumulate;
using mozilla::intl::LocaleService;
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
                            public nsITimerCallback,
                            public nsIObserver,
                            public nsSupportsWeakReference
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSITIMERCALLBACK
  NS_DECL_NSIOBSERVER

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

  // Return the hex-encoded hash of the whole URI.
  nsresult GetSpecHash(nsACString& aSpec, nsACString& hexEncodedHash);

  // Strip url parameters, fragments, and user@pass fields from the URI spec
  // using nsIURL. Hash data URIs and return blob URIs unfiltered.
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
    nsAutoCString errorName;
    mozilla::GetErrorName(rv, errorName);
    LOG(("Error in LookupSpecInternal() [rv = %s, this = %p]",
         errorName.get(), this));
    return mPendingLookup->LookupNext(); // ignore this lookup and move to next
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

  OriginAttributes attrs;
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
  Preferences::GetCString(PREF_DOWNLOAD_ALLOW_TABLE, allowlist);
  if (!allowlist.IsEmpty()) {
    tables.Append(allowlist);
  }
  nsAutoCString blocklist;
  Preferences::GetCString(PREF_DOWNLOAD_BLOCK_TABLE, blocklist);
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
  Preferences::GetCString(PREF_DOWNLOAD_BLOCK_TABLE, blockList);
  if (!mAllowlistOnly && FindInReadable(blockList, tables)) {
    mPendingLookup->mBlocklistCount++;
    Accumulate(mozilla::Telemetry::APPLICATION_REPUTATION_LOCAL, BLOCK_LIST);
    LOG(("Found principal %s on blocklist [this = %p]", mSpec.get(), this));
    return mPendingLookup->OnComplete(true, NS_OK,
      nsIApplicationReputationService::VERDICT_DANGEROUS);
  }

  nsAutoCString allowList;
  Preferences::GetCString(PREF_DOWNLOAD_ALLOW_TABLE, allowList);
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
                  nsIRequestObserver,
                  nsIObserver,
                  nsISupportsWeakReference)

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

static const char16_t* const kBinaryFileExtensions[] = {
    // Extracted from the "File Type Policies" Chrome extension
    //u".001",
    //u".7z",
    //u".ace",
    //u".action", // Mac script
    //u".ad", // Windows
    u".ade", // MS Access
    u".adp", // MS Access
    u".apk", // Android package
    u".app", // Executable application
    u".application", // MS ClickOnce
    u".appref-ms", // MS ClickOnce
    //u".arc",
    //u".arj",
    u".as", // Mac archive
    u".asp", // Windows Server script
    u".asx", // Windows Media Player
    //u".b64",
    //u".balz",
    u".bas", // Basic script
    u".bash", // Linux shell
    u".bat", // Windows shell
    //u".bhx",
    //u".bin",
    u".bz", // Linux archive (bzip)
    u".bz2", // Linux archive (bzip2)
    u".bzip2", // Linux archive (bzip2)
    u".cab", // Windows archive
    u".cdr", // Mac disk image
    u".cfg", // Windows
    u".chi", // Windows Help
    u".chm", // Windows Help
    u".class", // Java
    u".cmd", // Windows executable
    u".com", // Windows executable
    u".command", // Mac script
    u".cpgz", // Mac archive
    //u".cpio",
    u".cpl", // Windows executable
    u".crt", // Windows signed certificate
    u".crx", // Chrome extensions
    u".csh", // Linux shell
    u".dart", // Mac disk image
    u".dc42", // Apple DiskCopy Image
    u".deb", // Linux package
    u".dex", // Android
    u".diskcopy42", // Apple DiskCopy Image
    u".dll", // Windows executable
    u".dmg", // Mac disk image
    u".dmgpart", // Mac disk image
    //u".docb", // MS Office
    //u".docm", // MS Word
    //u".docx", // MS Word
    //u".dotm", // MS Word
    //u".dott", // MS Office
    u".drv", // Windows driver
    u".dvdr", // Mac Disk image
    u".efi", // Firmware
    u".eml", // MS Outlook
    u".exe", // Windows executable
    //u".fat",
    u".fon", // Windows font
    u".fxp", // MS FoxPro
    u".gadget", // Windows
    u".grp", // Windows
    u".gz", // Linux archive (gzip)
    u".gzip", // Linux archive (gzip)
    u".hfs", // Mac disk image
    u".hlp", // Windows Help
    u".hqx", // Mac archive
    u".hta", // HTML trusted application
    u".htm",
    u".html",
    u".htt", // MS HTML template
    u".img", // Mac disk image
    u".imgpart", // Mac disk image
    u".inf", // Windows installer
    u".ini", // Generic config file
    u".ins", // IIS config
    //u".inx", // InstallShield
    u".iso", // CD image
    u".isp", // IIS config
    //u".isu", // InstallShield
    u".jar", // Java
    u".jnlp", // Java
    //u".job", // Windows
    u".js", // JavaScript script
    u".jse", // JScript
    u".ksh", // Linux shell
    //u".lha",
    u".lnk", // Windows
    u".local", // Windows
    //u".lpaq1",
    //u".lpaq5",
    //u".lpaq8",
    //u".lzh",
    //u".lzma",
    u".mad", // MS Access
    u".maf", // MS Access
    u".mag", // MS Access
    u".mam", // MS Access
    u".manifest", // Windows
    u".maq", // MS Access
    u".mar", // MS Access
    u".mas", // MS Access
    u".mat", // MS Access
    u".mau", // Media attachment
    u".mav", // MS Access
    u".maw", // MS Access
    u".mda", // MS Access
    u".mdb", // MS Access
    u".mde", // MS Access
    u".mdt", // MS Access
    u".mdw", // MS Access
    u".mdz", // MS Access
    u".mht", // MS HTML
    u".mhtml", // MS HTML
    u".mim", // MS Mail
    u".mmc", // MS Office
    u".mof", // Windows
    u".mpkg", // Mac installer
    u".msc", // Windows executable
    u".msg", // MS Outlook
    u".msh", // Windows shell
    u".msh1", // Windows shell
    u".msh1xml", // Windows shell
    u".msh2", // Windows shell
    u".msh2xml", // Windows shell
    u".mshxml", // Windows
    u".msi", // Windows installer
    u".msp", // Windows installer
    u".mst", // Windows installer
    u".ndif", // Mac disk image
    //u".ntfs", // 7z
    u".ocx", // ActiveX
    u".ops", // MS Office
    //u".out", // Linux binary
    //u".paf", // PortableApps package
    //u".paq8f",
    //u".paq8jd",
    //u".paq8l",
    //u".paq8o",
    u".partial", // Downloads
    u".pax", // Mac archive
    u".pcd", // Microsoft Visual Test
    u".pdf", // Adobe Acrobat
    //u".pea",
    u".pet", // Linux package
    u".pif", // Windows
    u".pkg", // Mac installer
    u".pl", // Perl script
    u".plg", // MS Visual Studio
    //u".potx", // MS PowerPoint
    //u".ppam", // MS PowerPoint
    //u".ppsx", // MS PowerPoint
    //u".pptm", // MS PowerPoint
    //u".pptx", // MS PowerPoint
    u".prf", // MS Outlook
    u".prg", // Windows
    u".ps1", // Windows shell
    u".ps1xml", // Windows shell
    u".ps2", // Windows shell
    u".ps2xml", // Windows shell
    u".psc1", // Windows shell
    u".psc2", // Windows shell
    u".pst", // MS Outlook
    u".pup", // Linux package
    u".py", // Python script
    u".pyc", // Python binary
    u".pyw", // Python GUI
    //u".quad",
    //u".r00",
    //u".r01",
    //u".r02",
    //u".r03",
    //u".r04",
    //u".r05",
    //u".r06",
    //u".r07",
    //u".r08",
    //u".r09",
    //u".r10",
    //u".r11",
    //u".r12",
    //u".r13",
    //u".r14",
    //u".r15",
    //u".r16",
    //u".r17",
    //u".r18",
    //u".r19",
    //u".r20",
    //u".r21",
    //u".r22",
    //u".r23",
    //u".r24",
    //u".r25",
    //u".r26",
    //u".r27",
    //u".r28",
    //u".r29",
    //u".rar",
    u".rb", // Ruby script
    u".reg", // Windows Registry
    u".rels", // MS Office
    //u".rgs", // Windows Registry
    u".rpm", // Linux package
    //u".rtf", // MS Office
    //u".run", // Linux shell
    u".scf", // Windows shell
    u".scr", // Windows
    u".sct", // Windows shell
    u".search-ms", // Windows
    u".sh", // Linux shell
    u".shar", // Linux shell
    u".shb", // Windows
    u".shs", // Windows shell
    //u".sldm", // MS PowerPoint
    //u".sldx", // MS PowerPoint
    u".slp", // Linux package
    u".smi", // Mac disk image
    u".sparsebundle", // Mac disk image
    u".sparseimage", // Mac disk image
    u".spl", // Adobe Flash
    //u".squashfs",
    u".svg",
    u".swf", // Adobe Flash
    u".swm", // Windows Imaging
    u".sys", // Windows
    u".tar", // Linux archive
    u".taz", // Linux archive (bzip2)
    u".tbz", // Linux archive (bzip2)
    u".tbz2", // Linux archive (bzip2)
    u".tcsh", // Linux shell
    u".tgz", // Linux archive (gzip)
    //u".toast", // Roxio disk image
    //u".torrent", // Bittorrent
    u".tpz", // Linux archive (gzip)
    u".txz", // Linux archive (xz)
    u".tz", // Linux archive (gzip)
    //u".u3p", // U3 Smart Apps
    u".udf", // MS Excel
    u".udif", // Mac disk image
    u".url", // Windows
    //u".uu",
    //u".uue",
    u".vb", // Visual Basic script
    u".vbe", // Visual Basic script
    u".vbs", // Visual Basic script
    //u".vbscript", // Visual Basic script
    u".vhd", // Windows virtual hard drive
    u".vhdx", // Windows virtual hard drive
    u".vmdk", // VMware virtual disk
    u".vsd", // MS Visio
    u".vsmacros", // MS Visual Studio
    u".vss", // MS Visio
    u".vst", // MS Visio
    u".vsw", // MS Visio
    u".website",  // Windows
    u".wim", // Windows Imaging
    //u".workflow", // Mac Automator
    //u".wrc", // FreeArc archive
    u".ws", // Windows script
    u".wsc", // Windows script
    u".wsf", // Windows script
    u".wsh", // Windows script
    u".xar", // MS Excel
    u".xbap", // XAML Browser Application
    u".xip", // Mac archive
    //u".xlsm", // MS Excel
    //u".xlsx", // MS Excel
    //u".xltm", // MS Excel
    //u".xltx", // MS Excel
    u".xml",
    u".xnk", // MS Exchange
    u".xrm-ms", // Windows
    u".xsl", // XML Stylesheet
    //u".xxe",
    u".xz", // Linux archive (xz)
    u".z", // InstallShield
#ifdef XP_WIN // disable on Mac/Linux, see 1167493
    u".zip", // Generic archive
#endif
    u".zipx", // WinZip
    //u".zpaq",
};

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

  for (size_t i = 0; i < ArrayLength(kBinaryFileExtensions); ++i) {
    if (StringEndsWith(fileName, nsDependentString(kBinaryFileExtensions[i]))) {
      return true;
    }
  }

  return false;
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
    LOG(("Not eligible for remote lookups [this=%p]", this));
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
  nsDependentCSubstring signerDER(
    const_cast<char *>(aChain.element(0).certificate().data()),
    aChain.element(0).certificate().size());
  rv = certDB->ConstructX509(signerDER, getter_AddRefs(signer));
  NS_ENSURE_SUCCESS(rv, rv);

  for (int i = 1; i < aChain.element_size(); ++i) {
    // Get the issuer.
    nsCOMPtr<nsIX509Cert> issuer;
    nsDependentCSubstring issuerDER(
      const_cast<char *>(aChain.element(i).certificate().data()),
      aChain.element(i).certificate().size());
    rv = certDB->ConstructX509(issuerDER, getter_AddRefs(issuer));
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

    nsCOMPtr<nsIRedirectHistoryEntry> redirectEntry = do_QueryInterface(supports, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIPrincipal> principal;
    rv = redirectEntry->GetPrincipal(getter_AddRefs(principal));
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
PendingLookup::GetSpecHash(nsACString& aSpec, nsACString& hexEncodedHash)
{
  nsresult rv;

  nsCOMPtr<nsICryptoHash> cryptoHash =
    do_CreateInstance("@mozilla.org/security/hash;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = cryptoHash->Init(nsICryptoHash::SHA256);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = cryptoHash->Update(reinterpret_cast<const uint8_t*>(aSpec.BeginReading()),
                          aSpec.Length());
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString binaryHash;
  rv = cryptoHash->Finish(false, binaryHash);
  NS_ENSURE_SUCCESS(rv, rv);

  // This needs to match HexEncode() in Chrome's
  // src/base/strings/string_number_conversions.cc
  static const char* const hex = "0123456789ABCDEF";
  hexEncodedHash.SetCapacity(2 * binaryHash.Length());
  for (size_t i = 0; i < binaryHash.Length(); ++i) {
    auto c = static_cast<const unsigned char>(binaryHash[i]);
    hexEncodedHash.Append(hex[(c >> 4) & 0x0F]);
    hexEncodedHash.Append(hex[c & 0x0F]);
  }

  return NS_OK;
}

nsresult
PendingLookup::GetStrippedSpec(nsIURI* aUri, nsACString& escaped)
{
  if (NS_WARN_IF(!aUri)) {
    return NS_ERROR_INVALID_ARG;
  }

  nsresult rv;
  rv = aUri->GetScheme(escaped);
  NS_ENSURE_SUCCESS(rv, rv);

  if (escaped.EqualsLiteral("blob")) {
    aUri->GetSpec(escaped);
    LOG(("PendingLookup::GetStrippedSpec(): blob URL left unstripped as '%s' [this = %p]",
         PromiseFlatCString(escaped).get(), this));
    return NS_OK;

  } else if (escaped.EqualsLiteral("data")) {
    // Replace URI with "data:<everything before comma>,SHA256(<whole URI>)"
    aUri->GetSpec(escaped);
    int32_t comma = escaped.FindChar(',');
    if (comma > -1 &&
        static_cast<nsCString::size_type>(comma) < escaped.Length() - 1) {
      MOZ_ASSERT(comma > 4, "Data URIs start with 'data:'");
      nsAutoCString hexEncodedHash;
      rv = GetSpecHash(escaped, hexEncodedHash);
      if (NS_SUCCEEDED(rv)) {
        escaped.Truncate(comma + 1);
        escaped.Append(hexEncodedHash);
      }
    }

    LOG(("PendingLookup::GetStrippedSpec(): data URL stripped to '%s' [this = %p]",
         PromiseFlatCString(escaped).get(), this));
    return NS_OK;
  }

  // If aURI is not an nsIURL, we do not want to check the lists or send a
  // remote query.
  nsCOMPtr<nsIURL> url = do_QueryInterface(aUri, &rv);
  if (NS_FAILED(rv)) {
    LOG(("PendingLookup::GetStrippedSpec(): scheme '%s' is not supported [this = %p]",
         PromiseFlatCString(escaped).get(), this));
    return rv;
  }

  nsCString temp;
  rv = url->GetHostPort(temp);
  NS_ENSURE_SUCCESS(rv, rv);

  escaped.Append("://");
  escaped.Append(temp);

  rv = url->GetFilePath(temp);
  NS_ENSURE_SUCCESS(rv, rv);

  // nsIUrl.filePath starts with '/'
  escaped.Append(temp);

  LOG(("PendingLookup::GetStrippedSpec(): URL stripped to '%s' [this = %p]",
       PromiseFlatCString(escaped).get(), this));
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
  LOG(("Application Reputation verdict is %u, obtained in %f ms [this = %p]",
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
  nsAutoCString serviceUrl;
  NS_ENSURE_SUCCESS(Preferences::GetCString(PREF_SB_APP_REP_URL, serviceUrl),
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
                                              table),
                      NS_ERROR_NOT_AVAILABLE);
    if (table.IsEmpty()) {
      LOG(("Blocklist is empty [this = %p]", this));
      return NS_ERROR_NOT_AVAILABLE;
    }
  }
  {
    nsAutoCString table;
    NS_ENSURE_SUCCESS(Preferences::GetCString(PREF_DOWNLOAD_ALLOW_TABLE,
                                              table),
                      NS_ERROR_NOT_AVAILABLE);
    if (table.IsEmpty()) {
      LOG(("Allowlist is empty [this = %p]", this));
      return NS_ERROR_NOT_AVAILABLE;
    }
  }

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
  rv = LocaleService::GetInstance()->GetAppLocaleAsLangTag(locale);
  NS_ENSURE_SUCCESS(rv, rv);
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
  LOG(("Serialized protocol buffer [this = %p]: (length=%zu) %s", this,
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
    mozilla::OriginAttributes attrs;
    attrs.mFirstPartyDomain.AssignLiteral(NECKO_SAFEBROWSING_FIRST_PARTY_DOMAIN);
    loadInfo->SetOriginAttributes(attrs);
  }

  nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(mChannel, &rv));
  NS_ENSURE_SUCCESS(rv, rv);
  mozilla::Unused << httpChannel;

  // Upload the protobuf to the application reputation service.
  nsCOMPtr<nsIUploadChannel2> uploadChannel = do_QueryInterface(mChannel, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = uploadChannel->ExplicitSetUploadStream(sstream,
    NS_LITERAL_CSTRING("application/octet-stream"), serialized.size(),
    NS_LITERAL_CSTRING("POST"), false);
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

///////////////////////////////////////////////////////////////////////////////
// nsIObserver implementation
NS_IMETHODIMP
PendingLookup::Observe(nsISupports *aSubject, const char *aTopic,
                       const char16_t *aData)
{
  if (!strcmp(aTopic, "quit-application")) {
    if (mTimeoutTimer) {
      mTimeoutTimer->Cancel();
      mTimeoutTimer = nullptr;
    }
    if (mChannel) {
      mChannel->Cancel(NS_ERROR_ABORT);
    }
  }
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

  // Add an observer for shutdown
  nsCOMPtr<nsIObserverService> observerService =
    mozilla::services::GetObserverService();
  if (!observerService) {
    return NS_ERROR_FAILURE;
  }

  observerService->AddObserver(lookup, "quit-application", true);
  return lookup->StartLookup();
}
