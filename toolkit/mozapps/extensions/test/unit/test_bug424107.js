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

function run_test()
{
  // Copy an initial add-on into the profile.
  var extension = gProfD.clone()
  extension.append("extensions");
  extension.create(Components.interfaces.nsIFile.DIRECTORY_TYPE, 0755);
  extension.append("bug424107@tests.mozilla.org");
  extension.create(Components.interfaces.nsIFile.DIRECTORY_TYPE, 0755);
  var sourcerdf = do_get_file("data/test_bug424107_1.rdf");
  sourcerdf.copyTo(extension, "install.rdf");
  
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "5", "1.9");
  startupEM();
  var addon = gEM.getItemForID("bug424107@tests.mozilla.org");
  do_check_neq(addon, null);
  do_check_eq(addon.version, 1);

  // Uninstall
  extension.remove(true);

  restartEM();
  addon = gEM.getItemForID("bug424107@tests.mozilla.org");
  do_check_eq(addon, null);

  // Install a new version
  extension.create(Components.interfaces.nsIFile.DIRECTORY_TYPE, 0755);
  sourcerdf = do_get_file("data/test_bug424107_2.rdf");
  sourcerdf.copyTo(extension, "install.rdf");

  restartEM();
  addon = gEM.getItemForID("bug424107@tests.mozilla.org");
  do_check_neq(addon, null);
  do_check_eq(addon.version, 2);
}

