/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const { PromiseTestUtils } = ChromeUtils.import(
  "resource://testing-common/PromiseTestUtils.jsm"
);
PromiseTestUtils.allowMatchingRejectionsGlobally(
  /Unable to arm timer, the object has been finalized\./
);
PromiseTestUtils.allowMatchingRejectionsGlobally(
  /IOUtils\.profileBeforeChange getter: IOUtils: profileBeforeChange phase has already finished/
);

const { Preferences } = ChromeUtils.import(
  "resource://gre/modules/Preferences.jsm"
);
const { PrefRec, getPrefsGUIDForTest } = ChromeUtils.import(
  "resource://services-sync/engines/prefs.js"
);
const PREFS_GUID = getPrefsGUIDForTest();
const { Service } = ChromeUtils.import("resource://services-sync/service.js");

const DEFAULT_THEME_ID = "default-theme@mozilla.org";
const COMPACT_THEME_ID = "firefox-compact-light@mozilla.org";

AddonTestUtils.init(this);
AddonTestUtils.createAppInfo(
  "xpcshell@tests.mozilla.org",
  "XPCShell",
  "1",
  "1.9.2"
);
AddonTestUtils.overrideCertDB();

// Attempting to set the
// security.turn_off_all_security_so_that_viruses_can_take_over_this_computer
// preference to enable Cu.exitIfInAutomation crashes, probably due to
// shutdown behaviors faked by AddonTestUtils.jsm's cleanup function.
do_disable_fast_shutdown();

