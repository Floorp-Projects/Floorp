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
 * the Mozilla Foundation.
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
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK *****
 */

// This just verifies that FUEL integrates to the add-ons manager

var testdata = {
  dummyid: "fuel-dummy-extension@mozilla.org",
  dummyname: "Dummy Extension",
  inspectorid: "addon1@tests.mozilla.org",
  inspectorname: "Test Addon",
  missing: "fuel.fuel-test-missing",
  dummy: "fuel.fuel-test"
};

var Application = null

function run_test() {
  var cm = AM_Cc["@mozilla.org/categorymanager;1"].
           getService(AM_Ci.nsICategoryManager);

  try {
    var contract = cm.getCategoryEntry("JavaScript-global-privileged-property",
                                       "Application");
    Application = AM_Cc[contract].getService(AM_Ci.extIApplication);
  }
  catch (e) {
    // This application does not include a FUEL variant.
    return;
  }

  do_test_pending();
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

  const profileDir = gProfD.clone();
  profileDir.append("extensions");

  writeInstallRDFForExtension({
    id: "addon1@tests.mozilla.org",
    version: "1.0",
    name: "Test Addon",
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "1"
    }],
  }, profileDir);

  startupManager();

  Application.getExtensions(function(extensions) {
    // test to see if the extensions object is available
    do_check_neq(extensions, null);

    // test to see if a non-existant extension exists
    do_check_true(!extensions.has(testdata.dummyid));

    // test to see if an extension exists
    do_check_true(extensions.has(testdata.inspectorid));

    var inspector = extensions.get(testdata.inspectorid);
    do_check_eq(inspector.id, testdata.inspectorid);
    do_check_eq(inspector.name, testdata.inspectorname);
    do_check_eq(inspector.version, "1.0");
    do_check_true(inspector.firstRun, true);
    do_check_true(inspector.enabled);

    // test to see if extension find works
    do_check_eq(extensions.all.length, 1);
    // STORAGE TESTING
    // Make sure the we are given the same extension (cached) so things like .storage work right
    inspector.storage.set("test", "simple check");
    do_check_true(inspector.storage.has("test"));

    var inspector2 = extensions.get(testdata.inspectorid);
    do_check_eq(inspector2.id, testdata.inspectorid);
    do_check_true(inspector.storage.has("test"));
    do_check_eq(inspector2.storage.get("test", "cache"), inspector.storage.get("test", "original"));

    inspector.events.addListener("disable", onGenericEvent);
    inspector.events.addListener("enable", onGenericEvent);
    inspector.events.addListener("uninstall", onGenericEvent);
    inspector.events.addListener("cancel", onGenericEvent);

    AddonManager.getAddonByID(testdata.inspectorid, function(a) {
      a.userDisabled = true;

      do_check_eq(gLastEvent, "disable");

      // enabling after a disable will only fire a 'cancel' event
      // see - http://mxr.mozilla.org/seamonkey/source/toolkit/mozapps/extensions/src/nsExtensionManager.js.in#5216
      a.userDisabled = false;
      do_check_eq(gLastEvent, "cancel");

      a.uninstall();
      do_check_eq(gLastEvent, "uninstall");

      a.cancelUninstall();
      do_check_eq(gLastEvent, "cancel");

      // PREF TESTING
      // Reset the install event preference, so that we can test it again later
      //inspector.prefs.get("install-event-fired").reset();

      // test the value of the preference root
      do_check_eq(extensions.all[0].prefs.root, "extensions.addon1@tests.mozilla.org.");

      // test getting nonexistent values
      var itemValue = inspector.prefs.getValue(testdata.missing, "default");
      do_check_eq(itemValue, "default");

      do_check_eq(inspector.prefs.get(testdata.missing), null);

      // test setting and getting a value
      inspector.prefs.setValue(testdata.dummy, "dummy");
      itemValue = inspector.prefs.getValue(testdata.dummy, "default");
      do_check_eq(itemValue, "dummy");

      // test for overwriting an existing value
      inspector.prefs.setValue(testdata.dummy, "smarty");
      itemValue = inspector.prefs.getValue(testdata.dummy, "default");
      do_check_eq(itemValue, "smarty");

      // test setting and getting a value
      inspector.prefs.get(testdata.dummy).value = "dummy2";
      itemValue = inspector.prefs.get(testdata.dummy).value;
      do_check_eq(itemValue, "dummy2");

      // test resetting a pref [since there is no default value, the pref should disappear]
      inspector.prefs.get(testdata.dummy).reset();
      var itemValue = inspector.prefs.getValue(testdata.dummy, "default");
      do_check_eq(itemValue, "default");

      // test to see if a non-existant property exists
      do_check_true(!inspector.prefs.has(testdata.dummy));

      inspector.prefs.events.addListener("change", onPrefChange);
      inspector.prefs.setValue("fuel.fuel-test", "change event");
    });
  });
}

function onGenericEvent(event) {
  gLastEvent = event.type;
}

function onPrefChange(evt) {
  Application.getExtensions(function(extensions) {
    var inspector3 = extensions.get(testdata.inspectorid);

    do_check_eq(evt.data, testdata.dummy);
    inspector3.prefs.events.removeListener("change", onPrefChange);

    inspector3.prefs.get("fuel.fuel-test").events.addListener("change", onPrefChange2);
    inspector3.prefs.setValue("fuel.fuel-test", "change event2");
  });
}

function onPrefChange2(evt) {
  do_check_eq(evt.data, testdata.dummy);

  do_test_finished();
}
