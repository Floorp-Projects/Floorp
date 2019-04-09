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

  (if (defined? 'file-map-executable)
    (allow file-map-executable file-read*
      (subpath "/System/Library")
      (subpath "/usr/lib")
      (subpath app-path))
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

  (allow mach-lookup
    (global-name "com.apple.coreservices.launchservicesd"))
)SANDBOX_LITERAL";

}  // namespace mozilla

#endif  // mozilla_SandboxPolicyUtility_h
