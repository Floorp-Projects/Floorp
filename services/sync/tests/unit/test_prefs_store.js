/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const {Preferences} = ChromeUtils.import("resource://gre/modules/Preferences.jsm");
const {PrefRec} = ChromeUtils.import("resource://services-sync/engines/prefs.js");
const {Service} = ChromeUtils.import("resource://services-sync/service.js");

const PREFS_GUID = CommonUtils.encodeBase64URL(Services.appinfo.ID);

AddonTestUtils.init(this);
AddonTestUtils.createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");
AddonTestUtils.overrideCertDB();
AddonTestUtils.awaitPromise(AddonTestUtils.promiseStartupManager());

function makePersona(id) {
  return {
    id: id || Math.random().toString(),
    name: Math.random().toString(),
    headerURL: "http://localhost:1234/a",
  };
}

add_task(async function run_test() {
  _("Test fixtures.");
  // read our custom prefs file before doing anything.
  Services.prefs.readDefaultPrefsFromFile(do_get_file("prefs_test_prefs_store.js"));

  let engine = Service.engineManager.get("prefs");
  let store = engine._store;
  let prefs = new Preferences();
  try {
    _("The GUID corresponds to XUL App ID.");
    let allIDs = await store.getAllIDs();
    let ids = Object.keys(allIDs);
    Assert.equal(ids.length, 1);
    Assert.equal(ids[0], PREFS_GUID);
    Assert.ok(allIDs[PREFS_GUID]);

    Assert.ok((await store.itemExists(PREFS_GUID)));
    Assert.equal(false, (await store.itemExists("random-gibberish")));

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
    Assert.strictEqual(record.value["services.sync.prefs.sync.testing.int"], null);
    Assert.strictEqual(record.value["services.sync.prefs.sync.testing.string"], null);
    Assert.strictEqual(record.value["services.sync.prefs.sync.testing.bool"], null);
    Assert.strictEqual(record.value["services.sync.prefs.sync.testing.dont.change"], null);
    Assert.strictEqual(record.value["services.sync.prefs.sync.testing.nonexistent"], null);
    Assert.strictEqual(record.value["services.sync.prefs.sync.testing.default"], null);

    // but this control pref has a non-default value so that value is synced.
    Assert.strictEqual(record.value["services.sync.prefs.sync.testing.turned.off"], false);

    _("Unsyncable prefs are treated correctly.");
    // Prefs we consider unsyncable (since they are URLs that won't be stable on
    // another firefox) shouldn't be included - neither the value nor the
    // control pref should appear.
    Assert.strictEqual(record.value["testing.unsynced.url"], undefined);
    Assert.strictEqual(record.value["services.sync.prefs.sync.testing.unsynced.url"], undefined);
    // Other URLs with user prefs should be synced, though.
    Assert.strictEqual(record.value["testing.synced.url"], "https://www.example.com");
    Assert.strictEqual(record.value["services.sync.prefs.sync.testing.synced.url"], null);

    _("Update some prefs, including one that's to be reset/deleted.");
    // This pref is not going to be reset or deleted as there's no "control pref"
    // in either the incoming record or locally.
    prefs.set("testing.deleted-without-control-pref", "I'm deleted-without-control-pref");
    // Another pref with only a local control pref.
    prefs.set("testing.deleted-with-local-control-pref", "I'm deleted-with-local-control-pref");
    prefs.set("services.sync.prefs.sync.testing.deleted-with-local-control-pref", true);
    // And a pref without a local control pref but one that's incoming.
    prefs.set("testing.deleted-with-incoming-control-pref", "I'm deleted-with-incoming-control-pref");
    record = new PrefRec("prefs", PREFS_GUID);
    record.value = {
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
    await store.update(record);
    Assert.strictEqual(prefs.get("testing.int"), 42);
    Assert.strictEqual(prefs.get("testing.string"), "im in ur prefs");
    Assert.strictEqual(prefs.get("testing.bool"), false);
    Assert.strictEqual(prefs.get("testing.deleted-without-control-pref"), "I'm deleted-without-control-pref");
    Assert.strictEqual(prefs.get("testing.deleted-with-local-control-pref"), undefined);
    Assert.strictEqual(prefs.get("testing.deleted-with-incoming-control-pref"), "I'm deleted-with-incoming-control-pref");
    Assert.strictEqual(prefs.get("testing.dont.change"), "Please don't change me.");
    Assert.strictEqual(prefs.get("testing.somepref"), undefined);
    Assert.strictEqual(prefs.get("testing.synced.url"), "https://www.example.com");
    Assert.strictEqual(prefs.get("testing.unsynced.url"), "https://www.example.com/2");
    Assert.strictEqual(Svc.Prefs.get("prefs.sync.testing.somepref"), undefined);
    Assert.strictEqual(prefs.get("services.sync.prefs.dangerously_allow_arbitrary"), false);
    Assert.strictEqual(prefs.get("services.sync.prefs.sync.services.sync.prefs.dangerously_allow_arbitrary"), undefined);

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
  Services.prefs.readDefaultPrefsFromFile(do_get_file("prefs_test_prefs_store.js"));
  // configure so that arbitrary prefs are synced.
  Services.prefs.setBoolPref("services.sync.prefs.dangerously_allow_arbitrary", true);

  let engine = Service.engineManager.get("prefs");
  let store = engine._store;
  let prefs = new Preferences();
  try {
    _("Update some prefs");
    // This pref is not going to be reset or deleted as there's no "control pref"
    // in either the incoming record or locally.
    prefs.set("testing.deleted-without-control-pref", "I'm deleted-without-control-pref");
    // Another pref with only a local control pref.
    prefs.set("testing.deleted-with-local-control-pref", "I'm deleted-with-local-control-pref");
    prefs.set("services.sync.prefs.sync.testing.deleted-with-local-control-pref", true);
    // And a pref without a local control pref but one that's incoming.
    prefs.set("testing.deleted-with-incoming-control-pref", "I'm deleted-with-incoming-control-pref");
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
    Assert.strictEqual(prefs.get("testing.deleted-without-control-pref"), "I'm deleted-without-control-pref");
    Assert.strictEqual(prefs.get("testing.deleted-with-local-control-pref"), undefined);
    Assert.strictEqual(prefs.get("testing.deleted-with-incoming-control-pref"), undefined);
    Assert.strictEqual(prefs.get("testing.somepref"), "im a new pref from other device");
    Assert.strictEqual(Svc.Prefs.get("prefs.sync.testing.somepref"), true);
    Assert.strictEqual(prefs.get("services.sync.prefs.dangerously_allow_arbitrary"), true);
    Assert.strictEqual(prefs.get("services.sync.prefs.sync.services.sync.prefs.dangerously_allow_arbitrary"), undefined);
  } finally {
    prefs.resetBranch("");
  }
});
