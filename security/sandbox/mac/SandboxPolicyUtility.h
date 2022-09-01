/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_SandboxPolicyUtility_h
#define mozilla_SandboxPolicyUtility_h

namespace mozilla {

static const char SandboxPolicyUtility[] = R"SANDBOX_LITERAL(
  (version 1)

  (define should-log (param "SHOULD_LOG"))
  (define app-path (param "APP_PATH"))
  (define crashPort (param "CRASH_PORT"))
  (define isRosettaTranslated (param "IS_ROSETTA_TRANSLATED"))

  (define (moz-deny feature)
    (if (string=? should-log "TRUE")
      (deny feature)
      (deny feature (with no-log))))

  (moz-deny default)
  ; These are not included in (deny default)
  (moz-deny process-info*)
  ; This isn't available in some older macOS releases.
  (if (defined? 'nvram*)
    (moz-deny nvram*))
  ; This property requires macOS 10.10+
  (if (defined? 'file-map-executable)
    (moz-deny file-map-executable))

  ; Needed for things like getpriority()/setpriority()/pthread_setname()
  (allow process-info-pidinfo process-info-setcontrol (target self))

  (if (defined? 'file-map-executable)
    (begin
      (if (string=? isRosettaTranslated "TRUE")
        (allow file-map-executable (subpath "/private/var/db/oah")))
      (allow file-map-executable file-read*
        (subpath "/System/Library")
        (subpath "/usr/lib")
        (subpath app-path)))
    (allow file-read*
      (subpath "/System/Library")
      (subpath "/usr/lib")
      (subpath app-path)))

  (if (string? crashPort)
    (allow mach-lookup (global-name crashPort)))

  (allow signal (target self))
  (allow sysctl-read)
  (allow file-read*
    (literal "/dev/random")
    (literal "/dev/urandom")
    (subpath "/usr/share/icu"))

  ; Timezone
  (allow file-read*
    (subpath "/private/var/db/timezone")
    (subpath "/usr/share/zoneinfo")
    (subpath "/usr/share/zoneinfo.default")
    (literal "/private/etc/localtime"))

  (allow mach-lookup
    (global-name "com.apple.coreservices.launchservicesd"))
)SANDBOX_LITERAL";

static const char SandboxPolicyUtilityAudioDecoderAppleMediaAddend[] =
    R"SANDBOX_LITERAL(
  ; For Utility AudioDecoder AppleMedia codecs
  (define macosVersion (string->number (param "MAC_OS_VERSION")))
  (if (>= macosVersion 1013)
   (allow mach-lookup
    ; bug 1565575
    (global-name "com.apple.audio.AudioComponentRegistrar")))
)SANDBOX_LITERAL";

}  // namespace mozilla

#endif  // mozilla_SandboxPolicyUtility_h
