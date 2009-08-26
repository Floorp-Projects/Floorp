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

const ID = "bug511091@tests.mozilla.org";
const ADDON = "test_bug511091";

function run_test()
{
  // Setup for test
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1");

  // Install test add-on
  startupEM();
  gEM.installItemFromFile(do_get_addon(ADDON), NS_INSTALL_LOCATION_APPPROFILE);
  var addon = gEM.getItemForID(ID);
  do_check_neq(addon, null);
  do_check_eq(getManifestProperty(ID, "iconURL"), "chrome://mozapps/skin/xpinstall/xpinstallItemGeneric.png");
  restartEM();

  var location = gEM.getInstallLocation(ID);
  var file = location.getItemFile(ID, "icon.png");
  var ioservice = Components.classes["@mozilla.org/network/io-service;1"]
                            .getService(Components.interfaces.nsIIOService);

  var addon = gEM.getItemForID(ID);
  do_check_neq(addon, null);
  var uri = ioservice.newURI(getManifestProperty(ID, "iconURL"), null, null);
  uri.QueryInterface(Components.interfaces.nsIFileURL);
  do_check_true(uri.file.equals(file));
  gEM.disableItem(ID);
  restartEM();

  addon = gEM.getItemForID(ID);
  do_check_neq(addon, null);
  uri = ioservice.newURI(getManifestProperty(ID, "iconURL"), null, null);
  uri.QueryInterface(Components.interfaces.nsIFileURL);
  do_check_true(uri.file.equals(file));

  shutdownEM();
}
