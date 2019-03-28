/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Test for bug 484579 <https://bugzilla.mozilla.org/show_bug.cgi?id=484579>:
 * nsIMIMEService.getTypeFromExtension may fail unexpectedly on Windows when
 * "Content Type" is empty in the registry.
 */

// We must use a file extension that isn't listed in nsExternalHelperAppService's
// defaultMimeEntries, otherwise the code takes a shortcut skipping the registry.
const FILE_EXTENSION = ".nfo";
// This is used to ensure the test properly used the mock, so that if we change
// the underlying code, it won't be skipped.
let gTestUsedOurMock = false;

function run_test() {
  // Activate the override of the file association data in the registry.
  registerMockWindowsRegKeyFactory();

  // Check the mock has been properly activated.
  let regKey = Cc["@mozilla.org/windows-registry-key;1"]
                 .createInstance(Ci.nsIWindowsRegKey);
  regKey.open(Ci.nsIWindowsRegKey.ROOT_KEY_CLASSES_ROOT, FILE_EXTENSION,
              Ci.nsIWindowsRegKey.ACCESS_QUERY_VALUE);
  Assert.equal(regKey.readStringValue("content type"), "",
               "Check the mock replied as expected.");
  Assert.ok(gTestUsedOurMock, "The test properly used the mock registry");
  // Reset gTestUsedOurMock, because we just used it.
  gTestUsedOurMock = false;
  // Try and get the MIME type associated with the extension. If this
  // operation does not throw an unexpected exception, the test succeeds.
  Assert.throws(() => {
    Cc["@mozilla.org/mime;1"].getService(Ci.nsIMIMEService)
                             .getTypeFromExtension(FILE_EXTENSION);
  }, /NS_ERROR_NOT_AVAILABLE/, "Should throw a NOT_AVAILABLE exception");

  Assert.ok(gTestUsedOurMock, "The test properly used the mock registry");
}

/**
 * Constructs a new mock registry key by wrapping the provided object.
 *
 * This mock implementation is tailored for this test, and forces consumers
 * of the readStringValue method to believe that the "Content Type" value of
 * the FILE_EXTENSION key under HKEY_CLASSES_ROOT is an empty string.
 *
 * The same value read from "HKEY_LOCAL_MACHINE\SOFTWARE\Classes" is not
 * affected.
 *
 * @param aWrappedObject   An actual nsIWindowsRegKey implementation.
 */
function MockWindowsRegKey(aWrappedObject) {
  this._wrappedObject = aWrappedObject;

  // This function creates a forwarding function for wrappedObject
  function makeForwardingFunction(functionName) {
    return function() {
      return aWrappedObject[functionName].apply(aWrappedObject, arguments);
    };
  }

  // Forward all the functions that are not explicitly overridden
  for (var propertyName in aWrappedObject) {
    if (!(propertyName in this)) {
      if (typeof aWrappedObject[propertyName] == "function") {
        this[propertyName] = makeForwardingFunction(propertyName);
      } else {
        this[propertyName] = aWrappedObject[propertyName];
      }
    }
  }
}

MockWindowsRegKey.prototype = {
  // --- Overridden nsISupports interface functions ---

  QueryInterface: ChromeUtils.generateQI([Ci.nsIWindowsRegKey]),

  // --- Overridden nsIWindowsRegKey interface functions ---

  open(aRootKey, aRelPath, aMode) {
    // Remember the provided root key and path
    this._rootKey = aRootKey;
    this._relPath = aRelPath;

    // Create the actual registry key
    return this._wrappedObject.open(aRootKey, aRelPath, aMode);
  },

  openChild(aRelPath, aMode) {
    // Open the child key and wrap it
    var innerKey = this._wrappedObject.openChild(aRelPath, aMode);
    var key = new MockWindowsRegKey(innerKey);

    // Set the properties of the child key and return it
    key._rootKey = this._rootKey;
    key._relPath = this._relPath + aRelPath;
    return key;
  },

  createChild(aRelPath, aMode) {
    // Create the child key and wrap it
    var innerKey = this._wrappedObject.createChild(aRelPath, aMode);
    var key = new MockWindowsRegKey(innerKey);

    // Set the properties of the child key and return it
    key._rootKey = this._rootKey;
    key._relPath = this._relPath + aRelPath;
    return key;
  },

  get childCount() {
    return this._wrappedObject.childCount;
  },

  get valueCount() {
    return this._wrappedObject.valueCount;
  },

  readStringValue(aName) {
    // If this is the key under test, return a fake value
    if (this._rootKey == Ci.nsIWindowsRegKey.ROOT_KEY_CLASSES_ROOT &&
        this._relPath.toLowerCase() == FILE_EXTENSION &&
        aName.toLowerCase() == "content type") {
      gTestUsedOurMock = true;
      return "";
    }
    // Return the real value from the registry
    return this._wrappedObject.readStringValue(aName);
  },
};

function registerMockWindowsRegKeyFactory() {
  const kMockCID = Components.ID("{9b23dfe9-296b-4740-ba1c-d39c9a16e55e}");
  const kWindowsRegKeyContractID = "@mozilla.org/windows-registry-key;1";
  // Preserve the original CID.
  let originalWindowsRegKeyCID = Cc[kWindowsRegKeyContractID].number;

  info("Create a mock RegKey factory");
  let originalRegKey = Cc["@mozilla.org/windows-registry-key;1"]
                          .createInstance(Ci.nsIWindowsRegKey);
  let mockWindowsRegKeyFactory = {
    createInstance(outer, iid) {
      if (outer != null) {
        throw Cr.NS_ERROR_NO_AGGREGATION;
      }
      info("Create a mock wrapper around RegKey");
      var key = new MockWindowsRegKey(originalRegKey);
      return key.QueryInterface(iid);
    },
  };
  info("Register the mock RegKey factory");
  let registrar = Components.manager.QueryInterface(Ci.nsIComponentRegistrar);
  registrar.registerFactory(
    kMockCID,
    "Mock Windows Registry Key Implementation",
    kWindowsRegKeyContractID,
    mockWindowsRegKeyFactory
  );

  registerCleanupFunction(() => {
    // Free references to the mock factory
    registrar.unregisterFactory(
      kMockCID,
      mockWindowsRegKeyFactory
    );
    // Restore the original factory
    registrar.registerFactory(
      Components.ID(originalWindowsRegKeyCID),
      "",
      kWindowsRegKeyContractID,
      null
    );
  });
}
