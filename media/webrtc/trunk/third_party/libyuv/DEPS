use_relative_paths = True

vars = {
  "libyuv_trunk" : "https://libyuv.googlecode.com/svn/trunk",

  # Override root_dir in your .gclient's custom_vars to specify a custom root
  # folder name.
  "root_dir": "trunk",
  "extra_gyp_flag": "-Dextra_gyp_flag=0",

  # Use this googlecode_url variable only if there is an internal mirror for it.
  # If you do not know, use the full path while defining your new deps entry.
  "googlecode_url": "http://%s.googlecode.com/svn",
  "chromium_trunk" : "http://src.chromium.org/svn/trunk",
  "chromium_revision": "152335",
}

# NOTE: Prefer revision numbers to tags for svn deps. Use http rather than
# https; the latter can cause problems for users behind proxies.
deps = {
  "../chromium_deps":
    File(Var("chromium_trunk") + "/src/DEPS@" + Var("chromium_revision")),

  "build":
    Var("chromium_trunk") + "/src/build@" + Var("chromium_revision"),

  "testing":
    Var("chromium_trunk") + "/src/testing@" + Var("chromium_revision"),

  "testing/gtest":
    From("chromium_deps", "src/testing/gtest"),

  "tools/clang":
    Var("chromium_trunk") + "/src/tools/clang@" + Var("chromium_revision"),

  "tools/gyp":
    From("chromium_deps", "src/tools/gyp"),

  "tools/python":
    Var("chromium_trunk") + "/src/tools/python@" + Var("chromium_revision"),

  "tools/valgrind":
    Var("chromium_trunk") + "/src/tools/valgrind@" + Var("chromium_revision"),

  # Needed by build/common.gypi.
  "tools/win/supalink":
    Var("chromium_trunk") + "/src/tools/win/supalink@" + Var("chromium_revision"),

  "third_party/libjpeg_turbo":
    From("chromium_deps", "src/third_party/libjpeg_turbo"),

  # Yasm assember required for libjpeg_turbo
  # TODO(fbarchard): Switch back to chromium version.
  "third_party/yasm":
    Var("chromium_trunk") + "/src/third_party/yasm@154708",

  "third_party/yasm/source/patched-yasm":
    Var("chromium_trunk") + "/deps/third_party/yasm/patched-yasm@154708",
}

deps_os = {
  "win": {
    # Use WebRTC's, stripped down, version of Cygwin (required by GYP).
    "third_party/cygwin":
      (Var("googlecode_url") % "webrtc") + "/deps/third_party/cygwin@2672",

    # Used by libjpeg-turbo.
    # TODO(fbarchard): Remove binaries and run yasm from build folder.
    "third_party/yasm/binaries":
      Var("chromium_trunk") + "/deps/third_party/yasm/binaries@154708",
    "third_party/yasm": None,
  },
  "unix": {
    "third_party/gold":
      From("chromium_deps", "src/third_party/gold"),
  },
}

hooks = [
  {
    # Pull clang on mac. If nothing changed, or on non-mac platforms, this takes
    # zero seconds to run. If something changed, it downloads a prebuilt clang.
    "pattern": ".",
    "action": ["python", Var("root_dir") + "/tools/clang/scripts/update.py",
               "--mac-only"],
  },
  {
    # A change to a .gyp, .gypi, or to GYP itself should run the generator.
    "pattern": ".",
    "action": ["python", Var("root_dir") + "/build/gyp_chromium",
               "--depth=" + Var("root_dir"), Var("root_dir") + "/libyuv_test.gyp",
               Var("extra_gyp_flag")],
  },
]