add_task(async function run_test() {
  _("Test fixtures.");
  // Part of this test ensures the default theme, via the preference
  // extensions.activeThemeID, is synced correctly - so we do a little
  // addons initialization to allow this to work.

  // Enable application scopes to ensure the builtin theme is going to
  // be installed as part of the the addon manager startup.
  Preferences.set("extensions.enabledScopes", AddonManager.SCOPE_APPLICATION);
  await AddonTestUtils.promiseStartupManager();

  // Install another built-in theme.
  await AddonManager.installBuiltinAddon("resource://builtin-themes/light/");

  const defaultThemeAddon = await AddonManager.getAddonByID(DEFAULT_THEME_ID);
  ok(defaultThemeAddon, "Got an addon wrapper for the default theme");

  const otherThemeAddon = await AddonManager.getAddonByID(COMPACT_THEME_ID);
  ok(otherThemeAddon, "Got an addon wrapper for the compact theme");

  await otherThemeAddon.enable();

  // read our custom prefs file before doing anything.
  Services.prefs.readDefaultPrefsFromFile(
    do_get_file("prefs_test_prefs_store.js")
  );

  let engine = Service.engineManager.get("prefs");
  let store = engine._store;
  let prefs = new Preferences();
  try {
    _("Expect the compact light theme to be active");
    Assert.strictEqual(prefs.get("extensions.activeThemeID"), COMPACT_THEME_ID);

    _("The GUID corresponds to XUL App ID.");
    let allIDs = await store.getAllIDs();
    let ids = Object.keys(allIDs);
    Assert.equal(ids.length, 1);
    Assert.equal(ids[0], PREFS_GUID);
    Assert.ok(allIDs[PREFS_GUID]);

    Assert.ok(await store.itemExists(PREFS_GUID));
    Assert.equal(false, await store.itemExists("random-gibberish"));

    _("Unknown prefs record is created as deleted.");
    let record = await store.createRecord("random-gibberish", "prefs");
    Assert.ok(record.deleted);

    _("Prefs record contains only prefs that should be synced.");
    record = await store.createRecord(PREFS_GUID, "prefs");
    Assert.strictEqual(record.value["testing.int"], 123);
    Assert.strictEqual(record.value["testing.string"], "ohai");
    Assert.strictEqual(record.value["testing.bool"], true);
    // non-existing prefs get null as the value
    Assert.strictEqual(record.value["testing.nonexistent"], null);
    // as do prefs that have a default value.
    Assert.strictEqual(record.value["testing.default"], null);
    Assert.strictEqual(record.value["testing.turned.off"], undefined);
    Assert.strictEqual(record.value["testing.not.turned.on"], undefined);

    _("Prefs record contains the correct control prefs.");
    // All control prefs which have the default value and where the pref
    // itself is synced should appear, but with null as the value.
    Assert.strictEqual(
      record.value["services.sync.prefs.sync.testing.int"],
      null
    );
    Assert.strictEqual(
      record.value["services.sync.prefs.sync.testing.string"],
      null
    );
    Assert.strictEqual(
      record.value["services.sync.prefs.sync.testing.bool"],
      null
    );
    Assert.strictEqual(
      record.value["services.sync.prefs.sync.testing.dont.change"],
      null
    );
    Assert.strictEqual(
      record.value["services.sync.prefs.sync.testing.nonexistent"],
      null
    );
    Assert.strictEqual(
      record.value["services.sync.prefs.sync.testing.default"],
      null
    );

    // but this control pref has a non-default value so that value is synced.
    Assert.strictEqual(
      record.value["services.sync.prefs.sync.testing.turned.off"],
      false
    );

    _("Unsyncable prefs are treated correctly.");
    // Prefs we consider unsyncable (since they are URLs that won't be stable on
    // another firefox) shouldn't be included - neither the value nor the
    // control pref should appear.
    Assert.strictEqual(record.value["testing.unsynced.url"], undefined);
    Assert.strictEqual(
      record.value["services.sync.prefs.sync.testing.unsynced.url"],
      undefined
    );
    // Other URLs with user prefs should be synced, though.
    Assert.strictEqual(
      record.value["testing.synced.url"],
      "https://www.example.com"
    );
    Assert.strictEqual(
      record.value["services.sync.prefs.sync.testing.synced.url"],
      null
    );

    _("Update some prefs, including one that's to be reset/deleted.");
    // This pref is not going to be reset or deleted as there's no "control pref"
    // in either the incoming record or locally.
    prefs.set(
      "testing.deleted-without-control-pref",
      "I'm deleted-without-control-pref"
    );
    // Another pref with only a local control pref.
    prefs.set(
      "testing.deleted-with-local-control-pref",
      "I'm deleted-with-local-control-pref"
    );
    prefs.set(
      "services.sync.prefs.sync.testing.deleted-with-local-control-pref",
      true
    );
    // And a pref without a local control pref but one that's incoming.
    prefs.set(
      "testing.deleted-with-incoming-control-pref",
      "I'm deleted-with-incoming-control-pref"
    );
    record = new PrefRec("prefs", PREFS_GUID);
    record.value = {
      "extensions.activeThemeID": DEFAULT_THEME_ID,
      "testing.int": 42,
      "testing.string": "im in ur prefs",
      "testing.bool": false,
      "testing.deleted-without-control-pref": null,
      "testing.deleted-with-local-control-pref": null,
      "testing.deleted-with-incoming-control-pref": null,
      "services.sync.prefs.sync.testing.deleted-with-incoming-control-pref": true,
      "testing.somepref": "im a new pref from other device",
      "services.sync.prefs.sync.testing.somepref": true,
      // Pretend some a stale remote client is overwriting it with a value
      // we consider unsyncable.
      "testing.synced.url": "blob:ebeb707a-502e-40c6-97a5-dd4bda901463",
      // Make sure we can replace the unsynced URL with a valid URL.
      "testing.unsynced.url": "https://www.example.com/2",
      // Make sure our "master control pref" is ignored.
      "services.sync.prefs.dangerously_allow_arbitrary": true,
      "services.sync.prefs.sync.services.sync.prefs.dangerously_allow_arbitrary": true,
    };

    const onceAddonEnabled = AddonTestUtils.promiseAddonEvent("onEnabled");

    await store.update(record);
    Assert.strictEqual(prefs.get("testing.int"), 42);
    Assert.strictEqual(prefs.get("testing.string"), "im in ur prefs");
    Assert.strictEqual(prefs.get("testing.bool"), false);
    Assert.strictEqual(
      prefs.get("testing.deleted-without-control-pref"),
      "I'm deleted-without-control-pref"
    );
    Assert.strictEqual(
      prefs.get("testing.deleted-with-local-control-pref"),
      undefined
    );
    Assert.strictEqual(
      prefs.get("testing.deleted-with-incoming-control-pref"),
      "I'm deleted-with-incoming-control-pref"
    );
    Assert.strictEqual(
      prefs.get("testing.dont.change"),
      "Please don't change me."
    );
    Assert.strictEqual(prefs.get("testing.somepref"), undefined);
    Assert.strictEqual(
      prefs.get("testing.synced.url"),
      "https://www.example.com"
    );
    Assert.strictEqual(
      prefs.get("testing.unsynced.url"),
      "https://www.example.com/2"
    );
    Assert.strictEqual(Svc.Prefs.get("prefs.sync.testing.somepref"), undefined);
    Assert.strictEqual(
      prefs.get("services.sync.prefs.dangerously_allow_arbitrary"),
      false
    );
    Assert.strictEqual(
      prefs.get(
        "services.sync.prefs.sync.services.sync.prefs.dangerously_allow_arbitrary"
      ),
      undefined
    );

    await onceAddonEnabled;
    ok(
      !defaultThemeAddon.userDisabled,
      "the default theme should have been enabled"
    );
    ok(
      otherThemeAddon.userDisabled,
      "the compact theme should have been disabled"
    );

    _("Only the current app's preferences are applied.");
    record = new PrefRec("prefs", "some-fake-app");
    record.value = {
      "testing.int": 98,
    };
    await store.update(record);
    Assert.equal(prefs.get("testing.int"), 42);
  } finally {
    prefs.resetBranch("");
  }
});

