/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests the preference-related properties of AddonManager
// eg: AddonManager.checkCompatibility, AddonManager.updateEnabled, etc

var gManagerEventsListener = {
  seenEvents: [],
  init() {
    let events = ["onCompatibilityModeChanged", "onCheckUpdateSecurityChanged",
                  "onUpdateModeChanged"];
    events.forEach(function(aEvent) {
      this[aEvent] = function() {
        info("Saw event " + aEvent);
        this.seenEvents.push(aEvent);
      };
    }, this);
    AddonManager.addManagerListener(this);
    // Try to add twice, to test that the second time silently fails.
    AddonManager.addManagerListener(this);
  },
  shutdown() {
    AddonManager.removeManagerListener(this);
  },
  expect(aEvents) {
    this.expectedEvents = aEvents;
  },
  checkExpected() {
    info("Checking expected events...");
    while (this.expectedEvents.length > 0) {
      let event = this.expectedEvents.pop();
      info("Looking for expected event " + event);
      let matchingEvents = this.seenEvents.filter(function(aSeenEvent) {
        return aSeenEvent == event;
      });
      Assert.equal(matchingEvents.length, 1);
    }
    this.seenEvents = [];
  }
};

add_task(async function setup() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

  Services.prefs.setBoolPref("extensions.update.enabled", true);
  Services.prefs.setBoolPref("extensions.update.autoUpdateDefault", true);
  Services.prefs.setBoolPref("extensions.strictCompatibility", true);
  Services.prefs.setBoolPref("extensions.checkUpdatesecurity", true);

  await promiseStartupManager();
  gManagerEventsListener.init();


  // AddonManager.updateEnabled
  gManagerEventsListener.expect(["onUpdateModeChanged"]);
  AddonManager.updateEnabled = false;
  gManagerEventsListener.checkExpected();
  Assert.ok(!AddonManager.updateEnabled);
  Assert.ok(!Services.prefs.getBoolPref("extensions.update.enabled"));

  gManagerEventsListener.expect([]);
  AddonManager.updateEnabled = false;
  gManagerEventsListener.checkExpected();
  Assert.ok(!AddonManager.updateEnabled);
  Assert.ok(!Services.prefs.getBoolPref("extensions.update.enabled"));

  gManagerEventsListener.expect(["onUpdateModeChanged"]);
  AddonManager.updateEnabled = true;
  gManagerEventsListener.checkExpected();
  Assert.ok(AddonManager.updateEnabled);
  Assert.ok(Services.prefs.getBoolPref("extensions.update.enabled"));

  gManagerEventsListener.expect([]);
  AddonManager.updateEnabled = true;
  gManagerEventsListener.checkExpected();
  Assert.ok(AddonManager.updateEnabled);
  Assert.ok(Services.prefs.getBoolPref("extensions.update.enabled"));

  // AddonManager.autoUpdateDefault
  gManagerEventsListener.expect(["onUpdateModeChanged"]);
  AddonManager.autoUpdateDefault = false;
  gManagerEventsListener.checkExpected();
  Assert.ok(!AddonManager.autoUpdateDefault);
  Assert.ok(!Services.prefs.getBoolPref("extensions.update.autoUpdateDefault"));

  gManagerEventsListener.expect([]);
  AddonManager.autoUpdateDefault = false;
  gManagerEventsListener.checkExpected();
  Assert.ok(!AddonManager.autoUpdateDefault);
  Assert.ok(!Services.prefs.getBoolPref("extensions.update.autoUpdateDefault"));

  gManagerEventsListener.expect(["onUpdateModeChanged"]);
  AddonManager.autoUpdateDefault = true;
  gManagerEventsListener.checkExpected();
  Assert.ok(AddonManager.autoUpdateDefault);
  Assert.ok(Services.prefs.getBoolPref("extensions.update.autoUpdateDefault"));

  gManagerEventsListener.expect([]);
  AddonManager.autoUpdateDefault = true;
  gManagerEventsListener.checkExpected();
  Assert.ok(AddonManager.autoUpdateDefault);
  Assert.ok(Services.prefs.getBoolPref("extensions.update.autoUpdateDefault"));

  // AddonManager.strictCompatibility
  gManagerEventsListener.expect(["onCompatibilityModeChanged"]);
  AddonManager.strictCompatibility = false;
  gManagerEventsListener.checkExpected();
  Assert.ok(!AddonManager.strictCompatibility);
  Assert.ok(!Services.prefs.getBoolPref("extensions.strictCompatibility"));

  gManagerEventsListener.expect([]);
  AddonManager.strictCompatibility = false;
  gManagerEventsListener.checkExpected();
  Assert.ok(!AddonManager.strictCompatibility);
  Assert.ok(!Services.prefs.getBoolPref("extensions.strictCompatibility"));

  gManagerEventsListener.expect(["onCompatibilityModeChanged"]);
  AddonManager.strictCompatibility = true;
  gManagerEventsListener.checkExpected();
  Assert.ok(AddonManager.strictCompatibility);
  Assert.ok(Services.prefs.getBoolPref("extensions.strictCompatibility"));

  gManagerEventsListener.expect([]);
  AddonManager.strictCompatibility = true;
  gManagerEventsListener.checkExpected();
  Assert.ok(AddonManager.strictCompatibility);
  Assert.ok(Services.prefs.getBoolPref("extensions.strictCompatibility"));


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
  Assert.ok(!AddonManager.checkCompatibility);
  Assert.ok(!Services.prefs.getBoolPref(COMPATIBILITY_PREF));

  gManagerEventsListener.expect([]);
  AddonManager.checkCompatibility = false;
  gManagerEventsListener.checkExpected();
  Assert.ok(!AddonManager.checkCompatibility);
  Assert.ok(!Services.prefs.getBoolPref(COMPATIBILITY_PREF));

  gManagerEventsListener.expect(["onCompatibilityModeChanged"]);
  AddonManager.checkCompatibility = true;
  gManagerEventsListener.checkExpected();
  Assert.ok(AddonManager.checkCompatibility);
  Assert.ok(!Services.prefs.prefHasUserValue(COMPATIBILITY_PREF));

  gManagerEventsListener.expect([]);
  AddonManager.checkCompatibility = true;
  gManagerEventsListener.checkExpected();
  Assert.ok(AddonManager.checkCompatibility);
  Assert.ok(!Services.prefs.prefHasUserValue(COMPATIBILITY_PREF));


  // AddonManager.checkUpdateSecurity
  gManagerEventsListener.expect(["onCheckUpdateSecurityChanged"]);
  AddonManager.checkUpdateSecurity = false;
  gManagerEventsListener.checkExpected();
  Assert.ok(!AddonManager.checkUpdateSecurity);
  if (AddonManager.checkUpdateSecurityDefault)
    Assert.ok(!Services.prefs.getBoolPref("extensions.checkUpdateSecurity"));
  else
    Assert.ok(!Services.prefs.prefHasUserValue("extensions.checkUpdateSecurity"));

  gManagerEventsListener.expect([]);
  AddonManager.checkUpdateSecurity = false;
  gManagerEventsListener.checkExpected();
  Assert.ok(!AddonManager.checkUpdateSecurity);
  if (AddonManager.checkUpdateSecurityDefault)
    Assert.ok(!Services.prefs.getBoolPref("extensions.checkUpdateSecurity"));
  else
    Assert.ok(!Services.prefs.prefHasUserValue("extensions.checkUpdateSecurity"));

  gManagerEventsListener.expect(["onCheckUpdateSecurityChanged"]);
  AddonManager.checkUpdateSecurity = true;
  gManagerEventsListener.checkExpected();
  Assert.ok(AddonManager.checkUpdateSecurity);
  if (!AddonManager.checkUpdateSecurityDefault)
    Assert.ok(Services.prefs.getBoolPref("extensions.checkUpdateSecurity"));
  else
    Assert.ok(!Services.prefs.prefHasUserValue("extensions.checkUpdateSecurity"));

  gManagerEventsListener.expect([]);
  AddonManager.checkUpdateSecurity = true;
  gManagerEventsListener.checkExpected();
  Assert.ok(AddonManager.checkUpdateSecurity);
  if (!AddonManager.checkUpdateSecurityDefault)
    Assert.ok(Services.prefs.getBoolPref("extensions.checkUpdateSecurity"));
  else
    Assert.ok(!Services.prefs.prefHasUserValue("extensions.checkUpdateSecurity"));

  gManagerEventsListener.shutdown();

  // After removing the listener, ensure we get no further events.
  gManagerEventsListener.expect([]);
  AddonManager.updateEnabled = false;
  gManagerEventsListener.checkExpected();
});
