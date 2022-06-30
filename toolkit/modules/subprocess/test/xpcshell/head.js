"use strict";

var { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);

// eslint-disable-next-line no-unused-vars
XPCOMUtils.defineLazyModuleGetters(this, {
  Subprocess: "resource://gre/modules/Subprocess.jsm",
});
