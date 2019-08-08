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
#include "mozilla/Components.h"
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
#include "nsLocalFileCommon.h"
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

#include "ApplicationReputationTelemetryUtils.h"

using mozilla::ArrayLength;
using mozilla::BasePrincipal;
using mozilla::OriginAttributes;
using mozilla::Preferences;
using mozilla::TimeStamp;
using mozilla::intl::LocaleService;
using mozilla::Telemetry::Accumulate;
using mozilla::Telemetry::AccumulateCategorical;
using safe_browsing::ClientDownloadRequest;
using safe_browsing::ClientDownloadRequest_CertificateChain;
using safe_browsing::ClientDownloadRequest_Resource;
using safe_browsing::ClientDownloadRequest_SignatureInfo;

// Preferences that we need to initialize the query.
#define PREF_SB_APP_REP_URL "browser.safebrowsing.downloads.remote.url"
#define PREF_SB_MALWARE_ENABLED "browser.safebrowsing.malware.enabled"
#define PREF_SB_DOWNLOADS_ENABLED "browser.safebrowsing.downloads.enabled"
#define PREF_SB_DOWNLOADS_REMOTE_ENABLED \
  "browser.safebrowsing.downloads.remote.enabled"
#define PREF_SB_DOWNLOADS_REMOTE_TIMEOUT \
  "browser.safebrowsing.downloads.remote.timeout_ms"
#define PREF_DOWNLOAD_BLOCK_TABLE "urlclassifier.downloadBlockTable"
#define PREF_DOWNLOAD_ALLOW_TABLE "urlclassifier.downloadAllowTable"

// Preferences that are needed to action the verdict.
#define PREF_BLOCK_DANGEROUS \
  "browser.safebrowsing.downloads.remote.block_dangerous"
#define PREF_BLOCK_DANGEROUS_HOST \
  "browser.safebrowsing.downloads.remote.block_dangerous_host"
#define PREF_BLOCK_POTENTIALLY_UNWANTED \
  "browser.safebrowsing.downloads.remote.block_potentially_unwanted"
#define PREF_BLOCK_UNCOMMON \
  "browser.safebrowsing.downloads.remote.block_uncommon"

// MOZ_LOG=ApplicationReputation:5
mozilla::LazyLogModule ApplicationReputationService::prlog(
    "ApplicationReputation");
#define LOG(args) \
  MOZ_LOG(ApplicationReputationService::prlog, mozilla::LogLevel::Debug, args)
#define LOG_ENABLED() \
  MOZ_LOG_TEST(ApplicationReputationService::prlog, mozilla::LogLevel::Debug)

/**
 * Our detection of executable/binary files uses 3 lists:
 * - kNonBinaryExecutables (below)
 * - kBinaryFileExtensions (below)
 * - sExecutableExts (in nsLocalFileCommon)
 *
 * On Windows, the `sExecutableExts` list is used to determine whether files
 * count as executable. For executable files, we will not offer an "open with"
 * option when downloading, only "save as".
 *
 * On all platforms, the combination of these lists is used to determine
 * whether files should be subject to application reputation checks.
 * Specifically, all files with extensions that:
 * - are in kBinaryFileExtensions, or
 * - are in sExecutableExts **and not in kNonBinaryExecutables**
 *
 * will be subject to checks.
 *
 * There are tests that verify that these lists are sorted and that extensions
 * never appear in both the sExecutableExts and kBinaryFileExtensions lists.
 *
 * When adding items to any lists:
 * - please prefer adding to sExecutableExts unless it is imperative users can
 *   (potentially automatically!) open such files with a helper application
 *   without first saving them (and that outweighs any associated risk).
 * - if adding executable items that shouldn't be submitted to apprep servers,
 *   add them to sExecutableExts and also to kNonBinaryExecutables.
 * - always add an associated comment in the kBinaryFileExtensions list. Add
 *   a commented-out entry with an `exec` annotation if you add the actual
 *   entry in sExecutableExts.
 *
 * When removing items please consider whether items should still be in the
 * sExecutableExts list even if removing them from the kBinaryFileExtensions
 * list, and vice versa.
 *
 * Note that there is a GTest that does its best to check some of these
 * invariants that you'll likely need to update if you're modifying these
 * lists.
 */

// Items that are in sExecutableExts but shouldn't be submitted for application
// reputation checks.
/* static */
const char* const ApplicationReputationService::kNonBinaryExecutables[] = {
    ".ad",
    ".air",
};

