/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_SandboxPolicies_h
#define mozilla_SandboxPolicies_h

namespace mozilla {

static const char pluginSandboxRules[] = R"(
  (version 1)

  (define should-log (param "SHOULD_LOG"))
  (define plugin-binary-path (param "PLUGIN_BINARY_PATH"))
  (define app-path (param "APP_PATH"))
  (define app-binary-path (param "APP_BINARY_PATH"))

  (if (string=? should-log "TRUE")
      (deny default)
      (deny default (with no-log)))

  (allow signal (target self))
  (allow sysctl-read)
  (allow iokit-open (iokit-user-client-class "IOHIDParamUserClient"))
  (allow file-read*
      (literal "/etc")
      (literal "/dev/random")
      (literal "/dev/urandom")
      (literal "/usr/share/icu/icudt51l.dat")
      (subpath "/System/Library/Displays/Overrides")
      (subpath "/System/Library/CoreServices/CoreTypes.bundle")
      (subpath "/System/Library/PrivateFrameworks")
      (regex #"^/usr/lib/libstdc\+\+\.[^/]*dylib$")
      (literal plugin-binary-path)
      (literal app-path)
      (literal app-binary-path))
)";

static const char widevinePluginSandboxRulesAddend[] = R"(
  (allow mach-lookup (global-name "com.apple.windowserver.active"))
)";