add_task(async function test_dangerously_allow() {
  _("services.sync.prefs.dangerously_allow_arbitrary");
  // read our custom prefs file before doing anything.
  Services.prefs.readDefaultPrefsFromFile(
    do_get_file("prefs_test_prefs_store.js")
  );
  // configure so that arbitrary prefs are synced.
  Services.prefs.setBoolPref(
    "services.sync.prefs.dangerously_allow_arbitrary",
    true
  );

  let engine = Service.engineManager.get("prefs");
  let store = engine._store;
  let prefs = new Preferences();
  try {
    _("Update some prefs");
    // This pref is not going to be reset or deleted as there's no "control pref"
    // in either the incoming record or locally.
    prefs.set(
      "testing.deleted-without-control-pref",
      "I'm deleted-without-control-pref"
    );
    // Another pref with only a local control pref.
    prefs.set(
      "testing.deleted-with-local-control-pref",
      "I'm deleted-with-local-control-pref"
    );
    prefs.set(
      "services.sync.prefs.sync.testing.deleted-with-local-control-pref",
      true
    );
    // And a pref without a local control pref but one that's incoming.
    prefs.set(
      "testing.deleted-with-incoming-control-pref",
      "I'm deleted-with-incoming-control-pref"
    );
    let record = new PrefRec("prefs", PREFS_GUID);
    record.value = {
      "testing.deleted-without-control-pref": null,
      "testing.deleted-with-local-control-pref": null,
      "testing.deleted-with-incoming-control-pref": null,
      "services.sync.prefs.sync.testing.deleted-with-incoming-control-pref": true,
      "testing.somepref": "im a new pref from other device",
      "services.sync.prefs.sync.testing.somepref": true,
      // Make sure our "master control pref" is ignored, even when it's already set.
      "services.sync.prefs.dangerously_allow_arbitrary": false,
      "services.sync.prefs.sync.services.sync.prefs.dangerously_allow_arbitrary": true,
    };
    await store.update(record);
    Assert.strictEqual(
      prefs.get("testing.deleted-without-control-pref"),
      "I'm deleted-without-control-pref"
    );
    Assert.strictEqual(
      prefs.get("testing.deleted-with-local-control-pref"),
      undefined
    );
    Assert.strictEqual(
      prefs.get("testing.deleted-with-incoming-control-pref"),
      undefined
    );
    Assert.strictEqual(
      prefs.get("testing.somepref"),
      "im a new pref from other device"
    );
    Assert.strictEqual(Svc.Prefs.get("prefs.sync.testing.somepref"), true);
    Assert.strictEqual(
      prefs.get("services.sync.prefs.dangerously_allow_arbitrary"),
      true
    );
    Assert.strictEqual(
      prefs.get(
        "services.sync.prefs.sync.services.sync.prefs.dangerously_allow_arbitrary"
      ),
      undefined
    );
  } finally {
    prefs.resetBranch("");
  }
});

add_task(async function test_incoming_sets_seen() {
  _("Test the sync-seen allow-list");

  let engine = Service.engineManager.get("prefs");
  let store = engine._store;
  let prefs = new Preferences();

  Services.prefs.readDefaultPrefsFromFile(
    do_get_file("prefs_test_prefs_store.js")
  );
  const defaultValue = "the value";
  Assert.equal(prefs.get("testing.seen"), defaultValue);

  let record = await store.createRecord(PREFS_GUID, "prefs");
  // Haven't seen a non-default value before, so remains null.
  Assert.strictEqual(record.value["testing.seen"], null);

  // pretend an incoming record with the default value - it might not be
  // the default everywhere, so we treat it specially.
  record = new PrefRec("prefs", PREFS_GUID);
  record.value = {
    "testing.seen": defaultValue,
  };
  await store.update(record);
  // Our special control value should now be set.
  Assert.strictEqual(
    prefs.get("services.sync.prefs.sync-seen.testing.seen"),
    true
  );
  // It's still the default value, so the value is not considered changed
  Assert.equal(prefs.isSet("testing.seen"), false);

  // But now that special control value is set, the record always contains the value.
  record = await store.createRecord(PREFS_GUID, "prefs");
  Assert.strictEqual(record.value["testing.seen"], defaultValue);
});

add_task(async function test_outgoing_when_changed() {
  _("Test the 'seen' pref is set first sync of non-default value");

  let engine = Service.engineManager.get("prefs");
  let store = engine._store;
  let prefs = new Preferences();
  prefs.resetBranch();

  Services.prefs.readDefaultPrefsFromFile(
    do_get_file("prefs_test_prefs_store.js")
  );
  const defaultValue = "the value";
  Assert.equal(prefs.get("testing.seen"), defaultValue);

  let record = await store.createRecord(PREFS_GUID, "prefs");
  // Haven't seen a non-default value before, so remains null.
  Assert.strictEqual(record.value["testing.seen"], null);

  // Change the value.
  prefs.set("testing.seen", "new value");
  record = await store.createRecord(PREFS_GUID, "prefs");
  // creating the record toggled that "seen" pref.
  Assert.strictEqual(
    prefs.get("services.sync.prefs.sync-seen.testing.seen"),
    true
  );
  Assert.strictEqual(prefs.get("testing.seen"), "new value");

  // Resetting the pref does not change that seen value.
  prefs.reset("testing.seen");
  Assert.strictEqual(prefs.get("testing.seen"), defaultValue);

  record = await store.createRecord(PREFS_GUID, "prefs");
  Assert.strictEqual(
    prefs.get("services.sync.prefs.sync-seen.testing.seen"),
    true
  );
});
