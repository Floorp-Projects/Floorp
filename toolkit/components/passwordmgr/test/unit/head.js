/**
 * Provides infrastructure for automated login components tests.
 */

"use strict";

// Globals

const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { LoginRecipesContent, LoginRecipesParent } = ChromeUtils.import(
  "resource://gre/modules/LoginRecipes.jsm"
);
const { LoginHelper } = ChromeUtils.import(
  "resource://gre/modules/LoginHelper.jsm"
);
const { FileTestUtils } = ChromeUtils.import(
  "resource://testing-common/FileTestUtils.jsm"
);
const { LoginTestUtils } = ChromeUtils.import(
  "resource://testing-common/LoginTestUtils.jsm"
);
const { MockDocument } = ChromeUtils.import(
  "resource://testing-common/MockDocument.jsm"
);

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(this, {
  DownloadPaths: "resource://gre/modules/DownloadPaths.jsm",
  FileUtils: "resource://gre/modules/FileUtils.jsm",
  OS: "resource://gre/modules/osfile.jsm",
});

const LoginInfo = Components.Constructor(
  "@mozilla.org/login-manager/loginInfo;1",
  "nsILoginInfo",
  "init"
);

const TestData = LoginTestUtils.testData;
const newPropertyBag = LoginHelper.newPropertyBag;

const NEW_PASSWORD_HEURISTIC_ENABLED_PREF =
  "signon.generation.confidenceThreshold";
const RELATED_REALMS_ENABLED_PREF = "signon.relatedRealms.enabled";
const IMPROVED_PASSWORD_RULES_PREF = "signon.improvedPasswordRules.enabled";
/**
 * All the tests are implemented with add_task, this starts them automatically.
 */
function run_test() {
  do_get_profile();
  run_next_test();
}

// Global helpers

/**
 * Returns a reference to a temporary file that is guaranteed not to exist and
 * is cleaned up later. See FileTestUtils.getTempFile for details.
 */
function getTempFile(leafName) {
  return FileTestUtils.getTempFile(leafName);
}

const RecipeHelpers = {
  initNewParent() {
    return new LoginRecipesParent({ defaults: null }).initializationPromise;
  },
};

// Initialization functions common to all tests

add_setup(async function test_common_initialize() {
  // Before initializing the service for the first time, we should copy the key
  // file required to decrypt the logins contained in the SQLite databases used
  // by migration tests.  This file is not required for the other tests.
  const keyDBName = "key4.db";
  await OS.File.copy(
    do_get_file(`data/${keyDBName}`).path,
    OS.Path.join(OS.Constants.Path.profileDir, keyDBName)
  );

  // Ensure that the service and the storage module are initialized.
  await Services.logins.initializationPromise;
  Services.prefs.setBoolPref(RELATED_REALMS_ENABLED_PREF, true);
  if (LoginHelper.relatedRealmsEnabled) {
    // Ensure that there is a mocked Remote Settings database for the
    // "websites-with-shared-credential-backends" collection
    await LoginTestUtils.remoteSettings.setupWebsitesWithSharedCredentials();
  }
});

add_setup(async function test_common_prefs() {
  Services.prefs.setStringPref(NEW_PASSWORD_HEURISTIC_ENABLED_PREF, "0.75");
});

/**
 * Compare two FormLike to see if they represent the same information. Elements
 * are compared using their @id attribute.
 */
function formLikeEqual(a, b) {
  Assert.strictEqual(
    Object.keys(a).length,
    Object.keys(b).length,
    "Check the formLikes have the same number of properties"
  );

  for (let propName of Object.keys(a)) {
    if (propName == "elements") {
      Assert.strictEqual(
        a.elements.length,
        b.elements.length,
        "Check element count"
      );
      for (let i = 0; i < a.elements.length; i++) {
        Assert.strictEqual(
          a.elements[i].id,
          b.elements[i].id,
          "Check element " + i + " id"
        );
      }
      continue;
    }
    Assert.strictEqual(
      a[propName],
      b[propName],
      "Compare formLike " + propName + " property"
    );
  }
}
