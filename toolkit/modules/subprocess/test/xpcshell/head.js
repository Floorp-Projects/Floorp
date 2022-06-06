"use strict";

var { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

// eslint-disable-next-line no-unused-vars
XPCOMUtils.defineLazyModuleGetters(this, {
  AppConstants: "resource://gre/modules/AppConstants.jsm",
  OS: "resource://gre/modules/osfile.jsm",
  Services: "resource://gre/modules/Services.jsm",
  Subprocess: "resource://gre/modules/Subprocess.jsm",
});