// Items that should be submitted for application reputation checks that users
// are able to open immediately (without first saving and then finding the
// file). If users shouldn't be able to open them immediately, add to
// sExecutableExts instead (see also the docstring comment above!).
/* static */
const char* const ApplicationReputationService::kBinaryFileExtensions[] = {
    // Originally extracted from the "File Type Policies" Chrome extension
    // Items listed with an `exec` comment are in the sExecutableExts list in
    // nsLocalFileCommon.h .
    //".001",
    //".7z",
    //".ace",
    ".action",  // Mac script
    //".ad", exec // Windows
    //".ade", exec  // MS Access
    //".adp", exec // MS Access
    //".air", exec // Adobe AIR installer; excluded from apprep checks.
    ".apk",  // Android package
    //".app", exec  // Executable application
    ".applescript",
    //".application", exec // MS ClickOnce
    ".appref-ms",  // MS ClickOnce
    //".arc",
    //".arj",
    ".as",  // Mac archive
    //".asp", exec  // Windows Server script
    ".asx",  // Windows Media Player
    //".b64",
    //".balz",
    //".bas", exec  // Basic script
    ".bash",  // Linux shell
    //".bat", exec  // Windows shell
    //".bhx",
    ".bin",
    ".btapp",      // uTorrent and Transmission
    ".btinstall",  // uTorrent and Transmission
    ".btkey",      // uTorrent and Transmission
    ".btsearch",   // uTorrent and Transmission
    ".btskin",     // uTorrent and Transmission
    ".bz",         // Linux archive (bzip)
    ".bz2",        // Linux archive (bzip2)
    ".bzip2",      // Linux archive (bzip2)
    ".cab",        // Windows archive
    ".caction",    // Automator action
    ".cdr",        // Mac disk image
    ".cfg",        // Windows
    ".chi",        // Windows Help
    //".chm", exec // Windows Help
    ".class",  // Java
    //".cmd", exec // Windows executable
    //".com", exec // Windows executable
    ".command",        // Mac script
    ".configprofile",  // Configuration file for Apple systems
    ".cpgz",           // Mac archive
    ".cpi",            // Control Panel Item. Executable used for adding icons
                       // to Control Panel
    //".cpio",
    //".cpl", exec  // Windows executable
    //".crt", exec  // Windows signed certificate
    ".crx",  // Chrome extensions
    ".csh",  // Linux shell
    //".csv",
    ".dart",        // Mac disk image
    ".dc42",        // Apple DiskCopy Image
    ".deb",         // Linux package
    ".definition",  // Automator action
    ".desktop",     // A shortcut that runs other files
    ".dex",         // Android
    ".dht",         // HTML
    ".dhtm",        // HTML
    ".dhtml",       // HTML
    ".diskcopy42",  // Apple DiskCopy Image
    ".dll",         // Windows executable
    ".dmg",         // Mac disk image
    ".dmgpart",     // Mac disk image
    ".doc",         // MS Office
    ".docb",        // MS Office
    ".docm",        // MS Word
    ".docx",        // MS Word
    ".dot",         // MS Word
    ".dotm",        // MS Word
    ".dott",        // MS Office
    ".dotx",        // MS Word
    ".drv",         // Windows driver
    ".dvdr",        // Mac Disk image
    ".dylib",       // Mach object dynamic library file
    ".efi",         // Firmware
    ".eml",         // MS Outlook
    //".exe", exec // Windows executable
    //".fat",
    ".fon",  // Windows font
    //".fxp", exec // MS FoxPro
    ".gadget",  // Windows
    //".gif",
    ".grp",   // Windows
    ".gz",    // Linux archive (gzip)
    ".gzip",  // Linux archive (gzip)
    ".hfs",   // Mac disk image
    //".hlp", exec // Windows Help
    ".hqx",  // Mac archive
    //".hta", exec // HTML trusted application
    ".htm", ".html",
    ".htt",  // MS HTML template
    //".ica",
    ".img",      // Mac disk image
    ".imgpart",  // Mac disk image
    //".inf", exec // Windows installer
    ".ini",  // Generic config file
    //".ins", exec // IIS config
    ".internetconnect",  // Configuration file for Apple system
    //".inx", // InstallShield
    ".iso",  // CD image
    //".isp", exec // IIS config
    //".isu", // InstallShield
    //".jar", exec // Java
    //".jnlp", exec // Java
    //".job", // Windows
    //".jpg",
    //".jpeg",
    //".js", exec  // JavaScript script
    //".jse", exec // JScript
    ".ksh",  // Linux shell
    //".lha",
    //".lnk", exec // Windows
    ".local",  // Windows
    //".lpaq1",
    //".lpaq5",
    //".lpaq8",
    //".lzh",
    //".lzma",
    //".mad", exec  // MS Access
    //".maf", exec  // MS Access
    //".mag", exec  // MS Access
    //".mam", exec  // MS Access
    ".manifest",  // Windows
    //".maq", exec  // MS Access
    //".mar", exec  // MS Access
    //".mas", exec  // MS Access
    //".mat", exec  // MS Access
    //".mau", exec  // Media attachment
    //".mav", exec  // MS Access
    //".maw", exec  // MS Access
    //".mda", exec  // MS Access
    //".mdb", exec  // MS Access
    //".mde", exec  // MS Access
    //".mdt", exec  // MS Access
    //".mdw", exec  // MS Access
    //".mdz", exec  // MS Access
    ".mht",    // MS HTML
    ".mhtml",  // MS HTML
    ".mim",    // MS Mail
    //".mkv",
    ".mmc",           // MS Office
    ".mobileconfig",  // Configuration file for Apple systems
    ".mof",           // Windows
    //".mov",
    //".mp3",
    //".mp4",
    ".mpkg",  // Mac installer
    //".msc", exec  // Windows executable
    ".msg",  // MS Outlook
    //".msh", exec  // Windows shell
    //".msh1", exec // Windows shell
    //".msh1xml", exec  // Windows shell
    //".msh2", exec // Windows shell
    //".msh2xml", exec // Windows shell
    //".mshxml", exec // Windows
    //".msi", exec  // Windows installer
    //".msp", exec  // Windows installer
    //".mst", exec  // Windows installer
    ".ndif",            // Mac disk image
    ".networkconnect",  // Configuration file for Apple systems
    //".ntfs", // 7z
    ".ocx",  // ActiveX
    //".ops", exec  // MS Office
    ".osas",  // AppleScript
    ".osax",  // AppleScript
    //".out", // Linux binary
    ".oxt",  // OpenOffice extension, can execute arbitrary code
    //".package",
    //".paf", // PortableApps package
    //".paq8f",
    //".paq8jd",
    //".paq8l",
    //".paq8o",
    ".partial",  // Downloads
    ".pax",      // Mac archive
    //".pcd", exec     // Microsoft Visual Test
    ".pdf",  // Adobe Acrobat
    //".pea",
    ".pet",  // Linux package
    //".pif", exec // Windows
    ".pkg",  // Mac installer
    ".pl",   // Perl script
    //".plg", exec // MS Visual Studio
    //".png",
    ".pot",   // MS PowerPoint
    ".potm",  // MS PowerPoint
    ".potx",  // MS PowerPoint
    ".ppam",  // MS PowerPoint
    ".pps",   // MS PowerPoint
    ".ppsm",  // MS PowerPoint
    ".ppsx",  // MS PowerPoint
    ".ppt",   // MS PowerPoint
    ".pptm",  // MS PowerPoint
    ".pptx",  // MS PowerPoint
    //".prf", exec // MS Outlook
    //".prg", exec // Windows
    ".ps1",     // Windows shell
    ".ps1xml",  // Windows shell
    ".ps2",     // Windows shell
    ".ps2xml",  // Windows shell
    ".psc1",    // Windows shell
    ".psc2",    // Windows shell
    //".pst", exec // MS Outlook
    ".pup",  // Linux package
    ".py",   // Python script
    ".pyc",  // Python binary
    ".pyd",  // Equivalent of a DLL, for python libraries
    ".pyo",  // Compiled python code
    ".pyw",  // Python GUI
    //".quad",
    //".r00",
    //".r01",
    //".r02",
    //".r03",
    //".r04",
    //".r05",
    //".r06",
    //".r07",
    //".r08",
    //".r09",
    //".r10",
    //".r11",
    //".r12",
    //".r13",
    //".r14",
    //".r15",
    //".r16",
    //".r17",
    //".r18",
    //".r19",
    //".r20",
    //".r21",
    //".r22",
    //".r23",
    //".r24",
    //".r25",
    //".r26",
    //".r27",
    //".r28",
    //".r29",
    //".rar",
    ".rb",  // Ruby script
    //".reg", exec  // Windows Registry
    ".rels",  // MS Office
    //".rgs", // Windows Registry
    ".rpm",  // Linux package
    ".rtf",  // MS Office
    //".run", // Linux shell
    //".scf", exec         // Windows shell
    ".scpt",   // AppleScript
    ".scptd",  // AppleScript
    //".scr", exec         // Windows
    //".sct", exec         // Windows shell
    ".search-ms",  // Windows
    ".seplugin",   // AppleScript
    ".service",    // Systemd service unit file
    //".settingcontent-ms", exec // Windows settings
    ".sh",    // Linux shell
    ".shar",  // Linux shell
    //".shb", exec         // Windows
    //".shs", exec         // Windows shell
    ".sht",           // HTML
    ".shtm",          // HTML
    ".shtml",         // HTML
    ".sldm",          // MS PowerPoint
    ".sldx",          // MS PowerPoint
    ".slk",           // MS Excel
    ".slp",           // Linux package
    ".smi",           // Mac disk image
    ".sparsebundle",  // Mac disk image
    ".sparseimage",   // Mac disk image
    ".spl",           // Adobe Flash
    //".squashfs",
    ".svg",
    ".swf",   // Adobe Flash
    ".swm",   // Windows Imaging
    ".sys",   // Windows
    ".tar",   // Linux archive
    ".taz",   // Linux archive (bzip2)
    ".tbz",   // Linux archive (bzip2)
    ".tbz2",  // Linux archive (bzip2)
    ".tcsh",  // Linux shell
    //".tif",
    ".tgz",  // Linux archive (gzip)
    //".toast", // Roxio disk image
    ".torrent",  // Bittorrent
    ".tpz",      // Linux archive (gzip)
    //".txt",
    ".txz",  // Linux archive (xz)
    ".tz",   // Linux archive (gzip)
    //".u3p", // U3 Smart Apps
    ".udf",   // MS Excel
    ".udif",  // Mac disk image
    //".url", exec  // Windows
    //".uu",
    //".uue",
    //".vb", exec  // Visual Basic script
    //".vbe", exec // Visual Basic script
    //".vbs", exec // Visual Basic script
    //".vbscript", // Visual Basic script
    //".vdx", exec // MS Visio
    ".vhd",   // Windows virtual hard drive
    ".vhdx",  // Windows virtual hard drive
    ".vmdk",  // VMware virtual disk
    //".vsd", exec  // MS Visio
    //".vsdm", exec // MS Visio
    //".vsdx", exec // MS Visio
    //".vsmacros", exec  // MS Visual Studio
    //".vss",  exec  // MS Visio
    //".vssm", exec  // MS Visio
    //".vssx", exec  // MS Visio
    //".vst",  exec  // MS Visio
    //".vstm", exec  // MS Visio
    //".vstx", exec  // MS Visio
    //".vsw",  exec  // MS Visio
    //".vsx",  exec  // MS Visio
    //".vtx",  exec  // MS Visio
    //".wav",
    //".webp",
    ".website",   // Windows
    ".wflow",     // Automator action
    ".wim",       // Windows Imaging
    ".workflow",  // Mac Automator
    //".wrc", // FreeArc archive
    //".ws",  exec  // Windows script
    //".wsc", exec  // Windows script
    //".wsf", exec  // Windows script
    //".wsh", exec  // Windows script
    ".xar",   // MS Excel
    ".xbap",  // XAML Browser Application
    ".xht", ".xhtm", ".xhtml",
    ".xip",     // Mac archive
    ".xla",     // MS Excel
    ".xlam",    // MS Excel
    ".xldm",    // MS Excel
    ".xll",     // MS Excel
    ".xlm",     // MS Excel
    ".xls",     // MS Excel
    ".xlsb",    // MS Excel
    ".xlsm",    // MS Excel
    ".xlsx",    // MS Excel
    ".xlt",     // MS Excel
    ".xltm",    // MS Excel
    ".xltx",    // MS Excel
    ".xlw",     // MS Excel
    ".xml",     // MS Excel
    ".xnk",     // MS Exchange
    ".xrm-ms",  // Windows
    ".xsl",     // XML Stylesheet
    //".xxe",
    ".xz",     // Linux archive (xz)
    ".z",      // InstallShield
#ifdef XP_WIN  // disable on Mac/Linux, see 1167493
    ".zip",    // Generic archive
#endif
    ".zipx",  // WinZip
              //".zpaq",
};

