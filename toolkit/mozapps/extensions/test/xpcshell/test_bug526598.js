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
 * Dave Townsend <dtownsend@oxymoronical.com>
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
 * the terms of any one of the MPL, the GPL or the LGPL
 *
 * ***** END LICENSE BLOCK *****
 */

function run_test() {
  do_test_pending();
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1");

  startupManager();

  installAllFiles([do_get_file("data/test_bug526598_1.xpi"),
                   do_get_file("data/test_bug526598_2.xpi")], function() {

    restartManager();

    AddonManager.getAddonsByIDs(["bug526598_1@tests.mozilla.org",
                                 "bug526598_2@tests.mozilla.org"],
                                 function([a1, a2]) {

      do_check_neq(a1, null);
      do_check_true(a1.hasResource("install.rdf"));
      let uri = a1.getResourceURI("install.rdf");
      do_check_true(uri instanceof AM_Ci.nsIFileURL);
      let file = uri.file;
      do_check_true(file.exists());
      do_check_true(file.isReadable());
      do_check_true(file.isWritable());

      do_check_neq(a2, null);
      do_check_true(a2.hasResource("install.rdf"));
      uri = a2.getResourceURI("install.rdf");
      do_check_true(uri instanceof AM_Ci.nsIFileURL);
      file = uri.file;
      do_check_true(file.exists());
      do_check_true(file.isReadable());
      do_check_true(file.isWritable());

      a1.uninstall();
      a2.uninstall();

      restartManager();

      AddonManager.getAddonsByIDs(["bug526598_1@tests.mozilla.org",
                                   "bug526598_2@tests.mozilla.org"],
                                   function([newa1, newa2]) {
        do_check_eq(newa1, null);
        do_check_eq(newa2, null);

        do_test_finished();
      });
    });
  });
}
