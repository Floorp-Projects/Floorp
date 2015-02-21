/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Sandbox.h"

#include "nsCocoaFeatures.h"
#include "mozilla/Preferences.h"

// XXX There are currently problems with the /usr/include/sandbox.h file on
// some/all of the Macs in Mozilla's build system.  For the time being (until
// this problem is resolved), we refer directly to what we need from it,
// rather than including it here.
extern "C" int sandbox_init(const char *profile, uint64_t flags, char **errorbuf);
extern "C" void sandbox_free_error(char *errorbuf);

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
  "    (literal \"%s\")\n"
  "    (literal \"%s\")\n"
  "    (literal \"%s\"))\n";

static const char contentSandboxRules[] =
  "(version 1)\n"
  "\n"
  "(define sandbox-level %d)\n"
  "(define macosMajorVersion %d)\n"
  "(define macosMinorVersion %d)\n"
  "(define appPath \"%s\")\n"
  "(define appBinaryPath \"%s\")\n"
  "(define appDir \"%s\")\n"
  "(define home-path \"%s\")\n"
  "\n"
  "(import \"/System/Library/Sandbox/Profiles/system.sb\")\n"
  "\n"
  "(if \n"
  "  (or\n"
  "    (< macosMinorVersion 9)\n"
  "    (= sandbox-level 0))\n"
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
  "    (define var-folders2-re (string-append var-folders-re \"/[^/]*/[^/]\"))\n"
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
  "    \n"
  "    (allow signal (target self))\n"
  "    (allow job-creation (literal \"/Library/CoreMediaIO/Plug-Ins/DAL\"))\n"
  "\n"
  "    (allow mach-lookup\n"
  "        (global-name \"com.apple.coreservices.launchservicesd\")\n"
  "        (global-name \"com.apple.coreservices.appleevents\")\n"
  "        (global-name \"com.apple.pasteboard.1\")\n"
  "        (global-name \"com.apple.window_proxies\")\n"
  "        (global-name \"com.apple.windowserver.active\")\n"
  "        (global-name \"com.apple.cvmsServ\")\n"
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
  "        (global-name \"com.apple.DesktopServicesHelper\")\n"
  "        (global-name \"com.apple.printtool.daemon\"))\n"
  "    \n"
  "    (allow iokit-open\n"
  "        (iokit-user-client-class \"AppleGraphicsControlClient\")\n"
  "        (iokit-user-client-class \"IOHIDParamUserClient\")\n"
  "        (iokit-user-client-class \"IOAudioControlUserClient\")\n"
  "        (iokit-user-client-class \"IOAudioEngineUserClient\")\n"
  "        (iokit-user-client-class \"IGAccelDevice\")\n"
  "        (iokit-user-client-class \"nvDevice\")\n"
  "        (iokit-user-client-class \"nvSharedUserClient\")\n"
  "        (iokit-user-client-class \"nvFermiGLContext\")\n"
  "        (iokit-user-client-class \"IGAccelGLContext\")\n"
  "        (iokit-user-client-class \"AGPMClient\")\n"
  "        (iokit-user-client-class \"IOSurfaceRootUserClient\")\n"
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
  "    \n"
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
  "    (allow file-read* (var-folders2-regex \"/com\\.apple\\.IconServices/\"))\n"
  "\n"
  "    (allow file-write* (var-folders2-regex \"/org\\.chromium\\.[a-zA-Z0-9]*$\"))\n"
  "    \n"
  "; the following rules should be removed when printing and \n"
  "; opening a file from disk are brokered through the main process\n"
  "    (allow file*\n"
  "        (require-all\n"
  "            (subpath home-path)\n"
  "            (require-not\n"
  "                (home-subpath \"/Library\"))))\n"
  "    \n"
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
  "  )\n"
  ")\n";

bool StartMacSandbox(MacSandboxInfo aInfo, nsCString &aErrorMessage)
{
  nsAutoCString profile;
  if (aInfo.type == MacSandboxType_Plugin) {
    if (nsCocoaFeatures::OnLionOrLater()) {
      profile.AppendPrintf(pluginSandboxRules, "", ";",
                           aInfo.pluginInfo.pluginPath.get(),
                           aInfo.pluginInfo.pluginBinaryPath.get(),
                           aInfo.appPath.get(),
                           aInfo.appBinaryPath.get());
    } else {
      profile.AppendPrintf(pluginSandboxRules, ";", "",
                           aInfo.pluginInfo.pluginPath.get(),
                           aInfo.pluginInfo.pluginBinaryPath.get(),
                           aInfo.appPath.get(),
                           aInfo.appBinaryPath.get());
    }
  }
  else if (aInfo.type == MacSandboxType_Content) {
    profile.AppendPrintf(contentSandboxRules,
                         Preferences::GetInt("security.sandbox.macos.content.level"),
                         nsCocoaFeatures::OSXVersionMajor(),
                         nsCocoaFeatures::OSXVersionMinor(),
                         aInfo.appPath.get(),
                         aInfo.appBinaryPath.get(),
                         aInfo.appDir.get(),
                         getenv("HOME"));
  }
  else {
    aErrorMessage.AppendPrintf("Unexpected sandbox type %u", aInfo.type);
    return false;
  }

  char *errorbuf = NULL;
  if (sandbox_init(profile.get(), 0, &errorbuf)) {
    if (errorbuf) {
      aErrorMessage.AppendPrintf("sandbox_init() failed with error \"%s\"",
                                 errorbuf);
      printf("profile: %s\n", profile.get());
      sandbox_free_error(errorbuf);
    }
    return false;
  }

  return true;
}

} // namespace mozilla
