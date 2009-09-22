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
 *      Dave Townsend <dtownsend@oxymoronical.com>.
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

// Creates a fake ProfDS directory key, copied from do_get_profile
function do_get_profile_startup() {
  let env = Components.classes["@mozilla.org/process/environment;1"]
                      .getService(Components.interfaces.nsIEnvironment);
  // the python harness sets this in the environment for us
  let profd = env.get("XPCSHELL_TEST_PROFILE_DIR");
  let file = Components.classes["@mozilla.org/file/local;1"]
                       .createInstance(Components.interfaces.nsILocalFile);
  file.initWithPath(profd);

  let dirSvc = Components.classes["@mozilla.org/file/directory_service;1"]
                         .getService(Components.interfaces.nsIProperties);
  let provider = {
    getFile: function(prop, persistent) {
      persistent.value = true;
      if (prop == "ProfDS") {
        return file.clone();
      }
      throw Components.results.NS_ERROR_FAILURE;
    },
    QueryInterface: function(iid) {
      if (iid.equals(Components.interfaces.nsIDirectoryProvider) ||
          iid.equals(Components.interfaces.nsISupports)) {
        return this;
      }
      throw Components.results.NS_ERROR_NO_INTERFACE;
    }
  };
  dirSvc.QueryInterface(Components.interfaces.nsIDirectoryService)
        .registerProvider(provider);
  return file.clone();
}

// Finds the test nsIPluginTag
function get_test_plugintag() {
  var host = Cc["@mozilla.org/plugin/host;1"].
             getService(Ci.nsIPluginHost);
  var tags = host.getPluginTags({});
  for (var i = 0; i < tags.length; i++) {
    if (tags[i].name == "Test Plug-in")
      return tags[i];
  }
  return null;
}

function run_test() {
  do_get_profile_startup();

  var plugin = get_test_plugintag();
  do_check_true(plugin == null);

  // Initialises a profile folder
  do_get_profile();

  var plugin = get_test_plugintag();
  do_check_false(plugin == null);
}
