/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_SandboxPolicyRDD_h
#define mozilla_SandboxPolicyRDD_h

namespace mozilla {

static const char SandboxPolicyRDD[] = R"SANDBOX_LITERAL(
  (version 1)

  (define should-log (param "SHOULD_LOG"))
  (define macosVersion (string->number (param "MAC_OS_VERSION")))
  (define app-path (param "APP_PATH"))
  (define home-path (param "HOME_PATH"))
  (define crashPort (param "CRASH_PORT"))
  (define isRosettaTranslated (param "IS_ROSETTA_TRANSLATED"))

  (define (moz-deny feature)
    (if (string=? should-log "TRUE")
      (deny feature)
      (deny feature (with no-log))))

  (moz-deny default)
  ; These are not included in (deny default)
  (moz-deny process-info*)
  (moz-deny nvram*)
  (moz-deny iokit-get-properties)
  (moz-deny file-map-executable)

  ; Needed for things like getpriority()/setpriority()/pthread_setname()
  (allow process-info-pidinfo process-info-setcontrol (target self))

  (if (string=? isRosettaTranslated "TRUE")
    (allow file-map-executable (subpath "/private/var/db/oah")))

  (allow file-map-executable file-read*
    (subpath "/System")
    (subpath "/usr/lib")
    (subpath "/Library/GPUBundles")
    (subpath app-path))

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

  (allow sysctl-read
    (sysctl-name-regex #"^sysctl\.")
    (sysctl-name "kern.ostype")
    (sysctl-name "kern.osversion")
    (sysctl-name "kern.osrelease")
    (sysctl-name "kern.osproductversion")
    (sysctl-name "kern.version")
    ; TODO: remove "kern.hostname". Without it the tests hang, but the hostname
    ; is arguably sensitive information, so we should see what can be done about
    ; removing it.
    (sysctl-name "kern.hostname")
    (sysctl-name "hw.machine")
    (sysctl-name "hw.memsize")
    (sysctl-name "hw.model")
    (sysctl-name "hw.ncpu")
    (sysctl-name "hw.activecpu")
    (sysctl-name "hw.byteorder")
    (sysctl-name "hw.pagesize_compat")
    (sysctl-name "hw.logicalcpu_max")
    (sysctl-name "hw.physicalcpu_max")
    (sysctl-name "hw.busfrequency_compat")
    (sysctl-name "hw.busfrequency_max")
    (sysctl-name "hw.cpufrequency")
    (sysctl-name "hw.cpufrequency_compat")
    (sysctl-name "hw.cpufrequency_max")
    (sysctl-name "hw.l2cachesize")
    (sysctl-name "hw.l3cachesize")
    (sysctl-name "hw.cachelinesize")
    (sysctl-name "hw.cachelinesize_compat")
    (sysctl-name "hw.tbfrequency_compat")
    (sysctl-name "hw.vectorunit")
    (sysctl-name "hw.optional.sse2")
    (sysctl-name "hw.optional.sse3")
    (sysctl-name "hw.optional.sse4_1")
    (sysctl-name "hw.optional.sse4_2")
    (sysctl-name "hw.optional.avx1_0")
    (sysctl-name "hw.optional.avx2_0")
    (sysctl-name "hw.optional.avx512f")
    (sysctl-name "machdep.cpu.vendor")
    (sysctl-name "machdep.cpu.family")
    (sysctl-name "machdep.cpu.model")
    (sysctl-name "machdep.cpu.stepping")
    (sysctl-name "debug.intel.gstLevelGST")
    (sysctl-name "debug.intel.gstLoaderControl"))

  (define (home-regex home-relative-regex)
    (regex (string-append "^" (regex-quote home-path) home-relative-regex)))
  (define (home-subpath home-relative-subpath)
    (subpath (string-append home-path home-relative-subpath)))
  (define (home-literal home-relative-literal)
    (literal (string-append home-path home-relative-literal)))
  (define (allow-shared-list domain)
    (allow file-read*
           (home-regex (string-append "/Library/Preferences/" (regex-quote domain)))))

  (allow ipc-posix-shm-read-data ipc-posix-shm-write-data
    (ipc-posix-name-regex #"^CFPBS:"))

  (allow mach-lookup
    (global-name "com.apple.CoreServices.coreservicesd")
    (global-name "com.apple.coreservices.launchservicesd")
    (global-name "com.apple.lsd.mapdb"))

  (allow file-read*
      (subpath "/Library/ColorSync/Profiles")
      (literal "/")
      (literal "/private/tmp")
      (literal "/private/var/tmp")
      (home-subpath "/Library/Colors")
      (home-subpath "/Library/ColorSync/Profiles"))

  (allow mach-lookup
    ; bug 1392988
    (xpc-service-name "com.apple.coremedia.videodecoder")
    (xpc-service-name "com.apple.coremedia.videoencoder"))

  (if (>= macosVersion 1100)
    (allow mach-lookup
      ; bug 1655655
      (global-name "com.apple.trustd.agent")))

  ; Only supported on macOS 10.10+
  (if (defined? 'iokit-get-properties)
    (allow iokit-get-properties
      (iokit-property "board-id")
      (iokit-property "class-code")
      (iokit-property "vendor-id")
      (iokit-property "device-id")
      (iokit-property "IODVDBundleName")
      (iokit-property "IOGLBundleName")
      (iokit-property "IOGVACodec")
      (iokit-property "IOGVAHEVCDecode")
      (iokit-property "IOAVDHEVCDecodeCapabilities")
      (iokit-property "IOGVAHEVCEncode")
      (iokit-property "IOGVAXDecode")
      (iokit-property "IOPCITunnelled")
      (iokit-property "IOVARendererID")
      (iokit-property "MetalPluginName")
      (iokit-property "MetalPluginClassName")))

; accelerated graphics
  (allow user-preference-read (preference-domain "com.apple.opengl"))
  (allow user-preference-read (preference-domain "com.nvidia.OpenGL"))
  (allow mach-lookup
      (global-name "com.apple.cvmsServ")
      (global-name "com.apple.MTLCompilerService"))
  (allow iokit-open
      (iokit-connection "IOAccelerator")
      (iokit-user-client-class "IOAccelerationUserClient")
      (iokit-user-client-class "IOSurfaceRootUserClient")
      (iokit-user-client-class "IOSurfaceSendRight")
      (iokit-user-client-class "IOFramebufferSharedUserClient")
      (iokit-user-client-class "AGPMClient")
      (iokit-user-client-class "AppleGraphicsControlClient"))

  (allow mach-lookup
    ; bug 1565575
    (global-name "com.apple.audio.AudioComponentRegistrar"))
)SANDBOX_LITERAL";

}  // namespace mozilla

#endif  // mozilla_SandboxPolicyRDD_h
