vars = {
  # Override root_dir in your .gclient's custom_vars to specify a custom root
  # folder name.
  'root_dir': 'libyuv',
  'chromium_git': 'https://chromium.googlesource.com',
  'chromium_revision': '222a3fe7a738486a887bb53cffb8e3b52376f609',
  'swarming_revision': 'ebc8dab6f8b8d79ec221c94de39a921145abd404',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling lss
  # and whatever else without interference from each other.
  'lss_revision': '3f6478ac95edf86cd3da300c2c0d34a438f5dbeb',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling catapult
  # and whatever else without interference from each other.
  'catapult_revision': '4ee31ea3b497ffe08391e88a5434e0a340e48342',
}

deps = {
  Var('root_dir') + '/build':
    Var('chromium_git') + '/chromium/src/build' + '@' + '47e07d6798693fd71c02e25097c97865b5271c40',
  Var('root_dir') + '/buildtools':
    Var('chromium_git') + '/chromium/buildtools.git' + '@' + 'a7cc7a3e21a061975b33dcdcd81a9716ba614c3c',
  Var('root_dir') + '/testing':
    Var('chromium_git') + '/chromium/src/testing' + '@' + '178a302b13e943c679f3bbeb0a7e511f7c318404',
  Var('root_dir') + '/testing/gtest':
    Var('chromium_git') + '/external/github.com/google/googletest.git' + '@' + '6f8a66431cb592dad629028a50b3dd418a408c87',
  Var('root_dir') + '/testing/gmock':
    Var('chromium_git') + '/external/googlemock.git' + '@' + '0421b6f358139f02e102c9c332ce19a33faf75be', # from svn revision 566
  Var('root_dir') + '/third_party':
    Var('chromium_git') + '/chromium/src/third_party' + '@' + '4f196478f68c139a5deec388fd1f426a9251b4b0',
  Var('root_dir') + '/third_party/catapult':
   Var('chromium_git') + '/external/github.com/catapult-project/catapult.git' + '@' + Var('catapult_revision'),
  Var('root_dir') + '/third_party/colorama/src':
    Var('chromium_git') + '/external/colorama.git' + '@' + '799604a1041e9b3bc5d2789ecbd7e8db2e18e6b8',
  Var('root_dir') + '/third_party/libjpeg_turbo':
    Var('chromium_git') + '/chromium/deps/libjpeg_turbo.git' + '@' + '7260e4d8b8e1e40b17f03fafdf1cd83296900f76',
  Var('root_dir') + '/third_party/yasm/source/patched-yasm':
    Var('chromium_git') + '/chromium/deps/yasm/patched-yasm.git' + '@' + '7da28c6c7c6a1387217352ce02b31754deb54d2a',
  Var('root_dir') + '/tools':
    Var('chromium_git') + '/chromium/src/tools' + '@' + '54fd165044db88eca930ab9d20a6340b76136d91',
  Var('root_dir') + '/tools/gyp':
    Var('chromium_git') + '/external/gyp.git' + '@' + 'e7079f0e0e14108ab0dba58728ff219637458563',
   Var('root_dir') + '/tools/swarming_client':
     Var('chromium_git') + '/external/swarming.client.git' + '@' +  Var('swarming_revision'),

  # libyuv-only dependencies (not present in Chromium).
  Var('root_dir') + '/third_party/gflags':
    Var('chromium_git') + '/external/webrtc/deps/third_party/gflags' + '@' + '892576179b45861b53e04a112996a738309cf364',
  Var('root_dir') + '/third_party/gflags/src':
    Var('chromium_git') + '/external/github.com/gflags/gflags' + '@' + '03bebcb065c83beff83d50ae025a55a4bf94dfca',
  Var('root_dir') + '/third_party/gtest-parallel':
    Var('chromium_git') + '/external/webrtc/deps/third_party/gtest-parallel' + '@' + '8768563f5c580f8fc416a13c35c8f23b8a602821',
}

