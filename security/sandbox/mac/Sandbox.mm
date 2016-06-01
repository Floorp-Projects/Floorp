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
  static bool OnLionOrLater();
  static int32_t OSXVersionMinor();

private:
  static void GetSystemVersion(int32_t& aMajor, int32_t& aMinor, int32_t& aBugFix);
  static int32_t GetVersionNumber();
  static int32_t mOSXVersion;
};

int32_t OSXVersion::mOSXVersion = -1;

bool OSXVersion::OnLionOrLater()
{
  return (GetVersionNumber() >= MAC_OS_X_VERSION_10_7_HEX);
}

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
  // Illegal syntax on OS X 10.6, needed on 10.7 and up.
  "%s(allow iokit-open (iokit-user-client-class \"IOHIDParamUserClient\"))\n"
  // Needed only on OS X 10.6
  "%s(allow file-read-data (literal \"%s\"))\n"
  "(allow mach-lookup\n"
  "    (global-name \"com.apple.cfprefsd.agent\")\n"
  "    (global-name \"com.apple.cfprefsd.daemon\")\n"
  "    (global-name \"com.apple.system.opendirectoryd.libinfo\")\n"
  "    (global-name \"com.apple.system.logger\")\n"
  "    (global-name \"com.apple.ls.boxd\"))\n"
  "(allow file-read*\n"
  "    (regex #\"^/etc$\")\n"
  "    (regex #\"^/dev/u?random$\")\n"
  "    (regex #\"^/(private/)?var($|/)\")\n"
  "    (literal \"/usr/share/icu/icudt51l.dat\")\n"
  "    (regex #\"^/System/Library/Displays/Overrides/*\")\n"
  "    (regex #\"^/System/Library/CoreServices/CoreTypes.bundle/*\")\n"
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
  "(define home-path \"%s\")\n"
  "\n"
  "; -------- START system.sb -------- \n"
  "(version 1)\n"
  "\n"
  ";;; Allow registration of per-pid services.\n"
  "(allow mach-register\n"
  "  (local-name-regex #\"\"))\n"
  "\n"
  ";;; Allow read access to standard system paths.\n"
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
  ";;; Allow access to standard special files.\n"
  "(allow file-read*\n"
  "  (literal \"/dev/autofs_nowait\")\n"
  "  (literal \"/dev/random\")\n"
  "  (literal \"/dev/urandom\")\n"
  "  (literal \"/private/etc/master.passwd\")\n"
  "  (literal \"/private/etc/passwd\"))\n"
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
  "(allow network-outbound\n"
  "  (literal \"/private/var/run/asl_input\")\n"
  "  (literal \"/private/var/run/syslog\"))\n"
  "\n"
  ";;; Allow creation of core dumps.\n"
  "(allow file-write-create\n"
  "  (require-all (regex #\"^/cores/\")\n"
  "    (vnode-type REGULAR-FILE)))\n"
  "\n"
  ";;; Allow IPC to standard system agents.\n"
  "(allow ipc-posix-shm-read*\n"
  "  (ipc-posix-name #\"apple.shm.notification_center\")\n"
  "  (ipc-posix-name-regex #\"^apple\.shm\.cfprefsd\.\"))\n"
  "\n"
  "(allow mach-lookup\n"
  "  (global-name \"com.apple.appsleep\")\n"
  "  (global-name \"com.apple.bsd.dirhelper\")\n"
  "  (global-name \"com.apple.cfprefsd.agent\")\n"
  "  (global-name \"com.apple.cfprefsd.daemon\")\n"
  "  (global-name \"com.apple.diagnosticd\")\n"
  "  (global-name \"com.apple.espd\")\n"
  "  (global-name \"com.apple.secinitd\")\n"
  "  (global-name \"com.apple.system.DirectoryService.libinfo_v1\")\n"
  "  (global-name \"com.apple.system.logger\")\n"
  "  (global-name \"com.apple.system.notification_center\")\n"
  "  (global-name \"com.apple.system.opendirectoryd.libinfo\")\n"
  "  (global-name \"com.apple.system.opendirectoryd.membership\")\n"
  "  (global-name \"com.apple.trustd\")\n"
  "  (global-name \"com.apple.trustd.agent\")\n"
  "  (global-name \"com.apple.xpc.activity.unmanaged\")\n"
  "  (global-name \"com.apple.xpcd\")\n"
  "  (local-name \"com.apple.cfprefsd.agent\"))\n"
  "\n"
  ";;; Allow mostly harmless operations.\n"
  "(allow sysctl-read)\n"
  "\n"
  ";;; (system-graphics) - Allow access to graphics hardware.\n"
  "(define (system-graphics)\n"
  "  ;; Preferences\n"
  "  (allow user-preference-read\n"
  "    (preference-domain \"com.apple.opengl\")\n"
  "    (preference-domain \"com.nvidia.OpenGL\"))\n"
  "  ;; OpenGL memory debugging\n"
  "  (allow mach-lookup\n"
  "    (global-name \"com.apple.gpumemd.source\"))\n"
  "  ;; CVMS\n"
  "  (allow mach-lookup\n"
  "    (global-name \"com.apple.cvmsServ\"))\n"
  "  ;; OpenCL\n"
  "  (allow iokit-open\n"
  "    (iokit-connection \"IOAccelerator\")\n"
  "    (iokit-user-client-class \"IOAccelerationUserClient\")\n"
  "    (iokit-user-client-class \"IOSurfaceRootUserClient\")\n"
  "    (iokit-user-client-class \"IOSurfaceSendRight\"))\n"
  "  ;; CoreVideo CVCGDisplayLink\n"
  "  (allow iokit-open\n"
  "    (iokit-user-client-class \"IOFramebufferSharedUserClient\"))\n"
  "  ;; H.264 Acceleration\n"
  "  (allow iokit-open\n"
  "    (iokit-user-client-class \"AppleSNBFBUserClient\"))\n"
  "  ;; QuartzCore\n"
  "  (allow iokit-open\n"
  "    (iokit-user-client-class \"AGPMClient\")\n"
  "    (iokit-user-client-class \"AppleGraphicsControlClient\")\n"
  "    (iokit-user-client-class \"AppleGraphicsPolicyClient\"))\n"
  "  ;; OpenGL\n"
  "  (allow iokit-open\n"
  "    (iokit-user-client-class \"AppleMGPUPowerControlClient\"))\n"
  "  ;; DisplayServices\n"
  "  (allow iokit-set-properties\n"
  "    (require-all (iokit-connection \"IODisplay\")\n"
  "      (require-any (iokit-property \"brightness\")\n"
  "        (iokit-property \"linear-brightness\")\n"
  "        (iokit-property \"commit\")\n"
  "        (iokit-property \"rgcs\")\n"
  "        (iokit-property \"ggcs\")\n"
  "        (iokit-property \"bgcs\")))))\n"
  "\n"
  ";;; (system-network) - Allow access to the network.\n"
  "(define (system-network)\n"
  "  (allow file-read*\n"
  "     (literal \"/Library/Preferences/com.apple.networkd.plist\"))\n"
  "  (allow mach-lookup\n"
  "     (global-name \"com.apple.SystemConfiguration.PPPController\")\n"
  "     (global-name \"com.apple.SystemConfiguration.SCNetworkReachability\")\n"
  "     (global-name \"com.apple.nehelper\")\n"
  "     (global-name \"com.apple.networkd\")\n"
  "     (global-name \"com.apple.nsurlstorage-cache\")\n"
  "     (global-name \"com.apple.symptomsd\")\n"
  "     (global-name \"com.apple.usymptomsd\"))\n"
  "  (allow network-outbound\n"
  "     (control-name \"com.apple.netsrc\")\n"
  "     (control-name \"com.apple.network.statistics\"))\n"
  "  (allow system-socket\n"
  "     (require-all (socket-domain AF_SYSTEM)\n"
  "       (socket-protocol 2)) ; SYSPROTO_CONTROL\n"
  "     (socket-domain AF_ROUTE)))\n"
  "\n"
  "; -------- END system.sb -------- \n"
  "\n"
  "(if \n"
  "  (or\n"
  "    (< macosMinorVersion 9)\n"
  "    (< sandbox-level 1))\n"
  "  (allow default)\n"
  "  (begin\n"
  "    (deny default)\n"
  "    (debug deny)\n"
  "\n"
  "    (define resolving-literal literal)\n"
  "    (define resolving-subpath subpath)\n"
  "    (define resolving-regex regex)\n"
  "\n"
  "    (define container-path appPath)\n"
  "    (define appdir-path appDir)\n"
  "    (define var-folders-re \"^/private/var/folders/[^/][^/]\")\n"
  "    (define var-folders2-re (string-append var-folders-re \"/[^/]+/[^/]\"))\n"
  "\n"
  "    (define (home-regex home-relative-regex)\n"
  "      (resolving-regex (string-append \"^\" (regex-quote home-path) home-relative-regex)))\n"
  "    (define (home-subpath home-relative-subpath)\n"
  "      (resolving-subpath (string-append home-path home-relative-subpath)))\n"
  "    (define (home-literal home-relative-literal)\n"
  "      (resolving-literal (string-append home-path home-relative-literal)))\n"
  "\n"
  "    (define (container-regex container-relative-regex)\n"
  "      (resolving-regex (string-append \"^\" (regex-quote container-path) container-relative-regex)))\n"
  "    (define (container-subpath container-relative-subpath)\n"
  "      (resolving-subpath (string-append container-path container-relative-subpath)))\n"
  "    (define (container-literal container-relative-literal)\n"
  "      (resolving-literal (string-append container-path container-relative-literal)))\n"
  "\n"
  "    (define (var-folders-regex var-folders-relative-regex)\n"
  "      (resolving-regex (string-append var-folders-re var-folders-relative-regex)))\n"
  "    (define (var-folders2-regex var-folders2-relative-regex)\n"
  "      (resolving-regex (string-append var-folders2-re var-folders2-relative-regex)))\n"
  "\n"
  "    (define (appdir-regex appdir-relative-regex)\n"
  "      (resolving-regex (string-append \"^\" (regex-quote appdir-path) appdir-relative-regex)))\n"
  "    (define (appdir-subpath appdir-relative-subpath)\n"
  "      (resolving-subpath (string-append appdir-path appdir-relative-subpath)))\n"
  "    (define (appdir-literal appdir-relative-literal)\n"
  "      (resolving-literal (string-append appdir-path appdir-relative-literal)))\n"
  "\n"
  "    (define (allow-shared-preferences-read domain)\n"
  "          (begin\n"
  "            (if (defined? `user-preference-read)\n"
  "              (allow user-preference-read (preference-domain domain)))\n"
  "            (allow file-read*\n"
  "                   (home-literal (string-append \"/Library/Preferences/\" domain \".plist\"))\n"
  "                   (home-regex (string-append \"/Library/Preferences/ByHost/\" (regex-quote domain) \"\\..*\\.plist$\")))\n"
  "            ))\n"
  "\n"
  "    (define (allow-shared-list domain)\n"
  "      (allow file-read*\n"
  "             (home-regex (string-append \"/Library/Preferences/\" (regex-quote domain)))))\n"
  "\n"
  "    (allow file-read-metadata)\n"
  "\n"
  "    (allow ipc-posix-shm\n"
  "        (ipc-posix-name-regex \"^/tmp/com.apple.csseed:\")\n"
  "        (ipc-posix-name-regex \"^CFPBS:\")\n"
  "        (ipc-posix-name-regex \"^AudioIO\"))\n"
  "\n"
  "    (allow file-read-metadata\n"
  "        (literal \"/home\")\n"
  "        (literal \"/net\")\n"
  "        (regex \"^/private/tmp/KSInstallAction\\.\")\n"
  "        (var-folders-regex \"/\")\n"
  "        (home-subpath \"/Library\"))\n"
  "\n"
  "    (allow signal (target self))\n"
  "    (allow job-creation (literal \"/Library/CoreMediaIO/Plug-Ins/DAL\"))\n"
  "    (allow iokit-set-properties (iokit-property \"IOAudioControlValue\"))\n"
  "\n"
  "    (allow mach-lookup\n"
  "        (global-name \"com.apple.coreservices.launchservicesd\")\n"
  "        (global-name \"com.apple.coreservices.appleevents\")\n"
  "        (global-name \"com.apple.pasteboard.1\")\n"
  "        (global-name \"com.apple.window_proxies\")\n"
  "        (global-name \"com.apple.windowserver.active\")\n"
  "        (global-name \"com.apple.audio.coreaudiod\")\n"
  "        (global-name \"com.apple.audio.audiohald\")\n"
  "        (global-name \"com.apple.PowerManagement.control\")\n"
  "        (global-name \"com.apple.cmio.VDCAssistant\")\n"
  "        (global-name \"com.apple.SystemConfiguration.configd\")\n"
  "        (global-name \"com.apple.iconservices\")\n"
  "        (global-name \"com.apple.cookied\")\n"
  "        (global-name \"com.apple.printuitool.agent\")\n"
  "        (global-name \"com.apple.printtool.agent\")\n"
  "        (global-name \"com.apple.cache_delete\")\n"
  "        (global-name \"com.apple.pluginkit.pkd\")\n"
  "        (global-name \"com.apple.bird\")\n"
  "        (global-name \"com.apple.ocspd\")\n"
  "        (global-name \"com.apple.cmio.AppleCameraAssistant\")\n"
  "        (global-name \"com.apple.DesktopServicesHelper\")\n"
  "        (global-name \"com.apple.printtool.daemon\"))\n"
  "\n"
  "    (allow iokit-open\n"
  "        (iokit-user-client-class \"IOHIDParamUserClient\")\n"
  "        (iokit-user-client-class \"IOAudioControlUserClient\")\n"
  "        (iokit-user-client-class \"IOAudioEngineUserClient\")\n"
  "        (iokit-user-client-class \"IGAccelDevice\")\n"
  "        (iokit-user-client-class \"nvDevice\")\n"
  "        (iokit-user-client-class \"nvSharedUserClient\")\n"
  "        (iokit-user-client-class \"nvFermiGLContext\")\n"
  "        (iokit-user-client-class \"IGAccelGLContext\")\n"
  "        (iokit-user-client-class \"IGAccelSharedUserClient\")\n"
  "        (iokit-user-client-class \"IGAccelVideoContextMain\")\n"
  "        (iokit-user-client-class \"IGAccelVideoContextMedia\")\n"
  "        (iokit-user-client-class \"IGAccelVideoContextVEBox\")\n"
  "        (iokit-user-client-class \"RootDomainUserClient\")\n"
  "        (iokit-user-client-class \"IOUSBDeviceUserClientV2\")\n"
  "        (iokit-user-client-class \"IOUSBInterfaceUserClientV2\"))\n"
  "\n"
  "; depending on systems, the 1st, 2nd or both rules are necessary\n"
  "    (allow-shared-preferences-read \"com.apple.HIToolbox\")\n"
  "    (allow file-read-data (literal \"/Library/Preferences/com.apple.HIToolbox.plist\"))\n"
  "\n"
  "    (allow-shared-preferences-read \"com.apple.ATS\")\n"
  "    (allow file-read-data (literal \"/Library/Preferences/.GlobalPreferences.plist\"))\n"
  "\n"
  "    (allow file-read*\n"
  "        (subpath \"/Library/Fonts\")\n"
  "        (subpath \"/Library/Audio/Plug-Ins\")\n"
  "        (subpath \"/Library/CoreMediaIO/Plug-Ins/DAL\")\n"
  "        (subpath \"/Library/Spelling\")\n"
  "        (subpath \"/private/etc/cups/ppd\")\n"
  "        (subpath \"/private/var/run/cupsd\")\n"
  "        (literal \"/\")\n"
  "        (literal \"/private/tmp\")\n"
  "        (literal \"/private/var/tmp\")\n"
  "\n"
  "        (home-literal \"/.CFUserTextEncoding\")\n"
  "        (home-literal \"/Library/Preferences/com.apple.DownloadAssessment.plist\")\n"
  "        (home-subpath \"/Library/Colors\")\n"
  "        (home-subpath \"/Library/Fonts\")\n"
  "        (home-subpath \"/Library/FontCollections\")\n"
  "        (home-subpath \"/Library/Keyboard Layouts\")\n"
  "        (home-subpath \"/Library/Input Methods\")\n"
  "        (home-subpath \"/Library/PDF Services\")\n"
  "        (home-subpath \"/Library/Spelling\")\n"
  "\n"
  "        (subpath appdir-path)\n"
  "\n"
  "        (literal appPath)\n"
  "        (literal appBinaryPath))\n"
  "\n"
  "    (allow-shared-list \"org.mozilla.plugincontainer\")\n"
  "\n"
  "; the following 2 rules should be removed when microphone and camera access\n"
  "; are brokered through the content process\n"
  "    (allow device-microphone)\n"
  "    (allow device-camera)\n"
  "\n"
  "    (allow file* (var-folders2-regex \"/com\\.apple\\.IntlDataCache\\.le$\"))\n"
  "    (allow file-read*\n"
  "        (var-folders2-regex \"/com\\.apple\\.IconServices/\")\n"
  "        (var-folders2-regex \"/[^/]+\\.mozrunner/extensions/[^/]+/chrome/[^/]+/content/[^/]+\\.j(s|ar)$\"))\n"
  "\n"
  "    (allow file-write* (var-folders2-regex \"/org\\.chromium\\.[a-zA-Z0-9]*$\"))\n"
  "    (allow file-read*\n"
  "        (home-regex \"/Library/Application Support/[^/]+/Extensions/[^/]/\")\n"
  "        (resolving-regex \"/Library/Application Support/[^/]+/Extensions/[^/]/\")\n"
  "        (home-regex \"/Library/Application Support/Firefox/Profiles/[^/]+/extensions/\")\n"
  "        (home-regex \"/Library/Application Support/Firefox/Profiles/[^/]+/weave/\"))\n"
  "\n"
  "; the following rules should be removed when printing and \n"
  "; opening a file from disk are brokered through the main process\n"
  "    (if\n"
  "      (< sandbox-level 2)\n"
  "      (allow file*\n"
  "          (require-not\n"
  "              (home-subpath \"/Library\")))\n"
  "      (allow file*\n"
  "          (require-all\n"
  "              (subpath home-path)\n"
  "              (require-not\n"
  "                  (home-subpath \"/Library\")))))\n"
  "\n"
  "; printing\n"
  "    (allow authorization-right-obtain\n"
  "           (right-name \"system.print.operator\")\n"
  "           (right-name \"system.printingmanager\"))\n"
  "    (allow mach-lookup\n"
  "           (global-name \"com.apple.printuitool.agent\")\n"
  "           (global-name \"com.apple.printtool.agent\")\n"
  "           (global-name \"com.apple.printtool.daemon\")\n"
  "           (global-name \"com.apple.sharingd\")\n"
  "           (global-name \"com.apple.metadata.mds\")\n"
  "           (global-name \"com.apple.mtmd.xpc\")\n"
  "           (global-name \"com.apple.FSEvents\")\n"
  "           (global-name \"com.apple.locum\")\n"
  "           (global-name \"com.apple.ImageCaptureExtension2.presence\"))\n"
  "    (allow file-read*\n"
  "           (home-literal \"/.cups/lpoptions\")\n"
  "           (home-literal \"/.cups/client.conf\")\n"
  "           (literal \"/private/etc/cups/lpoptions\")\n"
  "           (literal \"/private/etc/cups/client.conf\")\n"
  "           (subpath \"/private/etc/cups/ppd\")\n"
  "           (literal \"/private/var/run/cupsd\"))\n"
  "    (allow-shared-preferences-read \"org.cups.PrintingPrefs\")\n"
  "    (allow-shared-preferences-read \"com.apple.finder\")\n"
  "    (allow-shared-preferences-read \"com.apple.LaunchServices\")\n"
  "    (allow-shared-preferences-read \".GlobalPreferences\")\n"
  "    (allow network-outbound\n"
  "        (literal \"/private/var/run/cupsd\")\n"
  "        (literal \"/private/var/run/mDNSResponder\"))\n"
  "\n"
  "; print preview\n"
  "    (if (> macosMinorVersion 9)\n"
  "        (allow lsopen))\n"
  "    (allow file-write* file-issue-extension (var-folders2-regex \"/\"))\n"
  "    (allow file-read-xattr (literal \"/Applications/Preview.app\"))\n"
  "    (allow mach-task-name)\n"
  "    (allow mach-register)\n"
  "    (allow file-read-data\n"
  "        (regex \"^/Library/Printers/[^/]+/PDEs/[^/]+.plugin\")\n"
  "        (subpath \"/Library/PDF Services\")\n"
  "        (subpath \"/Applications/Preview.app\")\n"
  "        (home-literal \"/Library/Preferences/com.apple.ServicesMenu.Services.plist\"))\n"
  "    (allow mach-lookup\n"
  "        (global-name \"com.apple.pbs.fetch_services\")\n"
  "        (global-name \"com.apple.tsm.uiserver\")\n"
  "        (global-name \"com.apple.ls.boxd\")\n"
  "        (global-name \"com.apple.coreservices.quarantine-resolver\")\n"
  "        (global-name-regex \"_OpenStep$\"))\n"
  "    (allow appleevent-send\n"
  "        (appleevent-destination \"com.apple.preview\")\n"
  "        (appleevent-destination \"com.apple.imagecaptureextension2\"))\n"
  "\n"
  "; accelerated graphics\n"
  "    (allow-shared-preferences-read \"com.apple.opengl\")\n"
  "    (allow-shared-preferences-read \"com.nvidia.OpenGL\")\n"
  "    (allow mach-lookup\n"
  "        (global-name \"com.apple.cvmsServ\"))\n"
  "    (allow iokit-open\n"
  "        (iokit-connection \"IOAccelerator\")\n"
  "        (iokit-user-client-class \"IOAccelerationUserClient\")\n"
  "        (iokit-user-client-class \"IOSurfaceRootUserClient\")\n"
  "        (iokit-user-client-class \"IOSurfaceSendRight\")\n"
  "        (iokit-user-client-class \"IOFramebufferSharedUserClient\")\n"
  "        (iokit-user-client-class \"AppleSNBFBUserClient\")\n"
  "        (iokit-user-client-class \"AGPMClient\")\n"
  "        (iokit-user-client-class \"AppleGraphicsControlClient\")\n"
  "        (iokit-user-client-class \"AppleGraphicsPolicyClient\"))\n"
  "\n"
  "; bug 1153809\n"
  "    (allow iokit-open\n"
  "        (iokit-user-client-class \"NVDVDContextTesla\")\n"
  "        (iokit-user-client-class \"Gen6DVDContext\"))\n"
  "\n"
  "; bug 1190032\n"
  "    (allow file*\n"
  "        (home-regex \"/Library/Caches/TemporaryItems/plugtmp.*\"))\n"
  "\n"
  "; bug 1201935\n"
  "    (allow file-read*\n"
  "        (home-subpath \"/Library/Caches/TemporaryItems\"))\n"
  "\n"
  "; bug 1237847\n"
  "    (allow file-read*\n"
  "        (subpath appTempDir))\n"
  "    (allow file-write*\n"
  "        (subpath appTempDir))\n"
  "  )\n"
  ")\n";

bool StartMacSandbox(MacSandboxInfo aInfo, std::string &aErrorMessage)
{
  char *profile = NULL;
  if (aInfo.type == MacSandboxType_Plugin) {
    if (OSXVersion::OnLionOrLater()) {
      asprintf(&profile, pluginSandboxRules, "", ";",
               aInfo.pluginInfo.pluginPath.c_str(),
               aInfo.pluginInfo.pluginBinaryPath.c_str(),
               aInfo.appPath.c_str(),
               aInfo.appBinaryPath.c_str());
    } else {
      asprintf(&profile, pluginSandboxRules, ";", "",
               aInfo.pluginInfo.pluginPath.c_str(),
               aInfo.pluginInfo.pluginBinaryPath.c_str(),
               aInfo.appPath.c_str(),
               aInfo.appBinaryPath.c_str());
    }

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
    asprintf(&profile, contentSandboxRules, aInfo.level,
             OSXVersion::OSXVersionMinor(),
             aInfo.appPath.c_str(),
             aInfo.appBinaryPath.c_str(),
             aInfo.appDir.c_str(),
             aInfo.appTempDir.c_str(),
             getenv("HOME"));
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
