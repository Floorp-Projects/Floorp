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
 * Mozilla Corporation.
 *
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Dave Townsend <dtownsend@oxymoronical.com>
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
 * the terms of any one of the MPL, the GPL or the LGPL
 *
 * ***** END LICENSE BLOCK *****
 */

function run_test() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1");

  startupEM();

  gEM.installItemFromFile(do_get_file("data/test_bug541420.xpi"),
                          NS_INSTALL_LOCATION_APPPROFILE);

  restartEM();
  do_check_neq(gEM.getItemForID("bug541420@tests.mozilla.org"), null);

  var il = gEM.getInstallLocation("bug541420@tests.mozilla.org");
  var file = il.getItemFile("bug541420@tests.mozilla.org", "binary");
  do_check_true(file.exists());
  do_check_true(file.isReadable());
  do_check_true(file.isWritable());

  // We don't understand executable permissions on Windows since we don't
  // support NTFS permissions so we don't need to test there. OSX's isExecutable
  // only tests if the file is an application so it is better to just check the
  // raw permission bits
  if (!("nsIWindowsRegKey" in Components.interfaces))
    do_check_true((file.permissions & 0111) != 0);

  gEM.uninstallItem("bug541420@tests.mozilla.org");

  restartEM();
  do_check_eq(gEM.getItemForID("bug541420@tests.mozilla.org"), null);
}
