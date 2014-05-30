use_relative_paths = True

vars = {
  # Override root_dir in your .gclient's custom_vars to specify a custom root
  # folder name.
  "root_dir": "trunk",
  "extra_gyp_flag": "-Dextra_gyp_flag=0",

  # Use this googlecode_url variable only if there is an internal mirror for it.
  # If you do not know, use the full path while defining your new deps entry.
  "googlecode_url": "http://%s.googlecode.com/svn",
  "sourceforge_url": "http://svn.code.sf.net/p/%(repo)s/code",
  "chromium_trunk" : "http://src.chromium.org/svn/trunk",
  "chromium_revision": "249215",

  # A small subset of WebKit is needed for the Android Python test framework.
  "webkit_trunk": "http://src.chromium.org/blink/trunk",
}

# NOTE: Prefer revision numbers to tags for svn deps. Use http rather than
# https; the latter can cause problems for users behind proxies.
deps = {
  "../chromium_deps":
    File(Var("chromium_trunk") + "/src/DEPS@" + Var("chromium_revision")),

  "../chromium_gn":
    File(Var("chromium_trunk") + "/src/.gn@" + Var("chromium_revision")),

  "build":
    Var("chromium_trunk") + "/src/build@" + Var("chromium_revision"),

  # Needed by common.gypi.
  "google_apis/build":
    Var("chromium_trunk") + "/src/google_apis/build@" + Var("chromium_revision"),

  "testing":
    Var("chromium_trunk") + "/src/testing@" + Var("chromium_revision"),

  "testing/gmock":
    From("chromium_deps", "src/testing/gmock"),

  "testing/gtest":
    From("chromium_deps", "src/testing/gtest"),

  "third_party/clang_format":
    Var("chromium_trunk") + "/src/third_party/clang_format@" + Var("chromium_revision"),

  "third_party/clang_format/script":
    From("chromium_deps", "src/third_party/clang_format/script"),

  "third_party/expat":
    Var("chromium_trunk") + "/src/third_party/expat@" + Var("chromium_revision"),

  # When rolling gflags, also update deps/third_party/webrtc/webrtc.DEPS/DEPS
  # in Chromium's repo.
  "third_party/gflags/src":
    (Var("googlecode_url") % "gflags") + "/trunk/src@84",

  "third_party/icu/":
    From("chromium_deps", "src/third_party/icu"),

  "third_party/jsoncpp/":
    Var("chromium_trunk") + "/src/third_party/jsoncpp@" + Var("chromium_revision"),

  "third_party/jsoncpp/source":
    (Var("sourceforge_url") % {"repo": "jsoncpp"}) + "/trunk/jsoncpp@248",

  "third_party/junit/":
    (Var("googlecode_url") % "webrtc") + "/deps/third_party/junit@3367",

  "third_party/libjpeg":
    Var("chromium_trunk") + "/src/third_party/libjpeg@" + Var("chromium_revision"),

  "third_party/libjpeg_turbo":
    From("chromium_deps", "src/third_party/libjpeg_turbo"),

  "third_party/libsrtp/":
    From("chromium_deps", "src/third_party/libsrtp"),

  "third_party/libvpx":
    Var("chromium_trunk") + "/deps/third_party/libvpx@248011",

  "third_party/libyuv":
    (Var("googlecode_url") % "libyuv") + "/trunk@979",

  "third_party/opus":
    Var("chromium_trunk") + "/src/third_party/opus@245176",

  "third_party/opus/src":
    Var("chromium_trunk") + "/deps/third_party/opus@239448",

  "third_party/protobuf":
    Var("chromium_trunk") + "/src/third_party/protobuf@" + Var("chromium_revision"),

  "third_party/sqlite/":
    Var("chromium_trunk") + "/src/third_party/sqlite@" + Var("chromium_revision"),

  "third_party/yasm":
    Var("chromium_trunk") + "/src/third_party/yasm@" + Var("chromium_revision"),

  "third_party/yasm/source/patched-yasm":
    From("chromium_deps", "src/third_party/yasm/source/patched-yasm"),

  "tools/clang":
    Var("chromium_trunk") + "/src/tools/clang@" + Var("chromium_revision"),

  "tools/gn":
    Var("chromium_trunk") + "/src/tools/gn@" + Var("chromium_revision"),

  "tools/gyp":
    From("chromium_deps", "src/tools/gyp"),

  "tools/protoc_wrapper":
    Var("chromium_trunk") + "/src/tools/protoc_wrapper@" + Var("chromium_revision"),

  "tools/python":
    Var("chromium_trunk") + "/src/tools/python@" + Var("chromium_revision"),

  "tools/swarming_client":
    From("chromium_deps", "src/tools/swarming_client"),

  "tools/valgrind":
    Var("chromium_trunk") + "/src/tools/valgrind@" + Var("chromium_revision"),

  # Needed by build/common.gypi.
  "tools/win/supalink":
    Var("chromium_trunk") + "/src/tools/win/supalink@" + Var("chromium_revision"),

  "net/third_party/nss":
      Var("chromium_trunk") + "/src/net/third_party/nss@" + Var("chromium_revision"),

  "third_party/usrsctp/":
    Var("chromium_trunk") + "/src/third_party/usrsctp@" + Var("chromium_revision"),

  "third_party/usrsctp/usrsctplib":
    (Var("googlecode_url") % "sctp-refimpl") + "/trunk/KERN/usrsctp/usrsctplib@8723",
}