deps_os = {
  'android': {
    Var('root_dir') + '/base':
      Var('chromium_git') + '/chromium/src/base' + '@' + 'b9d4d9b0e5373bbdb5403c68d51e7385d78a09d0',
    Var('root_dir') + '/third_party/android_tools':
      Var('chromium_git') + '/android_tools.git' + '@' + 'b43a6a289a7588b1769814f04dd6c7d7176974cc',
    Var('root_dir') + '/third_party/ced/src':
      Var('chromium_git') + '/external/github.com/google/compact_enc_det.git' + '@' + '368a9cc09ad868a3d28f0b5ad4a733f263c46409',
    Var('root_dir') + '/third_party/icu':
      Var('chromium_git') + '/chromium/deps/icu.git' + '@' + '9cd2828740572ba6f694b9365236a8356fd06147',
    Var('root_dir') + '/third_party/jsr-305/src':
      Var('chromium_git') + '/external/jsr-305.git' + '@' + '642c508235471f7220af6d5df2d3210e3bfc0919',
    Var('root_dir') + '/third_party/junit/src':
      Var('chromium_git') + '/external/junit.git' + '@' + '64155f8a9babcfcf4263cf4d08253a1556e75481',
    Var('root_dir') + '/third_party/lss':
      Var('chromium_git') + '/linux-syscall-support.git' + '@' + Var('lss_revision'),
    Var('root_dir') + '/third_party/mockito/src':
      Var('chromium_git') + '/external/mockito/mockito.git' + '@' + 'de83ad4598ad4cf5ea53c69a8a8053780b04b850',
    Var('root_dir') + '/third_party/requests/src':
      Var('chromium_git') + '/external/github.com/kennethreitz/requests.git' + '@' + 'f172b30356d821d180fa4ecfa3e71c7274a32de4',
    Var('root_dir') + '/third_party/robolectric/robolectric':
      Var('chromium_git') + '/external/robolectric.git' + '@' + 'e38b49a12fdfa17a94f0382cc8ffaf69132fd09b',
  },
  'ios': {
    Var('root_dir') + '/ios':
      Var('chromium_git') + '/chromium/src/ios' + '@' + '291daef6af7764f8475089c65808d52ee50b496e',
  },
  'unix': {
    Var('root_dir') + '/third_party/lss':
      Var('chromium_git') + '/linux-syscall-support.git' + '@' + Var('lss_revision'),
  },
  'win': {
    # Dependencies used by libjpeg-turbo
    Var('root_dir') + '/third_party/yasm/binaries':
      Var('chromium_git') + '/chromium/deps/yasm/binaries.git' + '@' + '52f9b3f4b0aa06da24ef8b123058bb61ee468881',
  },
}

# Define rules for which include paths are allowed in our source.
include_rules = [ '+gflags' ]

pre_deps_hooks = [
  {
    # Remove any symlinks from before 177567c518b121731e507e9b9c4049c4dc96e4c8.
    # TODO(kjellander): Remove this in March 2017.
    'name': 'cleanup_links',
    'pattern': '.',
    'action': ['python', Var('root_dir') + '/cleanup_links.py'],
  },
]

