/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// The Mac sandbox module is a static library (a Library in moz.build terms)
// that can be linked into any binary (for example plugin-container or XUL).
// It must not have dependencies on any other Mozilla module.  This is why,
// for example, it has its own OS X version detection code, rather than
// linking to nsCocoaFeatures.mm in XUL.

#include "Sandbox.h"

#include <stdio.h>
#include <stdlib.h>
#include <CoreFoundation/CoreFoundation.h>

#include "mozilla/Assertions.h"

// XXX There are currently problems with the /usr/include/sandbox.h file on
// some/all of the Macs in Mozilla's build system.  For the time being (until
// this problem is resolved), we refer directly to what we need from it,
// rather than including it here.
extern "C" int sandbox_init(const char *profile, uint64_t flags, char **errorbuf);
extern "C" void sandbox_free_error(char *errorbuf);

#define MAC_OS_X_VERSION_10_0_HEX  0x00001000
#define MAC_OS_X_VERSION_10_6_HEX  0x00001060
#define MAC_OS_X_VERSION_10_7_HEX  0x00001070
#define MAC_OS_X_VERSION_10_8_HEX  0x00001080
#define MAC_OS_X_VERSION_10_9_HEX  0x00001090
#define MAC_OS_X_VERSION_10_10_HEX 0x000010A0

// Note about "major", "minor" and "bugfix" in the following code:
//
// The code decomposes an OS X version number into these components, and in
// doing so follows Apple's terminology in Gestalt.h.  But this is very
// misleading, because in other contexts Apple uses the "minor" component of
// an OS X version number to indicate a "major" release (for example the "9"
// in OS X 10.9.5), and the "bugfix" component to indicate a "minor" release
// (for example the "5" in OS X 10.9.5).

class OSXVersion {
public:
  static int32_t OSXVersionMinor();

private:
  static void GetSystemVersion(int32_t& aMajor, int32_t& aMinor, int32_t& aBugFix);
  static int32_t GetVersionNumber();
  static int32_t mOSXVersion;
};

int32_t OSXVersion::mOSXVersion = -1;

int32_t OSXVersion::OSXVersionMinor()
{
  return (GetVersionNumber() & 0xF0) >> 4;
}

void
OSXVersion::GetSystemVersion(int32_t& aMajor, int32_t& aMinor, int32_t& aBugFix)
{
  SInt32 major = 0, minor = 0, bugfix = 0;

  CFURLRef url =
    CFURLCreateWithString(kCFAllocatorDefault,
                          CFSTR("file:///System/Library/CoreServices/SystemVersion.plist"),
                          NULL);
  CFReadStreamRef stream =
    CFReadStreamCreateWithFile(kCFAllocatorDefault, url);
  CFReadStreamOpen(stream);
  CFDictionaryRef sysVersionPlist = (CFDictionaryRef)
    CFPropertyListCreateWithStream(kCFAllocatorDefault,
                                   stream, 0, kCFPropertyListImmutable,
                                   NULL, NULL);
  CFReadStreamClose(stream);
  CFRelease(stream);
  CFRelease(url);

  CFStringRef versionString = (CFStringRef)
    CFDictionaryGetValue(sysVersionPlist, CFSTR("ProductVersion"));
  CFArrayRef versions =
    CFStringCreateArrayBySeparatingStrings(kCFAllocatorDefault,
                                           versionString, CFSTR("."));
  CFIndex count = CFArrayGetCount(versions);
  if (count > 0) {
    CFStringRef component = (CFStringRef) CFArrayGetValueAtIndex(versions, 0);
    major = CFStringGetIntValue(component);
    if (count > 1) {
      component = (CFStringRef) CFArrayGetValueAtIndex(versions, 1);
      minor = CFStringGetIntValue(component);
      if (count > 2) {
        component = (CFStringRef) CFArrayGetValueAtIndex(versions, 2);
        bugfix = CFStringGetIntValue(component);
      }
    }
  }
  CFRelease(sysVersionPlist);
  CFRelease(versions);

  // If 'major' isn't what we expect, assume the oldest version of OS X we
  // currently support (OS X 10.6).
  if (major != 10) {
    aMajor = 10; aMinor = 6; aBugFix = 0;
  } else {
    aMajor = major; aMinor = minor; aBugFix = bugfix;
  }
}

int32_t
OSXVersion::GetVersionNumber()
{
  if (mOSXVersion == -1) {
    int32_t major, minor, bugfix;
    GetSystemVersion(major, minor, bugfix);
    mOSXVersion = MAC_OS_X_VERSION_10_0_HEX + (minor << 4) + bugfix;
  }
  return mOSXVersion;
}

