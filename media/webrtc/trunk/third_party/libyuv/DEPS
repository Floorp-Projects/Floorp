vars = {
  "libyuv_trunk" : "https://libyuv.googlecode.com/svn/trunk",
  "chromium_trunk" : "http://src.chromium.org/svn/trunk",
  "chromium_revision": "95033",
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

  "trunk/third_party/yasm/":
    Var("chromium_trunk") + "/src/third_party/yasm@" + Var("chromium_revision"),
}


hooks = [
  # A change to a .gyp, .gypi, or to GYP itself should run the generator.
  {
    "pattern": ".",
    "action": ["python", "trunk/build/gyp_chromium", "--depth=trunk", "trunk/libyuv_test.gyp"],
  },
]
