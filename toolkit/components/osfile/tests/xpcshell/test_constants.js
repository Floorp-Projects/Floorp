"use strict";

const { OS } = ChromeUtils.import("resource://gre/modules/osfile.jsm");
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm", this);

// Test that OS.Constants is defined correctly.
add_task(async function check_definition() {
  Assert.ok(OS.Constants != null);
  Assert.ok(!!OS.Constants.Win || !!OS.Constants.libc);
  Assert.ok(OS.Constants.Path != null);
  Assert.ok(OS.Constants.Sys != null);
  // check system name
  Assert.equal(Services.appinfo.OS, OS.Constants.Sys.Name);

  // check if using DEBUG build
  if (Cc["@mozilla.org/xpcom/debug;1"].getService(Ci.nsIDebug2).isDebugBuild) {
    Assert.ok(OS.Constants.Sys.DEBUG);
  } else {
    Assert.ok(typeof OS.Constants.Sys.DEBUG == "undefined");
  }
});
