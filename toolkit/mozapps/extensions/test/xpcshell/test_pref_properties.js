/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests the preference-related properties of AddonManager
// eg: AddonManager.checkCompatibility, AddonManager.updateEnabled, etc

var gManagerEventsListener = {
  seenEvents: [],
  init: function() {
    let events = ["onCompatibilityModeChanged", "onCheckUpdateSecurityChanged",
                  "onUpdateModeChanged"];
    events.forEach(function(aEvent) {
      this[aEvent] = function() {
        do_print("Saw event " + aEvent);
        this.seenEvents.push(aEvent);
      }
    }, this);
    AddonManager.addManagerListener(this);
    // Try to add twice, to test that the second time silently fails.
    AddonManager.addManagerListener(this);
  },
  shutdown: function() {
    AddonManager.removeManagerListener(this);
  },
  expect: function(aEvents) {
    this.expectedEvents = aEvents;
  },
  checkExpected: function() {
    do_print("Checking expected events...");
    while (this.expectedEvents.length > 0) {
      let event = this.expectedEvents.pop();
      do_print("Looking for expected event " + event);
      let matchingEvents = this.seenEvents.filter(function(aSeenEvent) {
        return aSeenEvent == event;
      });
      do_check_eq(matchingEvents.length, 1);
    }
    this.seenEvents = [];
  }
}

