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

const URI_XPINSTALL_CONFIRM_DIALOG = "chrome://mozapps/content/xpinstall/xpinstallConfirm.xul";

// Finds the index of the given xpi in the dialogparamblock strings
function findXPI(dpb, name) {
  for (var i = 0; i < 5; i++) {
    if (dpb.GetString(i * 4 + 1).substr(-(name.length + 1)) == "/" + name)
      return i * 4;
  }
  do_throw(name + " wasn't in the list");
}

// Called to display the XPInstall dialog
var WindowWatcher = {
  openWindow: function(parent, url, name, features, arguments) {
    do_check_eq(url, URI_XPINSTALL_CONFIRM_DIALOG);
    var dpb = arguments.QueryInterface(Ci.nsISupportsInterfacePointer)
                       .data.QueryInterface(Ci.nsIDialogParamBlock);
    do_check_eq(dpb.GetInt(1), 20);

    // Not defined what order they will be in so find them based on the filename
    var unsigned = findXPI(dpb, "unsigned.xpi");
    var signed = findXPI(dpb, "signed.xpi");
    var untrusted = findXPI(dpb, "signed-untrusted.xpi");
    var no_o = findXPI(dpb, "signed-no-o.xpi");
    var no_cn = findXPI(dpb, "signed-no-cn.xpi");

    // Test the names and certs are correct
    do_check_eq(dpb.GetString(unsigned), "XPI Test");
    do_check_eq(dpb.GetString(unsigned + 3), "");

    do_check_eq(dpb.GetString(signed), "Signed XPI Test");
    do_check_eq(dpb.GetString(signed + 3), "Object Signer");
    do_check_eq(dpb.GetString(no_o), "Signed XPI Test (No Org)");
    do_check_eq(dpb.GetString(no_o + 3), "Object Signer");
    do_check_eq(dpb.GetString(no_cn), "Signed XPI Test (No Common Name)");
    do_check_eq(dpb.GetString(no_cn + 3), "Mozilla Testing");

    // XPIs signed by an unknown CA just appear to not be signed at all
    do_check_eq(dpb.GetString(untrusted), "Signed XPI Test - Untrusted");
    do_check_eq(dpb.GetString(untrusted + 3), "");

    // Confirm the install
    dpb.SetInt(0, 0);
  },

  QueryInterface: function(iid) {
    if (iid.equals(Ci.nsIWindowWatcher)
     || iid.equals(Ci.nsISupports))
      return this;

    throw Cr.NS_ERROR_NO_INTERFACE;
  }
}

var WindowWatcherFactory = {
  createInstance: function createInstance(outer, iid) {
    if (outer != null)
      throw Components.results.NS_ERROR_NO_AGGREGATION;
    return WindowWatcher.QueryInterface(iid);
  }
};

var registrar = Components.manager.QueryInterface(Ci.nsIComponentRegistrar);
registrar.registerFactory(Components.ID("{1dfeb90a-2193-45d5-9cb8-864928b2af55}"),
                          "Fake Window Watcher",
                          "@mozilla.org/embedcomp/window-watcher;1", WindowWatcherFactory);

function run_test()
{
  // Setup for test
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1");

  // Copy the certificate db to the profile
  do_get_file("data/cert8.db").copyTo(gProfD, null);
  do_get_file("data/key3.db").copyTo(gProfD, null);
  do_get_file("data/secmod.db").copyTo(gProfD, null);

  // Copy the test add-ons into the install location
  var il = gProfD.clone();
  il.append("extensions");
  if (!il.exists())
    il.create(Ci.nsIFile.DIRECTORY_TYPE, 0755);
  do_get_file("data/unsigned.xpi").copyTo(il, null);
  do_get_file("data/signed.xpi").copyTo(il, null);
  do_get_file("data/signed-untrusted.xpi").copyTo(il, null);
  do_get_file("data/signed-tampered.xpi").copyTo(il, null);
  do_get_file("data/signed-no-o.xpi").copyTo(il, null);
  do_get_file("data/signed-no-cn.xpi").copyTo(il, null);

  // Starting the EM will detect and attempt to install the xpis
  startupEM();
  do_check_neq(gEM.getItemForID("signed-xpi@tests.mozilla.org"), null);
  do_check_neq(gEM.getItemForID("unsigned-xpi@tests.mozilla.org"), null);
  do_check_neq(gEM.getItemForID("untrusted-xpi@tests.mozilla.org"), null);
  do_check_eq(gEM.getItemForID("tampered-xpi@tests.mozilla.org"), null);
  do_check_neq(gEM.getItemForID("signed-xpi-no-o@tests.mozilla.org"), null);
  do_check_neq(gEM.getItemForID("signed-xpi-no-cn@tests.mozilla.org"), null);

  shutdownEM();
}