deps_os = {
  "win": {
    "third_party/winsdk_samples/src":
      (Var("googlecode_url") % "webrtc") + "/deps/third_party/winsdk_samples_v71@3145",

    # Used by libjpeg-turbo.
    "third_party/yasm/binaries":
      From("chromium_deps", "src/third_party/yasm/binaries"),

    # NSS, for SSLClientSocketNSS.
    "third_party/nss":
      From("chromium_deps", "src/third_party/nss"),

    # SyzyASan to make it possible to run tests under ASan on Windows.
    "third_party/syzygy/binaries":
      From("chromium_deps", "src/third_party/syzygy/binaries"),
  },

  "mac": {
    # NSS, for SSLClientSocketNSS.
    "third_party/nss":
      From("chromium_deps", "src/third_party/nss"),
  },

  "ios": {
    # NSS, for SSLClientSocketNSS.
    "third_party/nss":
      From("chromium_deps", "src/third_party/nss"),

    # class-dump utility to generate header files for undocumented SDKs.
    "testing/iossim/third_party/class-dump":
      From("chromium_deps", "src/testing/iossim/third_party/class-dump"),

    # Helper for running under the simulator.
    "testing/iossim":
      Var("chromium_trunk") + "/src/testing/iossim@" + Var("chromium_revision"),
  },

  "unix": {
    "third_party/gold":
      From("chromium_deps", "src/third_party/gold"),
  },

  "android": {
    # Precompiled tools needed for Android test execution. Needed since we can't
    # compile them from source in WebRTC since they depend on Chromium's base.
    "tools/android":
      (Var("googlecode_url") % "webrtc") + "/deps/tools/android@4258",

    "third_party/android_tools":
      From("chromium_deps", "src/third_party/android_tools"),

    "third_party/android_testrunner":
      Var("chromium_trunk") + "/src/third_party/android_testrunner@" + Var("chromium_revision"),

    "third_party/WebKit/Tools/Scripts":
      Var("webkit_trunk") + "/Tools/Scripts@151677",

    "third_party/openssl":
      From("chromium_deps", "src/third_party/openssl"),
  },
}

