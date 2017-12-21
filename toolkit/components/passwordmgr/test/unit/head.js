/**
 * Provides infrastructure for automated login components tests.
 */

"use strict";

// Globals

let { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/LoginRecipes.jsm");
Cu.import("resource://gre/modules/LoginHelper.jsm");
Cu.import("resource://testing-common/FileTestUtils.jsm");
Cu.import("resource://testing-common/LoginTestUtils.jsm");
Cu.import("resource://testing-common/MockDocument.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "DownloadPaths",
                                  "resource://gre/modules/DownloadPaths.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "FileUtils",
                                  "resource://gre/modules/FileUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "OS",
                                  "resource://gre/modules/osfile.jsm");

const LoginInfo =
      Components.Constructor("@mozilla.org/login-manager/loginInfo;1",
                             "nsILoginInfo", "init");

const TestData = LoginTestUtils.testData;
const newPropertyBag = LoginHelper.newPropertyBag;

/**
 * All the tests are implemented with add_task, this starts them automatically.
 */
function run_test()
{
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
    return (new LoginRecipesParent({ defaults: null })).initializationPromise;
  },
};

// Initialization functions common to all tests

add_task(async function test_common_initialize()
{
  // Before initializing the service for the first time, we should copy the key
  // file required to decrypt the logins contained in the SQLite databases used
  // by migration tests.  This file is not required for the other tests.
  await OS.File.copy(do_get_file("data/key3.db").path,
                     OS.Path.join(OS.Constants.Path.profileDir, "key3.db"));

  // Ensure that the service and the storage module are initialized.
  await Services.logins.initializationPromise;

  // Ensure that every test file starts with an empty database.
  LoginTestUtils.clearData();

  // Clean up after every test.
  registerCleanupFunction(() => LoginTestUtils.clearData());
});

/**
 * Compare two FormLike to see if they represent the same information. Elements
 * are compared using their @id attribute.
 */
function formLikeEqual(a, b) {
  Assert.strictEqual(Object.keys(a).length, Object.keys(b).length,
                     "Check the formLikes have the same number of properties");

  for (let propName of Object.keys(a)) {
    if (propName == "elements") {
      Assert.strictEqual(a.elements.length, b.elements.length, "Check element count");
      for (let i = 0; i < a.elements.length; i++) {
        Assert.strictEqual(a.elements[i].id, b.elements[i].id, "Check element " + i + " id");
      }
      continue;
    }
    Assert.strictEqual(a[propName], b[propName], "Compare formLike " + propName + " property");
  }
}
