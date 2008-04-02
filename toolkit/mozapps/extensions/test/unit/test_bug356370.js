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
 * Portions created by the Initial Developer are Copyright (C) 2008
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

function write_cache_line(stream, location, id, mtime) {
  var line = location + "\t" + id + "\trel%" + id + "\t" + Math.floor(mtime / 1000) + "\t\r\n";
  stream.write(line, line.length);
}

/**
 * This copies two extensions, a default extensions datasource into the profile
 * It also manufactures an extensions.cache file with invalid items.
 * There are 3 test extensions:
 *   bug356370_1@tests.mozilla.org exists in app-profile and an unused version is in invalid-lo
 *   bug356370_2@tests.mozilla.org exists in invalid-hi and an unusd version is in app-profile
 *   bug356370_3@tests.mozilla.org exists in invalid
 *
 * After startup only the first two should exist in the correct install location
 * and installing extensions should be successful.
 */
function setup_profile() {
  // Set up the profile with some existing extensions
  // Not nice to copy the extensions datasource in, but bringing up the EM to
  // create it properly will invalidate the test
  var source = do_get_file("toolkit/mozapps/extensions/test/unit/data/test_bug356370.rdf");
  source.copyTo(gProfD, "extensions.rdf");

  // Must programmatically generate the cache since it depends on the mimetimes
  // being accurate.
  var cache = gProfD.clone();
  cache.append("extensions.cache");
  var foStream = Components.classes["@mozilla.org/network/file-output-stream;1"]
                           .createInstance(Components.interfaces.nsIFileOutputStream);
  foStream.init(cache, 0x02 | 0x08 | 0x20, 0666, 0);  // Write, create, truncate

  var addon = gProfD.clone();
  addon.append("extensions");
  addon.append("bug356370_1@tests.mozilla.org");
  source = do_get_file("toolkit/mozapps/extensions/test/unit/data/test_bug356370_1.rdf");
  addon.create(Components.interfaces.nsIFile.DIRECTORY_TYPE, 0755);
  source.copyTo(addon, "install.rdf");
  write_cache_line(foStream, "app-profile", "bug356370_1@tests.mozilla.org",
                   addon.lastModifiedTime);

  addon = gProfD.clone();
  addon.append("extensions");
  addon.append("bug356370_2@tests.mozilla.org");
  source = do_get_file("toolkit/mozapps/extensions/test/unit/data/test_bug356370_2.rdf");
  addon.create(Components.interfaces.nsIFile.DIRECTORY_TYPE, 0755);
  source.copyTo(addon, "install.rdf");
  write_cache_line(foStream, "app-profile", "bug356370_2@tests.mozilla.org",
                   addon.lastModifiedTime);

  // Write out a set of invalid entries
  write_cache_line(foStream, "invalid-lo", "bug356370_1@tests.mozilla.org", 0);
  write_cache_line(foStream, "invalid-hi", "bug356370_2@tests.mozilla.org", 0);
  write_cache_line(foStream, "invalid", "bug356370_3@tests.mozilla.org", 0);
  foStream.close();
}

function run_test() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9");
  gPrefs.setCharPref("extensions.lastAppVersion", "4");
  setup_profile();

  startupEM();
  do_check_neq(gEM.getItemForID("bug356370_1@tests.mozilla.org"), null);
  do_check_eq(getManifestProperty("bug356370_1@tests.mozilla.org", "installLocation"), "app-profile");
  do_check_neq(gEM.getItemForID("bug356370_2@tests.mozilla.org"), null);
  do_check_eq(getManifestProperty("bug356370_2@tests.mozilla.org", "installLocation"), "app-profile");
  // This should still be disabled
  do_check_eq(getManifestProperty("bug356370_2@tests.mozilla.org", "isDisabled"), "true");
  do_check_eq(gEM.getItemForID("bug356370_3@tests.mozilla.org"), null);

  gEM.installItemFromFile(do_get_addon("test_bug257155"), NS_INSTALL_LOCATION_APPPROFILE);
  do_check_neq(gEM.getItemForID("bug257155@tests.mozilla.org"), null);

  restartEM();
  do_check_neq(gEM.getItemForID("bug257155@tests.mozilla.org"), null);
  do_check_neq(gEM.getItemForID("bug356370_1@tests.mozilla.org"), null);
  do_check_neq(gEM.getItemForID("bug356370_2@tests.mozilla.org"), null);
  do_check_eq(gEM.getItemForID("bug356370_3@tests.mozilla.org"), null);
}
