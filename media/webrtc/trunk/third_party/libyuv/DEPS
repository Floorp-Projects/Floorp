vars = {
  "libyuv_trunk" : "https://libyuv.googlecode.com/svn/trunk",
  "chromium_trunk" : "http://src.chromium.org/svn/trunk",
  "chromium_revision": "120526",
  # Use this googlecode_url variable only if there is an internal mirror for it.
  # If you do not know, use the full path while defining your new deps entry.
  "googlecode_url": "http://%s.googlecode.com/svn",
}

deps = {
  "trunk/build":
    Var("chromium_trunk") + "/src/build@" + Var("chromium_revision"),

  "trunk/testing":
    Var("chromium_trunk") + "/src/testing@" + Var("chromium_revision"),

  "trunk/testing/gtest":
    (Var("googlecode_url") % "googletest") + "/trunk@573",

  "trunk/tools/gyp":
    (Var("googlecode_url") % "gyp") + "/trunk@985",

  # Needed by build/common.gypi.
  "trunk/tools/win/supalink":
    Var("chromium_trunk") + "/src/tools/win/supalink@" + Var("chromium_revision"),

  # Dependencies used by libjpeg-turbo
  # Optional jpeg decoder
  "trunk/third_party/libjpeg_turbo/":
    Var("chromium_trunk") + "/deps/third_party/libjpeg_turbo@149334",

  # Yasm assember required for libjpeg_turbo
  "trunk/third_party/yasm/":
    Var("chromium_trunk") + "/src/third_party/yasm@" + Var("chromium_revision"),

  # TODO(fbarchard): Review yasm dependency
  "trunk/third_party/yasm/source/patched-yasm":
   Var("chromium_trunk") + "/deps/third_party/yasm/patched-yasm@73761",

  "trunk/third_party/yasm/binaries":
   Var("chromium_trunk") + "/deps/third_party/yasm/binaries@74228",
}

deps_os = {
  "win": {
    "trunk/third_party/cygwin/":
      Var("chromium_trunk") + "/deps/third_party/cygwin@66844",
  }
}

hooks = [
  # A change to a .gyp, .gypi, or to GYP itself should run the generator.
  {
    "pattern": ".",
    "action": ["python", "trunk/build/gyp_chromium", "--depth=trunk", "trunk/libyuv_test.gyp"],
  },
]
