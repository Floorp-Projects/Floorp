/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_SandboxPolicyGMP_h
#define mozilla_SandboxPolicyGMP_h

namespace mozilla {

static const char SandboxPolicyGMP[] = R"SANDBOX_LITERAL(
  (version 1)

  (define should-log (param "SHOULD_LOG"))
  (define plugin-binary-path (param "PLUGIN_BINARY_PATH"))
  (define app-path (param "APP_PATH"))
  (define app-binary-path (param "APP_BINARY_PATH"))
  (define hasWindowServer (param "HAS_WINDOW_SERVER"))

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
      (subpath "/System/Library/PrivateFrameworks")
      (regex #"^/usr/lib/libstdc\+\+\.[^/]*dylib$")
      (literal plugin-binary-path)
      (literal app-binary-path)
      (subpath app-path))
    (allow file-read*
      (subpath "/System/Library/PrivateFrameworks")
      (regex #"^/usr/lib/libstdc\+\+\.[^/]*dylib$")
      (literal plugin-binary-path)
      (literal app-binary-path)
      (subpath app-path)))

  (allow signal (target self))
  (allow sysctl-read)
  (allow iokit-open (iokit-user-client-class "IOHIDParamUserClient"))
  (allow file-read*
      (literal "/etc")
      (literal "/dev/random")
      (literal "/dev/urandom")
      (literal "/usr/share/icu/icudt51l.dat")
      (subpath "/System/Library/Displays/Overrides")
      (subpath "/System/Library/CoreServices/CoreTypes.bundle"))

  (if (string=? hasWindowServer "TRUE")
    (allow mach-lookup (global-name "com.apple.windowserver.active")))
)SANDBOX_LITERAL";

}  // namespace mozilla

#endif  // mozilla_SandboxPolicyGMP_h
