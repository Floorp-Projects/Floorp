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
 * Dave Townsend <dtownsend@oxymoronical.com>.
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
 * the terms of any one of the MPL, the GPL or the LGPL
 *
 * ***** END LICENSE BLOCK *****
 */

const NO_INSTALL_SCRIPT = -204;

var listener = {
  onStateChange: function(index, state, value) {
    if (state == Components.interfaces.nsIXPIProgressDialog.INSTALL_DONE)
      do_check_eq(value, NO_INSTALL_SCRIPT);
  },

  onProgress: function(index, value, maxValue) {
  }
}

function run_test() {
  // Setup for test
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1");
  startupEM();

  var xpi = do_get_addon("test_bug436207");
  var ioservice = Components.classes["@mozilla.org/network/io-service;1"]
                            .getService(Components.interfaces.nsIIOService);
  var uri = ioservice.newFileURI(xpi);

  var xpim = Components.classes["@mozilla.org/xpinstall/install-manager;1"]
                       .createInstance(Components.interfaces.nsIXPInstallManager);
  xpim.initManagerFromChrome([uri.spec], 1, listener);
}