hooks = [
  {
    # Copy .gn from temporary place (../chromium_gn) to root_dir.
    "name": "copy .gn",
    "pattern": ".",
    "action": ["python", Var("root_dir") + "/build/cp.py",
               Var("root_dir") + "/../chromium_gn/.gn",
               Var("root_dir")],
  },
  # Pull GN binaries. This needs to be before running GYP below.
  {
    "name": "gn_win",
    "pattern": "tools/gn/bin/win/gn.exe.sha1",
    "action": [ "download_from_google_storage",
                "--no_resume",
                "--platform=win32",
                "--no_auth",
                "--bucket", "chromium-gn",
                "-s", Var("root_dir") + "/tools/gn/bin/win/gn.exe.sha1",
    ],
  },
  {
    "name": "gn_mac",
    "pattern": "tools/gn/bin/mac/gn.sha1",
    "action": [ "download_from_google_storage",
                "--no_resume",
                "--platform=darwin",
                "--no_auth",
                "--bucket", "chromium-gn",
                "-s", Var("root_dir") + "/tools/gn/bin/mac/gn.sha1",
    ],
  },
  {
    "name": "gn_linux",
    "pattern": "tools/gn/bin/linux/gn.sha1",
    "action": [ "download_from_google_storage",
                "--no_resume",
                "--platform=linux*",
                "--no_auth",
                "--bucket", "chromium-gn",
                "-s", Var("root_dir") + "/tools/gn/bin/linux/gn.sha1",
    ],
  },
  {
    "name": "gn_linux32",
    "pattern": "tools/gn/bin/linux/gn32.sha1",
    "action": [ "download_from_google_storage",
                "--no_resume",
                "--platform=linux*",
                "--no_auth",
                "--bucket", "chromium-gn",
                "-s", Var("root_dir") + "/tools/gn/bin/linux/gn32.sha1",
    ],
  },
  # Pull clang-format binaries using checked-in hashes.
  {
    "name": "clang_format_win",
    "pattern": "third_party/clang_format/bin/win/clang-format.exe.sha1",
    "action": [ "download_from_google_storage",
                "--no_resume",
                "--platform=win32",
                "--no_auth",
                "--bucket", "chromium-clang-format",
                "-s", Var("root_dir") + "/third_party/clang_format/bin/win/clang-format.exe.sha1",
    ],
  },
  {
    "name": "clang_format_mac",
    "pattern": "third_party/clang_format/bin/mac/clang-format.sha1",
    "action": [ "download_from_google_storage",
                "--no_resume",
                "--platform=darwin",
                "--no_auth",
                "--bucket", "chromium-clang-format",
                "-s", Var("root_dir") + "/third_party/clang_format/bin/mac/clang-format.sha1",
    ],
  },
  {
    "name": "clang_format_linux",
    "pattern": "third_party/clang_format/bin/linux/clang-format.sha1",
    "action": [ "download_from_google_storage",
                "--no_resume",
                "--platform=linux*",
                "--no_auth",
                "--bucket", "chromium-clang-format",
                "-s", Var("root_dir") + "/third_party/clang_format/bin/linux/clang-format.sha1",
    ],
  },
  {
    # Pull clang on mac. If nothing changed, or on non-mac platforms, this takes
    # zero seconds to run. If something changed, it downloads a prebuilt clang.
    "pattern": ".",
    "action": ["python", Var("root_dir") + "/tools/clang/scripts/update.py",
               "--mac-only"],
  },
  {
    # Download test resources, i.e. video and audio files from Google Storage.
    "pattern": "\\.sha1",
    "action": ["download_from_google_storage",
               "--directory",
               "--recursive",
               "--num_threads=10",
               "--no_auth",
               "--bucket", "chromium-webrtc-resources",
               Var("root_dir") + "/resources"],
  },
  {
    # A change to a .gyp, .gypi, or to GYP itself should run the generator.
    "pattern": ".",
    "action": ["python", Var("root_dir") + "/webrtc/build/gyp_webrtc",
               Var("extra_gyp_flag")],
  },
]

