vars = {
  # Use this googlecode_url variable only if there is an internal mirror for it.
  # If you do not know, use the full path while defining your new deps entry.
  "googlecode_url": "http://%s.googlecode.com/svn",
  "chromium_trunk" : "http://src.chromium.org/svn/trunk",
  "chromium_revision": "162524",
  # Still needs the libjingle_revision here because some of
  # the deps have to be pulled from libjingle repository.
  "libjingle_revision": "204",
}

# NOTE: Prefer revision numbers to tags for svn deps. Use http rather than
# https; the latter can cause problems for users behind proxies.
deps = {
  "trunk/chromium_deps":
    File(Var("chromium_trunk") + "/src/DEPS@" + Var("chromium_revision")),

  "trunk/third_party/webrtc":
    From("trunk/chromium_deps", "src/third_party/webrtc"),

  # WebRTC deps.
  "trunk/third_party/libvpx":
    From("trunk/chromium_deps", "src/third_party/libvpx"),

  "trunk/third_party/opus/source":
    "http://git.opus-codec.org/opus.git",

  "trunk/build":
    Var("chromium_trunk") + "/src/build@" + Var("chromium_revision"),

  # Needed by common.gypi.
  "trunk/google_apis/build":
    Var("chromium_trunk") + "/src/google_apis/build@" + Var("chromium_revision"),

  "trunk/testing/gtest":
    From("trunk/chromium_deps", "src/testing/gtest"),

  "trunk/tools/gyp":
    From("trunk/chromium_deps", "src/tools/gyp"),

  "trunk/tools/clang":
    Var("chromium_trunk") + "/src/tools/clang@" + Var("chromium_revision"),

  # Needed by build/common.gypi.
  "trunk/tools/win/supalink":
    Var("chromium_trunk") + "/src/tools/win/supalink@" + Var("chromium_revision"),

  "trunk/third_party/protobuf":
    Var("chromium_trunk") + "/src/third_party/protobuf@" + Var("chromium_revision"),

  "trunk/third_party/libjpeg_turbo/":
    From("trunk/chromium_deps", "src/third_party/libjpeg_turbo"),

  "trunk/third_party/libjpeg":
    Var("chromium_trunk") + "/src/third_party/libjpeg@" + Var("chromium_revision"),

  "trunk/third_party/yasm":
    Var("chromium_trunk") + "/src/third_party/yasm@" + Var("chromium_revision"),

  "trunk/third_party/expat":
    Var("chromium_trunk") + "/src/third_party/expat@" + Var("chromium_revision"),

  "trunk/third_party/yasm/source/patched-yasm":
    From("trunk/chromium_deps", "src/third_party/yasm/source/patched-yasm"),

  "trunk/third_party/libyuv":
    From("trunk/chromium_deps", "src/third_party/libyuv"),

  # libjingle deps.
  "trunk/third_party/libjingle/":
    File(Var("chromium_trunk") + "/src/third_party/libjingle/libjingle.gyp@" + Var("chromium_revision")),

  "trunk/third_party/libjingle/source":
    From("trunk/chromium_deps", "src/third_party/libjingle/source"),

  "trunk/third_party/libjingle/overrides/talk/base":
    (Var("googlecode_url") % "libjingle") + "/trunk/talk/base@" + Var("libjingle_revision"),

  "trunk/third_party/libsrtp/":
    From("trunk/chromium_deps", "src/third_party/libsrtp"),

  "trunk/third_party/jsoncpp/":
    Var("chromium_trunk") + "/src/third_party/jsoncpp@" + Var("chromium_revision"),

  "trunk/third_party/jsoncpp/source":
    "http://jsoncpp.svn.sourceforge.net/svnroot/jsoncpp/trunk/jsoncpp@248",
}

deps_os = {
  "win": {
    "trunk/third_party/cygwin/":
      Var("chromium_trunk") + "/deps/third_party/cygwin@66844",

    # Used by libjpeg-turbo
    "trunk/third_party/yasm/binaries":
      From("trunk/chromium_deps", "src/third_party/yasm/binaries"),
  },
  "unix": {
    "trunk/third_party/gold":
      From("trunk/chromium_deps", "src/third_party/gold"),
  },
}

hooks = [
  {
    # Pull clang on mac. If nothing changed, or on non-mac platforms, this takes
    # zero seconds to run. If something changed, it downloads a prebuilt clang.
    "pattern": ".",
    "action": ["python", "trunk/tools/clang/scripts/update.py", "--mac-only"],
  },
  {
    # Update the cygwin mount on Windows.
    # This is necessary to get the correct mapping between e.g. /bin and the
    # cygwin path on Windows. Without it we can't run bash scripts in actions.
    # Ideally this should be solved in "pylib/gyp/msvs_emulation.py".
    "pattern": ".",
    "action": ["python", "trunk/build/win/setup_cygwin_mount.py",
               "--win-only"],
  },
  {
    # A change to a .gyp, .gypi, or to GYP itself should run the generator.
    "pattern": ".",
    "action": ["python", "trunk/build/gyp_chromium", "--depth=trunk", "trunk/peerconnection_all.gyp"],
  },
]