function run_test() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

  Services.prefs.setBoolPref("extensions.update.enabled", true);
  Services.prefs.setBoolPref("extensions.update.autoUpdateDefault", true);
  Services.prefs.setBoolPref("extensions.strictCompatibility", true);
  Services.prefs.setBoolPref("extensions.checkUpdatesecurity", true);

  startupManager();
  gManagerEventsListener.init();


  // AddonManager.updateEnabled
  gManagerEventsListener.expect(["onUpdateModeChanged"]);
  AddonManager.updateEnabled = false;
  gManagerEventsListener.checkExpected();
  do_check_false(AddonManager.updateEnabled);
  do_check_false(Services.prefs.getBoolPref("extensions.update.enabled"));

  gManagerEventsListener.expect([]);
  AddonManager.updateEnabled = false;
  gManagerEventsListener.checkExpected();
  do_check_false(AddonManager.updateEnabled);
  do_check_false(Services.prefs.getBoolPref("extensions.update.enabled"));

  gManagerEventsListener.expect(["onUpdateModeChanged"]);
  AddonManager.updateEnabled = true;
  gManagerEventsListener.checkExpected();
  do_check_true(AddonManager.updateEnabled);
  do_check_true(Services.prefs.getBoolPref("extensions.update.enabled"));

  gManagerEventsListener.expect([]);
  AddonManager.updateEnabled = true;
  gManagerEventsListener.checkExpected();
  do_check_true(AddonManager.updateEnabled);
  do_check_true(Services.prefs.getBoolPref("extensions.update.enabled"));

  // AddonManager.autoUpdateDefault
  gManagerEventsListener.expect(["onUpdateModeChanged"]);
  AddonManager.autoUpdateDefault = false;
  gManagerEventsListener.checkExpected();
  do_check_false(AddonManager.autoUpdateDefault);
  do_check_false(Services.prefs.getBoolPref("extensions.update.autoUpdateDefault"));

  gManagerEventsListener.expect([]);
  AddonManager.autoUpdateDefault = false;
  gManagerEventsListener.checkExpected();
  do_check_false(AddonManager.autoUpdateDefault);
  do_check_false(Services.prefs.getBoolPref("extensions.update.autoUpdateDefault"));

  gManagerEventsListener.expect(["onUpdateModeChanged"]);
  AddonManager.autoUpdateDefault = true;
  gManagerEventsListener.checkExpected();
  do_check_true(AddonManager.autoUpdateDefault);
  do_check_true(Services.prefs.getBoolPref("extensions.update.autoUpdateDefault"));

  gManagerEventsListener.expect([]);
  AddonManager.autoUpdateDefault = true;
  gManagerEventsListener.checkExpected();
  do_check_true(AddonManager.autoUpdateDefault);
  do_check_true(Services.prefs.getBoolPref("extensions.update.autoUpdateDefault"));

  // AddonManager.strictCompatibility
  gManagerEventsListener.expect(["onCompatibilityModeChanged"]);
  AddonManager.strictCompatibility = false;
  gManagerEventsListener.checkExpected();
  do_check_false(AddonManager.strictCompatibility);
  do_check_false(Services.prefs.getBoolPref("extensions.strictCompatibility"));

  gManagerEventsListener.expect([]);
  AddonManager.strictCompatibility = false;
  gManagerEventsListener.checkExpected();
  do_check_false(AddonManager.strictCompatibility);
  do_check_false(Services.prefs.getBoolPref("extensions.strictCompatibility"));

  gManagerEventsListener.expect(["onCompatibilityModeChanged"]);
  AddonManager.strictCompatibility = true;
  gManagerEventsListener.checkExpected();
  do_check_true(AddonManager.strictCompatibility);
  do_check_true(Services.prefs.getBoolPref("extensions.strictCompatibility"));

  gManagerEventsListener.expect([]);
  AddonManager.strictCompatibility = true;
  gManagerEventsListener.checkExpected();
  do_check_true(AddonManager.strictCompatibility);
  do_check_true(Services.prefs.getBoolPref("extensions.strictCompatibility"));


  // AddonManager.checkCompatibility
  if (isNightlyChannel()) {
    var version = "nightly";
  } else {
    version = Services.appinfo.version.replace(/^([^\.]+\.[0-9]+[a-z]*).*/gi, "$1");
  }
  const COMPATIBILITY_PREF = "extensions.checkCompatibility." + version;

  gManagerEventsListener.expect(["onCompatibilityModeChanged"]);
  AddonManager.checkCompatibility = false;
  gManagerEventsListener.checkExpected();
  do_check_false(AddonManager.checkCompatibility);
  do_check_false(Services.prefs.getBoolPref(COMPATIBILITY_PREF));

  gManagerEventsListener.expect([]);
  AddonManager.checkCompatibility = false;
  gManagerEventsListener.checkExpected();
  do_check_false(AddonManager.checkCompatibility);
  do_check_false(Services.prefs.getBoolPref(COMPATIBILITY_PREF));

  gManagerEventsListener.expect(["onCompatibilityModeChanged"]);
  AddonManager.checkCompatibility = true;
  gManagerEventsListener.checkExpected();
  do_check_true(AddonManager.checkCompatibility);
  do_check_false(Services.prefs.prefHasUserValue(COMPATIBILITY_PREF));

  gManagerEventsListener.expect([]);
  AddonManager.checkCompatibility = true;
  gManagerEventsListener.checkExpected();
  do_check_true(AddonManager.checkCompatibility);
  do_check_false(Services.prefs.prefHasUserValue(COMPATIBILITY_PREF));


  // AddonManager.checkUpdateSecurity
  gManagerEventsListener.expect(["onCheckUpdateSecurityChanged"]);
  AddonManager.checkUpdateSecurity = false;
  gManagerEventsListener.checkExpected();
  do_check_false(AddonManager.checkUpdateSecurity);
  if (AddonManager.checkUpdateSecurityDefault)
    do_check_false(Services.prefs.getBoolPref("extensions.checkUpdateSecurity"));
  else
    do_check_false(Services.prefs.prefHasUserValue("extensions.checkUpdateSecurity"));

  gManagerEventsListener.expect([]);
  AddonManager.checkUpdateSecurity = false;
  gManagerEventsListener.checkExpected();
  do_check_false(AddonManager.checkUpdateSecurity);
  if (AddonManager.checkUpdateSecurityDefault)
    do_check_false(Services.prefs.getBoolPref("extensions.checkUpdateSecurity"));
  else
    do_check_false(Services.prefs.prefHasUserValue("extensions.checkUpdateSecurity"));

  gManagerEventsListener.expect(["onCheckUpdateSecurityChanged"]);
  AddonManager.checkUpdateSecurity = true;
  gManagerEventsListener.checkExpected();
  do_check_true(AddonManager.checkUpdateSecurity);
  if (!AddonManager.checkUpdateSecurityDefault)
    do_check_true(Services.prefs.getBoolPref("extensions.checkUpdateSecurity"));
  else
    do_check_false(Services.prefs.prefHasUserValue("extensions.checkUpdateSecurity"));

  gManagerEventsListener.expect([]);
  AddonManager.checkUpdateSecurity = true;
  gManagerEventsListener.checkExpected();
  do_check_true(AddonManager.checkUpdateSecurity);
  if (!AddonManager.checkUpdateSecurityDefault)
    do_check_true(Services.prefs.getBoolPref("extensions.checkUpdateSecurity"));
  else
    do_check_false(Services.prefs.prefHasUserValue("extensions.checkUpdateSecurity"));

  gManagerEventsListener.shutdown();

  // AddonManager.hotfixID
  let hotfixID = "hotfix@tests.mozilla.org";
  Services.prefs.setCharPref("extensions.hotfix.id", hotfixID);
  do_check_eq(AddonManager.hotfixID, hotfixID);
  // Change the pref and make sure the property is updated
  hotfixID = "hotfix2@tests.mozilla.org";
  Services.prefs.setCharPref("extensions.hotfix.id", hotfixID);
  do_check_eq(AddonManager.hotfixID, hotfixID);
  // Test an invalid pref value
  hotfixID = 99;
  Services.prefs.deleteBranch("extensions.hotfix.id");
  Services.prefs.setIntPref("extensions.hotfix.id", hotfixID);
  do_check_eq(AddonManager.hotfixID, null);
  Services.prefs.clearUserPref("extensions.hotfix.id");

  // After removing the listener, ensure we get no further events.
  gManagerEventsListener.expect([]);
  AddonManager.updateEnabled = false;
  gManagerEventsListener.checkExpected();
}
