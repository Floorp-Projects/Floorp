/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Provides infrastructure for automated login components tests.
 */

"use strict";

////////////////////////////////////////////////////////////////////////////////
//// Globals

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/LoginRecipes.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "DownloadPaths",
                                  "resource://gre/modules/DownloadPaths.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "FileUtils",
                                  "resource://gre/modules/FileUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "OS",
                                  "resource://gre/modules/osfile.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Promise",
                                  "resource://gre/modules/Promise.jsm");

const LoginInfo =
      Components.Constructor("@mozilla.org/login-manager/loginInfo;1",
                             "nsILoginInfo", "init");

// Import LoginTestUtils.jsm as LoginTestUtils.
XPCOMUtils.defineLazyModuleGetter(this, "LoginTestUtils",
                                  "resource://testing-common/LoginTestUtils.jsm");
LoginTestUtils.Assert = Assert;
const TestData = LoginTestUtils.testData;

/**
 * All the tests are implemented with add_task, this starts them automatically.
 */
function run_test()
{
  do_get_profile();
  run_next_test();
}

////////////////////////////////////////////////////////////////////////////////
//// Global helpers

// Some of these functions are already implemented in other parts of the source
// tree, see bug 946708 about sharing more code.

// While the previous test file should have deleted all the temporary files it
// used, on Windows these might still be pending deletion on the physical file
// system.  Thus, start from a new base number every time, to make a collision
// with a file that is still pending deletion highly unlikely.
let gFileCounter = Math.floor(Math.random() * 1000000);

/**
 * Returns a reference to a temporary file, that is guaranteed not to exist, and
 * to have never been created before.
 *
 * @param aLeafName
 *        Suggested leaf name for the file to be created.
 *
 * @return nsIFile pointing to a non-existent file in a temporary directory.
 *
 * @note It is not enough to delete the file if it exists, or to delete the file
 *       after calling nsIFile.createUnique, because on Windows the delete
 *       operation in the file system may still be pending, preventing a new
 *       file with the same name to be created.
 */
function getTempFile(aLeafName)
{
  // Prepend a serial number to the extension in the suggested leaf name.
  let [base, ext] = DownloadPaths.splitBaseNameAndExtension(aLeafName);
  let leafName = base + "-" + gFileCounter + ext;
  gFileCounter++;

  // Get a file reference under the temporary directory for this test file.
  let file = FileUtils.getFile("TmpD", [leafName]);
  do_check_false(file.exists());

  do_register_cleanup(function () {
    if (file.exists()) {
      file.remove(false);
    }
  });

  return file;
}

/**
 * Returns a new XPCOM property bag with the provided properties.
 *
 * @param aProperties
 *        Each property of this object is copied to the property bag.  This
 *        parameter can be omitted to return an empty property bag.
 *
 * @return A new property bag, that is an instance of nsIWritablePropertyBag,
 *         nsIWritablePropertyBag2, nsIPropertyBag, and nsIPropertyBag2.
 */
function newPropertyBag(aProperties)
{
  let propertyBag = Cc["@mozilla.org/hash-property-bag;1"]
                      .createInstance(Ci.nsIWritablePropertyBag);
  if (aProperties) {
    for (let [name, value] of Iterator(aProperties)) {
      propertyBag.setProperty(name, value);
    }
  }
  return propertyBag.QueryInterface(Ci.nsIPropertyBag)
                    .QueryInterface(Ci.nsIPropertyBag2)
                    .QueryInterface(Ci.nsIWritablePropertyBag2);
}

////////////////////////////////////////////////////////////////////////////////

const RecipeHelpers = {
  initNewParent() {
    return (new LoginRecipesParent({ defaults: false })).initializationPromise;
  },

  /**
   * Create a document for the given URL containing the given HTML with the ownerDocument of all <form>s having a mocked location.
   */
  createTestDocument(aDocumentURL, aHTML = "<form>") {
    let parser = Cc["@mozilla.org/xmlextras/domparser;1"].
                 createInstance(Ci.nsIDOMParser);
    parser.init();
    let parsedDoc = parser.parseFromString(aHTML, "text/html");

    // Mock the document.location object so we can unit test without a frame. We use a proxy
    // instead of just assigning to the property since it's not configurable or writable.
    let document = new Proxy(parsedDoc, {
      get(target, property, receiver) {
        // document.location is normally null when a document is outside of a "browsing context".
        // See https://html.spec.whatwg.org/#the-location-interface
        if (property == "location") {
          return new URL(aDocumentURL);
        }
        return target[property];
      },
    });

    for (let form of parsedDoc.forms) {
      // Assign form.ownerDocument to the proxy so document.location works.
      Object.defineProperty(form, "ownerDocument", {
        value: document,
      });
    }
    return parsedDoc;
  }
};

//// Initialization functions common to all tests

add_task(function test_common_initialize()
{
  // Before initializing the service for the first time, we should copy the key
  // file required to decrypt the logins contained in the SQLite databases used
  // by migration tests.  This file is not required for the other tests.
  yield OS.File.copy(do_get_file("data/key3.db").path,
                     OS.Path.join(OS.Constants.Path.profileDir, "key3.db"));

  // Ensure that the service and the storage module are initialized.
  yield Services.logins.initializationPromise;

  // Ensure that every test file starts with an empty database.
  LoginTestUtils.clearData();

  // Clean up after every test.
  do_register_cleanup(() => LoginTestUtils.clearData());
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
