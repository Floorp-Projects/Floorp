/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Automated Testing Code.
 *
 * The Initial Developer of the Original Code is
 * Paolo Amadini <http://www.amadzone.org/>.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/**
 * Test for bug 484579 <https://bugzilla.mozilla.org/show_bug.cgi?id=484579>:
 * nsIMIMEService.getTypeFromExtension may fail unexpectedly on Windows when
 * "Content Type" is empty in the registry.
 */
function run_test() {
  // --- Preliminary platform check ---

  // If this test is not running on the Windows platform, stop now, before
  // calling XPCOMUtils.generateQI during the MockWindowsRegKey declaration.
  if (!("@mozilla.org/windows-registry-key;1" in Components.classes))
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

  var originalWindowsRegKeyFactory;
  var mockWindowsRegKeyFactory;

  const kWindowsRegKeyCID = "{a53bc624-d577-4839-b8ec-bb5040a52ff4}";
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
    originalWindowsRegKeyFactory = Components.manager.getClassObject(
                                   Cc[kWindowsRegKeyContractID], Ci.nsIFactory);

    // Register the mock factory
    componentRegistrar.registerFactory(
      Components.ID(kWindowsRegKeyCID),
      "Mock Windows Registry Key Implementation",
      kWindowsRegKeyContractID,
      mockWindowsRegKeyFactory
    );
  }

  function unregisterMockWindowsRegKeyFactory() {
    // Free references to the mock factory
    componentRegistrar.unregisterFactory(
      Components.ID(kWindowsRegKeyCID),
      mockWindowsRegKeyFactory
    );

    // Restore the original factory
    componentRegistrar.registerFactory(
      Components.ID(kWindowsRegKeyCID),
      kWindowsRegKeyClassName,
      kWindowsRegKeyContractID,
      originalWindowsRegKeyFactory
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
