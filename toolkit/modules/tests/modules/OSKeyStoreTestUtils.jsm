/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var EXPORTED_SYMBOLS = ["OSKeyStoreTestUtils"];

ChromeUtils.import("resource://gre/modules/OSKeyStore.jsm", this);
const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "UpdateUtils",
  "resource://gre/modules/UpdateUtils.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { TestUtils } = ChromeUtils.import(
  "resource://testing-common/TestUtils.jsm"
);

var OSKeyStoreTestUtils = {
  TEST_ONLY_REAUTH: "toolkit.osKeyStore.unofficialBuildOnlyLogin",

  setup() {
    this.ORIGINAL_STORE_LABEL = OSKeyStore.STORE_LABEL;
    OSKeyStore.STORE_LABEL =
      "test-" +
      Math.random()
        .toString(36)
        .substr(2);
  },

  async cleanup() {
    await OSKeyStore.cleanup();
    OSKeyStore.STORE_LABEL = this.ORIGINAL_STORE_LABEL;
  },

  /**
   * Checks whether or not the test can be run by bypassing
   * the OS login dialog. We do not want the user to be able to
   * do so with in official builds.
   * @returns {boolean} True if the test can be preformed.
   */
  canTestOSKeyStoreLogin() {
    // Skip on Linux due to bug 1527745.
    return (
      UpdateUtils.getUpdateChannel(false) == "default" &&
      AppConstants.platform != "linux"
    );
  },

  // Wait for the observer message that simulates login success of failure.
  async waitForOSKeyStoreLogin(login = false) {
    const str = login ? "pass" : "cancel";

    let prevValue = Services.prefs.getStringPref(this.TEST_ONLY_REAUTH, "");
    Services.prefs.setStringPref(this.TEST_ONLY_REAUTH, str);

    await TestUtils.topicObserved(
      "oskeystore-testonly-reauth",
      (subject, data) => data == str
    );

    Services.prefs.setStringPref(this.TEST_ONLY_REAUTH, prevValue);
  },
};
