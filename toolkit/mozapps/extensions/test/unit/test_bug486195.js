/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 *   Dave Townsend <dtownsend@oxymoronical.com>.
 *
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
 * ***** END LICENSE BLOCK *****
 */

const Cc = Components.classes;
const Ci = Components.interfaces;

const ID = "bug486195@tests.mozilla.org";
const REGKEY = "SOFTWARE\\Mozilla\\XPCShell\\Extensions";

function run_test()
{
  if (!("nsIWindowsRegKey" in Ci))
    return;

  var extension = do_get_file("data/test_bug486195");

  Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

  // --- Modified nsIWindowsRegKey implementation ---

  /**
   * This is a mock nsIWindowsRegistry implementation. It only implements the
   * methods that the extension manager requires.
   */
  function MockWindowsRegKey() {
  }

  MockWindowsRegKey.prototype = {
    // --- Overridden nsISupports interface functions ---

    QueryInterface: XPCOMUtils.generateQI([Ci.nsIWindowsRegKey]),

    // --- Overridden nsIWindowsRegKey interface functions ---

    open: function(aRootKey, aRelPath, aMode) {
      if (aRootKey != Ci.nsIWindowsRegKey.ROOT_KEY_LOCAL_MACHINE)
        throw Components.results.NS_ERROR_FAILURE;
      if (aRelPath != REGKEY)
        throw Components.results.NS_ERROR_FAILURE;
    },

    close: function() {
    },

    get valueCount() {
      return 1;
    },

    getValueName: function(aIndex) {
      if (aIndex == 0)
        return ID;
      throw Components.results.NS_ERROR_FAILURE;
    },

    readStringValue: function(aName) {
      if (aName == ID)
        return extension.path;
      throw Components.results.NS_ERROR_FAILURE;
    }
  };

  var factory = {
    createInstance: function(aOuter, aIid) {
      if (aOuter != null)
        throw Cr.NS_ERROR_NO_AGGREGATION;

      var key = new MockWindowsRegKey();
      return key.QueryInterface(aIid);
    }
  };

  Components.manager.QueryInterface(Ci.nsIComponentRegistrar)
            .registerFactory(Components.ID("{0478de5b-0f38-4edb-851d-4c99f1ed8eba}"),
                             "Mock Windows Registry Implementation",
                             "@mozilla.org/windows-registry-key;1", factory);

  // Setup for test
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1");

  // Install test add-on
  startupEM();
  var addon = gEM.getItemForID(ID);
  do_check_neq(addon, null);
  var installLocation = gEM.getInstallLocation(ID);
  var path = installLocation.getItemLocation(ID);
  do_check_eq(extension.path, path.path);
  path.append("install.rdf");
  path = installLocation.getItemLocation(ID);
  do_check_eq(extension.path, path.path);

  shutdownEM();

  Components.manager.unregisterFactory(Components.ID("{0478de5b-0f38-4edb-851d-4c99f1ed8eba}"),
                                       factory);
}
