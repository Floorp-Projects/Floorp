/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Test for bug 484579 <https://bugzilla.mozilla.org/show_bug.cgi?id=484579>:
 * nsIMIMEService.getTypeFromExtension may fail unexpectedly on Windows when
 * "Content Type" is empty in the registry.
 */
function run_test() {
  // --- Preliminary platform check ---

  // If this test is not running on the Windows platform, stop now, before
  // calling XPCOMUtils.generateQI during the MockWindowsRegKey declaration.
  if (mozinfo.os != "win")
    return;

  // --- Modified nsIWindowsRegKey implementation ---

  Cu.import("resource://gre/modules/XPCOMUtils.jsm");

  /**
   * Constructs a new mock registry key by wrapping the provided object.
   *
   * This mock implementation is tailored for this test, and forces consumers
   * of the readStringValue method to believe that the "Content Type" value of
   * the ".txt" key under HKEY_CLASSES_ROOT is an empty string.
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
      }
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

    QueryInterface: XPCOMUtils.generateQI([Ci.nsIWindowsRegKey]),

    // --- Overridden nsIWindowsRegKey interface functions ---

    open: function(aRootKey, aRelPath, aMode) {
      // Remember the provided root key and path
      this._rootKey = aRootKey;
      this._relPath = aRelPath;

      // Create the actual registry key
      return this._wrappedObject.open(aRootKey, aRelPath, aMode);
    },

    openChild: function(aRelPath, aMode) {
      // Open the child key and wrap it
      var innerKey = this._wrappedObject.openChild(aRelPath, aMode);
      var key = new MockWindowsRegKey(innerKey);

      // Set the properties of the child key and return it
      key._rootKey = this._rootKey;
      key._relPath = this._relPath + aRelPath;
      return key;
    },

    createChild: function(aRelPath, aMode) {
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

    readStringValue: function(aName) {
      // If this is the key under test, return a fake value
      if (this._rootKey == Ci.nsIWindowsRegKey.ROOT_KEY_CLASSES_ROOT &&
          this._relPath.toLowerCase() == ".txt" &&
          aName.toLowerCase() == "content type") {
        return "";
      }

      // Return the real value in the registry
      return this._wrappedObject.readStringValue(aName);
    }
  };

  // --- Mock nsIWindowsRegKey factory ---

  var componentRegistrar = Components.manager.
                           QueryInterface(Ci.nsIComponentRegistrar);

  var originalWindowsRegKeyCID;
  var mockWindowsRegKeyFactory;

  const kMockCID = Components.ID("{9b23dfe9-296b-4740-ba1c-d39c9a16e55e}");
  const kWindowsRegKeyContractID = "@mozilla.org/windows-registry-key;1";
  const kWindowsRegKeyClassName = "nsWindowsRegKey";

  function registerMockWindowsRegKeyFactory() {
    mockWindowsRegKeyFactory = {
      createInstance: function(aOuter, aIid) {
        if (aOuter != null)
          throw Cr.NS_ERROR_NO_AGGREGATION;

        var innerKey = originalWindowsRegKeyFactory.createInstance(null, aIid);
        var key = new MockWindowsRegKey(innerKey);

        return key.QueryInterface(aIid);
      }
    };

    // Preserve the original factory
    originalWindowsRegKeyCID = Cc[kWindowsRegKeyContractID].number;

    // Register the mock factory
    componentRegistrar.registerFactory(
      kMockCID,
      "Mock Windows Registry Key Implementation",
      kWindowsRegKeyContractID,
      mockWindowsRegKeyFactory
    );
  }

  function unregisterMockWindowsRegKeyFactory() {
    // Free references to the mock factory
    componentRegistrar.unregisterFactory(
      kMockCID,
      mockWindowsRegKeyFactory
    );

    // Restore the original factory
    componentRegistrar.registerFactory(
      Components.ID(originalWindowsRegKeyCID),
      "",
      kWindowsRegKeyContractID,
      null
    );
  }

  // --- Test procedure ---

  // Activate the override of the ".txt" file association data in the registry
  registerMockWindowsRegKeyFactory();
  try {
    // Try and get the MIME type associated with the extension. If this
    // operation does not throw an unexpected exception, the test succeeds.
    var type = Cc["@mozilla.org/mime;1"].
               getService(Ci.nsIMIMEService).
               getTypeFromExtension(".txt");
  } catch (e if (e instanceof Ci.nsIException &&
                 e.result == Cr.NS_ERROR_NOT_AVAILABLE)) {
    // This is an expected exception, thrown if the type can't be determined
  } finally {
    // Ensure we restore the original factory when the test is finished
    unregisterMockWindowsRegKeyFactory();
  }
}