static const char* const kMozNonBinaryExecutables[] = {
    ".001", ".7z",   ".ace",  ".arc",   ".arj",    ".b64",      ".balz",
    ".bhx", ".cpio", ".fat",  ".lha",   ".lpaq1",  ".lpaq5",    ".lpaq8",
    ".lzh", ".lzma", ".ntfs", ".paq8f", ".paq8jd", ".paq8l",    ".paq8o",
    ".pea", ".quad", ".r00",  ".r01",   ".r02",    ".r03",      ".r04",
    ".r05", ".r06",  ".r07",  ".r08",   ".r09",    ".r10",      ".r11",
    ".r12", ".r13",  ".r14",  ".r15",   ".r16",    ".r17",      ".r18",
    ".r19", ".r20",  ".r21",  ".r22",   ".r23",    ".r24",      ".r25",
    ".r26", ".r27",  ".r28",  ".r29",   ".rar",    ".squashfs", ".uu",
    ".uue", ".wrc",  ".xxe",  ".zpaq",  ".toast",
};

static const char* const kSafeFileExtensions[] = {
    ".jpg",  ".jpeg", ".mp3",      ".mp4",  ".png",  ".csv",  ".ica",
    ".gif",  ".txt",  ".package",  ".tif",  ".webp", ".mkv",  ".wav",
    ".mov",  ".paf",  ".vbscript", ".ad",   ".inx",  ".isu",  ".job",
    ".rgs",  ".u3p",  ".out",      ".run",  ".bmp",  ".css",  ".ehtml",
    ".flac", ".ico",  ".jfif",     ".m4a",  ".m4v",  ".mpeg", ".mpg",
    ".oga",  ".ogg",  ".ogm",      ".ogv",  ".opus", ".pjp",  ".pjpeg",
    ".svgz", ".text", ".tiff",     ".weba", ".webm", ".xbm",
};

enum class LookupType { AllowlistOnly, BlocklistOnly, BothLists };

// Define the reasons that download protection service accepts or blocks this
// download. This is now used for telemetry purposes and xpcshell test. Please
// also update the xpcshell-test if a reason is added.
//
// LocalWhitelist       : URL is found in the local whitelist
// LocalBlocklist       : URL is found in the local blocklist
// NonBinary            : The downloaded non-binary file is not found in the
// local blocklist VerdictSafe          : Remote lookup reports the download is
// safe VerdictUnknown       : Remote lookup reports unknown, we treat this as a
// safe download VerdictDangerous     : Remote lookup reports the download is
// dangerous VerdictDangerousHost : Remote lookup reports the download is from a
// dangerous host VerdictUnwanted      : Remote lookup reports the download is
// potentially unwatned VerdictUncommon      : Remote lookup reports the
// download is uncommon VerdictUnrecognized  : The verdict type from remote
// lookup is not defined in the csd.proto DangerousPrefOff     : The download is
// dangerous, but the corresponding preference is off DangerousHostPrefOff : The
// download is from a dangerous host, but the corresponding preference is off
// UnwantedPrefOff      : The download is potentially unwanted, but the
// corresponding preference is off UncommonPrefOff      : The download us
// uncommon, but the coressponding preference is off NetworkError         :
// There is an error while requesting remote lookup RemoteLookupDisabled :
// Remote lookup is disabled or the remote lookup URL is empty InternalError :
// An unexpected internal error DPDisabled           : Download protection is
// disabled
using Reason = mozilla::Telemetry::LABELS_APPLICATION_REPUTATION_REASON;

class PendingDBLookup;

// A single use class private to ApplicationReputationService encapsulating an
// nsIApplicationReputationQuery and an nsIApplicationReputationCallback. Once
// created by ApplicationReputationService, it is guaranteed to call mCallback.
// This class is private to ApplicationReputationService.
class PendingLookup final : public nsIStreamListener,
                            public nsITimerCallback,
                            public nsIObserver,
                            public nsSupportsWeakReference {
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

  // The target filename for the downloaded file.
  nsCString mFileName;

  // True if extension of this file matches any extension in the
  // kBinaryFileExtensions or sExecutableExts list.
  bool mIsBinaryFile;

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
  // The source URI of the download (i.e. final URI after any redirects).
  nsTArray<nsCString> mAnylistSpecs;
  // The referrer and possibly any redirects.
  nsTArray<nsCString> mBlocklistSpecs;

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

  // The clock records the start time of a remote lookup request, used by
  // telemetry.
  PRIntervalTime mTelemetryRemoteRequestStartMs;

  // Returns the type of download binary for the file.
  ClientDownloadRequest::DownloadType GetDownloadType(
      const nsACString& aFilename);

  // Clean up and call the callback. PendingLookup must not be used after this
  // function is called.
  nsresult OnComplete(uint32_t aVerdict, Reason aReason, nsresult aRv);

  // Wrapper function for nsIStreamListener.onStopRequest to make it easy to
  // guarantee calling the callback
  nsresult OnStopRequestInternal(nsIRequest* aRequest, nsISupports* aContext,
                                 nsresult aResult, uint32_t& aVerdict,
                                 Reason& aReason);

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
  nsresult GenerateWhitelistStringsForPair(nsIX509Cert* certificate,
                                           nsIX509Cert* issuer);

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

  // Adds the redirects to mBlocklistSpecs to be looked up.
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
  nsresult SendRemoteQueryInternal(Reason& aReason);
};

// A single-use class for looking up a single URI in the safebrowsing DB. This
// class is private to PendingLookup.
class PendingDBLookup final : public nsIUrlClassifierCallback {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIURLCLASSIFIERCALLBACK

  // Constructor and destructor
  explicit PendingDBLookup(PendingLookup* aPendingLookup);

  // Look up the given URI in the safebrowsing DBs, optionally on both the allow
  // list and the blocklist. If there is a match, call
  // PendingLookup::OnComplete. Otherwise, call PendingLookup::LookupNext.
  nsresult LookupSpec(const nsACString& aSpec, const LookupType& aLookupType);

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
  LookupType mLookupType;
  RefPtr<PendingLookup> mPendingLookup;
  nsresult LookupSpecInternal(const nsACString& aSpec);
};

NS_IMPL_ISUPPORTS(PendingDBLookup, nsIUrlClassifierCallback)

PendingDBLookup::PendingDBLookup(PendingLookup* aPendingLookup)
    : mLookupType(LookupType::BothLists), mPendingLookup(aPendingLookup) {
  LOG(("Created pending DB lookup [this = %p]", this));
}

PendingDBLookup::~PendingDBLookup() {
  LOG(("Destroying pending DB lookup [this = %p]", this));
  mPendingLookup = nullptr;
}

nsresult PendingDBLookup::LookupSpec(const nsACString& aSpec,
                                     const LookupType& aLookupType) {
  LOG(("Checking principal %s [this=%p]", aSpec.Data(), this));
  mSpec = aSpec;
  mLookupType = aLookupType;
  nsresult rv = LookupSpecInternal(aSpec);
  if (NS_FAILED(rv)) {
    nsAutoCString errorName;
    mozilla::GetErrorName(rv, errorName);
    LOG(("Error in LookupSpecInternal() [rv = %s, this = %p]", errorName.get(),
         this));
    return mPendingLookup->LookupNext();  // ignore this lookup and move to next
  }
  // LookupSpecInternal has called nsIUrlClassifierCallback.lookup, which is
  // guaranteed to call HandleEvent.
  return rv;
}