namespace mozilla {

static const char pluginSandboxRules[] =
  "(version 1)\n"
  "(deny default)\n"
  "(allow signal (target self))\n"
  "(allow sysctl-read)\n"
  "(allow iokit-open (iokit-user-client-class \"IOHIDParamUserClient\"))\n"
  "(allow mach-lookup\n"
  "    (global-name \"com.apple.cfprefsd.agent\")\n"
  "    (global-name \"com.apple.cfprefsd.daemon\")\n"
  "    (global-name \"com.apple.system.opendirectoryd.libinfo\")\n"
  "    (global-name \"com.apple.system.logger\")\n"
  "    (global-name \"com.apple.ls.boxd\"))\n"
  "(allow file-read*\n"
  "    (regex #\"^/etc$\")\n"
  "    (regex #\"^/dev/u?random$\")\n"
  "    (literal \"/usr/share/icu/icudt51l.dat\")\n"
  "    (regex #\"^/System/Library/Displays/Overrides/*\")\n"
  "    (regex #\"^/System/Library/CoreServices/CoreTypes.bundle/*\")\n"
  "    (regex #\"^/System/Library/PrivateFrameworks/*\")\n"
  "    (regex #\"^/usr/lib/libstdc\\+\\+\\..*dylib$\")\n"
  "    (literal \"%s\")\n"
  "    (literal \"%s\")\n"
  "    (literal \"%s\"))\n";

static const char widevinePluginSandboxRulesAddend[] =
  "(allow mach-lookup (global-name \"com.apple.windowserver.active\"))\n";

static const char contentSandboxRules[] =
  "(version 1)\n"
  "\n"
  "(define sandbox-level %d)\n"
  "(define macosMinorVersion %d)\n"
  "(define appPath \"%s\")\n"
  "(define appBinaryPath \"%s\")\n"
  "(define appDir \"%s\")\n"
  "(define appTempDir \"%s\")\n"
  "(define hasProfileDir %d)\n"
  "(define profileDir \"%s\")\n"
  "(define home-path \"%s\")\n"
  "\n"
  "; Allow read access to standard system paths.\n"
  "(allow file-read*\n"
  "  (require-all (file-mode #o0004)\n"
  "    (require-any (subpath \"/Library/Filesystems/NetFSPlugins\")\n"
  "      (subpath \"/System\")\n"
  "      (subpath \"/private/var/db/dyld\")\n"
  "      (subpath \"/usr/lib\")\n"
  "      (subpath \"/usr/share\"))))\n"
  "\n"
  "(allow file-read-metadata\n"
  "  (literal \"/etc\")\n"
  "  (literal \"/tmp\")\n"
  "  (literal \"/var\")\n"
  "  (literal \"/private/etc/localtime\"))\n"
  "\n"
  "; Allow read access to standard special files.\n"
  "(allow file-read*\n"
  "  (literal \"/dev/autofs_nowait\")\n"
  "  (literal \"/dev/random\")\n"
  "  (literal \"/dev/urandom\"))\n"
  "\n"
  "(allow file-read*\n"
  "  file-write-data\n"
  "  (literal \"/dev/null\")\n"
  "  (literal \"/dev/zero\"))\n"
  "\n"
  "(allow file-read*\n"
  "  file-write-data\n"
  "  file-ioctl\n"
  "  (literal \"/dev/dtracehelper\"))\n"
  "\n"
  "; Used to read hw.ncpu, hw.physicalcpu_max, kern.ostype, and others\n"
  "(allow sysctl-read)\n"
  "\n"
  "(begin\n"
  "  (deny default)\n"
  "  (debug deny)\n"
  "\n"
  "  (define resolving-literal literal)\n"
  "  (define resolving-subpath subpath)\n"
  "  (define resolving-regex regex)\n"
  "\n"
  "  (define container-path appPath)\n"
  "  (define appdir-path appDir)\n"
  "  (define var-folders-re \"^/private/var/folders/[^/][^/]\")\n"
  "  (define var-folders2-re (string-append var-folders-re \"/[^/]+/[^/]\"))\n"
  "\n"
  "  (define (home-regex home-relative-regex)\n"
  "    (resolving-regex (string-append \"^\" (regex-quote home-path) home-relative-regex)))\n"
  "  (define (home-subpath home-relative-subpath)\n"
  "    (resolving-subpath (string-append home-path home-relative-subpath)))\n"
  "  (define (home-literal home-relative-literal)\n"
  "    (resolving-literal (string-append home-path home-relative-literal)))\n"
  "\n"
  "  (define (profile-subpath profile-relative-subpath)\n"
  "    (resolving-subpath (string-append profileDir profile-relative-subpath)))\n"
  "\n"
  "  (define (var-folders-regex var-folders-relative-regex)\n"
  "    (resolving-regex (string-append var-folders-re var-folders-relative-regex)))\n"
  "  (define (var-folders2-regex var-folders2-relative-regex)\n"
  "    (resolving-regex (string-append var-folders2-re var-folders2-relative-regex)))\n"
  "\n"
  "  (define (allow-shared-preferences-read domain)\n"
  "        (begin\n"
  "          (if (defined? `user-preference-read)\n"
  "            (allow user-preference-read (preference-domain domain)))\n"
  "          (allow file-read*\n"
  "                 (home-literal (string-append \"/Library/Preferences/\" domain \".plist\"))\n"
  "                 (home-regex (string-append \"/Library/Preferences/ByHost/\" (regex-quote domain) \"\\..*\\.plist$\")))\n"
  "          ))\n"
  "\n"
  "  (define (allow-shared-list domain)\n"
  "    (allow file-read*\n"
  "           (home-regex (string-append \"/Library/Preferences/\" (regex-quote domain)))))\n"
  "\n"
  "  (allow ipc-posix-shm\n"
  "      (ipc-posix-name-regex \"^/tmp/com.apple.csseed:\")\n"
  "      (ipc-posix-name-regex \"^CFPBS:\")\n"
  "      (ipc-posix-name-regex \"^AudioIO\"))\n"
  "\n"
  "  (allow file-read-metadata\n"
  "      (literal \"/home\")\n"
  "      (literal \"/net\")\n"
  "      (regex \"^/private/tmp/KSInstallAction\\.\")\n"
  "      (var-folders-regex \"/\")\n"
  "      (home-subpath \"/Library\"))\n"
  "\n"
  "  (allow signal (target self))\n"
  "  (allow job-creation (literal \"/Library/CoreMediaIO/Plug-Ins/DAL\"))\n"
  "  (allow iokit-set-properties (iokit-property \"IOAudioControlValue\"))\n"
  "\n"
  "  (allow mach-lookup\n"
  "      (global-name \"com.apple.coreservices.launchservicesd\")\n"
  "      (global-name \"com.apple.coreservices.appleevents\")\n"
  "      (global-name \"com.apple.pasteboard.1\")\n"
  "      (global-name \"com.apple.window_proxies\")\n"
  "      (global-name \"com.apple.windowserver.active\")\n"
  "      (global-name \"com.apple.audio.coreaudiod\")\n"
  "      (global-name \"com.apple.audio.audiohald\")\n"
  "      (global-name \"com.apple.PowerManagement.control\")\n"
  "      (global-name \"com.apple.cmio.VDCAssistant\")\n"
  "      (global-name \"com.apple.SystemConfiguration.configd\")\n"
  "      (global-name \"com.apple.iconservices\")\n"
  "      (global-name \"com.apple.cookied\")\n"
  "      (global-name \"com.apple.cache_delete\")\n"
  "      (global-name \"com.apple.pluginkit.pkd\")\n"
  "      (global-name \"com.apple.bird\")\n"
  "      (global-name \"com.apple.ocspd\")\n"
  "      (global-name \"com.apple.cmio.AppleCameraAssistant\")\n"
  "      (global-name \"com.apple.DesktopServicesHelper\"))\n"
  "\n"
  "; bug 1312273\n"
  "  (if (= macosMinorVersion 9)\n"
  "     (allow mach-lookup (global-name \"com.apple.xpcd\")))\n"
  "\n"
  "  (allow iokit-open\n"
  "      (iokit-user-client-class \"IOHIDParamUserClient\")\n"
  "      (iokit-user-client-class \"IOAudioControlUserClient\")\n"
  "      (iokit-user-client-class \"IOAudioEngineUserClient\")\n"
  "      (iokit-user-client-class \"IGAccelDevice\")\n"
  "      (iokit-user-client-class \"nvDevice\")\n"
  "      (iokit-user-client-class \"nvSharedUserClient\")\n"
  "      (iokit-user-client-class \"nvFermiGLContext\")\n"
  "      (iokit-user-client-class \"IGAccelGLContext\")\n"
  "      (iokit-user-client-class \"IGAccelSharedUserClient\")\n"
  "      (iokit-user-client-class \"IGAccelVideoContextMain\")\n"
  "      (iokit-user-client-class \"IGAccelVideoContextMedia\")\n"
  "      (iokit-user-client-class \"IGAccelVideoContextVEBox\")\n"
  "      (iokit-user-client-class \"RootDomainUserClient\")\n"
  "      (iokit-user-client-class \"IOUSBDeviceUserClientV2\")\n"
  "      (iokit-user-client-class \"IOUSBInterfaceUserClientV2\"))\n"
  "\n"
  "; depending on systems, the 1st, 2nd or both rules are necessary\n"
  "  (allow-shared-preferences-read \"com.apple.HIToolbox\")\n"
  "  (allow file-read-data (literal \"/Library/Preferences/com.apple.HIToolbox.plist\"))\n"
  "\n"
  "  (allow-shared-preferences-read \"com.apple.ATS\")\n"
  "  (allow file-read-data (literal \"/Library/Preferences/.GlobalPreferences.plist\"))\n"
  "\n"
  "  (allow file-read*\n"
  "      (subpath \"/Library/Fonts\")\n"
  "      (subpath \"/Library/Audio/Plug-Ins\")\n"
  "      (subpath \"/Library/CoreMediaIO/Plug-Ins/DAL\")\n"
  "      (subpath \"/Library/Spelling\")\n"
  "      (literal \"/\")\n"
  "      (literal \"/private/tmp\")\n"
  "      (literal \"/private/var/tmp\")\n"
  "\n"
  "      (home-literal \"/.CFUserTextEncoding\")\n"
  "      (home-literal \"/Library/Preferences/com.apple.DownloadAssessment.plist\")\n"
  "      (home-subpath \"/Library/Colors\")\n"
  "      (home-subpath \"/Library/Fonts\")\n"
  "      (home-subpath \"/Library/FontCollections\")\n"
  "      (home-subpath \"/Library/Keyboard Layouts\")\n"
  "      (home-subpath \"/Library/Input Methods\")\n"
  "      (home-subpath \"/Library/Spelling\")\n"
  "\n"
  "      (subpath appdir-path)\n"
  "\n"
  "      (literal appPath)\n"
  "      (literal appBinaryPath))\n"
  "\n"
  "  (allow-shared-list \"org.mozilla.plugincontainer\")\n"
  "\n"
  "; the following rule should be removed when microphone access\n"
  "; is brokered through the content process\n"
  "  (allow device-microphone)\n"
  "\n"
  "  (allow file* (var-folders2-regex \"/com\\.apple\\.IntlDataCache\\.le$\"))\n"
  "  (allow file-read*\n"
  "      (var-folders2-regex \"/com\\.apple\\.IconServices/\")\n"
  "      (var-folders2-regex \"/[^/]+\\.mozrunner/extensions/[^/]+/chrome/[^/]+/content/[^/]+\\.j(s|ar)$\"))\n"
  "\n"
  "  (allow file-write* (var-folders2-regex \"/org\\.chromium\\.[a-zA-Z0-9]*$\"))\n"
  "\n"
  "; Per-user and system-wide Extensions dir\n"
  "  (allow file-read*\n"
  "      (home-regex \"/Library/Application Support/[^/]+/Extensions/[^/]/\")\n"
  "      (resolving-regex \"/Library/Application Support/[^/]+/Extensions/[^/]/\"))\n"
  "\n"
  "; The following rules impose file access restrictions which get\n"
  "; more restrictive in higher levels. When file-origin-specific\n"
  "; content processes are used for file:// origin browsing, the\n"
  "; global file-read* permission should be removed from each level.\n"
  "\n"
  "; level 1: global read access permitted, no global write access\n"
  "  (if (= sandbox-level 1) (allow file-read*))\n"
  "\n"
  "; level 2: global read access permitted, no global write access,\n"
  ";          no read/write access to ~/Library,\n"
  ";          no read/write access to $PROFILE,\n"
  ";          read access permitted to $PROFILE/{extensions,weave,chrome}\n"
  "  (if (= sandbox-level 2)\n"
  "    (if (not (zero? hasProfileDir))\n"
  "      ; we have a profile dir\n"
  "      (begin\n"
  "        (allow file-read* (require-all\n"
  "              (require-not (home-subpath \"/Library\"))\n"
  "              (require-not (subpath profileDir))))\n"
  "        (allow file-read*\n"
  "              (profile-subpath \"/extensions\")\n"
  "              (profile-subpath \"/weave\")\n"
  "              (profile-subpath \"/chrome\")))\n"
  "      ; we don't have a profile dir\n"
  "      (allow file-read* (require-not (home-subpath \"/Library\")))))\n"
  "\n"
  "; accelerated graphics\n"
  "  (allow-shared-preferences-read \"com.apple.opengl\")\n"
  "  (allow-shared-preferences-read \"com.nvidia.OpenGL\")\n"
  "  (allow mach-lookup\n"
  "      (global-name \"com.apple.cvmsServ\"))\n"
  "  (allow iokit-open\n"
  "      (iokit-connection \"IOAccelerator\")\n"
  "      (iokit-user-client-class \"IOAccelerationUserClient\")\n"
  "      (iokit-user-client-class \"IOSurfaceRootUserClient\")\n"
  "      (iokit-user-client-class \"IOSurfaceSendRight\")\n"
  "      (iokit-user-client-class \"IOFramebufferSharedUserClient\")\n"
  "      (iokit-user-client-class \"AppleSNBFBUserClient\")\n"
  "      (iokit-user-client-class \"AGPMClient\")\n"
  "      (iokit-user-client-class \"AppleGraphicsControlClient\")\n"
  "      (iokit-user-client-class \"AppleGraphicsPolicyClient\"))\n"
  "\n"
  "; bug 1153809\n"
  "  (allow iokit-open\n"
  "      (iokit-user-client-class \"NVDVDContextTesla\")\n"
  "      (iokit-user-client-class \"Gen6DVDContext\"))\n"
  "\n"
  "; bug 1201935\n"
  "  (allow file-read*\n"
  "      (home-subpath \"/Library/Caches/TemporaryItems\"))\n"
  "\n"
  "; bug 1237847\n"
  "  (allow file-read*\n"
  "      (subpath appTempDir))\n"
  "  (allow file-write*\n"
  "      (subpath appTempDir))\n"
#ifdef DEBUG
  "\n"
  "; bug 1303987\n"
  "  (allow file-write* (var-folders-regex \"/\"))\n"
#endif
  ")\n";

bool StartMacSandbox(MacSandboxInfo aInfo, std::string &aErrorMessage)
{
  char *profile = NULL;
  if (aInfo.type == MacSandboxType_Plugin) {
    asprintf(&profile, pluginSandboxRules,
             aInfo.pluginInfo.pluginBinaryPath.c_str(),
             aInfo.appPath.c_str(),
             aInfo.appBinaryPath.c_str());

    if (profile &&
      aInfo.pluginInfo.type == MacSandboxPluginType_GMPlugin_EME_Widevine) {
      char *widevineProfile = NULL;
      asprintf(&widevineProfile, "%s%s", profile,
        widevinePluginSandboxRulesAddend);
      free(profile);
      profile = widevineProfile;
    }
  }
  else if (aInfo.type == MacSandboxType_Content) {
    MOZ_ASSERT(aInfo.level >= 1);
    if (aInfo.level >= 1) {
      asprintf(&profile, contentSandboxRules, aInfo.level,
               OSXVersion::OSXVersionMinor(),
               aInfo.appPath.c_str(),
               aInfo.appBinaryPath.c_str(),
               aInfo.appDir.c_str(),
               aInfo.appTempDir.c_str(),
               aInfo.hasSandboxedProfile ? 1 : 0,
               aInfo.profileDir.c_str(),
               getenv("HOME"));
    } else {
      fprintf(stderr,
        "Content sandbox disabled due to sandbox level setting\n");
      return false;
    }
  }
  else {
    char *msg = NULL;
    asprintf(&msg, "Unexpected sandbox type %u", aInfo.type);
    if (msg) {
      aErrorMessage.assign(msg);
      free(msg);
    }
    return false;
  }

  if (!profile) {
    fprintf(stderr, "Out of memory in StartMacSandbox()!\n");
    return false;
  }

  char *errorbuf = NULL;
  int rv = sandbox_init(profile, 0, &errorbuf);
  if (rv) {
    if (errorbuf) {
      char *msg = NULL;
      asprintf(&msg, "sandbox_init() failed with error \"%s\"", errorbuf);
      if (msg) {
        aErrorMessage.assign(msg);
        free(msg);
      }
      fprintf(stderr, "profile: %s\n", profile);
      sandbox_free_error(errorbuf);
    }
  }
  free(profile);
  if (rv) {
    return false;
  }

  return true;
}

} // namespace mozilla
