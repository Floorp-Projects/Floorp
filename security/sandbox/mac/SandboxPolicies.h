/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_SandboxPolicies_h
#define mozilla_SandboxPolicies_h

namespace mozilla {

static const char pluginSandboxRules[] = R"SANDBOX_LITERAL(
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
)SANDBOX_LITERAL";

static const char widevinePluginSandboxRulesAddend[] = R"SANDBOX_LITERAL(
  (allow mach-lookup (global-name "com.apple.windowserver.active"))
)SANDBOX_LITERAL";

static const char contentSandboxRules[] = R"SANDBOX_LITERAL(
  (version 1)

  (define should-log (param "SHOULD_LOG"))
  (define sandbox-level-1 (param "SANDBOX_LEVEL_1"))
  (define sandbox-level-2 (param "SANDBOX_LEVEL_2"))
  (define sandbox-level-3 (param "SANDBOX_LEVEL_3"))
  (define macosMinorVersion (string->number (param "MAC_OS_MINOR")))
  (define appPath (param "APP_PATH"))
  (define appBinaryPath (param "APP_BINARY_PATH"))
  (define appdir-path (param "APP_DIR"))
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
      (subpath "/Library/GPUBundles")
      (subpath appdir-path))
    (allow file-read*
        (subpath "/System")
        (subpath "/usr/lib")
        (subpath "/Library/GPUBundles")
        (subpath appdir-path)))

  ; Allow read access to standard system paths.
  (allow file-read*
    (require-all (file-mode #o0004)
      (require-any
        (subpath "/Library/Filesystems/NetFSPlugins")
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

  (allow signal (target self))

  (if (>= macosMinorVersion 13)
    (allow mach-lookup
      ; bug 1392988
      (xpc-service-name "com.apple.coremedia.videodecoder")
      (xpc-service-name "com.apple.coremedia.videoencoder")))

; bug 1312273
  (if (= macosMinorVersion 9)
     (allow mach-lookup (global-name "com.apple.xpcd")))

  (allow iokit-open
     (iokit-user-client-class "IOHIDParamUserClient"))

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
      (subpath "/Library/Spelling")
      (literal "/")
      (literal "/private/tmp")
      (literal "/private/var/tmp")
      (home-literal "/.CFUserTextEncoding")
      (home-literal "/Library/Preferences/com.apple.DownloadAssessment.plist")
      (home-subpath "/Library/Colors")
      (home-subpath "/Library/Keyboard Layouts")
      (home-subpath "/Library/Input Methods")
      (home-subpath "/Library/Spelling")
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

  ; Fonts
  (allow file-read*
    (subpath "/Library/Fonts")
    (subpath "/Library/Application Support/Apple/Fonts")
    (home-subpath "/Library/Fonts")
    ; Allow read access to paths allowed via sandbox extensions.
    ; This is needed for fonts in non-standard locations normally
    ; due to third party font managers. The extensions are
    ; automatically issued by the font server in response to font
    ; API calls.
    (extension "com.apple.app-sandbox.read"))
  ; Fonts may continue to work without explicitly allowing these
  ; services because, at present, connections are made to the services
  ; before the sandbox is enabled as a side-effect of some API calls.
  (allow mach-lookup
    (global-name "com.apple.fonts")
    (global-name "com.apple.FontObjectsServer"))
  (if (<= macosMinorVersion 11)
    (allow mach-lookup (global-name "com.apple.FontServer")))

  ; Fonts
  ; Workaround for sandbox extensions not being automatically
  ; issued for fonts on 10.11 and earlier versions (bug 1460917).
  (if (<= macosMinorVersion 11)
    (allow file-read*
      (regex #"\.[oO][tT][fF]$"          ; otf
             #"\.[tT][tT][fF]$"          ; ttf
             #"\.[tT][tT][cC]$"          ; ttc
             #"\.[oO][tT][cC]$"          ; otc
             #"\.[dD][fF][oO][nN][tT]$") ; dfont
      (home-subpath "/Library/FontCollections")
      (home-subpath "/Library/Application Support/Adobe/CoreSync/plugins/livetype")
      (home-subpath "/Library/Application Support/FontAgent")
      (regex #"\.fontvault/")
      (home-subpath "/FontExplorer X/Font Library")))
)SANDBOX_LITERAL";

// These are additional rules that are added to the content process rules for
// file content processes.
static const char fileContentProcessAddend[] = R"SANDBOX_LITERAL(
  ; This process has blanket file read privileges
  (allow file-read*)

  ; File content processes need access to iconservices to draw file icons in
  ; directory listings
  (allow mach-lookup (global-name "com.apple.iconservices"))
)SANDBOX_LITERAL";

// These are additional rules that are added to the content process rules when
// audio remoting is not enabled. (Once audio remoting is always used these
// will be deleted.)
static const char contentProcessAudioAddend[] = R"SANDBOX_LITERAL(
  (allow ipc-posix-shm-read* ipc-posix-shm-write-data
    (ipc-posix-name-regex #"^AudioIO"))

  (allow mach-lookup
    (global-name "com.apple.audio.coreaudiod")
    (global-name "com.apple.audio.audiohald"))

  (if (>= macosMinorVersion 13)
    (allow mach-lookup
    ; bug 1376163
    (global-name "com.apple.audio.AudioComponentRegistrar")))

  (allow iokit-open (iokit-user-client-class "IOAudioEngineUserClient"))

  (allow file-read* (subpath "/Library/Audio/Plug-Ins"))

  (allow device-microphone)
)SANDBOX_LITERAL";

// The "Safe Mode" Flash NPAPI plugin process profile
static const char flashPluginSandboxRules[] = R"SANDBOX_LITERAL(
  (version 1)

  ; Parameters
  (define shouldLog (param "SHOULD_LOG"))
  (define sandbox-level-1 (param "SANDBOX_LEVEL_1"))
  (define sandbox-level-2 (param "SANDBOX_LEVEL_2"))
  (define macosMinorVersion (string->number (param "MAC_OS_MINOR")))
  (define homeDir (param "HOME_PATH"))
  (define tempDir (param "DARWIN_USER_TEMP_DIR"))
  (define pluginPath (param "PLUGIN_BINARY_PATH"))

  (if (string=? shouldLog "TRUE")
      (deny default)
      (deny default (with no-log)))
  (debug deny)
  (allow system-audit file-read-metadata)
  ; These are not included in (deny default)
  (deny process-info*)
  ; This isn't available in some older macOS releases.
  (if (defined? 'nvram*)
    (deny nvram*))

  ; Allow read access to standard system paths.
  (allow file-read*
    (require-all (file-mode #o0004)
      (require-any
        (subpath "/System")
        (subpath "/usr/lib")
        (subpath "/Library/Filesystems/NetFSPlugins")
        (subpath "/Library/GPUBundles")
        (subpath "/usr/share"))))
  (allow file-read-metadata
         (literal "/etc")
         (literal "/tmp")
         (literal "/var")
         (literal "/private/etc/localtime"))
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

  ; Graphics
  (allow user-preference-read
         (preference-domain "com.apple.opengl")
         (preference-domain "com.nvidia.OpenGL"))
  (allow mach-lookup
         (global-name "com.apple.cvmsServ"))
  (allow iokit-open
         (iokit-connection "IOAccelerator")
         (iokit-user-client-class "IOAccelerationUserClient")
         (iokit-user-client-class "IOSurfaceRootUserClient")
         (iokit-user-client-class "IOSurfaceSendRight"))
  (allow iokit-open
         (iokit-user-client-class "AppleIntelMEUserClient")
         (iokit-user-client-class "AppleSNBFBUserClient"))
  (allow iokit-open
         (iokit-user-client-class "AGPMClient")
         (iokit-user-client-class "AppleGraphicsControlClient")
         (iokit-user-client-class "AppleGraphicsPolicyClient"))

  ; Network
  (allow file-read*
         (literal "/Library/Preferences/com.apple.networkd.plist"))
  (allow mach-lookup
         (global-name "com.apple.SystemConfiguration.PPPController")
         (global-name "com.apple.SystemConfiguration.SCNetworkReachability")
         (global-name "com.apple.nehelper")
         (global-name "com.apple.networkd")
         (global-name "com.apple.nsurlstorage-cache")
         (global-name "com.apple.symptomsd")
         (global-name "com.apple.usymptomsd"))
  (allow network-outbound
         (control-name "com.apple.netsrc")
         (control-name "com.apple.network.statistics"))
  (allow system-socket
         (require-all (socket-domain AF_SYSTEM)
                      (socket-protocol 2)) ; SYSPROTO_CONTROL
         (socket-domain AF_ROUTE))
  (allow network-outbound
      (literal "/private/var/run/mDNSResponder")
      (literal "/private/var/run/asl_input")
      (literal "/private/var/run/syslog")
      (remote tcp)
      (remote udp))
  (allow network-inbound
      (local udp))

  (allow process-info-pidinfo)
  (allow process-info-setcontrol (target self))

  ; macOS 10.9 does not support the |sysctl-name| predicate
  (if (= macosMinorVersion 9)
      (allow sysctl-read)
      (allow sysctl-read
        (sysctl-name
          "hw.activecpu"
          "hw.availcpu"
          "hw.busfrequency_max"
          "hw.cpu64bit_capable"
          "hw.cputype"
          "hw.physicalcpu_max"
          "hw.logicalcpu_max"
          "hw.machine"
          "hw.model"
          "hw.ncpu"
          "hw.optional.avx1_0"
          "hw.optional.avx2_0"
          "hw.optional.sse2"
          "hw.optional.sse3"
          "hw.optional.sse4_1"
          "hw.optional.sse4_2"
          "hw.optional.x86_64"
          "kern.hostname"
          "kern.maxfilesperproc"
          "kern.memorystatus_level"
          "kern.osrelease"
          "kern.ostype"
          "kern.osvariant_status"
          "kern.osversion"
          "kern.safeboot"
          "kern.version"
          "vm.footprint_suspend")))

  ; Utilities for allowing access to home subdirectories
  (define home-library-path
    (string-append homeDir "/Library"))

  (define (home-subpath home-relative-subpath)
    (subpath (string-append homeDir home-relative-subpath)))

  (define home-library-prefs-path
    (string-append homeDir "/Library" "/Preferences"))

  (define (home-literal home-relative-literal)
    (literal (string-append homeDir home-relative-literal)))

  (define (home-library-regex home-library-relative-regex)
    (regex (string-append "^" (regex-quote home-library-path))
           home-library-relative-regex))

  (define (home-library-subpath home-library-relative-subpath)
      (subpath (string-append home-library-path home-library-relative-subpath)))

  (define (home-library-literal home-library-relative-literal)
      (literal (string-append home-library-path home-library-relative-literal)))

  (define (home-library-preferences-literal
           home-library-preferences-relative-literal)
      (literal (string-append home-library-prefs-path
                home-library-preferences-relative-literal)))

  ; Utility for allowing access to a temp dir subdirectory
  (define (tempDir-regex tempDir-relative-regex)
    (regex (string-append "^" (regex-quote tempDir)) tempDir-relative-regex))

  ; Read-only paths
  (allow file-read*
      (literal "/")
      (literal "/private/etc/services")
      (literal "/private/etc/resolv.conf")
      (literal "/private/var/run/resolv.conf")
      (subpath "/Library/Frameworks")
      (subpath "/Library/Managed Preferences")
      (home-literal "/.CFUserTextEncoding")
      (home-library-subpath "/Audio")
      (home-library-subpath "/ColorPickers")
      (home-library-subpath "/ColorSync")
      (subpath "/Library/Components")
      (home-library-subpath "/Components")
      (subpath "/Library/Contextual Menu Items")
      (subpath "/Library/Input Methods")
      (home-library-subpath "/Input Methods")
      (subpath "/Library/InputManagers")
      (home-library-subpath "/InputManagers")
      (home-library-subpath "/KeyBindings")
      (subpath "/Library/Keyboard Layouts")
      (home-library-subpath "/Keyboard Layouts")
      (subpath "/Library/Spelling")
      (home-library-subpath "/Spelling")
      (home-library-literal "/Caches/com.apple.coreaudio.components.plist")
      (subpath "/Library/Audio/Sounds")
      (subpath "/Library/Audio/Plug-Ins/Components")
      (home-library-subpath "/Audio/Plug-Ins/Components")
      (subpath "/Library/Audio/Plug-Ins/HAL")
      (subpath "/Library/CoreMediaIO/Plug-Ins/DAL")
      (subpath "/Library/QuickTime")
      (home-library-subpath "/QuickTime")
      (subpath "/Library/Video/Plug-Ins")
      (home-library-subpath "/Caches/QuickTime")
      (subpath "/Library/ColorSync")
      (home-literal "/Library/Preferences/com.apple.lookup.shared.plist"))

  (allow iokit-open
      (iokit-user-client-class "IOAudioControlUserClient")
      (iokit-user-client-class "IOAudioEngineUserClient")
      (iokit-user-client-class "IOHIDParamUserClient")
      (iokit-user-client-class "RootDomainUserClient"))

  ; Services
  (allow mach-lookup
      (global-name "com.apple.audio.AudioComponentRegistrar")
      (global-name "com.apple.DiskArbitration.diskarbitrationd")
      (global-name "com.apple.ImageCaptureExtension2.presence")
      (global-name "com.apple.PowerManagement.control")
      (global-name "com.apple.SecurityServer")
      (global-name "com.apple.SystemConfiguration.PPPController")
      (global-name "com.apple.SystemConfiguration.configd")
      (global-name "com.apple.UNCUserNotification")
      (global-name "com.apple.audio.audiohald")
      (global-name "com.apple.audio.coreaudiod")
      (global-name "com.apple.cfnetwork.AuthBrokerAgent")
      (global-name "com.apple.lsd.mapdb")
      (global-name "com.apple.pasteboard.1") ; Allows paste into input field
      (global-name "com.apple.dock.server")
      (global-name "com.apple.dock.fullscreen")
      (global-name "com.apple.coreservices.appleevents")
      (global-name "com.apple.coreservices.launchservicesd")
      (global-name "com.apple.window_proxies")
      (local-name "com.apple.tsm.portname")
      (global-name "com.apple.axserver")
      (global-name "com.apple.pbs.fetch_services")
      (global-name "com.apple.tccd.system")
      (global-name "com.apple.tsm.uiserver")
      (global-name "com.apple.inputmethodkit.launchagent")
      (global-name "com.apple.inputmethodkit.launcher")
      (global-name "com.apple.inputmethodkit.getxpcendpoint")
      (global-name "com.apple.decalog4.incoming")
      (global-name "com.apple.windowserver.active"))

  ; Fonts
  (allow file-read*
    (subpath "/Library/Fonts")
    (subpath "/Library/Application Support/Apple/Fonts")
    (home-library-subpath "/Fonts")
    ; Allow read access to paths allowed via sandbox extensions.
    ; This is needed for fonts in non-standard locations normally
    ; due to third party font managers. The extensions are
    ; automatically issued by the font server in response to font
    ; API calls.
    (extension "com.apple.app-sandbox.read"))
  ; Fonts may continue to work without explicitly allowing these
  ; services because, at present, connections are made to the services
  ; before the sandbox is enabled as a side-effect of some API calls.
  (allow mach-lookup
    (global-name "com.apple.fonts")
    (global-name "com.apple.FontObjectsServer"))
  (if (<= macosMinorVersion 11)
    (allow mach-lookup (global-name "com.apple.FontServer")))

  ; Fonts
  ; Workaround for sandbox extensions not being automatically
  ; issued for fonts on 10.11 and earlier versions (bug 1460917).
  (if (<= macosMinorVersion 11)
    (allow file-read*
      (regex #"\.[oO][tT][fF]$"          ; otf
             #"\.[tT][tT][fF]$"          ; ttf
             #"\.[tT][tT][cC]$"          ; ttc
             #"\.[oO][tT][cC]$"          ; otc
             #"\.[dD][fF][oO][nN][tT]$") ; dfont
      (home-subpath "/Library/FontCollections")
      (home-subpath "/Library/Application Support/Adobe/CoreSync/plugins/livetype")
      (home-subpath "/Library/Application Support/FontAgent")
      (regex #"\.fontvault/")
      (home-subpath "/FontExplorer X/Font Library")))

  (if (string=? sandbox-level-1 "TRUE") (begin
    ; Open file dialogs
    (allow mach-lookup
	; needed for the dialog sidebar
	(global-name "com.apple.coreservices.sharedfilelistd.xpc")
	; bird(8) -- "Documents in the Cloud"
	; needed to avoid iCloud error dialogs and to display iCloud files
	(global-name "com.apple.bird")
	(global-name "com.apple.bird.token")
	; needed for icons in the file dialog
	(global-name "com.apple.iconservices"))
    ; Needed for read access to files selected by the user with the
    ; file dialog. The extensions are granted when the dialog is
    ; displayed. Unfortunately (testing revealed) that displaying
    ; the file dialog grants access to all files within the directory
    ; displayed by the file dialog--a small improvement compared
    ; to global read access.
    (allow file-read*
	(extension "com.apple.app-sandbox.read-write"))))

  (allow ipc-posix-shm*
      (ipc-posix-name-regex #"^AudioIO")
      (ipc-posix-name-regex #"^CFPBS:"))

  (allow ipc-posix-shm-read*
      (ipc-posix-name-regex #"^/tmp/com\.apple\.csseed\.")
      (ipc-posix-name "FNetwork.defaultStorageSession")
      (ipc-posix-name "apple.shm.notification_center"))

  ; Printing
  (allow network-outbound (literal "/private/var/run/cupsd"))
  (allow mach-lookup
      (global-name "com.apple.printuitool.agent")
      (global-name "com.apple.printtool.agent")
      (global-name "com.apple.printtool.daemon"))
  (allow file-read*
      (subpath "/Library/Printers")
      (home-literal "/.cups/lpoptions")
      (home-literal "/.cups/client.conf")
      (literal "/private/etc/cups/client.conf")
      (literal "/private/etc/cups/lpoptions")
      (subpath "/private/etc/cups/ppd")
      (literal "/private/var/run/cupsd"))
  (allow user-preference-read
      (preference-domain "org.cups.PrintingPrefs"))
  ; Temporary files read/written here during printing
  (allow file-read* file-write-create file-write-data
      (tempDir-regex "/FlashTmp"))

  ; Camera/Mic
  (allow device-camera)
  (allow device-microphone)

  ; Path to the plugin binary, user cache dir, and user temp dir
  (allow file-read* (subpath pluginPath))

  ; Per Adobe, needed for Flash LocalConnection functionality
  (allow ipc-posix-sem
      (ipc-posix-name "MacromediaSemaphoreDig"))

  ; Flash debugger and enterprise deployment config files
  (allow file-read*
      (home-literal "/mm.cfg")
      (home-literal "/mms.cfg"))

  (allow file-read* file-write-create file-write-mode file-write-owner
      (home-library-literal "/Caches/Adobe")
      (home-library-preferences-literal "/Macromedia"))

  (allow file-read* file-write-create file-write-data
      (literal "/Library/Application Support/Macromedia/mms.cfg")
      (home-library-literal "/Application Support/Macromedia/mms.cfg")
      (home-library-subpath "/Caches/Adobe/Flash Player")
      (home-library-subpath "/Preferences/Macromedia/Flash Player"))

  (allow file-read*
      (literal "/Library/PreferencePanes/Flash Player.prefPane")
      (home-library-literal "/PreferencePanes/Flash Player.prefPane")
      (home-library-regex "/Application Support/Macromedia/ss\.(cfg|cfn|sgn)$"))

  (allow network-bind (local ip))

  (deny file-write-create (vnode-type SYMLINK))
)SANDBOX_LITERAL";

}

#endif // mozilla_SandboxPolicies_h