nsresult PendingDBLookup::LookupSpecInternal(const nsACString& aSpec) {
  nsresult rv;

  nsCOMPtr<nsIURI> uri;
  nsCOMPtr<nsIIOService> ios = do_GetService(NS_IOSERVICE_CONTRACTID, &rv);
  rv = ios->NewURI(aSpec, nullptr, nullptr, getter_AddRefs(uri));
  NS_ENSURE_SUCCESS(rv, rv);

  OriginAttributes attrs;
  nsCOMPtr<nsIPrincipal> principal =
      BasePrincipal::CreateContentPrincipal(uri, attrs);
  if (!principal) {
    return NS_ERROR_FAILURE;
  }

  // Check local lists to see if the URI has already been whitelisted or
  // blacklisted.
  LOG(("Checking DB service for principal %s [this = %p]", mSpec.get(), this));
  nsCOMPtr<nsIUrlClassifierDBService> dbService =
      mozilla::components::UrlClassifierDB::Service(&rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString tables;
  nsAutoCString allowlist;
  Preferences::GetCString(PREF_DOWNLOAD_ALLOW_TABLE, allowlist);
  if ((mLookupType != LookupType::BlocklistOnly) && !allowlist.IsEmpty()) {
    tables.Append(allowlist);
  }
  nsAutoCString blocklist;
  Preferences::GetCString(PREF_DOWNLOAD_BLOCK_TABLE, blocklist);
  if ((mLookupType != LookupType::AllowlistOnly) && !blocklist.IsEmpty()) {
    if (!tables.IsEmpty()) {
      tables.Append(',');
    }
    tables.Append(blocklist);
  }
  return dbService->Lookup(principal, tables, this);
}

NS_IMETHODIMP
PendingDBLookup::HandleEvent(const nsACString& tables) {
  // HandleEvent is guaranteed to call either:
  // 1) PendingLookup::OnComplete if the URL matches the blocklist, or
  // 2) PendingLookup::LookupNext if the URL does not match the blocklist.
  // Blocklisting trumps allowlisting.
  nsAutoCString blockList;
  Preferences::GetCString(PREF_DOWNLOAD_BLOCK_TABLE, blockList);
  if ((mLookupType != LookupType::AllowlistOnly) &&
      FindInReadable(blockList, tables)) {
    mPendingLookup->mBlocklistCount++;
    Accumulate(mozilla::Telemetry::APPLICATION_REPUTATION_LOCAL, BLOCK_LIST);
    LOG(("Found principal %s on blocklist [this = %p]", mSpec.get(), this));
    return mPendingLookup->OnComplete(
        nsIApplicationReputationService::VERDICT_DANGEROUS,
        Reason::LocalBlocklist, NS_OK);
  }

  nsAutoCString allowList;
  Preferences::GetCString(PREF_DOWNLOAD_ALLOW_TABLE, allowList);
  if ((mLookupType != LookupType::BlocklistOnly) &&
      FindInReadable(allowList, tables)) {
    mPendingLookup->mAllowlistCount++;
    Accumulate(mozilla::Telemetry::APPLICATION_REPUTATION_LOCAL, ALLOW_LIST);
    LOG(("Found principal %s on allowlist [this = %p]", mSpec.get(), this));
    // Don't call onComplete, since blocklisting trumps allowlisting
    return mPendingLookup->LookupNext();
  }

  LOG(("Didn't find principal %s on any list [this = %p]", mSpec.get(), this));
  Accumulate(mozilla::Telemetry::APPLICATION_REPUTATION_LOCAL, NO_LIST);
  return mPendingLookup->LookupNext();
}

NS_IMPL_ISUPPORTS(PendingLookup, nsIStreamListener, nsIRequestObserver,
                  nsIObserver, nsISupportsWeakReference)

PendingLookup::PendingLookup(nsIApplicationReputationQuery* aQuery,
                             nsIApplicationReputationCallback* aCallback)
    : mIsBinaryFile(false),
      mBlocklistCount(0),
      mAllowlistCount(0),
      mQuery(aQuery),
      mCallback(aCallback) {
  LOG(("Created pending lookup [this = %p]", this));
}

PendingLookup::~PendingLookup() {
  LOG(("Destroying pending lookup [this = %p]", this));
}

static const char* const kDmgFileExtensions[] = {
    ".cdr",          ".dart",        ".dc42",  ".diskcopy42",
    ".dmg",          ".dmgpart",     ".dvdr",  ".img",
    ".imgpart",      ".iso",         ".ndif",  ".smi",
    ".sparsebundle", ".sparseimage", ".toast", ".udif",
};

static const char* const kRarFileExtensions[] = {
    ".r00", ".r01", ".r02", ".r03", ".r04", ".r05", ".r06", ".r07",
    ".r08", ".r09", ".r10", ".r11", ".r12", ".r13", ".r14", ".r15",
    ".r16", ".r17", ".r18", ".r19", ".r20", ".r21", ".r22", ".r23",
    ".r24", ".r25", ".r26", ".r27", ".r28", ".r29", ".rar",
};

static const char* const kZipFileExtensions[] = {
    ".zip",   // Generic archive
    ".zipx",  // WinZip
};

static const char* GetFileExt(const nsACString& aFilename,
                              const char* const aFileExtensions[],
                              const size_t aLength) {
  for (size_t i = 0; i < aLength; ++i) {
    if (StringEndsWith(aFilename, nsDependentCString(aFileExtensions[i]))) {
      return aFileExtensions[i];
    }
  }
  return nullptr;
}

static const char* GetFileExt(const nsACString& aFilename) {
#define _GetFileExt(_f, _l) GetFileExt(_f, _l, ArrayLength(_l))
  const char* ext = _GetFileExt(
      aFilename, ApplicationReputationService::kBinaryFileExtensions);
  if (ext == nullptr &&
      !_GetFileExt(aFilename,
                   ApplicationReputationService::kNonBinaryExecutables)) {
    ext = _GetFileExt(aFilename, sExecutableExts);
  }
  return ext;
}

// Returns true if the file extension matches one in the given array.
static bool IsFileType(const nsACString& aFilename,
                       const char* const aFileExtensions[],
                       const size_t aLength) {
  return GetFileExt(aFilename, aFileExtensions, aLength) != nullptr;
}

static bool IsBinary(const nsACString& aFilename) {
  return IsFileType(aFilename,
                    ApplicationReputationService::kBinaryFileExtensions,
                    ArrayLength(
                        ApplicationReputationService::kBinaryFileExtensions)) ||
         (!IsFileType(
              aFilename, ApplicationReputationService::kNonBinaryExecutables,
              ArrayLength(
                  ApplicationReputationService::kNonBinaryExecutables)) &&
          IsFileType(aFilename, sExecutableExts, ArrayLength(sExecutableExts)));
}

ClientDownloadRequest::DownloadType PendingLookup::GetDownloadType(
    const nsACString& aFilename) {
  MOZ_ASSERT(IsBinary(aFilename));

  // From
  // https://cs.chromium.org/chromium/src/chrome/common/safe_browsing/download_protection_util.cc?l=17
  if (StringEndsWith(aFilename, NS_LITERAL_CSTRING(".zip"))) {
    return ClientDownloadRequest::ZIPPED_EXECUTABLE;
  } else if (StringEndsWith(aFilename, NS_LITERAL_CSTRING(".apk"))) {
    return ClientDownloadRequest::ANDROID_APK;
  } else if (StringEndsWith(aFilename, NS_LITERAL_CSTRING(".app")) ||
             StringEndsWith(aFilename, NS_LITERAL_CSTRING(".applescript")) ||
             StringEndsWith(aFilename, NS_LITERAL_CSTRING(".cdr")) ||
             StringEndsWith(aFilename, NS_LITERAL_CSTRING(".dart")) ||
             StringEndsWith(aFilename, NS_LITERAL_CSTRING(".dc42")) ||
             StringEndsWith(aFilename, NS_LITERAL_CSTRING(".diskcopy42")) ||
             StringEndsWith(aFilename, NS_LITERAL_CSTRING(".dmg")) ||
             StringEndsWith(aFilename, NS_LITERAL_CSTRING(".dmgpart")) ||
             StringEndsWith(aFilename, NS_LITERAL_CSTRING(".dvdr")) ||
             StringEndsWith(aFilename, NS_LITERAL_CSTRING(".img")) ||
             StringEndsWith(aFilename, NS_LITERAL_CSTRING(".imgpart")) ||
             StringEndsWith(aFilename, NS_LITERAL_CSTRING(".iso")) ||
             StringEndsWith(aFilename, NS_LITERAL_CSTRING(".mpkg")) ||
             StringEndsWith(aFilename, NS_LITERAL_CSTRING(".ndif")) ||
             StringEndsWith(aFilename, NS_LITERAL_CSTRING(".osas")) ||
             StringEndsWith(aFilename, NS_LITERAL_CSTRING(".osax")) ||
             StringEndsWith(aFilename, NS_LITERAL_CSTRING(".pkg")) ||
             StringEndsWith(aFilename, NS_LITERAL_CSTRING(".scpt")) ||
             StringEndsWith(aFilename, NS_LITERAL_CSTRING(".scptd")) ||
             StringEndsWith(aFilename, NS_LITERAL_CSTRING(".seplugin")) ||
             StringEndsWith(aFilename, NS_LITERAL_CSTRING(".smi")) ||
             StringEndsWith(aFilename, NS_LITERAL_CSTRING(".sparsebundle")) ||
             StringEndsWith(aFilename, NS_LITERAL_CSTRING(".sparseimage")) ||
             StringEndsWith(aFilename, NS_LITERAL_CSTRING(".toast")) ||
             StringEndsWith(aFilename, NS_LITERAL_CSTRING(".udif"))) {
    return ClientDownloadRequest::MAC_EXECUTABLE;
  }

  return ClientDownloadRequest::WIN_EXECUTABLE;  // default to Windows binaries
}

nsresult PendingLookup::LookupNext() {
  // We must call LookupNext or SendRemoteQuery upon return.
  // Look up all of the URLs that could allow or block this download.
  // Blocklist first.

  // If a url is in blocklist we should call PendingLookup::OnComplete directly.
  MOZ_ASSERT(mBlocklistCount == 0);

  int index = mAnylistSpecs.Length() - 1;
  nsCString spec;
  if (index >= 0) {
    // Check the source URI only.
    spec = mAnylistSpecs[index];
    mAnylistSpecs.RemoveElementAt(index);
    RefPtr<PendingDBLookup> lookup(new PendingDBLookup(this));

    // We don't need to check whitelist if the file is not a binary file.
    auto type =
        mIsBinaryFile ? LookupType::BothLists : LookupType::BlocklistOnly;
    return lookup->LookupSpec(spec, type);
  }

  index = mBlocklistSpecs.Length() - 1;
  if (index >= 0) {
    // Check the referrer and redirect chain.
    spec = mBlocklistSpecs[index];
    mBlocklistSpecs.RemoveElementAt(index);
    RefPtr<PendingDBLookup> lookup(new PendingDBLookup(this));
    return lookup->LookupSpec(spec, LookupType::BlocklistOnly);
  }

  // Now that we've looked up all of the URIs against the blocklist,
  // if any of mAnylistSpecs or mAllowlistSpecs matched the allowlist,
  // go ahead and pass.
  if (mAllowlistCount > 0) {
    return OnComplete(nsIApplicationReputationService::VERDICT_SAFE,
                      Reason::LocalWhitelist, NS_OK);
  }

  MOZ_ASSERT_IF(!mIsBinaryFile, mAllowlistSpecs.Length() == 0);

  // Only binary signatures remain.
  index = mAllowlistSpecs.Length() - 1;
  if (index >= 0) {
    spec = mAllowlistSpecs[index];
    LOG(("PendingLookup::LookupNext: checking %s on allowlist", spec.get()));
    mAllowlistSpecs.RemoveElementAt(index);
    RefPtr<PendingDBLookup> lookup(new PendingDBLookup(this));
    return lookup->LookupSpec(spec, LookupType::AllowlistOnly);
  }

  if (!mFileName.IsEmpty()) {
    if (IsBinary(mFileName)) {
      AccumulateCategorical(
          mozilla::Telemetry::LABELS_APPLICATION_REPUTATION_BINARY_TYPE::
              BinaryFile);
    } else if (IsFileType(mFileName, kSafeFileExtensions,
                          ArrayLength(kSafeFileExtensions))) {
      AccumulateCategorical(
          mozilla::Telemetry::LABELS_APPLICATION_REPUTATION_BINARY_TYPE::
              NonBinaryFile);
    } else if (IsFileType(mFileName, kMozNonBinaryExecutables,
                          ArrayLength(kMozNonBinaryExecutables))) {
      AccumulateCategorical(
          mozilla::Telemetry::LABELS_APPLICATION_REPUTATION_BINARY_TYPE::
              MozNonBinaryFile);
    } else {
      AccumulateCategorical(
          mozilla::Telemetry::LABELS_APPLICATION_REPUTATION_BINARY_TYPE::
              UnknownFile);
    }
  } else {
    AccumulateCategorical(
        mozilla::Telemetry::LABELS_APPLICATION_REPUTATION_BINARY_TYPE::
            MissingFilename);
  }

  if (IsFileType(mFileName, kDmgFileExtensions,
                 ArrayLength(kDmgFileExtensions))) {
    AccumulateCategorical(
        mozilla::Telemetry::LABELS_APPLICATION_REPUTATION_BINARY_ARCHIVE::
            DmgFile);
  } else if (IsFileType(mFileName, kRarFileExtensions,
                        ArrayLength(kRarFileExtensions))) {
    AccumulateCategorical(
        mozilla::Telemetry::LABELS_APPLICATION_REPUTATION_BINARY_ARCHIVE::
            RarFile);
  } else if (IsFileType(mFileName, kZipFileExtensions,
                        ArrayLength(kZipFileExtensions))) {
    AccumulateCategorical(
        mozilla::Telemetry::LABELS_APPLICATION_REPUTATION_BINARY_ARCHIVE::
            ZipFile);
  } else if (mIsBinaryFile) {
    AccumulateCategorical(
        mozilla::Telemetry::LABELS_APPLICATION_REPUTATION_BINARY_ARCHIVE::
            OtherBinaryFile);
  }

  // There are no more URIs to check against local list. If the file is
  // not eligible for remote lookup, bail.
  if (!mIsBinaryFile) {
    LOG(("Not eligible for remote lookups [this=%p]", this));
    return OnComplete(nsIApplicationReputationService::VERDICT_SAFE,
                      Reason::NonBinaryFile, NS_OK);
  }

  nsresult rv = SendRemoteQuery();
  if (NS_FAILED(rv)) {
    return OnComplete(nsIApplicationReputationService::VERDICT_SAFE,
                      Reason::InternalError, rv);
  }
  return NS_OK;
}

nsCString PendingLookup::EscapeCertificateAttribute(
    const nsACString& aAttribute) {
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

nsCString PendingLookup::EscapeFingerprint(const nsACString& aFingerprint) {
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

nsresult PendingLookup::GenerateWhitelistStringsForPair(
    nsIX509Cert* certificate, nsIX509Cert* issuer) {
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
  whitelistString.Append(EscapeFingerprint(NS_ConvertUTF16toUTF8(fingerprint)));

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

nsresult PendingLookup::GenerateWhitelistStringsForChain(
    const safe_browsing::ClientDownloadRequest_CertificateChain& aChain) {
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
      const_cast<char*>(aChain.element(0).certificate().data()),
      aChain.element(0).certificate().size());
  rv = certDB->ConstructX509(signerDER, getter_AddRefs(signer));
  NS_ENSURE_SUCCESS(rv, rv);

  for (int i = 1; i < aChain.element_size(); ++i) {
    // Get the issuer.
    nsCOMPtr<nsIX509Cert> issuer;
    nsDependentCSubstring issuerDER(
        const_cast<char*>(aChain.element(i).certificate().data()),
        aChain.element(i).certificate().size());
    rv = certDB->ConstructX509(issuerDER, getter_AddRefs(issuer));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = GenerateWhitelistStringsForPair(signer, issuer);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  return NS_OK;
}

nsresult PendingLookup::GenerateWhitelistStrings() {
  for (int i = 0; i < mRequest.signature().certificate_chain_size(); ++i) {
    nsresult rv = GenerateWhitelistStringsForChain(
        mRequest.signature().certificate_chain(i));
    NS_ENSURE_SUCCESS(rv, rv);
  }
  return NS_OK;
}

nsresult PendingLookup::AddRedirects(nsIArray* aRedirects) {
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

    nsCOMPtr<nsIRedirectHistoryEntry> redirectEntry =
        do_QueryInterface(supports, &rv);
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
    mBlocklistSpecs.AppendElement(spec);
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

nsresult PendingLookup::StartLookup() {
  mStartTime = TimeStamp::Now();
  nsresult rv = DoLookupInternal();
  if (NS_FAILED(rv)) {
    return OnComplete(nsIApplicationReputationService::VERDICT_SAFE,
                      Reason::InternalError, NS_OK);
  }
  return rv;
}

nsresult PendingLookup::GetSpecHash(nsACString& aSpec,
                                    nsACString& hexEncodedHash) {
  nsresult rv;

  nsCOMPtr<nsICryptoHash> cryptoHash =
      do_CreateInstance("@mozilla.org/security/hash;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = cryptoHash->Init(nsICryptoHash::SHA256);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = cryptoHash->Update(
      reinterpret_cast<const uint8_t*>(aSpec.BeginReading()), aSpec.Length());
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString binaryHash;
  rv = cryptoHash->Finish(false, binaryHash);
  NS_ENSURE_SUCCESS(rv, rv);

  // This needs to match HexEncode() in Chrome's
  // src/base/strings/string_number_conversions.cc
  static const char* const hex = "0123456789ABCDEF";
  hexEncodedHash.SetCapacity(2 * binaryHash.Length());
  for (size_t i = 0; i < binaryHash.Length(); ++i) {
    auto c = static_cast<unsigned char>(binaryHash[i]);
    hexEncodedHash.Append(hex[(c >> 4) & 0x0F]);
    hexEncodedHash.Append(hex[c & 0x0F]);
  }

  return NS_OK;
}

nsresult PendingLookup::GetStrippedSpec(nsIURI* aUri, nsACString& escaped) {
  if (NS_WARN_IF(!aUri)) {
    return NS_ERROR_INVALID_ARG;
  }

  nsresult rv;
  rv = aUri->GetScheme(escaped);
  NS_ENSURE_SUCCESS(rv, rv);

  if (escaped.EqualsLiteral("blob")) {
    aUri->GetSpec(escaped);
    LOG(
        ("PendingLookup::GetStrippedSpec(): blob URL left unstripped as '%s' "
         "[this = %p]",
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

    LOG(
        ("PendingLookup::GetStrippedSpec(): data URL stripped to '%s' [this = "
         "%p]",
         PromiseFlatCString(escaped).get(), this));
    return NS_OK;
  }

  // If aURI is not an nsIURL, we do not want to check the lists or send a
  // remote query.
  nsCOMPtr<nsIURL> url = do_QueryInterface(aUri, &rv);
  if (NS_FAILED(rv)) {
    LOG(
        ("PendingLookup::GetStrippedSpec(): scheme '%s' is not supported [this "
         "= %p]",
         PromiseFlatCString(escaped).get(), this));
    return rv;
  }

  nsCString temp;
  rv = url->GetHostPort(temp);
  NS_ENSURE_SUCCESS(rv, rv);

  escaped.AppendLiteral("://");
  escaped.Append(temp);

  rv = url->GetFilePath(temp);
  NS_ENSURE_SUCCESS(rv, rv);

  // nsIUrl.filePath starts with '/'
  escaped.Append(temp);

  LOG(("PendingLookup::GetStrippedSpec(): URL stripped to '%s' [this = %p]",
       PromiseFlatCString(escaped).get(), this));
  return NS_OK;
}

nsresult PendingLookup::DoLookupInternal() {
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
    mBlocklistSpecs.AppendElement(referrerSpec);
    resource->set_referrer(referrerSpec.get());
  }
  nsCOMPtr<nsIArray> redirects;
  rv = mQuery->GetRedirects(getter_AddRefs(redirects));
  if (redirects) {
    AddRedirects(redirects);
  } else {
    LOG(("ApplicationReputation: Got no redirects [this=%p]", this));
  }

  rv = mQuery->GetSuggestedFileName(mFileName);
  if (NS_SUCCEEDED(rv) && !mFileName.IsEmpty()) {
    mIsBinaryFile = IsBinary(mFileName);
    LOG(("Suggested filename: %s [binary = %d, this = %p]", mFileName.get(),
         mIsBinaryFile, this));
  } else {
    nsAutoCString errorName;
    mozilla::GetErrorName(rv, errorName);
    LOG(("No suggested filename [rv = %s, this = %p]", errorName.get(), this));
    mFileName = EmptyCString();
  }

  // We can skip parsing certificate for non-binary files because we only
  // check local block list for them.
  if (mIsBinaryFile) {
    nsCOMPtr<nsIArray> sigArray;
    rv = mQuery->GetSignatureInfo(getter_AddRefs(sigArray));
    NS_ENSURE_SUCCESS(rv, rv);

    if (sigArray) {
      rv = ParseCertificates(sigArray);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    rv = GenerateWhitelistStrings();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Start the call chain.
  return LookupNext();
}

nsresult PendingLookup::OnComplete(uint32_t aVerdict, Reason aReason,
                                   nsresult aRv) {
  if (NS_FAILED(aRv)) {
    nsAutoCString errorName;
    mozilla::GetErrorName(aRv, errorName);
    LOG(
        ("Failed sending remote query for application reputation "
         "[rv = %s, this = %p]",
         errorName.get(), this));
  }

  if (mTimeoutTimer) {
    mTimeoutTimer->Cancel();
    mTimeoutTimer = nullptr;
  }

  bool shouldBlock = true;
  switch (aVerdict) {
    case nsIApplicationReputationService::VERDICT_DANGEROUS:
      if (!Preferences::GetBool(PREF_BLOCK_DANGEROUS, true)) {
        shouldBlock = false;
        aReason = Reason::DangerousPrefOff;
      }
      break;
    case nsIApplicationReputationService::VERDICT_UNCOMMON:
      if (!Preferences::GetBool(PREF_BLOCK_UNCOMMON, true)) {
        shouldBlock = false;
        aReason = Reason::UncommonPrefOff;
      }
      break;
    case nsIApplicationReputationService::VERDICT_POTENTIALLY_UNWANTED:
      if (!Preferences::GetBool(PREF_BLOCK_POTENTIALLY_UNWANTED, true)) {
        shouldBlock = false;
        aReason = Reason::UnwantedPrefOff;
      }
      break;
    case nsIApplicationReputationService::VERDICT_DANGEROUS_HOST:
      if (!Preferences::GetBool(PREF_BLOCK_DANGEROUS_HOST, true)) {
        shouldBlock = false;
        aReason = Reason::DangerousHostPrefOff;
      }
      break;
    default:
      shouldBlock = false;
      break;
  }

  AccumulateCategorical(aReason);
  Accumulate(mozilla::Telemetry::APPLICATION_REPUTATION_SHOULD_BLOCK,
             shouldBlock);

  double t = (TimeStamp::Now() - mStartTime).ToMilliseconds();
  LOG(("Application Reputation verdict is %u, obtained in %f ms [this = %p]",
       aVerdict, t, this));
  if (shouldBlock) {
    LOG(("Application Reputation check failed, blocking bad binary [this = %p]",
         this));
  } else {
    LOG(("Application Reputation check passed [this = %p]", this));
  }

  nsresult res = mCallback->OnComplete(shouldBlock, aRv, aVerdict);
  return res;
}

nsresult PendingLookup::ParseCertificates(nsIArray* aSigArray) {
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

      nsTArray<uint8_t> data;
      rv = cert->GetRawDER(data);
      NS_ENSURE_SUCCESS(rv, rv);

      // Add this certificate to the protobuf to send remotely.
      certChain->add_element()->set_certificate(data.Elements(), data.Length());

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

nsresult PendingLookup::SendRemoteQuery() {
  MOZ_ASSERT(!IsFileType(
      mFileName, ApplicationReputationService::kNonBinaryExecutables,
      ArrayLength(ApplicationReputationService::kNonBinaryExecutables)));
  Reason reason = Reason::NotSet;
  nsresult rv = SendRemoteQueryInternal(reason);
  if (NS_FAILED(rv)) {
    return OnComplete(nsIApplicationReputationService::VERDICT_SAFE, reason,
                      rv);
  }
  // SendRemoteQueryInternal has fired off the query and we call OnComplete in
  // the nsIStreamListener.onStopRequest.
  return rv;
}

nsresult PendingLookup::SendRemoteQueryInternal(Reason& aReason) {
  auto scopeExit = mozilla::MakeScopeExit([&aReason]() {
    if (aReason == Reason::NotSet) {
      aReason = Reason::InternalError;
    }
  });

  // If we aren't supposed to do remote lookups, bail.
  if (!Preferences::GetBool(PREF_SB_DOWNLOADS_REMOTE_ENABLED, false)) {
    LOG(("Remote lookups are disabled [this = %p]", this));
    aReason = Reason::RemoteLookupDisabled;
    return NS_ERROR_NOT_AVAILABLE;
  }
  // If the remote lookup URL is empty or absent, bail.
  nsAutoCString serviceUrl;
  if (NS_FAILED(Preferences::GetCString(PREF_SB_APP_REP_URL, serviceUrl)) ||
      serviceUrl.IsEmpty()) {
    LOG(("Remote lookup URL is empty or absent [this = %p]", this));
    aReason = Reason::RemoteLookupDisabled;
    return NS_ERROR_NOT_AVAILABLE;
  }

  LOG(("Sending remote query for application reputation [this = %p]", this));
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
  mRequest.mutable_digests()->set_sha256(
      std::string(sha256Hash.Data(), sha256Hash.Length()));
  mRequest.set_file_basename(mFileName.get());
  mRequest.set_download_type(GetDownloadType(mFileName));

  if (mRequest.signature().trusted()) {
    LOG(
        ("Got signed binary for remote application reputation check "
         "[this = %p]",
         this));
  } else {
    LOG(
        ("Got unsigned binary for remote application reputation check "
         "[this = %p]",
         this));
  }

  // Look for truncated hashes (see bug 1190020)
  const auto originalHashLength = sha256Hash.Length();
  if (originalHashLength == 0) {
    AccumulateCategorical(
        mozilla::Telemetry::LABELS_APPLICATION_REPUTATION_HASH_LENGTH::
            OriginalHashEmpty);
  } else if (originalHashLength < 32) {
    AccumulateCategorical(
        mozilla::Telemetry::LABELS_APPLICATION_REPUTATION_HASH_LENGTH::
            OriginalHashTooShort);
  } else if (originalHashLength > 32) {
    AccumulateCategorical(
        mozilla::Telemetry::LABELS_APPLICATION_REPUTATION_HASH_LENGTH::
            OriginalHashTooLong);
  } else if (!mRequest.has_digests()) {
    AccumulateCategorical(
        mozilla::Telemetry::LABELS_APPLICATION_REPUTATION_HASH_LENGTH::
            MissingDigest);
  } else if (!mRequest.digests().has_sha256()) {
    AccumulateCategorical(
        mozilla::Telemetry::LABELS_APPLICATION_REPUTATION_HASH_LENGTH::
            MissingSha256);
  } else if (mRequest.digests().sha256().size() != originalHashLength) {
    AccumulateCategorical(
        mozilla::Telemetry::LABELS_APPLICATION_REPUTATION_HASH_LENGTH::
            InvalidSha256);
  } else {
    AccumulateCategorical(
        mozilla::Telemetry::LABELS_APPLICATION_REPUTATION_HASH_LENGTH::
            ValidHash);
  }

  // Serialize the protocol buffer to a string. This can only fail if we are
  // out of memory, or if the protocol buffer req is missing required fields
  // (only the URL for now).
  std::string serialized;
  if (!mRequest.SerializeToString(&serialized)) {
    return NS_ERROR_UNEXPECTED;
  }

  if (LOG_ENABLED()) {
    nsAutoCString serializedStr(serialized.c_str(), serialized.length());
    serializedStr.ReplaceSubstring(NS_LITERAL_CSTRING("\0"),
                                   NS_LITERAL_CSTRING("\\0"));

    LOG(("Serialized protocol buffer [this = %p]: (length=%d) %s", this,
         serializedStr.Length(), serializedStr.get()));
  }

  // Set the input stream to the serialized protocol buffer
  nsCOMPtr<nsIStringInputStream> sstream =
      do_CreateInstance("@mozilla.org/io/string-input-stream;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = sstream->SetData(serialized.c_str(), serialized.length());
  NS_ENSURE_SUCCESS(rv, rv);

  // Set up the channel to transmit the request to the service.
  nsCOMPtr<nsIIOService> ios = do_GetService(NS_IOSERVICE_CONTRACTID, &rv);
  rv = ios->NewChannel(serviceUrl, nullptr, nullptr,
                       nullptr,  // aLoadingNode
                       nsContentUtils::GetSystemPrincipal(),
                       nullptr,  // aTriggeringPrincipal
                       nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_DATA_IS_NULL,
                       nsIContentPolicy::TYPE_OTHER, getter_AddRefs(mChannel));
  NS_ENSURE_SUCCESS(rv, rv);

  mChannel->SetLoadFlags(nsIChannel::LOAD_BYPASS_URL_CLASSIFIER);

  nsCOMPtr<nsILoadInfo> loadInfo = mChannel->LoadInfo();
  mozilla::OriginAttributes attrs;
  attrs.mFirstPartyDomain.AssignLiteral(NECKO_SAFEBROWSING_FIRST_PARTY_DOMAIN);
  loadInfo->SetOriginAttributes(attrs);

  nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(mChannel, &rv));
  NS_ENSURE_SUCCESS(rv, rv);
  mozilla::Unused << httpChannel;

  // Upload the protobuf to the application reputation service.
  nsCOMPtr<nsIUploadChannel2> uploadChannel = do_QueryInterface(mChannel, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = uploadChannel->ExplicitSetUploadStream(
      sstream, NS_LITERAL_CSTRING("application/octet-stream"),
      serialized.size(), NS_LITERAL_CSTRING("POST"), false);
  NS_ENSURE_SUCCESS(rv, rv);

  uint32_t timeoutMs =
      Preferences::GetUint(PREF_SB_DOWNLOADS_REMOTE_TIMEOUT, 10000);
  NS_NewTimerWithCallback(getter_AddRefs(mTimeoutTimer), this, timeoutMs,
                          nsITimer::TYPE_ONE_SHOT);

  mTelemetryRemoteRequestStartMs = PR_IntervalNow();

  rv = mChannel->AsyncOpen(this);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
PendingLookup::Notify(nsITimer* aTimer) {
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
PendingLookup::Observe(nsISupports* aSubject, const char* aTopic,
                       const char16_t* aData) {
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
static nsresult AppendSegmentToString(nsIInputStream* inputStream,
                                      void* closure, const char* rawSegment,
                                      uint32_t toOffset, uint32_t count,
                                      uint32_t* writeCount) {
  nsAutoCString* decodedData = static_cast<nsAutoCString*>(closure);
  decodedData->Append(rawSegment, count);
  *writeCount = count;
  return NS_OK;
}

NS_IMETHODIMP
PendingLookup::OnDataAvailable(nsIRequest* aRequest, nsIInputStream* aStream,
                               uint64_t offset, uint32_t count) {
  uint32_t read;
  return aStream->ReadSegments(AppendSegmentToString, &mResponse, count, &read);
}

NS_IMETHODIMP
PendingLookup::OnStartRequest(nsIRequest* aRequest) { return NS_OK; }

NS_IMETHODIMP
PendingLookup::OnStopRequest(nsIRequest* aRequest, nsresult aResult) {
  NS_ENSURE_STATE(mCallback);

  if (aResult != NS_ERROR_NET_TIMEOUT) {
    Accumulate(mozilla::Telemetry::APPLICATION_REPUTATION_REMOTE_LOOKUP_TIMEOUT,
               false);

    MOZ_ASSERT(mTelemetryRemoteRequestStartMs > 0);
    int32_t msecs = PR_IntervalToMilliseconds(PR_IntervalNow() -
                                              mTelemetryRemoteRequestStartMs);

    MOZ_ASSERT(msecs >= 0);
    mozilla::Telemetry::Accumulate(
        mozilla::Telemetry::APPLICATION_REPUTATION_REMOTE_LOOKUP_RESPONSE_TIME,
        msecs);
  }

  uint32_t verdict = nsIApplicationReputationService::VERDICT_SAFE;
  Reason reason = Reason::NotSet;
  nsresult rv =
      OnStopRequestInternal(aRequest, nullptr, aResult, verdict, reason);
  OnComplete(verdict, reason, rv);
  return rv;
}

nsresult PendingLookup::OnStopRequestInternal(nsIRequest* aRequest,
                                              nsISupports* aContext,
                                              nsresult aResult,
                                              uint32_t& aVerdict,
                                              Reason& aReason) {
  auto scopeExit = mozilla::MakeScopeExit([&aReason]() {
    // If |aReason| is not set while exiting, there must be an error.
    if (aReason == Reason::NotSet) {
      aReason = Reason::NetworkError;
    }
  });

  if (NS_FAILED(aResult)) {
    Accumulate(mozilla::Telemetry::APPLICATION_REPUTATION_SERVER,
               SERVER_RESPONSE_FAILED);
    AccumulateCategorical(NSErrorToLabel(aResult));
    return aResult;
  }

  nsresult rv;
  nsCOMPtr<nsIHttpChannel> channel = do_QueryInterface(aRequest, &rv);
  if (NS_FAILED(rv)) {
    Accumulate(mozilla::Telemetry::APPLICATION_REPUTATION_SERVER,
               SERVER_RESPONSE_FAILED);
    AccumulateCategorical(
        mozilla::Telemetry::LABELS_APPLICATION_REPUTATION_SERVER_2::
            FailGetChannel);
    return rv;
  }

  uint32_t status = 0;
  rv = channel->GetResponseStatus(&status);
  if (NS_FAILED(rv)) {
    Accumulate(mozilla::Telemetry::APPLICATION_REPUTATION_SERVER,
               SERVER_RESPONSE_FAILED);
    AccumulateCategorical(
        mozilla::Telemetry::LABELS_APPLICATION_REPUTATION_SERVER_2::
            FailGetResponse);
    return rv;
  }

  if (status != 200) {
    Accumulate(mozilla::Telemetry::APPLICATION_REPUTATION_SERVER,
               SERVER_RESPONSE_FAILED);
    AccumulateCategorical(HTTPStatusToLabel(status));
    return NS_ERROR_NOT_AVAILABLE;
  }

  std::string buf(mResponse.Data(), mResponse.Length());
  safe_browsing::ClientDownloadResponse response;
  if (!response.ParseFromString(buf)) {
    LOG(("Invalid protocol buffer response [this = %p]: %s", this,
         buf.c_str()));
    Accumulate(mozilla::Telemetry::APPLICATION_REPUTATION_SERVER,
               SERVER_RESPONSE_INVALID);
    return NS_ERROR_CANNOT_CONVERT_DATA;
  }

  Accumulate(mozilla::Telemetry::APPLICATION_REPUTATION_SERVER,
             SERVER_RESPONSE_VALID);
  AccumulateCategorical(
      mozilla::Telemetry::LABELS_APPLICATION_REPUTATION_SERVER_2::
          ResponseValid);

  // Clamp responses 0-7, we only know about 0-4 for now.
  Accumulate(mozilla::Telemetry::APPLICATION_REPUTATION_SERVER_VERDICT,
             std::min<uint32_t>(response.verdict(), 7));
  const char* ext = GetFileExt(mFileName);
  AccumulateCategoricalKeyed(nsCString(ext), VerdictToLabel(std::min<uint32_t>(
                                                 response.verdict(), 7)));
  switch (response.verdict()) {
    case safe_browsing::ClientDownloadResponse::DANGEROUS:
      aVerdict = nsIApplicationReputationService::VERDICT_DANGEROUS;
      aReason = Reason::VerdictDangerous;
      break;
    case safe_browsing::ClientDownloadResponse::DANGEROUS_HOST:
      aVerdict = nsIApplicationReputationService::VERDICT_DANGEROUS_HOST;
      aReason = Reason::VerdictDangerousHost;
      break;
    case safe_browsing::ClientDownloadResponse::POTENTIALLY_UNWANTED:
      aVerdict = nsIApplicationReputationService::VERDICT_POTENTIALLY_UNWANTED;
      aReason = Reason::VerdictUnwanted;
      break;
    case safe_browsing::ClientDownloadResponse::UNCOMMON:
      aVerdict = nsIApplicationReputationService::VERDICT_UNCOMMON;
      aReason = Reason::VerdictUncommon;
      break;
    case safe_browsing::ClientDownloadResponse::UNKNOWN:
      aVerdict = nsIApplicationReputationService::VERDICT_SAFE;
      aReason = Reason::VerdictUnknown;
      break;
    case safe_browsing::ClientDownloadResponse::SAFE:
      aVerdict = nsIApplicationReputationService::VERDICT_SAFE;
      aReason = Reason::VerdictSafe;
      break;
    default:
      // Treat everything else as safe
      aVerdict = nsIApplicationReputationService::VERDICT_SAFE;
      aReason = Reason::VerdictUnrecognized;
      break;
  }

  return NS_OK;
}

NS_IMPL_ISUPPORTS(ApplicationReputationService, nsIApplicationReputationService)

ApplicationReputationService*
    ApplicationReputationService::gApplicationReputationService = nullptr;

already_AddRefed<ApplicationReputationService>
ApplicationReputationService::GetSingleton() {
  if (!gApplicationReputationService) {
    // Note: This is cleared in the new ApplicationReputationService destructor.
    gApplicationReputationService = new ApplicationReputationService();
  }
  return do_AddRef(gApplicationReputationService);
}

ApplicationReputationService::ApplicationReputationService() {
  LOG(("Application reputation service started up"));
}

ApplicationReputationService::~ApplicationReputationService() {
  LOG(("Application reputation service shutting down"));
  MOZ_ASSERT(gApplicationReputationService == this);
  gApplicationReputationService = nullptr;
}

NS_IMETHODIMP
ApplicationReputationService::QueryReputation(
    nsIApplicationReputationQuery* aQuery,
    nsIApplicationReputationCallback* aCallback) {
  LOG(("Starting application reputation check [query=%p]", aQuery));
  NS_ENSURE_ARG_POINTER(aQuery);
  NS_ENSURE_ARG_POINTER(aCallback);

  nsresult rv = QueryReputationInternal(aQuery, aCallback);
  if (NS_FAILED(rv)) {
    Reason reason = rv == NS_ERROR_NOT_AVAILABLE ? Reason::DPDisabled
                                                 : Reason::InternalError;

    AccumulateCategorical(reason);
    Accumulate(mozilla::Telemetry::APPLICATION_REPUTATION_SHOULD_BLOCK, false);

    aCallback->OnComplete(false, rv,
                          nsIApplicationReputationService::VERDICT_SAFE);
  }
  return NS_OK;
}

nsresult ApplicationReputationService::QueryReputationInternal(
    nsIApplicationReputationQuery* aQuery,
    nsIApplicationReputationCallback* aCallback) {
  // If malware checks aren't enabled, don't query application reputation.
  if (!Preferences::GetBool(PREF_SB_MALWARE_ENABLED, false)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  if (!Preferences::GetBool(PREF_SB_DOWNLOADS_ENABLED, false)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsCOMPtr<nsIURI> uri;
  nsresult rv = aQuery->GetSourceURI(getter_AddRefs(uri));
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
