"use strict";

/* exported createHttpServer */

var { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
var { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

// eslint-disable-next-line no-unused-vars
XPCOMUtils.defineLazyModuleGetters(this, {
  AddonTestUtils: "resource://testing-common/AddonTestUtils.jsm",
  ExtensionTestUtils: "resource://testing-common/ExtensionXPCShellUtils.jsm",
});

// Remove this pref once bug 1535365 is fixed.
Services.prefs.setBoolPref("extensions.webextensions.remote", false);

ExtensionTestUtils.init(this);

Services.io.offline = true;

var createHttpServer = (...args) => {
  AddonTestUtils.maybeInit(this);
  return AddonTestUtils.createHttpServer(...args);
};
