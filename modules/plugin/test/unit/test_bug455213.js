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
 * Portions created by the Initial Developer are Copyright (C) 2007
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

const NS_APP_USER_PROFILE_50_DIR      = "ProfD";
const NS_APP_PROFILE_DIR_STARTUP      = "ProfDS";

// Plugin registry uses different field delimeters on different platforms
var DELIM = ":";
if ("@mozilla.org/windows-registry-key;1" in Components.classes)
  DELIM = "|";

var gProfD;
var gDirSvc = Cc["@mozilla.org/file/directory_service;1"].
             getService(Ci.nsIProperties);

// Creates a fake profile folder that the pluginhost will read our crafted
// pluginreg.dat from
function createProfileFolder() {
  gProfD = gDirSvc.get("CurProcD", Ci.nsILocalFile);
  gProfD = gProfD.parent.parent;
  gProfD.append("_tests");
  gProfD.append("xpcshell-simple");
  gProfD.append("test_plugin");
  gProfD.append("profile");
  
  if (gProfD.exists())
    gProfD.remove(true);
  gProfD.create(Components.interfaces.nsIFile.DIRECTORY_TYPE, 0755);
  
  var dirProvider = {
    getFile: function(prop, persistent) {
      persistent.value = true;
      if (prop == NS_APP_USER_PROFILE_50_DIR ||
          prop == NS_APP_PROFILE_DIR_STARTUP)
        return gProfD.clone();
      return null;
    },
    QueryInterface: function(iid) {
      if (iid.equals(Ci.nsIDirectoryServiceProvider) ||
          iid.equals(Ci.nsISupports)) {
        return this;
      }
      throw Components.results.NS_ERROR_NO_INTERFACE;
    }
  };
  gDirSvc.QueryInterface(Ci.nsIDirectoryService)
         .registerProvider(dirProvider);
}

// Writes out some plugin registry to the profile
function write_registry(version, info) {
  var header = "Generated File. Do not edit.\n\n";
  header += "[HEADER]\n";
  header += "Version" + DELIM + version + DELIM + "$\n\n";
  header += "[PLUGINS]\n";

  var registry = gProfD.clone();
  registry.append("pluginreg.dat");
  var foStream = Components.classes["@mozilla.org/network/file-output-stream;1"]
                           .createInstance(Components.interfaces.nsIFileOutputStream);
  // write, create, truncate
  foStream.init(registry, 0x02 | 0x08 | 0x20, 0666, 0); 

  var charset = "UTF-8"; // Can be any character encoding name that Mozilla supports
  var os = Cc["@mozilla.org/intl/converter-output-stream;1"].
           createInstance(Ci.nsIConverterOutputStream);
  os.init(foStream, charset, 0, 0x0000);
  
  os.writeString(header);
  os.writeString(info);
  os.close();
}

// Finds the test plugin library
function get_test_plugin() {
  var plugins = gDirSvc.get("CurProcD", Ci.nsILocalFile);
  plugins.append("plugins");
  do_check_true(plugins.exists());
  var plugin = plugins.clone();
  // OSX plugin
  plugin.append("Test.plugin");
  if (plugin.exists()) {
    plugin.normalize();
    return plugin;
  }
  plugin = plugins.clone();
  // *nix plugin
  plugin.append("libnptest.so");
  if (plugin.exists()) {
    plugin.normalize();
    return plugin;
  }
  // Windows plugin
  plugin = plugins.clone();
  plugin.append("nptest.dll");
  if (plugin.exists()) {
    plugin.normalize();
    return plugin;
  }
  return null;
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
  createProfileFolder();
  var file = get_test_plugin();
  if (!file)
    do_throw("Plugin library not found");

  // Write out a 0.9 version registry that marks the test plugin as disabled
  var registry = "";
  registry += file.leafName + DELIM + "$\n";
  registry += file.path + DELIM + "$\n";
  registry += file.lastModifiedTime + DELIM + "0" + DELIM + "0" + DELIM + "$\n";
  registry += "Plug-in for testing purposes." + DELIM + "$\n";
  registry += "Test Plug-in" + DELIM + "$\n";
  registry += "1\n";
  registry += "0" + DELIM + "application/x-test" + DELIM + "Test mimetype" +
              DELIM + "tst" + DELIM + "$\n";
  write_registry("0.9", registry);

  var plugin = get_test_plugintag();
  if (!plugin)
    do_throw("Plugin tag not found");

  // If the plugin was not rescanned then this version will not be correct
  do_check_eq(plugin.version, "1.0.0.0");
  do_check_eq(plugin.description, "Plug-in for testing purposes.");
  // If the plugin registry was not read then the plugin will not be disabled
  do_check_true(plugin.disabled);
  do_check_false(plugin.blocklisted);

  try {
    gProfD.remove(true);
  }
  catch (e) {
    // Failure to remove temp dir shouldn't be a test failure
  }
}
