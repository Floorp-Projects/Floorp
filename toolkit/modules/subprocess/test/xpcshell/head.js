"use strict";

var { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);
const { AppConstants } = ChromeUtils.importESModule(
  "resource://gre/modules/AppConstants.sys.mjs"
);

// eslint-disable-next-line no-unused-vars
XPCOMUtils.defineLazyModuleGetters(this, {
  Subprocess: "resource://gre/modules/Subprocess.jsm",
});
