# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# IMPORTANT:
# Please don't directly include this file if you are building via gyp_chromium,
# since gyp_chromium is automatically forcing its inclusion.
{
  # Variables expected to be overriden on the GYP command line (-D) or by
  # ~/.gyp/include.gypi.
  'variables': {
    # Putting a variables dict inside another variables dict looks kind of
    # weird.  This is done so that 'host_arch', 'chromeos', etc are defined as
    # variables within the outer variables dict here.  This is necessary
    # to get these variables defined for the conditions within this variables
    # dict that operate on these variables (e.g., for setting 'toolkit_views',
    # we need to have 'chromeos' already set).
    'variables': {
      'variables': {
        'includes': [
          'use_skia_on_mac.gypi',
        ],
        'variables': {
          # Whether we're building a ChromeOS build.
          'chromeos%': 0,

          # Whether we are using Views Toolkit
          'toolkit_views%': 0,

          # Whether or not we are using the Aura windowing framework.
          'use_aura%': 0,

          # Whether or not we are building the Ash shell.
          'use_ash%': 0,

          # Use OpenSSL instead of NSS. Under development: see http://crbug.com/62803
          'use_openssl%': 0,

          # Disable Virtual keyboard support by default.
          'use_virtual_keyboard%': 0,

          # Default setting for use_skia on mac platform.
          # This is typically overridden in use_skia_on_mac.gypi.
          'use_skia_on_mac%': 0,
        },
        # Copy conditionally-set variables out one scope.
        'chromeos%': '<(chromeos)',
        'use_aura%': '<(use_aura)',
        'use_ash%': '<(use_ash)',
        'use_openssl%': '<(use_openssl)',
        'use_virtual_keyboard%': '<(use_virtual_keyboard)',
        'use_skia_on_mac%': '<(use_skia_on_mac)',

        # Compute the architecture that we're building on.
        'conditions': [
          [ 'OS=="win"', {
            'host_arch%': 'ia32',
          }, {
            # This handles the Unix platforms for which there is some support.
            # Anything else gets passed through, which probably won't work very
            # well; such hosts should pass an explicit target_arch to gyp.
            'host_arch%':
              '<!(uname -m | sed -e "s/i.86/ia32/;s/x86_64/x64/;s/amd64/x64/;s/arm.*/arm/;s/i86pc/ia32/")',
          }],

          # Ash and ChromeOS require Aura.
          ['use_ash==1 or chromeos==1', {
            'use_aura%': 1,
          }],

          # Set value of toolkit_views based on OS.
          ['OS=="win" or chromeos==1 or use_aura==1', {
            'toolkit_views%': 1,
          }, {
            'toolkit_views%': 0,
          }],
        ],
      },

      # Copy conditionally-set variables out one scope.
      'chromeos%': '<(chromeos)',
      'host_arch%': '<(host_arch)',
      'toolkit_views%': '<(toolkit_views)',
      'use_aura%': '<(use_aura)',
      'use_ash%': '<(use_ash)',
      'use_openssl%': '<(use_openssl)',
      'use_virtual_keyboard%': '<(use_virtual_keyboard)',
      'use_skia_on_mac%': '<(use_skia_on_mac)',

      # We used to provide a variable for changing how libraries were built.
      # This variable remains until we can clean up all the users.
      # This needs to be one nested variables dict in so that dependent
      # gyp files can make use of it in their outer variables.  (Yikes!)
      # http://code.google.com/p/chromium/issues/detail?id=83308
      'library%': 'static_library',

      # Override branding to select the desired branding flavor.
      'branding%': 'Chromium',

      # Override buildtype to select the desired build flavor.
      # Dev - everyday build for development/testing
      # Official - release build (generally implies additional processing)
      # TODO(mmoss) Once 'buildtype' is fully supported (e.g. Windows gyp
      # conversion is done), some of the things which are now controlled by
      # 'branding', such as symbol generation, will need to be refactored based
      # on 'buildtype' (i.e. we don't care about saving symbols for non-Official
      # builds).
      'buildtype%': 'Dev',

      # Default architecture we're building for is the architecture we're
      # building on.
      'target_arch%': '<(host_arch)',

      # This variable tells WebCore.gyp and JavaScriptCore.gyp whether they are
      # are built under a chromium full build (1) or a webkit.org chromium
      # build (0).
      'inside_chromium_build%': 1,

      # Set to 1 to enable fast builds. It disables debug info for fastest
      # compilation.
      'fastbuild%': 0,

      # Set to 1 to enable dcheck in release without having to use the flag.
      'dcheck_always_on%': 0,

      # Disable file manager component extension by default.
      'file_manager_extension%': 0,

      # Disable WebUI TaskManager by default.
      'webui_task_manager%': 0,

      # Python version.
      'python_ver%': '2.6',

      # Set ARM-v7 compilation flags
      'armv7%': 0,

      # Set Neon compilation flags (only meaningful if armv7==1).
      'arm_neon%': 1,

      # The system root for cross-compiles. Default: none.
      'sysroot%': '',

      # The system libdir used for this ABI.
      'system_libdir%': 'lib',

      # On Linux, we build with sse2 for Chromium builds.
      'disable_sse2%': 0,

      # Use libjpeg-turbo as the JPEG codec used by Chromium.
      'use_libjpeg_turbo%': 1,

      # Variable 'component' is for cases where we would like to build some
      # components as dynamic shared libraries but still need variable
      # 'library' for static libraries.
      # By default, component is set to whatever library is set to and
      # it can be overriden by the GYP command line or by ~/.gyp/include.gypi.
      'component%': 'static_library',

      # Set to select the Title Case versions of strings in GRD files.
      'use_titlecase_in_grd_files%': 0,

      # Use translations provided by volunteers at launchpad.net.  This
      # currently only works on Linux.
      'use_third_party_translations%': 0,

      # Remoting compilation is enabled by default. Set to 0 to disable.
      'remoting%': 1,

      # P2P APIs are compiled in by default. Set to 0 to disable.
      # Also note that this should be enabled for remoting to compile.
      'p2p_apis%': 1,

      # Configuration policy is enabled by default. Set to 0 to disable.
      'configuration_policy%': 1,

      # Safe browsing is compiled in by default. Set to 0 to disable.
      'safe_browsing%': 1,

      # Speech input is compiled in by default. Set to 0 to disable.
      'input_speech%': 1,

      # Notifications are compiled in by default. Set to 0 to disable.
      'notifications%' : 1,

      # If this is set, the clang plugins used on the buildbot will be used.
      # Run tools/clang/scripts/update.sh to make sure they are compiled.
      # This causes 'clang_chrome_plugins_flags' to be set.
      # Has no effect if 'clang' is not set as well.
      'clang_use_chrome_plugins%': 0,

      # Enable building with ASAN (Clang's -faddress-sanitizer option).
      # -faddress-sanitizer only works with clang, but asan=1 implies clang=1
      # See https://sites.google.com/a/chromium.org/dev/developers/testing/addresssanitizer
      'asan%': 0,

      # Use the provided profiled order file to link Chrome image with it.
      # This makes Chrome faster by better using CPU cache when executing code.
      # This is known as PGO (profile guided optimization).
      # See https://sites.google.com/a/google.com/chrome-msk/dev/boot-speed-up-effort
      'order_text_section%' : "",

      # Set to 1 compile with -fPIC cflag on linux. This is a must for shared
      # libraries on linux x86-64 and arm, plus ASLR.
      'linux_fpic%': 1,

      # Enable navigator.registerProtocolHandler and supporting UI.
      'enable_register_protocol_handler%': 1,

      # Enable Web Intents support in WebKit, dispatching of intents,
      # and extensions Web Intents support.
      'enable_web_intents%': 1,

      # Enable Web Intents web content registration via HTML element
      # and WebUI managing such registrations.
      'enable_web_intents_tag%': 0,

      # Webrtc compilation is enabled by default. Set to 0 to disable.
      'enable_webrtc%': 1,

      # PPAPI by default does not support plugins making calls off the main
      # thread. Set to 1 to turn on experimental support for out-of-process
      # plugins to make call of the main thread.
      'enable_pepper_threading%': 0,

      # XInput2 multitouch support is disabled by default (use_xi2_mt=0).
      # Setting to non-zero value enables XI2 MT. When XI2 MT is enabled,
      # the input value also defines the required XI2 minor minimum version.
      # For example, use_xi2_mt=2 means XI2.2 or above version is required.
      'use_xi2_mt%': 0,

      # Use of precompiled headers on Windows.
      #
      # This is on by default in VS 2010, but off by default for VS
      # 2008 because of complications that it can cause with our
      # trybots etc.
      #
      # This variable may be explicitly set to 1 (enabled) or 0
      # (disabled) in ~/.gyp/include.gypi or via the GYP command line.
      # This setting will override the default.
      #
      # Note that a setting of 1 is probably suitable for most or all
      # Windows developers using VS 2008, since precompiled headers
      # provide a build speedup of 20-25%.  There are a couple of
      # small workarounds you may need to use when using VS 2008 (but
      # not 2010), see
      # http://code.google.com/p/chromium/wiki/WindowsPrecompiledHeaders
      # for details.
      'chromium_win_pch%': 0,

      # Enable plug-in installation by default.
      'enable_plugin_installation%': 1,

      # Specifies whether to use canvas_skia_skia.cc in place of platform
      # specific implementations of CanvasSkia. Affects text drawing in the
      # Chrome UI.
      # TODO(asvitkine): Enable this on all platforms and delete this flag.
      #                  http://crbug.com/105550
      'use_canvas_skia_skia%': 0,

      'conditions': [
        # TODO(epoger): Figure out how to set use_skia=1 for Mac outside of
        # the 'conditions' clause.  Initial attempts resulted in chromium and
        # webkit disagreeing on its setting.
        ['OS=="mac"', {
          'use_skia%': '<(use_skia_on_mac)',
          # Mac uses clang by default, so turn on the plugin as well.
          'clang_use_chrome_plugins%': 1,
        }, {
          'use_skia%': 1,
        }],

        # A flag for POSIX platforms
        ['OS=="win"', {
          'os_posix%': 0,
        }, {
          'os_posix%': 1,
        }],

        # A flag for BSD platforms
        ['OS=="freebsd" or OS=="openbsd"', {
          'os_bsd%': 1,
        }, {
          'os_bsd%': 0,
        }],

        # NSS usage.
        ['(OS=="linux" or OS=="freebsd" or OS=="openbsd" or OS=="solaris") and use_openssl==0', {
          'use_nss%': 1,
        }, {
          'use_nss%': 0,
        }],

        # Flags to use X11 on non-Mac POSIX platforms
        ['OS=="win" or OS=="mac" or OS=="android"', {
          'use_glib%': 0,
          'use_x11%': 0,
        }, {
          # TODO(dnicoara/msb) Wayland build should have these disabled.
          'use_glib%': 1,
          'use_x11%': 1,
        }],

        # Aura and most platforms never use GTK. 
        ['use_aura==1 or OS=="win" or OS=="mac" or OS=="android"', {
          'toolkit_uses_gtk%': 0,
        }, {
          'toolkit_uses_gtk%': 1,
        }],

        # We always use skia text rendering in Aura on Windows, since GDI
        # doesn't agree with our BackingStore.
        # TODO(beng): remove once skia text rendering is on by default.
        ['use_aura==1 and OS=="win"', {
          'enable_skia_text%': 1,
        }],

        # A flag to enable or disable our compile-time dependency
        # on gnome-keyring. If that dependency is disabled, no gnome-keyring
        # support will be available. This option is useful
        # for Linux distributions and for Aura.
        ['use_aura==1', {
          'use_gnome_keyring%': 0,
        }, {
          'use_gnome_keyring%': 1,
        }],

        ['toolkit_views==0 or OS=="mac"', {
          # GTK+ and Mac wants Title Case strings
          'use_titlecase_in_grd_files%': 1,
        }],

        # Enable some hacks to support Flapper only on Chrome OS.
        # Enable file manager extension on ChromeOS.
        ['chromeos==1', {
          'enable_flapper_hacks%': 1,
          'file_manager_extension%': 1,
        }, {
          'enable_flapper_hacks%': 0,
          'file_manager_extension%': 0,
        }],

        # Enable WebUI TaskManager on Aura.
        ['use_aura==1', {
          'webui_task_manager%': 1,
        }],

        ['OS=="android"', {
          'proprietary_codecs%': 1,
          'enable_webrtc%': 0,
        }],

        # Use GPU accelerated cross process image transport by default
        # on linux builds with the Aura window manager
        ['use_aura==1 and OS=="linux"', {
          'ui_compositor_image_transport%': 1,
        }, {
          'ui_compositor_image_transport%': 0,
        }],

        # Turn precompiled headers on by default for VS 2010.
        ['OS=="win" and MSVS_VERSION=="2010"', {
          'chromium_win_pch%': 1
        }],

        # No plugin installation allowed under Aura.
        ['use_aura==1', {
          'enable_plugin_installation%': 0,
        }, {
          'enable_plugin_installation%': 1,
        }],

        # Set to 0 to not use third_party/gold as the linker.
        # On by default for x64 Linux.  Off for ChromeOS as cross-compiling
        # makes things complicated.
        ['chromeos==0 and host_arch=="x64"', {
          'linux_use_gold_binary%': 1,
        }, {
          'linux_use_gold_binary%': 0,
        }],
      ],
    },

    # Copy conditionally-set variables out one scope.
    'branding%': '<(branding)',
    'buildtype%': '<(buildtype)',
    'target_arch%': '<(target_arch)',
    'host_arch%': '<(host_arch)',
    'library%': 'static_library',
    'toolkit_views%': '<(toolkit_views)',
    'ui_compositor_image_transport%': '<(ui_compositor_image_transport)',
    'use_aura%': '<(use_aura)',
    'use_ash%': '<(use_ash)',
    'use_openssl%': '<(use_openssl)',
    'use_nss%': '<(use_nss)',
    'os_bsd%': '<(os_bsd)',
    'os_posix%': '<(os_posix)',
    'use_glib%': '<(use_glib)',
    'toolkit_uses_gtk%': '<(toolkit_uses_gtk)',
    'use_skia%': '<(use_skia)',
    'use_x11%': '<(use_x11)',
    'use_gnome_keyring%': '<(use_gnome_keyring)',
    'linux_fpic%': '<(linux_fpic)',
    'enable_flapper_hacks%': '<(enable_flapper_hacks)',
    'enable_pepper_threading%': '<(enable_pepper_threading)',
    'chromeos%': '<(chromeos)',
    'use_virtual_keyboard%': '<(use_virtual_keyboard)',
    'use_skia_on_mac%': '<(use_skia_on_mac)',
    'use_xi2_mt%':'<(use_xi2_mt)',
    'file_manager_extension%': '<(file_manager_extension)',
    'webui_task_manager%': '<(webui_task_manager)',
    'inside_chromium_build%': '<(inside_chromium_build)',
    'fastbuild%': '<(fastbuild)',
    'dcheck_always_on%': '<(dcheck_always_on)',
    'python_ver%': '<(python_ver)',
    'armv7%': '<(armv7)',
    'arm_neon%': '<(arm_neon)',
    'sysroot%': '<(sysroot)',
    'system_libdir%': '<(system_libdir)',
    'disable_sse2%': '<(disable_sse2)',
    'component%': '<(component)',
    'use_titlecase_in_grd_files%': '<(use_titlecase_in_grd_files)',
    'use_third_party_translations%': '<(use_third_party_translations)',
    'remoting%': '<(remoting)',
    'enable_webrtc%': '<(enable_webrtc)',
    'chromium_win_pch%': '<(chromium_win_pch)',
    'p2p_apis%': '<(p2p_apis)',
    'configuration_policy%': '<(configuration_policy)',
    'safe_browsing%': '<(safe_browsing)',
    'input_speech%': '<(input_speech)',
    'notifications%': '<(notifications)',
    'clang_use_chrome_plugins%': '<(clang_use_chrome_plugins)',
    'asan%': '<(asan)',
    'order_text_section%': '<(order_text_section)',
    'enable_register_protocol_handler%': '<(enable_register_protocol_handler)',
    'enable_web_intents%': '<(enable_web_intents)',
    'enable_web_intents_tag%': '<(enable_web_intents_tag)',
    'enable_plugin_installation%': '<(enable_plugin_installation)',
    'linux_use_gold_binary%': '<(linux_use_gold_binary)',
    'use_canvas_skia_skia%': '<(use_canvas_skia_skia)',
    # Whether to build for Wayland display server
    'use_wayland%': 0,

    # Use system yasm instead of bundled one.
    'use_system_yasm%': 0,

    # Default to enabled PIE; this is important for ASLR but we need to be
    # able to turn it off for remote debugging on Chromium OS
    'linux_disable_pie%': 0,

    # The release channel that this build targets. This is used to restrict
    # channel-specific build options, like which installer packages to create.
    # The default is 'all', which does no channel-specific filtering.
    'channel%': 'all',

    # Override chromium_mac_pch and set it to 0 to suppress the use of
    # precompiled headers on the Mac.  Prefix header injection may still be
    # used, but prefix headers will not be precompiled.  This is useful when
    # using distcc to distribute a build to compile slaves that don't
    # share the same compiler executable as the system driving the compilation,
    # because precompiled headers rely on pointers into a specific compiler
    # executable's image.  Setting this to 0 is needed to use an experimental
    # Linux-Mac cross compiler distcc farm.
    'chromium_mac_pch%': 1,

    # Mac OS X SDK and deployment target support.
    # The SDK identifies the version of the system headers that will be used,
    # and corresponds to the MAC_OS_X_VERSION_MAX_ALLOWED compile-time macro.
    # "Maximum allowed" refers to the operating system version whose APIs are
    # available in the headers.
    # The deployment target identifies the minimum system version that the
    # built products are expected to function on.  It corresponds to the
    # MAC_OS_X_VERSION_MIN_REQUIRED compile-time macro.
    # To ensure these macros are available, #include <AvailabilityMacros.h>.
    # Additional documentation on these macros is available at
    # http://developer.apple.com/mac/library/technotes/tn2002/tn2064.html#SECTION3
    # Chrome normally builds with the Mac OS X 10.5 SDK and sets the
    # deployment target to 10.5.  Other projects, such as O3D, may override
    # these defaults.
    'mac_sdk%': '10.5',
    'mac_deployment_target%': '10.5',

    # Set to 1 to enable code coverage.  In addition to build changes
    # (e.g. extra CFLAGS), also creates a new target in the src/chrome
    # project file called "coverage".
    # Currently ignored on Windows.
    'coverage%': 0,

    # Although base/allocator lets you select a heap library via an
    # environment variable, the libcmt shim it uses sometimes gets in
    # the way.  To disable it entirely, and switch to normal msvcrt, do e.g.
    #  'win_use_allocator_shim': 0,
    #  'win_release_RuntimeLibrary': 2
    # to ~/.gyp/include.gypi, gclient runhooks --force, and do a release build.
    'win_use_allocator_shim%': 1, # 1 = shim allocator via libcmt; 0 = msvcrt

    # Whether usage of OpenMAX is enabled.
    'enable_openmax%': 0,

    # Whether proprietary audio/video codecs are assumed to be included with
    # this build (only meaningful if branding!=Chrome).
    'proprietary_codecs%': 0,

    # TODO(bradnelson): eliminate this when possible.
    # To allow local gyp files to prevent release.vsprops from being included.
    # Yes(1) means include release.vsprops.
    # Once all vsprops settings are migrated into gyp, this can go away.
    'msvs_use_common_release%': 1,

    # TODO(bradnelson): eliminate this when possible.
    # To allow local gyp files to override additional linker options for msvs.
    # Yes(1) means set use the common linker options.
    'msvs_use_common_linker_extras%': 1,

    # TODO(sgk): eliminate this if possible.
    # It would be nicer to support this via a setting in 'target_defaults'
    # in chrome/app/locales/locales.gypi overriding the setting in the
    # 'Debug' configuration in the 'target_defaults' dict below,
    # but that doesn't work as we'd like.
    'msvs_debug_link_incremental%': '2',

    # Needed for some of the largest modules.
    'msvs_debug_link_nonincremental%': '1',

    # Turn on Use Library Dependency Inputs for linking chrome.dll on Windows
    # to get incremental linking to be faster in debug builds.
    'incremental_chrome_dll%': '<!(python <(DEPTH)/tools/win/supalink/check_installed.py)',

    # This is the location of the sandbox binary. Chrome looks for this before
    # running the zygote process. If found, and SUID, it will be used to
    # sandbox the zygote process and, thus, all renderer processes.
    'linux_sandbox_path%': '',

    # Set this to true to enable SELinux support.
    'selinux%': 0,

    # Set this to true when building with Clang.
    # See http://code.google.com/p/chromium/wiki/Clang for details.
    'clang%': 0,
    'make_clang_dir%': 'third_party/llvm-build/Release+Asserts',

    # These two variables can be set in GYP_DEFINES while running
    # |gclient runhooks| to let clang run a plugin in every compilation.
    # Only has an effect if 'clang=1' is in GYP_DEFINES as well.
    # Example:
    #     GYP_DEFINES='clang=1 clang_load=/abs/path/to/libPrintFunctionNames.dylib clang_add_plugin=print-fns' gclient runhooks

    'clang_load%': '',
    'clang_add_plugin%': '',

    # The default type of gtest.
    'gtest_target_type%': 'executable',

    # Enable sampling based profiler.
    # See http://google-perftools.googlecode.com/svn/trunk/doc/cpuprofile.html
    'profiling%': '0',

    # Enable strict glibc debug mode.
    'glibcxx_debug%': 0,

    # Override whether we should use Breakpad on Linux. I.e. for Chrome bot.
    'linux_breakpad%': 0,
    # And if we want to dump symbols for Breakpad-enabled builds.
    'linux_dump_symbols%': 0,
    # And if we want to strip the binary after dumping symbols.
    'linux_strip_binary%': 0,
    # Strip the test binaries needed for Linux reliability tests.
    'linux_strip_reliability_tests%': 0,

    # Enable TCMalloc.
    'linux_use_tcmalloc%': 1,

    # Disable TCMalloc's debugallocation.
    'linux_use_debugallocation%': 0,

    # Disable TCMalloc's heapchecker.
    'linux_use_heapchecker%': 0,

    # Disable shadow stack keeping used by heapcheck to unwind the stacks
    # better.
    'linux_keep_shadow_stacks%': 0,

    # Set to 1 to link against libgnome-keyring instead of using dlopen().
    'linux_link_gnome_keyring%': 0,
    # Set to 1 to link against gsettings APIs instead of using dlopen().
    'linux_link_gsettings%': 0,

    # Set Thumb compilation flags.
    'arm_thumb%': 0,

    # Set ARM fpu compilation flags (only meaningful if armv7==1 and
    # arm_neon==0).
    'arm_fpu%': 'vfpv3',

    # Enable new NPDevice API.
    'enable_new_npdevice_api%': 0,

    # Enable EGLImage support in OpenMAX
    'enable_eglimage%': 1,

    # Enable a variable used elsewhere throughout the GYP files to determine
    # whether to compile in the sources for the GPU plugin / process.
    'enable_gpu%': 1,

    # .gyp files or targets should set chromium_code to 1 if they build
    # Chromium-specific code, as opposed to external code.  This variable is
    # used to control such things as the set of warnings to enable, and
    # whether warnings are treated as errors.
    'chromium_code%': 0,

    # TODO(thakis): Make this a blacklist instead, http://crbug.com/101600
    'enable_wexit_time_destructors%': 0,

    # Set to 1 to compile with the built in pdf viewer.
    'internal_pdf%': 0,

    # This allows to use libcros from the current system, ie. /usr/lib/
    # The cros_api will be pulled in as a static library, and all headers
    # from the system include dirs.
    'system_libcros%': 0,

    # NOTE: When these end up in the Mac bundle, we need to replace '-' for '_'
    # so Cocoa is happy (http://crbug.com/20441).
    'locales': [
      'am', 'ar', 'bg', 'bn', 'ca', 'cs', 'da', 'de', 'el', 'en-GB',
      'en-US', 'es-419', 'es', 'et', 'fa', 'fi', 'fil', 'fr', 'gu', 'he',
      'hi', 'hr', 'hu', 'id', 'it', 'ja', 'kn', 'ko', 'lt', 'lv',
      'ml', 'mr', 'ms', 'nb', 'nl', 'pl', 'pt-BR', 'pt-PT', 'ro', 'ru',
      'sk', 'sl', 'sr', 'sv', 'sw', 'ta', 'te', 'th', 'tr', 'uk',
      'vi', 'zh-CN', 'zh-TW',
    ],

    # Pseudo locales are special locales which are used for testing and
    # debugging. They don't get copied to the final app. For more info,
    # check out https://sites.google.com/a/chromium.org/dev/Home/fake-bidi
    'pseudo_locales': [
      'fake-bidi',
    ],

    'grit_defines': [],

    # If debug_devtools is set to 1, JavaScript files for DevTools are
    # stored as is and loaded from disk. Otherwise, a concatenated file
    # is stored in resources.pak. It is still possible to load JS files
    # from disk by passing --debug-devtools cmdline switch.
    'debug_devtools%': 0,

    # The Java Bridge is not compiled in by default.
    'java_bridge%': 0,

    # This flag is only used when disable_nacl==0 and disables all those
    # subcomponents which would require the installation of a native_client
    # untrusted toolchain.
    'disable_nacl_untrusted%': 0,

    # Disable Dart by default.
    'enable_dart%': 0,

    'conditions': [
      # Used to disable Native Client at compile time, for platforms where it
      # isn't supported (ARM)
      ['target_arch=="arm" and chromeos == 1', {
        'disable_nacl%': 1,
       }, {
        'disable_nacl%': 0,
      }],
      ['os_posix==1 and OS!="mac" and OS!="android"', {
        # This will set gcc_version to XY if you are running gcc X.Y.*.
        # This is used to tweak build flags for gcc 4.4.

	# disabled for Mozilla since it doesn't use this, and 'msys' messes $(CXX) up
	#        'gcc_version%': '<!(python <(DEPTH)/build/compiler_version.py)',
        # Figure out the python architecture to decide if we build pyauto.
	# disabled for mozilla because windows != mac and this runs a shell script
	#        'python_arch%': '<!(<(DEPTH)/build/linux/python_arch.sh <(sysroot)/usr/<(system_libdir)/libpython<(python_ver).so.1.0)',
        'conditions': [
          ['branding=="Chrome"', {
            'linux_breakpad%': 1,
          }],
          # All Chrome builds have breakpad symbols, but only process the
          # symbols from official builds.
          ['(branding=="Chrome" and buildtype=="Official")', {
            'linux_dump_symbols%': 1,
          }],
        ],
      }],  # os_posix==1 and OS!="mac" and OS!="android"
      ['OS=="android"', {
        # Location of Android NDK.
        'variables': {
          'variables': {
            'android_ndk_root%': '<!(/bin/echo -n $ANDROID_NDK_ROOT)',
            'android_target_arch%': 'arm',  # target_arch in android terms.

            # Switch between different build types, currently only '0' is
            # supported.
            'android_build_type%': 0,
          },
          'android_ndk_root%': '<(android_ndk_root)',
          'android_ndk_sysroot': '<(android_ndk_root)/platforms/android-9/arch-<(android_target_arch)',
          'android_build_type%': '<(android_build_type)',
        },
        'android_ndk_root%': '<(android_ndk_root)',
        'android_ndk_sysroot': '<(android_ndk_sysroot)',
        'android_ndk_include': '<(android_ndk_sysroot)/usr/include',
        'android_ndk_lib': '<(android_ndk_sysroot)/usr/lib',

        # Uses Android's crash report system
        'linux_breakpad%': 0,

        # Always uses openssl.
        'use_openssl%': 1,

        'proprietary_codecs%': '<(proprietary_codecs)',
        'safe_browsing%': 0,
        'configuration_policy%': 0,
        'input_speech%': 0,
        'java_bridge%': 1,
        'notifications%': 0,

        # Builds the gtest targets as a shared_library.
        # TODO(michaelbai): Use the fixed value 'shared_library' once it
        # is fully supported.
        'gtest_target_type%': '<(gtest_target_type)',

        # Uses system APIs for decoding audio and video.
        'use_libffmpeg%': '0',

        # Always use the chromium skia. The use_system_harfbuzz needs to
        # match use_system_skia.
        'use_system_skia%': '0',
        'use_system_harfbuzz%': '0',

        # Use the system icu.
        'use_system_icu%': 0,

        # Choose static link by build type.
        'conditions': [
          ['android_build_type==0', {
            'static_link_system_icu%': 1,
          }, {
            'static_link_system_icu%': 0,
          }],
        ],
        # Enable to use system sqlite.
        'use_system_sqlite%': '<(android_build_type)',
        # Enable to use system libjpeg.
        'use_system_libjpeg%': '<(android_build_type)',
        # Enable to use the system libexpat.
        'use_system_libexpat%': '<(android_build_type)',
        # Enable to use the system stlport, otherwise statically
        # link the NDK one?
        'use_system_stlport%': '<(android_build_type)',
        # Copy it out one scope.
        'android_build_type%': '<(android_build_type)',
      }],  # OS=="android"
      ['OS=="mac"', {
        # Enable clang on mac by default!
        'clang%': 1,
        # Compile in Breakpad support by default so that it can be
        # tested, even if it is not enabled by default at runtime.
        'mac_breakpad_compiled_in%': 1,
        'conditions': [
          # mac_product_name is set to the name of the .app bundle as it should
          # appear on disk.  This duplicates data from
          # chrome/app/theme/chromium/BRANDING and
          # chrome/app/theme/google_chrome/BRANDING, but is necessary to get
          # these names into the build system.
          ['branding=="Chrome"', {
            'mac_product_name%': 'Google Chrome',
          }, { # else: branding!="Chrome"
            'mac_product_name%': 'Chromium',
          }],

          ['branding=="Chrome" and buildtype=="Official"', {
            # Enable uploading crash dumps.
            'mac_breakpad_uploads%': 1,
            # Enable dumping symbols at build time for use by Mac Breakpad.
            'mac_breakpad%': 1,
            # Enable Keystone auto-update support.
            'mac_keystone%': 1,
          }, { # else: branding!="Chrome" or buildtype!="Official"
            'mac_breakpad_uploads%': 0,
            'mac_breakpad%': 0,
            'mac_keystone%': 0,
          }],
        ],
      }],  # OS=="mac"

      ['OS=="win"', {
        'conditions': [
          ['component=="shared_library"', {
            'win_use_allocator_shim%': 0,
          }],
          # Whether to use multiple cores to compile with visual studio. This is
          # optional because it sometimes causes corruption on VS 2005.
          # It is on by default on VS 2008 and off on VS 2005.
          ['MSVS_VERSION=="2005"', {
            'msvs_multi_core_compile%': 0,
          },{
            'msvs_multi_core_compile%': 1,
          }],
          # Don't do incremental linking for large modules on 32-bit.
          ['MSVS_OS_BITS==32', {
            'msvs_large_module_debug_link_mode%': '1',  # No
          },{
            'msvs_large_module_debug_link_mode%': '2',  # Yes
          }],
          ['MSVS_VERSION=="2010e" or MSVS_VERSION=="2008e" or MSVS_VERSION=="2005e"', {
            'msvs_express%': 1,
            'secure_atl%': 0,
          },{
            'msvs_express%': 0,
            'secure_atl%': 1,
          }],
        ],
        'nacl_win64_defines': [
          # This flag is used to minimize dependencies when building
          # Native Client loader for 64-bit Windows.
          'NACL_WIN64',
        ],
      }],

      ['os_posix==1 and chromeos==0 and target_arch!="arm"', {
        'use_cups%': 1,
      }, {
        'use_cups%': 0,
      }],

      # Set the relative path from this file to the GYP file of the JPEG
      # library used by Chromium.
      ['use_libjpeg_turbo==1', {
        'libjpeg_gyp_path': '../third_party/libjpeg_turbo/libjpeg.gyp',
      }, {
        'libjpeg_gyp_path': '../third_party/libjpeg/libjpeg.gyp',
      }],  # use_libjpeg_turbo==1

      # Options controlling the use of GConf (the classic GNOME configuration
      # system) and GIO, which contains GSettings (the new GNOME config system).
      ['chromeos==1', {
        'use_gconf%': 0,
        'use_gio%': 0,
      }, {
        'use_gconf%': 1,
        'use_gio%': 1,
      }],

      # Set up -D and -E flags passed into grit.
      ['branding=="Chrome"', {
        # TODO(mmoss) The .grd files look for _google_chrome, but for
        # consistency they should look for google_chrome_build like C++.
        'grit_defines': ['-D', '_google_chrome',
                         '-E', 'CHROMIUM_BUILD=google_chrome'],
      }, {
        'grit_defines': ['-D', '_chromium',
                         '-E', 'CHROMIUM_BUILD=chromium'],
      }],
      ['chromeos==1', {
        'grit_defines': ['-D', 'chromeos'],
      }],
      ['toolkit_views==1', {
        'grit_defines': ['-D', 'toolkit_views'],
      }],
      ['use_aura==1', {
        'grit_defines': ['-D', 'use_aura'],
      }],
      ['use_ash==1', {
        'grit_defines': ['-D', 'use_ash'],
      }],
      ['use_nss==1', {
        'grit_defines': ['-D', 'use_nss'],
      }],
      ['use_virtual_keyboard==1', {
        'grit_defines': ['-D', 'use_virtual_keyboard'],
      }],
      ['file_manager_extension==1', {
        'grit_defines': ['-D', 'file_manager_extension'],
      }],
      ['webui_task_manager==1', {
        'grit_defines': ['-D', 'webui_task_manager'],
      }],
      ['remoting==1', {
        'grit_defines': ['-D', 'remoting'],
      }],
      ['use_titlecase_in_grd_files==1', {
        'grit_defines': ['-D', 'use_titlecase'],
      }],
      ['use_third_party_translations==1', {
        'grit_defines': ['-D', 'use_third_party_translations'],
        'locales': [
          'ast', 'bs', 'ca@valencia', 'en-AU', 'eo', 'eu', 'gl', 'hy', 'ia',
          'ka', 'ku', 'kw', 'ms', 'ug'
        ],
      }],
      ['OS=="android"', {
        'grit_defines': ['-D', 'android'],
      }],
      ['clang_use_chrome_plugins==1', {
        'clang_chrome_plugins_flags':
            '<!(<(DEPTH)/tools/clang/scripts/plugin_flags.sh)',
      }],

      # Set use_ibus to 1 to enable ibus support.
      ['use_virtual_keyboard==1 and chromeos==1', {
        'use_ibus%': 1,
      }, {
        'use_ibus%': 0,
      }],

      ['enable_register_protocol_handler==1', {
        'grit_defines': ['-D', 'enable_register_protocol_handler'],
      }],

      ['enable_web_intents_tag==1', {
        'grit_defines': ['-D', 'enable_web_intents_tag'],
      }],

      ['asan==1', {
        'clang%': 1,
        # Do not use Chrome plugins for Clang. The Clang version in
        # third_party/asan may be different from the default one.
        'clang_use_chrome_plugins%': 0,
      }],
    ],
    # List of default apps to install in new profiles.  The first list contains
    # the source files as found in svn.  The second list, used only for linux,
    # contains the destination location for each of the files.  When a crx
    # is added or removed from the list, the chrome/browser/resources/
    # default_apps/external_extensions.json file must also be updated.
    'default_apps_list': [
      'browser/resources/default_apps/external_extensions.json',
      'browser/resources/default_apps/gmail.crx',
      'browser/resources/default_apps/search.crx',
      'browser/resources/default_apps/youtube.crx',
    ],
    'default_apps_list_linux_dest': [
      '<(PRODUCT_DIR)/default_apps/external_extensions.json',
      '<(PRODUCT_DIR)/default_apps/gmail.crx',
      '<(PRODUCT_DIR)/default_apps/search.crx',
      '<(PRODUCT_DIR)/default_apps/youtube.crx',
    ],
  },
  'target_defaults': {
    'variables': {
      # The condition that operates on chromium_code is in a target_conditions
      # section, and will not have access to the default fallback value of
      # chromium_code at the top of this file, or to the chromium_code
      # variable placed at the root variables scope of .gyp files, because
      # those variables are not set at target scope.  As a workaround,
      # if chromium_code is not set at target scope, define it in target scope
      # to contain whatever value it has during early variable expansion.
      # That's enough to make it available during target conditional
      # processing.
      'chromium_code%': '<(chromium_code)',

      # See http://gcc.gnu.org/onlinedocs/gcc-4.4.2/gcc/Optimize-Options.html
      'mac_release_optimization%': '3', # Use -O3 unless overridden
      'mac_debug_optimization%': '0',   # Use -O0 unless overridden

      # See http://msdn.microsoft.com/en-us/library/aa652360(VS.71).aspx
      'win_release_Optimization%': '2', # 2 = /Os
      'win_debug_Optimization%': '0',   # 0 = /Od

      # See http://msdn.microsoft.com/en-us/library/2kxx5t2c(v=vs.80).aspx
      # Tri-state: blank is default, 1 on, 0 off
      'win_release_OmitFramePointers%': '1',
      # Tri-state: blank is default, 1 on, 0 off
      'win_debug_OmitFramePointers%': '',

      # See http://msdn.microsoft.com/en-us/library/8wtf2dfz(VS.71).aspx
      'win_debug_RuntimeChecks%': '3',    # 3 = all checks enabled, 0 = off

      # See http://msdn.microsoft.com/en-us/library/47238hez(VS.71).aspx
      'win_debug_InlineFunctionExpansion%': '',    # empty = default, 0 = off,
      'win_release_InlineFunctionExpansion%': '2', # 1 = only __inline, 2 = max

      # VS inserts quite a lot of extra checks to algorithms like
      # std::partial_sort in Debug build which make them O(N^2)
      # instead of O(N*logN). This is particularly slow under memory
      # tools like ThreadSanitizer so we want it to be disablable.
      # See http://msdn.microsoft.com/en-us/library/aa985982(v=VS.80).aspx
      'win_debug_disable_iterator_debugging%': '0',

      'release_extra_cflags%': '',
      'debug_extra_cflags%': '',
      'release_valgrind_build%': 0,

      # the non-qualified versions are widely assumed to be *nix-only
      'win_release_extra_cflags%': '',
      'win_debug_extra_cflags%': '',

      # TODO(thakis): Make this a blacklist instead, http://crbug.com/101600
      'enable_wexit_time_destructors%': '<(enable_wexit_time_destructors)',

      # Only used by Windows build for now.  Can be used to build into a
      # differet output directory, e.g., a build_dir_prefix of VS2010_ would
      # output files in src/build/VS2010_{Debug,Release}.
      'build_dir_prefix%': '',

      'conditions': [
        ['OS=="win" and component=="shared_library"', {
          # See http://msdn.microsoft.com/en-us/library/aa652367.aspx
          'win_release_RuntimeLibrary%': '2', # 2 = /MT (nondebug DLL)
          'win_debug_RuntimeLibrary%': '3',   # 3 = /MTd (debug DLL)
        }, {
          # See http://msdn.microsoft.com/en-us/library/aa652367.aspx
          'win_release_RuntimeLibrary%': '0', # 0 = /MT (nondebug static)
          'win_debug_RuntimeLibrary%': '1',   # 1 = /MTd (debug static)
        }],
      ],
    },
    'conditions': [
      ['branding=="Chrome"', {
        'defines': ['GOOGLE_CHROME_BUILD'],
      }, {  # else: branding!="Chrome"
        'defines': ['CHROMIUM_BUILD'],
      }],
      ['component=="shared_library"', {
        'defines': ['COMPONENT_BUILD'],
      }],
      ['component=="shared_library" and incremental_chrome_dll==1', {
        # TODO(dpranke): We can't incrementally link chrome when
        # content is being built as a DLL because chrome links in
        # webkit_glue and webkit_glue depends on symbols defined in
        # content. We can remove this when we fix glue.
        # See http://code.google.com/p/chromium/issues/detail?id=98755 .
        'defines': ['COMPILE_CONTENT_STATICALLY'],
      }],
      ['toolkit_views==1', {
        'defines': ['TOOLKIT_VIEWS=1'],
      }],
      ['ui_compositor_image_transport==1', {
        'defines': ['UI_COMPOSITOR_IMAGE_TRANSPORT'],
      }],
      ['use_aura==1', {
        'defines': ['USE_AURA=1'],
      }],
      ['use_ash==1', {
        'defines': ['USE_ASH=1'],
      }],
      ['use_nss==1', {
        'defines': ['USE_NSS=1'],
      }],
      ['toolkit_uses_gtk==1', {
        'defines': ['TOOLKIT_USES_GTK=1'],
      }],
      ['toolkit_uses_gtk==1 and toolkit_views==0', {
        # TODO(erg): We are progressively sealing up use of deprecated features
        # in gtk in preparation for an eventual porting to gtk3.
        'defines': ['GTK_DISABLE_SINGLE_INCLUDES=1'],
      }],
      ['chromeos==1', {
        'defines': ['OS_CHROMEOS=1'],
      }],
      ['use_virtual_keyboard==1', {
        'defines': ['USE_VIRTUAL_KEYBOARD=1'],
      }],
      ['use_xi2_mt!=0', {
        'defines': ['USE_XI2_MT=<(use_xi2_mt)'],
      }],
      ['use_wayland==1', {
        'defines': ['USE_WAYLAND=1', 'WL_EGL_PLATFORM=1'],
      }],
      ['file_manager_extension==1', {
        'defines': ['FILE_MANAGER_EXTENSION=1'],
      }],
      ['webui_task_manager==1', {
        'defines': ['WEBUI_TASK_MANAGER=1'],
      }],
      ['profiling==1', {
        'defines': ['ENABLE_PROFILING=1'],
      }],
      ['OS=="linux" and glibcxx_debug==1', {
        'defines': ['_GLIBCXX_DEBUG=1',],
        'cflags_cc!': ['-fno-rtti'],
        'cflags_cc+': ['-frtti', '-g'],
      }],
      ['OS=="linux"', {
        # we need lrint(), which is ISOC99, and Xcode
	# already forces -std=c99 for mac below
        'defines': ['_ISOC99_SOURCE=1'],
      }],
      ['remoting==1', {
        'defines': ['ENABLE_REMOTING=1'],
      }],
      ['p2p_apis==1', {
        'defines': ['ENABLE_P2P_APIS=1'],
      }],
      ['proprietary_codecs==1', {
        'defines': ['USE_PROPRIETARY_CODECS'],
      }],
      ['enable_flapper_hacks==1', {
        'defines': ['ENABLE_FLAPPER_HACKS=1'],
      }],
      ['enable_pepper_threading==1', {
        'defines': ['ENABLE_PEPPER_THREADING'],
      }],
      ['configuration_policy==1', {
        'defines': ['ENABLE_CONFIGURATION_POLICY'],
      }],
      ['input_speech==1', {
        'defines': ['ENABLE_INPUT_SPEECH'],
      }],
      ['notifications==1', {
        'defines': ['ENABLE_NOTIFICATIONS'],
      }],
      ['fastbuild!=0', {

        'conditions': [
          # For Windows and Mac, we don't genererate debug information.
          ['OS=="win" or OS=="mac"', {
            'msvs_settings': {
              'VCLinkerTool': {
                'GenerateDebugInformation': 'false',
              },
              'VCCLCompilerTool': {
                'DebugInformationFormat': '0',
              }
            },
            'xcode_settings': {
              'GCC_GENERATE_DEBUGGING_SYMBOLS': 'NO',
            },
          }, { # else: OS != "win", generate less debug information.
            'variables': {
              'debug_extra_cflags': '-g1',
            },
          }],
          # Clang creates chubby debug information, which makes linking very
          # slow. For now, don't create debug information with clang.  See
          # http://crbug.com/70000
          ['OS=="linux" and clang==1', {
            'variables': {
              'debug_extra_cflags': '-g0',
            },
          }],
        ],  # conditions for fastbuild.
      }],  # fastbuild!=0
      ['dcheck_always_on!=0', {
        'defines': ['DCHECK_ALWAYS_ON=1'],
      }],  # dcheck_always_on!=0
      ['selinux==1', {
        'defines': ['CHROMIUM_SELINUX=1'],
      }],
      ['win_use_allocator_shim==0', {
        'conditions': [
          ['OS=="win"', {
            'defines': ['NO_TCMALLOC'],
          }],
        ],
      }],
      ['enable_gpu==1', {
        'defines': [
          'ENABLE_GPU=1',
        ],
      }],
      ['use_openssl==1', {
        'defines': [
          'USE_OPENSSL=1',
        ],
      }],
      ['enable_eglimage==1', {
        'defines': [
          'ENABLE_EGLIMAGE=1',
        ],
      }],
      ['use_skia==1', {
        'defines': [
          'USE_SKIA=1',
        ],
      }],
      ['coverage!=0', {
        'conditions': [
          ['OS=="mac"', {
            'xcode_settings': {
              'GCC_INSTRUMENT_PROGRAM_FLOW_ARCS': 'YES',  # -fprofile-arcs
              'GCC_GENERATE_TEST_COVERAGE_FILES': 'YES',  # -ftest-coverage
            },
            # Add -lgcov for types executable, shared_library, and
            # loadable_module; not for static_library.
            # This is a delayed conditional.
            'target_conditions': [
              ['_type!="static_library"', {
                'xcode_settings': { 'OTHER_LDFLAGS': [ '-lgcov' ] },
              }],
            ],
          }],
          ['OS=="linux" or OS=="android"', {
            'cflags': [ '-ftest-coverage',
                        '-fprofile-arcs' ],
            'link_settings': { 'libraries': [ '-lgcov' ] },
          }],
          # Finally, for Windows, we simply turn on profiling.
          ['OS=="win"', {
            'msvs_settings': {
              'VCLinkerTool': {
                'Profile': 'true',
              },
              'VCCLCompilerTool': {
                # /Z7, not /Zi, so coverage is happyb
                'DebugInformationFormat': '1',
                'AdditionalOptions': ['/Yd'],
              }
            }
         }],  # OS==win
        ],  # conditions for coverage
      }],  # coverage!=0
      ['OS=="win"', {
        'defines': [
          '__STD_C',
          '_CRT_SECURE_NO_DEPRECATE',
          '_SCL_SECURE_NO_DEPRECATE',
        ],
        'include_dirs': [
          '<(DEPTH)/third_party/wtl/include',
        ],
      }],  # OS==win
      ['enable_register_protocol_handler==1', {
        'defines': [
          'ENABLE_REGISTER_PROTOCOL_HANDLER=1',
        ],
      }],
      ['enable_web_intents==1', {
        'defines': [
          'ENABLE_WEB_INTENTS=1',
        ],
      }],
      ['OS=="win" and branding=="Chrome"', {
        'defines': ['ENABLE_SWIFTSHADER'],
      }],
      ['enable_dart==1', {
        'defines': ['WEBKIT_USING_DART=1'],
      }],
      ['enable_plugin_installation==1', {
        'defines': ['ENABLE_PLUGIN_INSTALLATION=1'],
      }],
    ],  # conditions for 'target_defaults'
    'target_conditions': [
      ['enable_wexit_time_destructors==1', {
        'conditions': [
          [ 'clang==1', {
            'cflags': [
              '-Wexit-time-destructors',
            ],
            'xcode_settings': {
              'WARNING_CFLAGS': [
                '-Wexit-time-destructors',
              ],
            },
          }],
        ],
      }],
      ['chromium_code==0', {
        'conditions': [
          [ 'os_posix==1 and OS!="mac"', {
            # We don't want to get warnings from third-party code,
            # so remove any existing warning-enabling flags like -Wall.
            'cflags!': [
              '-Wall',
              '-Wextra',
              '-Werror',
            ],
            'cflags_cc': [
              # Don't warn about hash_map in third-party code.
              '-Wno-deprecated',
            ],
            'cflags': [
              # Don't warn about printf format problems.
              # This is off by default in gcc but on in Ubuntu's gcc(!).
              '-Wno-format',
            ],
            'cflags_cc!': [
              # TODO(fischman): remove this.
              # http://code.google.com/p/chromium/issues/detail?id=90453
              '-Wsign-compare',
            ]
          }],
          [ 'os_posix==1 and os_bsd!=1 and OS!="mac" and OS!="android"', {
            'cflags': [
              # Don't warn about ignoring the return value from e.g. close().
              # This is off by default in some gccs but on by default in others.
              # BSD systems do not support this option, since they are usually
              # using gcc 4.2.1, which does not have this flag yet.
              '-Wno-unused-result',
            ],
          }],
          [ 'OS=="win"', {
            'defines': [
              '_CRT_SECURE_NO_DEPRECATE',
              '_CRT_NONSTDC_NO_WARNINGS',
              '_CRT_NONSTDC_NO_DEPRECATE',
              '_SCL_SECURE_NO_DEPRECATE',
            ],
            'msvs_disabled_warnings': [4800],
            'msvs_settings': {
              'VCCLCompilerTool': {
                'WarningLevel': '3',
                'WarnAsError': 'false', # TODO(maruel): Enable it.
                'Detect64BitPortabilityProblems': 'false',
              },
            },
          }],
          # TODO(darin): Unfortunately, some third_party code depends on base/
          [ 'OS=="win" and component=="shared_library"', {
            'msvs_disabled_warnings': [
              4251,  # class 'std::xx' needs to have dll-interface.
            ],
          }],
          [ 'OS=="mac"', {
            'xcode_settings': {
              'GCC_TREAT_WARNINGS_AS_ERRORS': 'NO',
              'WARNING_CFLAGS!': ['-Wall', '-Wextra'],
            },
          }],
        ],
      }, {
        'includes': [
           # Rules for excluding e.g. foo_win.cc from the build on non-Windows.
          'filename_rules.gypi',
        ],
        # In Chromium code, we define __STDC_FORMAT_MACROS in order to get the
        # C99 macros on Mac and Linux.
        'defines': [
          '__STDC_FORMAT_MACROS',
        ],
        'conditions': [
          ['OS=="win"', {
            # turn on warnings for signed/unsigned mismatch on chromium code.
            'msvs_settings': {
              'VCCLCompilerTool': {
                'AdditionalOptions': ['/we4389'],
              },
            },
          }],
          ['OS=="win" and component=="shared_library"', {
            'msvs_disabled_warnings': [
              4251,  # class 'std::xx' needs to have dll-interface.
            ],
          }],
        ],
      }],
    ],  # target_conditions for 'target_defaults'
    'default_configuration': 'Debug',
    'configurations': {
      # VCLinkerTool LinkIncremental values below:
      #   0 == default
      #   1 == /INCREMENTAL:NO
      #   2 == /INCREMENTAL
      # Debug links incremental, Release does not.
      #
      # Abstract base configurations to cover common attributes.
      #
      'Common_Base': {
        'abstract': 1,
        'msvs_configuration_attributes': {
          'OutputDirectory': '<(DEPTH)\\build\\<(build_dir_prefix)$(ConfigurationName)',
          'IntermediateDirectory': '$(OutDir)\\obj\\$(ProjectName)',
          'CharacterSet': '1',
        },
      },
      'x86_Base': {
        'abstract': 1,
        'msvs_settings': {
          'VCLinkerTool': {
            'TargetMachine': '1',
          },
        },
        'msvs_configuration_platform': 'Win32',
      },
      'x64_Base': {
        'abstract': 1,
        'msvs_configuration_platform': 'x64',
        'msvs_settings': {
          'VCLinkerTool': {
            'TargetMachine': '17', # x86 - 64
            'AdditionalLibraryDirectories!':
              ['<(DEPTH)/third_party/platformsdk_win7/files/Lib'],
            'AdditionalLibraryDirectories':
              ['<(DEPTH)/third_party/platformsdk_win7/files/Lib/x64'],
          },
          'VCLibrarianTool': {
            'AdditionalLibraryDirectories!':
              ['<(DEPTH)/third_party/platformsdk_win7/files/Lib'],
            'AdditionalLibraryDirectories':
              ['<(DEPTH)/third_party/platformsdk_win7/files/Lib/x64'],
          },
        },
        'defines': [
          # Not sure if tcmalloc works on 64-bit Windows.
          'NO_TCMALLOC',
        ],
      },
      'Debug_Base': {
        'abstract': 1,
        'defines': [
          'DYNAMIC_ANNOTATIONS_ENABLED=1',
          'WTF_USE_DYNAMIC_ANNOTATIONS=1',
        ],
        'xcode_settings': {
          'COPY_PHASE_STRIP': 'NO',
          'GCC_OPTIMIZATION_LEVEL': '<(mac_debug_optimization)',
          'OTHER_CFLAGS': [
            '<@(debug_extra_cflags)',
          ],
        },
        'msvs_settings': {
          'VCCLCompilerTool': {
            'Optimization': '<(win_debug_Optimization)',
            'PreprocessorDefinitions': ['_DEBUG'],
            'BasicRuntimeChecks': '<(win_debug_RuntimeChecks)',
            'RuntimeLibrary': '<(win_debug_RuntimeLibrary)',
            'conditions': [
              # According to MSVS, InlineFunctionExpansion=0 means
              # "default inlining", not "/Ob0".
              # Thus, we have to handle InlineFunctionExpansion==0 separately.
              ['win_debug_InlineFunctionExpansion==0', {
                'AdditionalOptions': ['/Ob0'],
              }],
              ['win_debug_InlineFunctionExpansion!=""', {
                'InlineFunctionExpansion':
                  '<(win_debug_InlineFunctionExpansion)',
              }],
              ['win_debug_disable_iterator_debugging==1', {
                'PreprocessorDefinitions': ['_HAS_ITERATOR_DEBUGGING=0'],
              }],

              # if win_debug_OmitFramePointers is blank, leave as default
              ['win_debug_OmitFramePointers==1', {
                'OmitFramePointers': 'true',
              }],
              ['win_debug_OmitFramePointers==0', {
                'OmitFramePointers': 'false',
                # The above is not sufficient (http://crbug.com/106711): it
                # simply eliminates an explicit "/Oy", but both /O2 and /Ox
                # perform FPO regardless, so we must explicitly disable.
                # We still want the false setting above to avoid having
                # "/Oy /Oy-" and warnings about overriding.
                'AdditionalOptions': ['/Oy-'],
              }],
            ],
            'AdditionalOptions': [ '<@(win_debug_extra_cflags)', ],
          },
          'VCLinkerTool': {
            'LinkIncremental': '<(msvs_debug_link_incremental)',
            # ASLR makes debugging with windbg difficult because Chrome.exe and
            # Chrome.dll share the same base name. As result, windbg will
            # name the Chrome.dll module like chrome_<base address>, where
            # <base address> typically changes with each launch. This in turn
            # means that breakpoints in Chrome.dll don't stick from one launch
            # to the next. For this reason, we turn ASLR off in debug builds.
            # Note that this is a three-way bool, where 0 means to pick up
            # the default setting, 1 is off and 2 is on.
            'RandomizedBaseAddress': 1,
          },
          'VCResourceCompilerTool': {
            'PreprocessorDefinitions': ['_DEBUG'],
          },
        },
        'conditions': [
          ['OS=="linux"', {
            'cflags': [
              '<@(debug_extra_cflags)',
            ],
          }],
          ['release_valgrind_build==0', {
            'xcode_settings': {
              'OTHER_CFLAGS': [
                '-fstack-protector-all',  # Implies -fstack-protector
              ],
            },
          }],
        ],
      },
      'Release_Base': {
        'abstract': 1,
        'defines': [
          'NDEBUG',
        ],
        'xcode_settings': {
          'DEAD_CODE_STRIPPING': 'YES',  # -Wl,-dead_strip
          'GCC_OPTIMIZATION_LEVEL': '<(mac_release_optimization)',
          'OTHER_CFLAGS': [ '<@(release_extra_cflags)', ],
        },
        'msvs_settings': {
          'VCCLCompilerTool': {
            'RuntimeLibrary': '<(win_release_RuntimeLibrary)',
            'conditions': [
              # In official builds, each target will self-select
              # an optimization level.
              ['buildtype!="Official"', {
                  'Optimization': '<(win_release_Optimization)',
                },
              ],
              # According to MSVS, InlineFunctionExpansion=0 means
              # "default inlining", not "/Ob0".
              # Thus, we have to handle InlineFunctionExpansion==0 separately.
              ['win_release_InlineFunctionExpansion==0', {
                'AdditionalOptions': ['/Ob0'],
              }],
              ['win_release_InlineFunctionExpansion!=""', {
                'InlineFunctionExpansion':
                  '<(win_release_InlineFunctionExpansion)',
              }],

              # if win_release_OmitFramePointers is blank, leave as default
              ['win_release_OmitFramePointers==1', {
                'OmitFramePointers': 'true',
              }],
              ['win_release_OmitFramePointers==0', {
                'OmitFramePointers': 'false',
                # The above is not sufficient (http://crbug.com/106711): it
                # simply eliminates an explicit "/Oy", but both /O2 and /Ox
                # perform FPO regardless, so we must explicitly disable.
                # We still want the false setting above to avoid having
                # "/Oy /Oy-" and warnings about overriding.
                'AdditionalOptions': ['/Oy-'],
              }],
            ],
            'AdditionalOptions': [ '<@(win_release_extra_cflags)', ],
          },
          'VCLinkerTool': {
            # LinkIncremental is a tri-state boolean, where 0 means default
            # (i.e., inherit from parent solution), 1 means false, and
            # 2 means true.
            'LinkIncremental': '1',
            # This corresponds to the /PROFILE flag which ensures the PDB
            # file contains FIXUP information (growing the PDB file by about
            # 5%) but does not otherwise alter the output binary. This
            # information is used by the Syzygy optimization tool when
            # decomposing the release image.
            'Profile': 'true',
          },
        },
        'conditions': [
          ['release_valgrind_build==0', {
            'defines': [
              'NVALGRIND',
              'DYNAMIC_ANNOTATIONS_ENABLED=0',
            ],
          }, {
            'defines': [
              'DYNAMIC_ANNOTATIONS_ENABLED=1',
              'WTF_USE_DYNAMIC_ANNOTATIONS=1',
            ],
          }],
          ['win_use_allocator_shim==0', {
            'defines': ['NO_TCMALLOC'],
          }],
          ['OS=="linux"', {
            'cflags': [
             '<@(release_extra_cflags)',
            ],
          }],
        ],
      },
      #
      # Concrete configurations
      #
      'Debug': {
        'inherit_from': ['Common_Base', 'x86_Base', 'Debug_Base'],
      },
      'Release': {
        'inherit_from': ['Common_Base', 'x86_Base', 'Release_Base'],
        'conditions': [
          ['msvs_use_common_release', {
            'includes': ['release.gypi'],
          }],
        ]
      },
      'conditions': [
        [ 'OS=="win"', {
          # TODO(bradnelson): add a gyp mechanism to make this more graceful.
          'Debug_x64': {
            'inherit_from': ['Common_Base', 'x64_Base', 'Debug_Base'],
          },
          'Release_x64': {
            'inherit_from': ['Common_Base', 'x64_Base', 'Release_Base'],
          },
        }],
      ],
    },
  },
  'conditions': [
    ['os_posix==1 and OS!="mac"', {
      'target_defaults': {
        # Enable -Werror by default, but put it in a variable so it can
        # be disabled in ~/.gyp/include.gypi on the valgrind builders.
        'variables': {
          'werror%': '-Werror',
        },
        'defines': [
          '_FILE_OFFSET_BITS=64',
        ],
        'cflags': [
          '<(werror)',  # See note above about the werror variable.
          '-pthread',
          '-fno-exceptions',
          '-fno-strict-aliasing',  # See http://crbug.com/32204
          '-Wall',
          # TODO(evan): turn this back on once all the builds work.
          # '-Wextra',
          # Don't warn about unused function params.  We use those everywhere.
          '-Wno-unused-parameter',
          # Don't warn about the "struct foo f = {0};" initialization pattern.
          '-Wno-missing-field-initializers',
          # Don't export any symbols (for example, to plugins we dlopen()).
          # Note: this is *required* to make some plugins work.
          '-fvisibility=hidden',
          '-pipe',
        ],
        'cflags_cc': [
          '-fno-rtti',
          '-fno-threadsafe-statics',
          # Make inline functions have hidden visiblity by default.
          # Surprisingly, not covered by -fvisibility=hidden.
          '-fvisibility-inlines-hidden',
          # GCC turns on -Wsign-compare for C++ under -Wall, but clang doesn't,
          # so we specify it explicitly.
          # TODO(fischman): remove this if http://llvm.org/PR10448 obsoletes it.
          # http://code.google.com/p/chromium/issues/detail?id=90453
          '-Wsign-compare',
        ],
        'ldflags': [
          '-pthread', '-Wl,-z,noexecstack',
        ],
        'configurations': {
          'Debug_Base': {
            'variables': {
              'debug_optimize%': '0',
            },
            'defines': [
              '_DEBUG',
            ],
            'cflags': [
              '-O>(debug_optimize)',
              '-g',
            ],
            'conditions' : [
              ['OS=="android"', {
                'cflags': [
                  '-fno-omit-frame-pointer',
                ],
              }],
            ],
            'target_conditions' : [
              ['_toolset=="target"', {
                'conditions': [
                  ['OS=="android" and debug_optimize==0', {
                    'cflags': [
                      '-mlong-calls',  # Needed when compiling with -O0
                    ],
                  }],
                  ['arm_thumb==1', {
                    'cflags': [
                      '-marm',
                    ],
                  }],
                 ],
              }],
            ],
          },
          'Release_Base': {
            'variables': {
              'release_optimize%': '2',
              # Binaries become big and gold is unable to perform GC
              # and remove unused sections for some of test targets
              # on 32 bit platform.
              # (This is currently observed only in chromeos valgrind bots)
              # The following flag is to disable --gc-sections linker
              # option for these bots.
              'no_gc_sections%': 0,

              # TODO(bradnelson): reexamine how this is done if we change the
              # expansion of configurations
              'release_valgrind_build%': 0,
            },
            'cflags': [
              '-O>(release_optimize)',
              # Don't emit the GCC version ident directives, they just end up
              # in the .comment section taking up binary size.
              '-fno-ident',
              # Put data and code in their own sections, so that unused symbols
              # can be removed at link time with --gc-sections.
              '-fdata-sections',
              '-ffunction-sections',
            ],
            'ldflags': [
              # Specifically tell the linker to perform optimizations.
              # See http://lwn.net/Articles/192624/ .
              '-Wl,-O1',
              '-Wl,--as-needed',
            ],
            'conditions' : [
              ['no_gc_sections==0', {
                'ldflags': [
                  '-Wl,--gc-sections',
                ],
              }],
              ['OS=="android"', {
                'cflags': [
                  '-fomit-frame-pointer',
                ],
              }],
              ['clang==1', {
                'cflags!': [
                  '-fno-ident',
                ],
              }],
              ['profiling==1', {
                'cflags': [
                  '-fno-omit-frame-pointer',
                  '-g',
                ],
              }],
              # At gyp time, we test the linker for ICF support; this flag
              # is then provided to us by gyp.  (Currently only gold supports
              # an --icf flag.)
              # There seems to be a conflict of --icf and -pie in gold which
              # can generate crashy binaries. As a security measure, -pie
              # takes precendence for now.
              ['LINKER_SUPPORTS_ICF==1 and release_valgrind_build==0', {
                'target_conditions': [
                  ['_toolset=="target"', {
                    'ldflags': [
                      #'-Wl,--icf=safe',
                      '-Wl,--icf=none',
                    ]
                  }]
                ]
              }],
            ]
          },
        },
        'variants': {
          'coverage': {
            'cflags': ['-fprofile-arcs', '-ftest-coverage'],
            'ldflags': ['-fprofile-arcs'],
          },
          'profile': {
            'cflags': ['-pg', '-g'],
            'ldflags': ['-pg'],
          },
          'symbols': {
            'cflags': ['-g'],
          },
        },
        'conditions': [
          ['target_arch=="ia32"', {
            'target_conditions': [
              ['_toolset=="target"', {
                'asflags': [
                  # Needed so that libs with .s files (e.g. libicudata.a)
                  # are compatible with the general 32-bit-ness.
                  '-32',
                ],
                # All floating-point computations on x87 happens in 80-bit
                # precision.  Because the C and C++ language standards allow
                # the compiler to keep the floating-point values in higher
                # precision than what's specified in the source and doing so
                # is more efficient than constantly rounding up to 64-bit or
                # 32-bit precision as specified in the source, the compiler,
                # especially in the optimized mode, tries very hard to keep
                # values in x87 floating-point stack (in 80-bit precision)
                # as long as possible. This has important side effects, that
                # the real value used in computation may change depending on
                # how the compiler did the optimization - that is, the value
                # kept in 80-bit is different than the value rounded down to
                # 64-bit or 32-bit. There are possible compiler options to
                # make this behavior consistent (e.g. -ffloat-store would keep
                # all floating-values in the memory, thus force them to be
                # rounded to its original precision) but they have significant
                # runtime performance penalty.
                #
                # -mfpmath=sse -msse2 makes the compiler use SSE instructions
                # which keep floating-point values in SSE registers in its
                # native precision (32-bit for single precision, and 64-bit
                # for double precision values). This means the floating-point
                # value used during computation does not change depending on
                # how the compiler optimized the code, since the value is
                # always kept in its specified precision.
                'conditions': [
                  ['branding=="Chromium" and disable_sse2==0', {
                    'cflags': [
                      '-march=pentium4',
                      '-msse2',
                      '-mfpmath=sse',
                    ],
                  }],
                  # ChromeOS targets Pinetrail, which is sse3, but most of the
                  # benefit comes from sse2 so this setting allows ChromeOS
                  # to build on other CPUs.  In the future -march=atom would
                  # help but requires a newer compiler.
                  ['chromeos==1 and disable_sse2==0', {
                    'cflags': [
                      '-msse2',
                    ],
                  }],
                  # Install packages have started cropping up with
                  # different headers between the 32-bit and 64-bit
                  # versions, so we have to shadow those differences off
                  # and make sure a 32-bit-on-64-bit build picks up the
                  # right files.
                  ['host_arch!="ia32"', {
                    'include_dirs+': [
                      '/usr/include32',
                    ],
                  }],
                ],
                # -mmmx allows mmintrin.h to be used for mmx intrinsics.
                # video playback is mmx and sse2 optimized.
                'cflags': [
                  '-m32',
                  '-mmmx',
                ],
                'ldflags': [
                  '-m32',
                ],
                'cflags_mozilla': [
                  '-m32',
                  '-mmmx',
                ],
              }],
            ],
          }],
          ['target_arch=="arm"', {
            'target_conditions': [
              ['_toolset=="target"', {
                'cflags_cc': [
                  # The codesourcery arm-2009q3 toolchain warns at that the ABI
                  # has changed whenever it encounters a varargs function. This
                  # silences those warnings, as they are not helpful and
                  # clutter legitimate warnings.
                  '-Wno-abi',
                ],
                'conditions': [
                  ['arm_thumb==1', {
                    'cflags': [
                    '-mthumb',
                    ]
                  }],
                  ['armv7==1', {
                    'cflags': [
                      '-march=armv7-a',
                      '-mtune=cortex-a8',
                      '-mfloat-abi=softfp',
                    ],
                    'conditions': [
                      ['arm_neon==1', {
                        'cflags': [ '-mfpu=neon', ],
                      }, {
                        'cflags': [ '-mfpu=<(arm_fpu)', ],
                      }]
                    ],
                  }],
                  ['OS=="android"', {
                    # The following flags are derived from what Android uses
                    # by default when building for arm.
                    'cflags': [ '-Wno-psabi', ],
                    'conditions': [
                      ['arm_thumb == 1', {
                        # Android toolchain doesn't support -mimplicit-it=thumb
                        'cflags!': [ '-Wa,-mimplicit-it=thumb', ],
                        'cflags': [ '-mthumb-interwork', ],
                      }],
                      ['armv7==0', {
                        # Flags suitable for Android emulator
                        'cflags': [
                          '-march=armv5te',
                          '-mtune=xscale',
                          '-msoft-float',
                        ],
                        'defines': [
                          '__ARM_ARCH_5__',
                          '__ARM_ARCH_5T__',
                          '__ARM_ARCH_5E__',
                          '__ARM_ARCH_5TE__',
                        ],
                      }],
                    ],
                  }],
                ],
              }],
            ],
          }],
          ['linux_fpic==1', {
            'cflags': [
              '-fPIC',
            ],
            'ldflags': [
              '-fPIC',
            ],
          }],
          # TODO(rkc): Currently building Chrome with the PIE flag causes
          # remote debugging to break (remote debugger does not get correct
          # section header offsets hence causing all symbol handling to go
          # kaboom). See crosbug.com/15266
          # Remove this flag once this issue is fixed.
          ['linux_disable_pie==1', {
            'target_conditions': [
              ['_type=="executable"', {
                'ldflags': [
                  '-nopie',
                ],
              }],
            ],
          }],
          ['sysroot!=""', {
            'target_conditions': [
              ['_toolset=="target"', {
                'cflags': [
                  '--sysroot=<(sysroot)',
                ],
                'ldflags': [
                  '--sysroot=<(sysroot)',
                ],
              }]]
          }],
          ['clang==1', {
            'cflags': [
              '-Wheader-hygiene',
              # Clang spots more unused functions.
              '-Wno-unused-function',
              # Don't die on dtoa code that uses a char as an array index.
              '-Wno-char-subscripts',
              # Especially needed for gtest macros using enum values from Mac
              # system headers.
              # TODO(pkasting): In C++11 this is legal, so this should be
              # removed when we change to that.  (This is also why we don't
              # bother fixing all these cases today.)
              '-Wno-unnamed-type-template-args',
              # WebKit uses nullptr in a legit way, other that that this warning
              # doesn't fire.
              '-Wno-c++11-compat',
              # This (rightyfully) complains about 'override', which we use
              # heavily.
              '-Wno-c++11-extensions',

              # Warns on switches on enums that cover all enum values but
              # also contain a default: branch. Chrome is full of that.
              '-Wno-covered-switch-default',

              # TODO(thakis): Reenable once this no longer complains about
              # Invalid() in gmocks's gmock-internal-utils.h
              # http://crbug.com/111806
              '-Wno-null-dereference',
            ],
            'cflags!': [
              # Clang doesn't seem to know know this flag.
              '-mfpmath=sse',
            ],
          }],
          ['clang==1 and clang_use_chrome_plugins==1', {
            'cflags': [
              '<(clang_chrome_plugins_flags)',
            ],
          }],
          ['clang==1 and clang_load!=""', {
            'cflags': [
              '-Xclang', '-load', '-Xclang', '<(clang_load)',
            ],
          }],
          ['clang==1 and clang_add_plugin!=""', {
            'cflags': [
              '-Xclang', '-add-plugin', '-Xclang', '<(clang_add_plugin)',
            ],
          }],
          ['clang==1 and "<(GENERATOR)"=="ninja"', {
            'cflags': [
              # See http://crbug.com/110262
              '-fcolor-diagnostics',
            ],
          }],
          ['asan==1', {
            'cflags': [
              '-faddress-sanitizer',
              '-fno-omit-frame-pointer',
              '-w',
            ],
            'ldflags': [
              '-faddress-sanitizer',
            ],
            'defines': [
              'ADDRESS_SANITIZER',
            ],
          }],
          ['linux_breakpad==1', {
            'cflags': [ '-g' ],
            'defines': ['USE_LINUX_BREAKPAD'],
          }],
          ['linux_use_heapchecker==1', {
            'variables': {'linux_use_tcmalloc%': 1},
            'defines': ['USE_HEAPCHECKER'],
          }],
          ['linux_use_tcmalloc==0', {
            'defines': ['NO_TCMALLOC'],
          }],
          ['linux_keep_shadow_stacks==1', {
            'defines': ['KEEP_SHADOW_STACKS'],
            'cflags': ['-finstrument-functions'],
          }],
          ['linux_use_gold_binary==1', {
            'variables': {
              # We pass the path to gold to the compiler.  gyp leaves
              # unspecified what the cwd is when running the compiler,
              # so the normal gyp path-munging fails us.  This hack
              # gets the right path.
              'gold_path': '<(PRODUCT_DIR)/../../third_party/gold',
            },
            'ldflags': [
              # Put our gold binary in the search path for the linker.
              '-B<(gold_path)',
            ],
          }],
        ],
      },
    }],
    # FreeBSD-specific options; note that most FreeBSD options are set above,
    # with Linux.
    ['OS=="freebsd"', {
      'target_defaults': {
        'ldflags': [
          '-Wl,--no-keep-memory',
        ],
      },
    }],
    # Android-specific options; note that most are set above with Linux.
    ['OS=="android"', {
      'variables': {
        'android_target_arch%': 'arm',  # target_arch in android terms.
        'conditions': [
          # Android uses x86 instead of ia32 for their target_arch designation.
          ['target_arch=="ia32"', {
            'android_target_arch%': 'x86',
          }],
          # Use shared stlport library when system one used.
          # Figure this out early since it needs symbols from libgcc.a, so it
          # has to be before that in the set of libraries.
          ['use_system_stlport==1', {
            'android_stlport_library': 'stlport',
          }, {
            'android_stlport_library': 'stlport_static',
          }],
        ],

        # Placing this variable here prevents from forking libvpx, used
        # by remoting.  Remoting is off, so it needn't built,
        # so forking it's deps seems like overkill.
        # But this variable need defined to properly run gyp.
        # A proper solution is to have an OS==android conditional
        # in third_party/libvpx/libvpx.gyp to define it.
        'libvpx_path': 'lib/linux/arm',
      },
      'target_defaults': {
        # Build a Release build by default to match Android build behavior.
        # This is typical with Android because Debug builds tend to be much
        # larger and run very slowly on constrained devices. It is still
        # possible to do a Debug build by specifying BUILDTYPE=Debug on the
        # 'make' command line.
        'default_configuration': 'Release',

        'variables': {
          'release_extra_cflags%': '',
         },

        'target_conditions': [
          # Settings for building device targets using Android's toolchain.
          # These are based on the setup.mk file from the Android NDK.
          #
          # The NDK Android executable link step looks as follows:
          #  $LDFLAGS
          #  $(TARGET_CRTBEGIN_DYNAMIC_O)  <-- crtbegin.o
          #  $(PRIVATE_OBJECTS)            <-- The .o that we built
          #  $(PRIVATE_STATIC_LIBRARIES)   <-- The .a that we built
          #  $(TARGET_LIBGCC)              <-- libgcc.a
          #  $(PRIVATE_SHARED_LIBRARIES)   <-- The .so that we built
          #  $(PRIVATE_LDLIBS)             <-- System .so
          #  $(TARGET_CRTEND_O)            <-- crtend.o
          #
          # For now the above are approximated for executables by adding
          # crtbegin.o to the end of the ldflags and 'crtend.o' to the end
          # of 'libraries'.
          #
          # The NDK Android shared library link step looks as follows:
          #  $LDFLAGS
          #  $(PRIVATE_OBJECTS)            <-- The .o that we built
          #  -l,--whole-archive
          #  $(PRIVATE_WHOLE_STATIC_LIBRARIES)
          #  -l,--no-whole-archive
          #  $(PRIVATE_STATIC_LIBRARIES)   <-- The .a that we built
          #  $(TARGET_LIBGCC)              <-- libgcc.a
          #  $(PRIVATE_SHARED_LIBRARIES)   <-- The .so that we built
          #  $(PRIVATE_LDLIBS)             <-- System .so
          #
          # For now, assume not need any whole static libs.
          #
          # For both executables and shared libraries, add the proper
          # libgcc.a to the start of libraries which puts it in the
          # proper spot after .o and .a files get linked in.
          #
          # TODO: The proper thing to do longer-tem would be proper gyp
          # support for a custom link command line.
          ['_toolset=="target"', {
            'cflags!': [
              '-pthread',  # Not supported by Android toolchain.
            ],
            'cflags': [
              '-U__linux__',  # Don't allow toolchain to claim -D__linux__
              '-ffunction-sections',
              '-funwind-tables',
              '-g',
              '-fstack-protector',
              '-fno-short-enums',
              '-finline-limit=64',
              '-Wa,--noexecstack',
              '-Wno-error=non-virtual-dtor',  # TODO(michaelbai): Fix warnings.
              '<@(release_extra_cflags)',
              # Note: This include is in cflags to ensure that it comes after
              # all of the includes.
              '-I<(android_ndk_include)',
            ],
            'defines': [
              'ANDROID',
              '__GNU_SOURCE=1',  # Necessary for clone()
              'USE_STLPORT=1',
              '_STLP_USE_PTR_SPECIALIZATIONS=1',
              'HAVE_OFF64_T',
              'HAVE_SYS_UIO_H',
              'ANDROID_BINSIZE_HACK', # Enable temporary hacks to reduce binsize.
            ],
            'ldflags!': [
              '-pthread',  # Not supported by Android toolchain.
            ],
            'ldflags': [
              '-nostdlib',
              '-Wl,--no-undefined',
              '-Wl,--icf=safe',  # Enable identical code folding to reduce size
              # Don't export symbols from statically linked libraries.
              '-Wl,--exclude-libs=ALL',
            ],
            'libraries': [
              '-l<(android_stlport_library)',
              # Manually link the libgcc.a that the cross compiler uses.
              '<!($CROSS_CC -print-libgcc-file-name)',
              '-lc',
              '-ldl',
              '-lstdc++',
              '-lm',
            ],
            'conditions': [
              ['android_build_type==0', {
                'ldflags': [
                  '--sysroot=<(android_ndk_sysroot)',
                ],
              }],
              # NOTE: The stlport header include paths below are specified in
              # cflags rather than include_dirs because they need to come
              # after include_dirs. Think of them like system headers, but
              # don't use '-isystem' because the arm-linux-androideabi-4.4.3
              # toolchain (circa Gingerbread) will exhibit strange errors.
              # The include ordering here is important; change with caution.
              ['use_system_stlport==0', {
                'cflags': [
                  '-I<(android_ndk_root)/sources/cxx-stl/stlport/stlport',
                ],
                'conditions': [
                  ['target_arch=="arm" and armv7==1', {
                    'ldflags': [
                      '-L<(android_ndk_root)/sources/cxx-stl/stlport/libs/armeabi-v7a',
                    ],
                  }],
                  ['target_arch=="arm" and armv7==0', {
                    'ldflags': [
                      '-L<(android_ndk_root)/sources/cxx-stl/stlport/libs/armeabi',
                    ],
                  }],
                  ['target_arch=="ia32"', {
                    'ldflags': [
                      '-L<(android_ndk_root)/sources/cxx-stl/stlport/libs/x86',
                    ],
                  }],
                ],
              }],
              ['target_arch=="ia32"', {
                # The x86 toolchain currently has problems with stack-protector.
                'cflags!': [
                  '-fstack-protector',
                ],
                'cflags': [
                  '-fno-stack-protector',
                ],
              }],
            ],
            'target_conditions': [
              ['_type=="executable"', {
                'ldflags': [
                  '-Bdynamic',
                  '-Wl,-dynamic-linker,/system/bin/linker',
                  '-Wl,--gc-sections',
                  '-Wl,-z,nocopyreloc',
                  # crtbegin_dynamic.o should be the last item in ldflags.
                  '<(android_ndk_lib)/crtbegin_dynamic.o',
                ],
                'libraries': [
                  # crtend_android.o needs to be the last item in libraries.
                  # Do not add any libraries after this!
                  '<(android_ndk_lib)/crtend_android.o',
                ],
              }],
              ['_type=="shared_library"', {
                'ldflags': [
                  '-Wl,-shared,-Bsymbolic',
                ],
              }],
            ],
          }],
          # Settings for building host targets using the system toolchain.
          ['_toolset=="host"', {
            'ldflags!': [
              '-Wl,-z,noexecstack',
              '-Wl,--gc-sections',
              '-Wl,-O1',
              '-Wl,--as-needed',
            ],
            'sources/': [
              ['exclude', '_android(_unittest)?\\.cc$'],
              ['exclude', '(^|/)android/']
            ],
          }],
        ],
      },
    }],
    ['OS=="solaris"', {
      'cflags!': ['-fvisibility=hidden'],
      'cflags_cc!': ['-fvisibility-inlines-hidden'],
    }],
    ['OS=="mac"', {
      'target_defaults': {
        'variables': {
          # These should end with %, but there seems to be a bug with % in
          # variables that are intended to be set to different values in
          # different targets, like these.
          'mac_pie': 1,        # Most executables can be position-independent.
          'mac_real_dsym': 0,  # Fake .dSYMs are fine in most cases.
          'mac_strip': 1,      # Strip debugging symbols from the target.
        },
        'mac_bundle': 0,
        'xcode_settings': {
          'ALWAYS_SEARCH_USER_PATHS': 'NO',
          'GCC_C_LANGUAGE_STANDARD': 'c99',         # -std=c99
          'GCC_CW_ASM_SYNTAX': 'NO',                # No -fasm-blocks
          'GCC_DYNAMIC_NO_PIC': 'NO',               # No -mdynamic-no-pic
                                                    # (Equivalent to -fPIC)
          'GCC_ENABLE_CPP_EXCEPTIONS': 'NO',        # -fno-exceptions
          'GCC_ENABLE_CPP_RTTI': 'NO',              # -fno-rtti
          'GCC_ENABLE_PASCAL_STRINGS': 'NO',        # No -mpascal-strings
          # GCC_INLINES_ARE_PRIVATE_EXTERN maps to -fvisibility-inlines-hidden
          'GCC_INLINES_ARE_PRIVATE_EXTERN': 'YES',
          'GCC_OBJC_CALL_CXX_CDTORS': 'YES',        # -fobjc-call-cxx-cdtors
          'GCC_SYMBOLS_PRIVATE_EXTERN': 'YES',      # -fvisibility=hidden
          'GCC_THREADSAFE_STATICS': 'NO',           # -fno-threadsafe-statics
          'GCC_TREAT_WARNINGS_AS_ERRORS': 'YES',    # -Werror
          'GCC_VERSION': '4.2',
          'GCC_WARN_ABOUT_MISSING_NEWLINE': 'YES',  # -Wnewline-eof
          # MACOSX_DEPLOYMENT_TARGET maps to -mmacosx-version-min
          'MACOSX_DEPLOYMENT_TARGET': '<(mac_deployment_target)',
          # Keep pch files below xcodebuild/.
          'SHARED_PRECOMPS_DIR': '$(CONFIGURATION_BUILD_DIR)/SharedPrecompiledHeaders',
          'USE_HEADERMAP': 'NO',
          'OTHER_CFLAGS': [
            '-fno-strict-aliasing',  # See http://crbug.com/32204
          ],
          'WARNING_CFLAGS': [
            '-Wall',
            '-Wendif-labels',
            '-Wextra',
            # Don't warn about unused function parameters.
            '-Wno-unused-parameter',
            # Don't warn about the "struct foo f = {0};" initialization
            # pattern.
            '-Wno-missing-field-initializers',
          ],
          'conditions': [
            ['chromium_mac_pch', {'GCC_PRECOMPILE_PREFIX_HEADER': 'YES'},
                                 {'GCC_PRECOMPILE_PREFIX_HEADER': 'NO'}
            ],
            ['clang==1', {
              'CC': '$(SOURCE_ROOT)/<(clang_dir)/clang',
              'LDPLUSPLUS': '$(SOURCE_ROOT)/<(clang_dir)/clang++',

              # Don't use -Wc++0x-extensions, which Xcode 4 enables by default
              # when buliding with clang. This warning is triggered when the
              # override keyword is used via the OVERRIDE macro from
              # base/compiler_specific.h.
              'CLANG_WARN_CXX0X_EXTENSIONS': 'NO',

              'GCC_VERSION': 'com.apple.compilers.llvm.clang.1_0',
              'WARNING_CFLAGS': [
                '-Wheader-hygiene',
                # Don't die on dtoa code that uses a char as an array index.
                # This is required solely for base/third_party/dmg_fp/dtoa.cc.
                '-Wno-char-subscripts',
                # Clang spots more unused functions.
                '-Wno-unused-function',
                # See comments on this flag higher up in this file.
                '-Wno-unnamed-type-template-args',
                # WebKit uses nullptr in a legit way, other that that this
                # warning doesn't fire.
                '-Wno-c++0x-compat',
                # This (rightyfully) complains about 'override', which we use
                # heavily.
                '-Wno-c++11-extensions',

                # Warns on switches on enums that cover all enum values but
                # also contain a default: branch. Chrome is full of that.
                '-Wno-covered-switch-default',

                # TODO(thakis): Reenable once this no longer complains about
                # Invalid() in gmock's gmock-internal-utils.h
                # http://crbug.com/111806
                '-Wno-null-dereference',
              ],
            }],
            ['clang==1 and clang_use_chrome_plugins==1', {
              'OTHER_CFLAGS': [
                '<(clang_chrome_plugins_flags)',
              ],
            }],
            ['clang==1 and clang_load!=""', {
              'OTHER_CFLAGS': [
                '-Xclang', '-load', '-Xclang', '<(clang_load)',
              ],
            }],
            ['clang==1 and clang_add_plugin!=""', {
              'OTHER_CFLAGS': [
                '-Xclang', '-add-plugin', '-Xclang', '<(clang_add_plugin)',
              ],
            }],
            ['clang==1 and "<(GENERATOR)"=="ninja"', {
              'OTHER_CFLAGS': [
                # See http://crbug.com/110262
                '-fcolor-diagnostics',
              ],
            }],
          ],
        },
        'conditions': [
          ['clang==1', {
            'variables': {
              'clang_dir': '../third_party/llvm-build/Release+Asserts/bin',
            },
          }],
          ['asan==1', {
            'xcode_settings': {
              'OTHER_CFLAGS': [
                '-faddress-sanitizer',
                '-w',
              ],
              'OTHER_LDFLAGS': [
                '-faddress-sanitizer',
                # The symbols below are referenced in the ASan runtime
                # library (compiled on OS X 10.6), but may be unavailable
                # on the prior OS X versions. Because Chromium is currently
                # targeting 10.5.0, we need to explicitly mark these
                # symbols as dynamic_lookup.
                '-Wl,-U,_malloc_default_purgeable_zone',
                '-Wl,-U,_malloc_zone_memalign',
                '-Wl,-U,_dispatch_sync_f',
                '-Wl,-U,_dispatch_async_f',
                '-Wl,-U,_dispatch_barrier_async_f',
                '-Wl,-U,_dispatch_group_async_f',
                '-Wl,-U,_dispatch_after_f',
              ],
            },
            'defines': [
              'ADDRESS_SANITIZER',
            ],
          }],
        ],
        'target_conditions': [
          ['_type!="static_library"', {
            'xcode_settings': {'OTHER_LDFLAGS': ['-Wl,-search_paths_first']},
          }],
          ['_mac_bundle', {
            'xcode_settings': {'OTHER_LDFLAGS': ['-Wl,-ObjC']},
          }],
          ['_type=="executable"', {
            'postbuilds': [
              {
                # Arranges for data (heap) pages to be protected against
                # code execution when running on Mac OS X 10.7 ("Lion"), and
                # ensures that the position-independent executable (PIE) bit
                # is set for ASLR when running on Mac OS X 10.5 ("Leopard").
                'variables': {
                  # Define change_mach_o_flags in a variable ending in _path
                  # so that GYP understands it's a path and performs proper
                  # relativization during dict merging.
                  'change_mach_o_flags':
                      'mac/change_mach_o_flags_from_xcode.sh',
                  'change_mach_o_flags_options%': [
                  ],
                  'target_conditions': [
                    ['mac_pie==0 or release_valgrind_build==1', {
                      # Don't enable PIE if it's unwanted. It's unwanted if
                      # the target specifies mac_pie=0 or if building for
                      # Valgrind, because Valgrind doesn't understand slide.
                      # See the similar mac_pie/release_valgrind_build check
                      # below.
                      'change_mach_o_flags_options': [
                        '--no-pie',
                      ],
                    }],
                  ],
                },
                'postbuild_name': 'Change Mach-O Flags',
                'action': [
                   '$(srcdir)$(os_sep)build$(os_sep)<(change_mach_o_flags)',
                  '>@(change_mach_o_flags_options)',
                ],
              },
            ],
            'conditions': [
              ['asan==1', {
                'variables': {
                 'asan_saves_file': 'asan.saves',
                },
                'xcode_settings': {
                  'CHROMIUM_STRIP_SAVE_FILE': '<(asan_saves_file)',
                },
              }],
            ],
            'target_conditions': [
              ['mac_pie==1 and release_valgrind_build==0', {
                # Turn on position-independence (ASLR) for executables. When
                # PIE is on for the Chrome executables, the framework will
                # also be subject to ASLR.
                # Don't do this when building for Valgrind, because Valgrind
                # doesn't understand slide. TODO: Make Valgrind on Mac OS X
                # understand slide, and get rid of the Valgrind check.
                'xcode_settings': {
                  'OTHER_LDFLAGS': [
                    '-Wl,-pie',  # Position-independent executable (MH_PIE)
                  ],
                },
              }],
            ],
          }],
          ['(_type=="executable" or _type=="shared_library" or \
             _type=="loadable_module") and mac_strip!=0', {
            'target_conditions': [
              ['mac_real_dsym == 1', {
                # To get a real .dSYM bundle produced by dsymutil, set the
                # debug information format to dwarf-with-dsym.  Since
                # strip_from_xcode will not be used, set Xcode to do the
                # stripping as well.
                'configurations': {
                  'Release_Base': {
                    'xcode_settings': {
                      'DEBUG_INFORMATION_FORMAT': 'dwarf-with-dsym',
                      'DEPLOYMENT_POSTPROCESSING': 'YES',
                      'STRIP_INSTALLED_PRODUCT': 'YES',
                      'target_conditions': [
                        ['_type=="shared_library" or _type=="loadable_module"', {
                          # The Xcode default is to strip debugging symbols
                          # only (-S).  Local symbols should be stripped as
                          # well, which will be handled by -x.  Xcode will
                          # continue to insert -S when stripping even when
                          # additional flags are added with STRIPFLAGS.
                          'STRIPFLAGS': '-x',
                        }],  # _type=="shared_library" or _type=="loadable_module"'
                      ],  # target_conditions
                    },  # xcode_settings
                  },  # configuration "Release"
                },  # configurations
              }, {  # mac_real_dsym != 1
                # To get a fast fake .dSYM bundle, use a post-build step to
                # produce the .dSYM and strip the executable.  strip_from_xcode
                # only operates in the Release configuration.
                'postbuilds': [
                  {
                    'variables': {
                      # Define strip_from_xcode in a variable ending in _path
                      # so that gyp understands it's a path and performs proper
                      # relativization during dict merging.
                      'strip_from_xcode': 'mac/strip_from_xcode',
                    },
                    'postbuild_name': 'Strip If Needed',
                    'action': ['$(srcdir)$(os_sep)build$(os_sep)<(strip_from_xcode)'],
                  },
                ],  # postbuilds
              }],  # mac_real_dsym
            ],  # target_conditions
          }],  # (_type=="executable" or _type=="shared_library" or
               #  _type=="loadable_module") and mac_strip!=0
        ],  # target_conditions
      },  # target_defaults
    }],  # OS=="mac"
    ['OS=="win"', {
      'target_defaults': {
        'defines': [
          '_WIN32_WINNT=0x0601',
          'WINVER=0x0601',
          'WIN32',
          '_WINDOWS',
          'NOMINMAX',
          'PSAPI_VERSION=1',
          '_CRT_RAND_S',
          'CERT_CHAIN_PARA_HAS_EXTRA_FIELDS',
          'WIN32_LEAN_AND_MEAN',
          '_ATL_NO_OPENGL',
          '_HAS_TR1=0',
        ],
        'conditions': [
          ['buildtype=="Official"', {
              # In official builds, targets can self-select an optimization
              # level by defining a variable named 'optimize', and setting it
              # to one of
              # - "size", optimizes for minimal code size - the default.
              # - "speed", optimizes for speed over code size.
              # - "max", whole program optimization and link-time code
              #   generation. This is very expensive and should be used
              #   sparingly.
              'variables': {
                'optimize%': 'size',
              },
              'target_conditions': [
                ['optimize=="size"', {
                    'msvs_settings': {
                      'VCCLCompilerTool': {
                        # 1, optimizeMinSpace, Minimize Size (/O1)
                        'Optimization': '1',
                        # 2, favorSize - Favor small code (/Os)
                        'FavorSizeOrSpeed': '2',
                      },
                    },
                  },
                ],
                ['optimize=="speed"', {
                    'msvs_settings': {
                      'VCCLCompilerTool': {
                        # 2, optimizeMaxSpeed, Maximize Speed (/O2)
                        'Optimization': '2',
                        # 1, favorSpeed - Favor fast code (/Ot)
                        'FavorSizeOrSpeed': '1',
                      },
                    },
                  },
                ],
                ['optimize=="max"', {
                    'msvs_settings': {
                      'VCCLCompilerTool': {
                        # 2, optimizeMaxSpeed, Maximize Speed (/O2)
                        'Optimization': '2',
                        # 1, favorSpeed - Favor fast code (/Ot)
                        'FavorSizeOrSpeed': '1',
                        # This implies link time code generation.
                        'WholeProgramOptimization': 'true',
                      },
                    },
                  },
                ],
              ],
            },
          ],
          ['component=="static_library"', {
            'defines': [
              '_HAS_EXCEPTIONS=0',
            ],
          }],
          ['secure_atl', {
            'defines': [
              '_SECURE_ATL',
            ],
          }],
        ],
        'msvs_system_include_dirs': [
          '<(DEPTH)/third_party/directxsdk/files/Include',
          '<(DEPTH)/third_party/platformsdk_win7/files/Include',
          '$(VSInstallDir)/VC/atlmfc/include',
        ],
        'msvs_cygwin_dirs': ['<(DEPTH)/third_party/cygwin'],
        'msvs_disabled_warnings': [4351, 4396, 4503, 4819,
          # TODO(maruel): These warnings are level 4. They will be slowly
          # removed as code is fixed.
          4100, 4121, 4125, 4127, 4130, 4131, 4189, 4201, 4238, 4244, 4245,
          4310, 4355, 4428, 4481, 4505, 4510, 4512, 4530, 4610, 4611, 4701,
          4702, 4706,
        ],
        'msvs_settings': {
          'VCCLCompilerTool': {
            'MinimalRebuild': 'false',
            'BufferSecurityCheck': 'true',
            'EnableFunctionLevelLinking': 'true',
            'RuntimeTypeInfo': 'false',
            'WarningLevel': '4',
            'WarnAsError': 'true',
            'DebugInformationFormat': '3',
            'conditions': [
              ['msvs_multi_core_compile', {
                'AdditionalOptions': ['/MP'],
              }],
              ['MSVS_VERSION=="2005e"', {
                'AdditionalOptions': ['/w44068'], # Unknown pragma to 4 (ATL)
              }],
              ['component=="shared_library"', {
                'ExceptionHandling': '1',  # /EHsc
              }, {
                'ExceptionHandling': '0',
              }],
            ],
          },
          'VCLibrarianTool': {
            'AdditionalOptions': ['/ignore:4221'],
            'AdditionalLibraryDirectories': [
              '<(DEPTH)/third_party/directxsdk/files/Lib/x86',
              '<(DEPTH)/third_party/platformsdk_win7/files/Lib',
            ],
          },
          'VCLinkerTool': {
            'AdditionalDependencies': [
              'wininet.lib',
              'dnsapi.lib',
              'version.lib',
              'msimg32.lib',
              'ws2_32.lib',
              'usp10.lib',
              'psapi.lib',
              'dbghelp.lib',
              'winmm.lib',
              'shlwapi.lib',
            ],
            'conditions': [
              ['msvs_express', {
                # Explicitly required when using the ATL with express
                'AdditionalDependencies': [
                  'atlthunk.lib',
                ],
              }],
              ['MSVS_VERSION=="2005e"', {
                # Non-express versions link automatically to these
                'AdditionalDependencies': [
                  'advapi32.lib',
                  'comdlg32.lib',
                  'ole32.lib',
                  'shell32.lib',
                  'user32.lib',
                  'winspool.lib',
                ],
              }],
            ],
            'AdditionalLibraryDirectories': [
              '<(DEPTH)/third_party/directxsdk/files/Lib/x86',
              '<(DEPTH)/third_party/platformsdk_win7/files/Lib',
            ],
            'GenerateDebugInformation': 'true',
            'MapFileName': '$(OutDir)\\$(TargetName).map',
            'ImportLibrary': '$(OutDir)\\lib\\$(TargetName).lib',
            'FixedBaseAddress': '1',
            # SubSystem values:
            #   0 == not set
            #   1 == /SUBSYSTEM:CONSOLE
            #   2 == /SUBSYSTEM:WINDOWS
            # Most of the executables we'll ever create are tests
            # and utilities with console output.
            'SubSystem': '1',
          },
          'VCMIDLTool': {
            'GenerateStublessProxies': 'true',
            'TypeLibraryName': '$(InputName).tlb',
            'OutputDirectory': '$(IntDir)',
            'HeaderFileName': '$(InputName).h',
            'DLLDataFileName': 'dlldata.c',
            'InterfaceIdentifierFileName': '$(InputName)_i.c',
            'ProxyFileName': '$(InputName)_p.c',
          },
          'VCResourceCompilerTool': {
            'Culture' : '1033',
            'AdditionalIncludeDirectories': [
              '<(DEPTH)',
              '<(SHARED_INTERMEDIATE_DIR)',
            ],
          },
        },
      },
    }],
    ['disable_nacl==1', {
      'target_defaults': {
        'defines': [
          'DISABLE_NACL',
        ],
      },
    }],
    ['OS=="win" and msvs_use_common_linker_extras', {
      'target_defaults': {
        'msvs_settings': {
          'VCLinkerTool': {
            'DelayLoadDLLs': [
              'dbghelp.dll',
              'dwmapi.dll',
              'shell32.dll',
              'uxtheme.dll',
            ],
          },
        },
        'configurations': {
          'x86_Base': {
            'msvs_settings': {
              'VCLinkerTool': {
                'AdditionalOptions': [
                  '/safeseh',
                  '/dynamicbase',
                  '/ignore:4199',
                  '/ignore:4221',
                  '/nxcompat',
                ],
              },
            },
          },
          'x64_Base': {
            'msvs_settings': {
              'VCLinkerTool': {
                'AdditionalOptions': [
                  # safeseh is not compatible with x64
                  '/dynamicbase',
                  '/ignore:4199',
                  '/ignore:4221',
                  '/nxcompat',
                ],
              },
            },
          },
        },
      },
    }],
    ['enable_new_npdevice_api==1', {
      'target_defaults': {
        'defines': [
          'ENABLE_NEW_NPDEVICE_API',
        ],
      },
    }],
    ['clang==1', {
      'make_global_settings': [
        ['CC', '<(make_clang_dir)/bin/clang'],
        ['CXX', '<(make_clang_dir)/bin/clang++'],
        ['LINK', '$(CXX)'],
        ['CC.host', '$(CC)'],
        ['CXX.host', '$(CXX)'],
        ['LINK.host', '$(LINK)'],
      ],
    }],
  ],
  'xcode_settings': {
    # DON'T ADD ANYTHING NEW TO THIS BLOCK UNLESS YOU REALLY REALLY NEED IT!
    # This block adds *project-wide* configuration settings to each project
    # file.  It's almost always wrong to put things here.  Specify your
    # custom xcode_settings in target_defaults to add them to targets instead.

    # In an Xcode Project Info window, the "Base SDK for All Configurations"
    # setting sets the SDK on a project-wide basis.  In order to get the
    # configured SDK to show properly in the Xcode UI, SDKROOT must be set
    # here at the project level.
    'SDKROOT': 'macosx<(mac_sdk)',  # -isysroot

    # The Xcode generator will look for an xcode_settings section at the root
    # of each dict and use it to apply settings on a file-wide basis.  Most
    # settings should not be here, they should be in target-specific
    # xcode_settings sections, or better yet, should use non-Xcode-specific
    # settings in target dicts.  SYMROOT is a special case, because many other
    # Xcode variables depend on it, including variables such as
    # PROJECT_DERIVED_FILE_DIR.  When a source group corresponding to something
    # like PROJECT_DERIVED_FILE_DIR is added to a project, in order for the
    # files to appear (when present) in the UI as actual files and not red
    # red "missing file" proxies, the correct path to PROJECT_DERIVED_FILE_DIR,
    # and therefore SYMROOT, needs to be set at the project level.
    'SYMROOT': '<(DEPTH)/xcodebuild',
  },
}
