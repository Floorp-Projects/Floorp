/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_SandboxPolicySocket_h
#define mozilla_SandboxPolicySocket_h

namespace mozilla {

static const char SandboxPolicySocket[] = R"SANDBOX_LITERAL(
  (version 1)

  (define should-log (param "SHOULD_LOG"))
  (define app-path (param "APP_PATH"))
  (define crashPort (param "CRASH_PORT"))
  (define home-path (param "HOME_PATH"))
  (define isRosettaTranslated (param "IS_ROSETTA_TRANSLATED"))

  (define (moz-deny feature)
    (if (string=? should-log "TRUE")
      (deny feature)
      (deny feature (with no-log))))

  (define (home-subpath home-relative-subpath)
    (subpath (string-append home-path home-relative-subpath)))
  (define (home-literal home-relative-literal)
    (literal (string-append home-path home-relative-literal)))
  (define (home-regex home-relative-regex)
    (regex (string-append "^" (regex-quote home-path) home-relative-regex)))

  (moz-deny default)
  ; These are not included in (deny default)
  (moz-deny process-info*)
  (moz-deny nvram*)
  (moz-deny file-map-executable)

  (if (string=? should-log "TRUE")
    (debug deny))

  ; Needed for things like getpriority()/setpriority()/pthread_setname()
  (allow process-info-pidinfo process-info-setcontrol (target self))

  (if (string=? isRosettaTranslated "TRUE")
    (allow file-map-executable (subpath "/private/var/db/oah")))

  (allow file-map-executable file-read*
    (subpath "/System/Library")
    (subpath "/usr/lib")
    (subpath app-path))

  (if (string? crashPort)
    (allow mach-lookup (global-name crashPort)))

  (allow signal (target self))
  (allow sysctl-read)
  (allow file-read*
    (literal "/dev/random")
    (literal "/dev/urandom")
    (subpath "/usr/share/icu"))

  ; For stat and symlink resolution
  (allow file-read-metadata (subpath "/"))

  ; Timezone
  (allow file-read*
    (subpath "/private/var/db/timezone")
    (subpath "/usr/share/zoneinfo")
    (subpath "/usr/share/zoneinfo.default")
    (literal "/private/etc/localtime"))

  ; Needed for some global preferences
  (allow file-read-data
    (literal "/Library/Preferences/.GlobalPreferences.plist")
    (home-literal "/Library/Preferences/.GlobalPreferences.plist")
    (home-regex #"/Library/Preferences/ByHost/\.GlobalPreferences.*")
    (home-literal "/Library/Preferences/com.apple.universalaccess.plist"))

  (allow file-read-data (literal "/private/etc/passwd"))

  (allow network-outbound
    (control-name "com.apple.netsrc")
    (literal "/private/var/run/mDNSResponder")
    (remote tcp)
    (remote udp))

  (allow system-socket
    (require-all (socket-domain AF_SYSTEM)
      (socket-protocol 2)) ; SYSPROTO_CONTROL
      (socket-domain AF_ROUTE))

  (allow network-bind network-inbound
    (local tcp)
    (local udp))

  ; Distributed notifications memory.
  (allow ipc-posix-shm-read-data
    (ipc-posix-name "apple.shm.notification_center"))

  ; Notification data from the security server database.
  (allow ipc-posix-shm
    (ipc-posix-name "com.apple.AppleDatabaseChanged"))

  ; From system.sb
  (allow mach-lookup
    (global-name "com.apple.bsd.dirhelper")
    (global-name "com.apple.coreservices.launchservicesd")
    (global-name "com.apple.system.notification_center"))

  ; resolv.conf and hosts file
  (allow file-read*
    (literal "/")
    (literal "/etc")
    (literal "/etc/hosts")
    (literal "/etc/resolv.conf")
    (literal "/private")
    (literal "/private/etc")
    (literal "/private/etc/hosts")
    (literal "/private/etc/resolv.conf")
    (literal "/private/var")
    (literal "/private/var/run")
    (literal "/private/var/run/resolv.conf")
    (literal "/var")
    (literal "/var/run"))

  ; Certificate databases
  (allow file-read*
    (subpath "/private/var/db/mds")
    (subpath "/Library/Keychains")
    (subpath "/System/Library/Keychains")
    (subpath "/System/Library/Security")
    (home-subpath "/Library/Keychains"))

  ; For enabling TCSM
  (allow sysctl-write
    (sysctl-name "kern.tcsm_enable"))
)SANDBOX_LITERAL";

}  // namespace mozilla

#endif  // mozilla_SandboxPolicySocket_h
