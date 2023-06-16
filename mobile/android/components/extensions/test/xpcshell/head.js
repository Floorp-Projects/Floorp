"use strict";

ChromeUtils.defineESModuleGetters(this, {
  AddonTestUtils: "resource://testing-common/AddonTestUtils.sys.mjs",
  ExtensionTestUtils:
    "resource://testing-common/ExtensionXPCShellUtils.sys.mjs",
});

// Remove this pref once bug 1535365 is fixed.
Services.prefs.setBoolPref("extensions.webextensions.remote", false);

// https_first automatically upgrades http to https, but the tests are not
// designed to expect that. And it is not easy to change that because
// nsHttpServer does not support https (bug 1742061). So disable https_first.
Services.prefs.setBoolPref("dom.security.https_first", false);

ExtensionTestUtils.init(this);

Services.io.offline = true;

var createHttpServer = (...args) => {
  AddonTestUtils.maybeInit(this);
  return AddonTestUtils.createHttpServer(...args);
};