static const char contentSandboxRules[] = R"(
  (version 1)

  (define should-log (param "SHOULD_LOG"))
  (define sandbox-level-1 (param "SANDBOX_LEVEL_1"))
  (define sandbox-level-2 (param "SANDBOX_LEVEL_2"))
  (define sandbox-level-3 (param "SANDBOX_LEVEL_3"))
  (define macosMinorVersion (string->number (param "MAC_OS_MINOR")))
  (define appPath (param "APP_PATH"))
  (define appBinaryPath (param "APP_BINARY_PATH"))
  (define appdir-path (param "APP_DIR"))
  (define appTempDir (param "APP_TEMP_DIR"))
  (define hasProfileDir (param "HAS_SANDBOXED_PROFILE"))
  (define profileDir (param "PROFILE_DIR"))
  (define home-path (param "HOME_PATH"))
  (define debugWriteDir (param "DEBUG_WRITE_DIR"))
  (define testingReadPath1 (param "TESTING_READ_PATH1"))
  (define testingReadPath2 (param "TESTING_READ_PATH2"))
  (define testingReadPath3 (param "TESTING_READ_PATH3"))
  (define testingReadPath4 (param "TESTING_READ_PATH4"))

  (if (string=? should-log "TRUE")
    (deny default)
    (deny default (with no-log)))
  (debug deny)
  ; These are not included in (deny default)
  (deny process-info*)
  ; This isn't available in some older macOS releases.
  (if (defined? 'nvram*)
    (deny nvram*))
  ; The next two properties both require macOS 10.10+
  (if (defined? 'iokit-get-properties)
    (deny iokit-get-properties))
  (if (defined? 'file-map-executable)
    (deny file-map-executable))

  (if (defined? 'file-map-executable)
    (allow file-map-executable file-read*
      (subpath "/System")
      (subpath "/usr/lib")
      (subpath appdir-path))
    (allow file-read*
        (subpath "/System")
        (subpath "/usr/lib")
        (subpath appdir-path)))

  ; Allow read access to standard system paths.
  (allow file-read*
    (require-all (file-mode #o0004)
      (require-any
        (subpath "/Library/Filesystems/NetFSPlugins")
        (subpath "/Library/GPUBundles")
        (subpath "/usr/share"))))

  ; Top-level directory metadata access (bug 1404298)
  (allow file-read-metadata (regex #"^/[^/]+$"))

  (allow file-read-metadata
    (literal "/private/etc/localtime")
    (regex #"^/private/tmp/KSInstallAction\."))

  ; Allow read access to standard special files.
  (allow file-read*
    (literal "/dev/autofs_nowait")
    (literal "/dev/random")
    (literal "/dev/urandom"))

  (allow file-read*
    file-write-data
    (literal "/dev/null")
    (literal "/dev/zero"))

  (allow file-read*
    file-write-data
    file-ioctl
    (literal "/dev/dtracehelper"))

  ; Needed for things like getpriority()/setpriority()
  (allow process-info-pidinfo process-info-setcontrol (target self))

  ; macOS 10.9 does not support the |sysctl-name| predicate, so unfortunately
  ; we need to allow all sysctl-reads there.
  (if (= macosMinorVersion 9)
    (allow sysctl-read)
    (allow sysctl-read
      (sysctl-name-regex #"^sysctl\.")
      (sysctl-name "kern.ostype")
      (sysctl-name "kern.osversion")
      (sysctl-name "kern.osrelease")
      (sysctl-name "kern.version")
      ; TODO: remove "kern.hostname". Without it the tests hang, but the hostname
      ; is arguably sensitive information, so we should see what can be done about
      ; removing it.
      (sysctl-name "kern.hostname")
      (sysctl-name "hw.machine")
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
      (sysctl-name "machdep.cpu.vendor")
      (sysctl-name "machdep.cpu.family")
      (sysctl-name "machdep.cpu.model")
      (sysctl-name "machdep.cpu.stepping")
      (sysctl-name "debug.intel.gstLevelGST")
      (sysctl-name "debug.intel.gstLoaderControl")))

  (define (home-regex home-relative-regex)
    (regex (string-append "^" (regex-quote home-path) home-relative-regex)))
  (define (home-subpath home-relative-subpath)
    (subpath (string-append home-path home-relative-subpath)))
  (define (home-literal home-relative-literal)
    (literal (string-append home-path home-relative-literal)))

  (define (profile-subpath profile-relative-subpath)
    (subpath (string-append profileDir profile-relative-subpath)))

  (define (allow-shared-list domain)
    (allow file-read*
           (home-regex (string-append "/Library/Preferences/" (regex-quote domain)))))

  (allow ipc-posix-shm-read-data ipc-posix-shm-write-data
    (ipc-posix-name-regex #"^CFPBS:"))
  (allow ipc-posix-shm-read* ipc-posix-shm-write-data
    (ipc-posix-name-regex #"^AudioIO"))

  (allow signal (target self))

  (allow mach-lookup
      (global-name "com.apple.audio.coreaudiod")
      (global-name "com.apple.audio.audiohald"))

  (if (>= macosMinorVersion 13)
    (allow mach-lookup
      ; bug 1376163
      (global-name "com.apple.audio.AudioComponentRegistrar")
      ; bug 1392988
      (xpc-service-name "com.apple.coremedia.videodecoder")
      (xpc-service-name "com.apple.coremedia.videoencoder")))

; bug 1312273
  (if (= macosMinorVersion 9)
     (allow mach-lookup (global-name "com.apple.xpcd")))

  (allow iokit-open
     (iokit-user-client-class "IOHIDParamUserClient")
     (iokit-user-client-class "IOAudioEngineUserClient"))

  ; Only supported on macOS 10.10+
  (if (defined? 'iokit-get-properties)
    (allow iokit-get-properties
      (iokit-property "board-id")
      (iokit-property "IODVDBundleName")
      (iokit-property "IOGLBundleName")
      (iokit-property "IOGVACodec")
      (iokit-property "IOGVAHEVCDecode")
      (iokit-property "IOGVAHEVCEncode")
      (iokit-property "IOPCITunnelled")
      (iokit-property "IOVARendererID")
      (iokit-property "MetalPluginName")
      (iokit-property "MetalPluginClassName")))

; depending on systems, the 1st, 2nd or both rules are necessary
  (allow user-preference-read (preference-domain "com.apple.HIToolbox"))
  (allow file-read-data (literal "/Library/Preferences/com.apple.HIToolbox.plist"))

  (allow user-preference-read (preference-domain "com.apple.ATS"))
  (allow file-read-data (literal "/Library/Preferences/.GlobalPreferences.plist"))

  (allow file-read*
      (subpath "/Library/Fonts")
      (subpath "/Library/Audio/Plug-Ins")
      (subpath "/Library/Spelling")
      (literal "/")
      (literal "/private/tmp")
      (literal "/private/var/tmp")

      (home-literal "/.CFUserTextEncoding")
      (home-literal "/Library/Preferences/com.apple.DownloadAssessment.plist")
      (home-subpath "/Library/Colors")
      (home-subpath "/Library/Fonts")
      (home-subpath "/Library/FontCollections")
      (home-subpath "/Library/Keyboard Layouts")
      (home-subpath "/Library/Input Methods")
      (home-subpath "/Library/Spelling")
      (home-subpath "/Library/Application Support/Adobe/CoreSync/plugins/livetype")
      (home-subpath "/Library/Application Support/FontAgent")

      (literal appPath)
      (literal appBinaryPath))

  (if (defined? 'file-map-executable)
    (begin
      (when testingReadPath1
        (allow file-read* file-map-executable (subpath testingReadPath1)))
      (when testingReadPath2
        (allow file-read* file-map-executable (subpath testingReadPath2)))
      (when testingReadPath3
        (allow file-read* file-map-executable (subpath testingReadPath3)))
      (when testingReadPath4
        (allow file-read* file-map-executable (subpath testingReadPath4))))
    (begin
      (when testingReadPath1
        (allow file-read* (subpath testingReadPath1)))
      (when testingReadPath2
        (allow file-read* (subpath testingReadPath2)))
      (when testingReadPath3
        (allow file-read* (subpath testingReadPath3)))
      (when testingReadPath4
        (allow file-read* (subpath testingReadPath4)))))

  (allow file-read-metadata (home-subpath "/Library"))

  (allow file-read-metadata
    (literal "/private/var")
    (subpath "/private/var/folders"))

  ; bug 1303987
  (if (string? debugWriteDir)
    (begin
      (allow file-write-data (subpath debugWriteDir))
      (allow file-write-create
        (require-all
          (subpath debugWriteDir)
          (vnode-type REGULAR-FILE)))))

  (allow-shared-list "org.mozilla.plugincontainer")

; the following rule should be removed when microphone access
; is brokered through the content process
  (allow device-microphone)

; Per-user and system-wide Extensions dir
  (allow file-read*
      (home-regex "/Library/Application Support/[^/]+/Extensions/")
      (regex "^/Library/Application Support/[^/]+/Extensions/"))

; bug 1393805
  (allow file-read*
      (home-subpath "/Library/Application Support/Mozilla/SystemExtensionsDev"))

; The following rules impose file access restrictions which get
; more restrictive in higher levels. When file-origin-specific
; content processes are used for file:// origin browsing, the
; global file-read* permission should be removed from each level.

; level 1: global read access permitted, no global write access
  (if (string=? sandbox-level-1 "TRUE") (allow file-read*))

; level 2: global read access permitted, no global write access,
;          no read/write access to ~/Library,
;          no read/write access to $PROFILE,
;          read access permitted to $PROFILE/{extensions,chrome}
  (if (string=? sandbox-level-2 "TRUE")
    (begin
      ; bug 1201935
      (allow file-read* (home-subpath "/Library/Caches/TemporaryItems"))
      (if (string=? hasProfileDir "TRUE")
        ; we have a profile dir
        (allow file-read* (require-all
          (require-not (home-subpath "/Library"))
          (require-not (subpath profileDir))))
        ; we don't have a profile dir
        (allow file-read* (require-not (home-subpath "/Library"))))))

  ; level 3: Does not have any of it's own rules. The global rules provide:
  ;          no global read/write access,
  ;          read access permitted to $PROFILE/{extensions,chrome}

  (if (string=? hasProfileDir "TRUE")
    ; we have a profile dir
    (allow file-read*
      (profile-subpath "/extensions")
      (profile-subpath "/chrome")))

; accelerated graphics
  (allow user-preference-read (preference-domain "com.apple.opengl"))
  (allow user-preference-read (preference-domain "com.nvidia.OpenGL"))
  (allow mach-lookup
      (global-name "com.apple.cvmsServ"))
  (allow iokit-open
      (iokit-connection "IOAccelerator")
      (iokit-user-client-class "IOAccelerationUserClient")
      (iokit-user-client-class "IOSurfaceRootUserClient")
      (iokit-user-client-class "IOSurfaceSendRight")
      (iokit-user-client-class "IOFramebufferSharedUserClient")
      (iokit-user-client-class "AGPMClient")
      (iokit-user-client-class "AppleGraphicsControlClient"))

; bug 1153809
  (allow iokit-open
      (iokit-user-client-class "NVDVDContextTesla")
      (iokit-user-client-class "Gen6DVDContext"))

  ; bug 1237847
  (allow file-read* file-write-data
    (subpath appTempDir))
  (allow file-write-create
    (require-all
      (subpath appTempDir)
      (vnode-type REGULAR-FILE)))

  ; bug 1382260
  ; We may need to load fonts from outside of the standard
  ; font directories whitelisted above. This is typically caused
  ; by a font manager. For now, whitelist any file with a
  ; font extension. Limit this to the common font types:
  ; files ending in .otf, .ttf, .ttc, .otc, and .dfont.
  (allow file-read*
    (regex #"\.[oO][tT][fF]$"           ; otf
           #"\.[tT][tT][fF]$"           ; ttf
           #"\.[tT][tT][cC]$"           ; ttc
           #"\.[oO][tT][cC]$"           ; otc
           #"\.[dD][fF][oO][nN][tT]$")) ; dfont

  ; bug 1404919
  ; Read access (recursively) within directories ending in .fontvault
  (allow file-read* (regex #"\.fontvault/"))

  ; bug 1429133
  ; Read access to the default FontExplorer font directory
  (allow file-read* (home-subpath "/FontExplorer X/Font Library"))
)";

static const char fileContentProcessAddend[] = R"(
  ; This process has blanket file read privileges
  (allow file-read*)

  ; File content processes need access to iconservices to draw file icons in
  ; directory listings
  (allow mach-lookup (global-name "com.apple.iconservices"))
)";


}

#endif // mozilla_SandboxPolicies_h