hooks = [
  {
    # This clobbers when necessary (based on get_landmines.py). It should be
    # an early hook but it will need to be run after syncing Chromium and
    # setting up the links, so the script actually exists.
    'name': 'landmines',
    'pattern': '.',
    'action': [
        'python',
        Var('root_dir') + '/build/landmines.py',
        '--landmine-scripts',
        Var('root_dir') + '/tools_libyuv/get_landmines.py',
        '--src-dir',
        Var('root_dir') + '',
    ],
  },
  # Android dependencies. Many are downloaded using Google Storage these days.
  # They're copied from https://cs.chromium.org/chromium/src/DEPS for all
  # such dependencies we share with Chromium.
  {
    # This downloads SDK extras and puts them in the
    # third_party/android_tools/sdk/extras directory.
    'name': 'sdkextras',
    'pattern': '.',
    # When adding a new sdk extras package to download, add the package
    # directory and zip file to .gitignore in third_party/android_tools.
    'action': ['python',
               Var('root_dir') + '/build/android/play_services/update.py',
               'download'
    ],
  },
  {
    'name': 'intellij',
    'pattern': '.',
    'action': ['python',
               Var('root_dir') + '/build/android/update_deps/update_third_party_deps.py',
               'download',
               '-b', 'chromium-intellij',
               '-l', 'third_party/intellij'
    ],
  },
  {
    'name': 'javax_inject',
    'pattern': '.',
    'action': ['python',
               Var('root_dir') + '/build/android/update_deps/update_third_party_deps.py',
               'download',
               '-b', 'chromium-javax-inject',
               '-l', 'third_party/javax_inject'
    ],
  },
  {
    'name': 'hamcrest',
    'pattern': '.',
    'action': ['python',
               Var('root_dir') + '/build/android/update_deps/update_third_party_deps.py',
               'download',
               '-b', 'chromium-hamcrest',
               '-l', 'third_party/hamcrest'
    ],
  },
  {
    'name': 'guava',
    'pattern': '.',
    'action': ['python',
               Var('root_dir') + '/build/android/update_deps/update_third_party_deps.py',
               'download',
               '-b', 'chromium-guava',
               '-l', 'third_party/guava'
    ],
  },
  {
    'name': 'android_support_test_runner',
    'pattern': '.',
    'action': ['python',
               Var('root_dir') + '/build/android/update_deps/update_third_party_deps.py',
               'download',
               '-b', 'chromium-android-support-test-runner',
               '-l', 'third_party/android_support_test_runner'
    ],
  },
  {
    'name': 'byte_buddy',
    'pattern': '.',
    'action': ['python',
               Var('root_dir') + '/build/android/update_deps/update_third_party_deps.py',
               'download',
               '-b', 'chromium-byte-buddy',
               '-l', 'third_party/byte_buddy'
    ],
  },
  {
    'name': 'espresso',
    'pattern': '.',
    'action': ['python',
               Var('root_dir') + '/build/android/update_deps/update_third_party_deps.py',
               'download',
               '-b', 'chromium-espresso',
               '-l', 'third_party/espresso'
    ],
  },
  {
    'name': 'robolectric_libs',
    'pattern': '.',
    'action': ['python',
               Var('root_dir') + '/build/android/update_deps/update_third_party_deps.py',
               'download',
               '-b', 'chromium-robolectric',
               '-l', 'third_party/robolectric'
    ],
  },
  {
    'name': 'apache_velocity',
    'pattern': '.',
    'action': ['python',
               Var('root_dir') + '/build/android/update_deps/update_third_party_deps.py',
               'download',
               '-b', 'chromium-apache-velocity',
               '-l', 'third_party/apache_velocity'
    ],
  },
  {
    'name': 'ow2_asm',
    'pattern': '.',
    'action': ['python',
               Var('root_dir') + '/build/android/update_deps/update_third_party_deps.py',
               'download',
               '-b', 'chromium-ow2-asm',
               '-l', 'third_party/ow2_asm'
    ],
  },
  {
    'name': 'icu4j',
    'pattern': '.',
    'action': ['python',
               Var('root_dir') + '/build/android/update_deps/update_third_party_deps.py',
               'download',
               '-b', 'chromium-icu4j',
               '-l', 'third_party/icu4j'
    ],
  },
  {
    'name': 'accessibility_test_framework',
    'pattern': '.',
    'action': ['python',
               Var('root_dir') + '/build/android/update_deps/update_third_party_deps.py',
               'download',
               '-b', 'chromium-accessibility-test-framework',
               '-l', 'third_party/accessibility_test_framework'
    ],
  },
  {
    'name': 'bouncycastle',
    'pattern': '.',
    'action': ['python',
               Var('root_dir') + '/build/android/update_deps/update_third_party_deps.py',
               'download',
               '-b', 'chromium-bouncycastle',
               '-l', 'third_party/bouncycastle'
    ],
  },
  {
    'name': 'sqlite4java',
    'pattern': '.',
    'action': ['python',
               Var('root_dir') + '/build/android/update_deps/update_third_party_deps.py',
               'download',
               '-b', 'chromium-sqlite4java',
               '-l', 'third_party/sqlite4java'
    ],
  },
  {
    'name': 'objenesis',
    'pattern': '.',
    'action': ['python',
               Var('root_dir') + '/build/android/update_deps/update_third_party_deps.py',
               'download',
               '-b', 'chromium-objenesis',
               '-l', 'third_party/objenesis'
    ],
  },
  {
    # Downloads the current stable linux sysroot to build/linux/ if needed.
    # This sysroot updates at about the same rate that the chrome build deps
    # change. This script is a no-op except for linux users who are doing
    # official chrome builds or cross compiling.
    'name': 'sysroot',
    'pattern': '.',
    'action': ['python', Var('root_dir') + '/build/linux/sysroot_scripts/install-sysroot.py',
               '--running-as-hook'],
  },
  {
    # Update the Windows toolchain if necessary.
    'name': 'win_toolchain',
    'pattern': '.',
    'action': ['python', Var('root_dir') + '/build/vs_toolchain.py', 'update'],
  },
  # Pull binutils for linux, enabled debug fission for faster linking /
  # debugging when used with clang on Ubuntu Precise.
  # https://code.google.com/p/chromium/issues/detail?id=352046
  {
    'name': 'binutils',
    'pattern': Var('root_dir') + '/third_party/binutils',
    'action': [
        'python',
        Var('root_dir') + '/third_party/binutils/download.py',
    ],
  },
  {
    # Pull clang if needed or requested via GYP_DEFINES.
    # Note: On Win, this should run after win_toolchain, as it may use it.
    'name': 'clang',
    'pattern': '.',
    'action': ['python', Var('root_dir') + '/tools/clang/scripts/update.py', '--if-needed'],
  },
  {
    # Update LASTCHANGE.
    'name': 'lastchange',
    'pattern': '.',
    'action': ['python', Var('root_dir') + '/build/util/lastchange.py',
               '-o', Var('root_dir') + '/build/util/LASTCHANGE'],
  },
  # Pull GN binaries.
  {
    'name': 'gn_win',
    'pattern': '.',
    'action': [ 'download_from_google_storage',
                '--no_resume',
                '--platform=win32',
                '--no_auth',
                '--bucket', 'chromium-gn',
                '-s', Var('root_dir') + '/buildtools/win/gn.exe.sha1',
    ],
  },
  {
    'name': 'gn_mac',
    'pattern': '.',
    'action': [ 'download_from_google_storage',
                '--no_resume',
                '--platform=darwin',
                '--no_auth',
                '--bucket', 'chromium-gn',
                '-s', Var('root_dir') + '/buildtools/mac/gn.sha1',
    ],
  },
  {
    'name': 'gn_linux64',
    'pattern': '.',
    'action': [ 'download_from_google_storage',
                '--no_resume',
                '--platform=linux*',
                '--no_auth',
                '--bucket', 'chromium-gn',
                '-s', Var('root_dir') + '/buildtools/linux64/gn.sha1',
    ],
  },
  # Pull clang-format binaries using checked-in hashes.
  {
    'name': 'clang_format_win',
    'pattern': '.',
    'action': [ 'download_from_google_storage',
                '--no_resume',
                '--platform=win32',
                '--no_auth',
                '--bucket', 'chromium-clang-format',
                '-s', Var('root_dir') + '/buildtools/win/clang-format.exe.sha1',
    ],
  },
  {
    'name': 'clang_format_mac',
    'pattern': '.',
    'action': [ 'download_from_google_storage',
                '--no_resume',
                '--platform=darwin',
                '--no_auth',
                '--bucket', 'chromium-clang-format',
                '-s', Var('root_dir') + '/buildtools/mac/clang-format.sha1',
    ],
  },
  {
    'name': 'clang_format_linux',
    'pattern': '.',
    'action': [ 'download_from_google_storage',
                '--no_resume',
                '--platform=linux*',
                '--no_auth',
                '--bucket', 'chromium-clang-format',
                '-s', Var('root_dir') + '/buildtools/linux64/clang-format.sha1',
    ],
  },
  # Pull luci-go binaries (isolate, swarming) using checked-in hashes.
  {
    'name': 'luci-go_win',
    'pattern': '.',
    'action': [ 'download_from_google_storage',
                '--no_resume',
                '--platform=win32',
                '--no_auth',
                '--bucket', 'chromium-luci',
                '-d', Var('root_dir') + '/tools/luci-go/win64',
    ],
  },
  {
    'name': 'luci-go_mac',
    'pattern': '.',
    'action': [ 'download_from_google_storage',
                '--no_resume',
                '--platform=darwin',
                '--no_auth',
                '--bucket', 'chromium-luci',
                '-d', Var('root_dir') + '/tools/luci-go/mac64',
    ],
  },
  {
    'name': 'luci-go_linux',
    'pattern': '.',
    'action': [ 'download_from_google_storage',
                '--no_resume',
                '--platform=linux*',
                '--no_auth',
                '--bucket', 'chromium-luci',
                '-d', Var('root_dir') + '/tools/luci-go/linux64',
    ],
  },
  {
    # Pull sanitizer-instrumented third-party libraries if requested via
    # GYP_DEFINES.
    # See src/third_party/instrumented_libraries/scripts/download_binaries.py.
    # TODO(kjellander): Update comment when GYP is completely cleaned up.
    'name': 'instrumented_libraries',
    'pattern': '\\.sha1',
    'action': ['python', Var('root_dir') + '/third_party/instrumented_libraries/scripts/download_binaries.py'],
  },
  {
    'name': 'clang_format_merge_driver',
    'pattern': '.',
    'action': [ 'python',
                Var('root_dir') + '/tools/clang_format_merge_driver/install_git_hook.py',
    ],
  },
]

recursedeps = [
  # buildtools provides clang_format, libc++, and libc++abi.
  Var('root_dir') + '/buildtools',
  # android_tools manages the NDK.
  Var('root_dir') + '/third_party/android_tools',
]
