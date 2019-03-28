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
    Assert.equal(record.value["testing.int"], 123);
    Assert.equal(record.value["testing.string"], "ohai");
    Assert.equal(record.value["testing.bool"], true);
    // non-existing prefs get null as the value
    Assert.equal(record.value["testing.nonexistent"], null);
    // as do prefs that have a default value.
    Assert.equal(record.value["testing.default"], null);
    Assert.equal(false, "testing.turned.off" in record.value);
    Assert.equal(false, "testing.not.turned.on" in record.value);
    // Prefs we consider unsyncable (since they are URLs that won't be stable on
    // another firefox) shouldn't be included
    Assert.equal(record.value["testing.unsynced.url"], null);
    // Other URLs with user prefs should be synced, though.
    Assert.equal(record.value["testing.synced.url"], "https://www.example.com");

    _("Prefs record contains non-default pref sync prefs too.");
    Assert.equal(record.value["services.sync.prefs.sync.testing.int"], null);
    Assert.equal(record.value["services.sync.prefs.sync.testing.string"], null);
    Assert.equal(record.value["services.sync.prefs.sync.testing.bool"], null);
    Assert.equal(record.value["services.sync.prefs.sync.testing.dont.change"], null);
    Assert.equal(record.value["services.sync.prefs.sync.testing.synced.url"], null);
    // but this one is a user_pref so *will* be synced.
    Assert.equal(record.value["services.sync.prefs.sync.testing.turned.off"], false);
    Assert.equal(record.value["services.sync.prefs.sync.testing.nonexistent"], null);
    Assert.equal(record.value["services.sync.prefs.sync.testing.default"], null);
    // This is a user pref, but shouldn't be synced since it refers to an invalid url
    Assert.equal(record.value["services.sync.prefs.sync.testing.unsynced.url"], null);

    _("Update some prefs, including one that's to be reset/deleted.");
    Svc.Prefs.set("testing.deleteme", "I'm going to be deleted!");
    record = new PrefRec("prefs", PREFS_GUID);
    record.value = {
      "testing.int": 42,
      "testing.string": "im in ur prefs",
      "testing.bool": false,
      "testing.deleteme": null,
      "testing.somepref": "im a new pref from other device",
      "services.sync.prefs.sync.testing.somepref": true,
      // Pretend some a stale remote client is overwriting it with a value
      // we consider unsyncable.
      "testing.synced.url": "blob:ebeb707a-502e-40c6-97a5-dd4bda901463",
      // Make sure we can replace the unsynced URL with a valid URL.
      "testing.unsynced.url": "https://www.example.com/2",
    };
    await store.update(record);
    Assert.equal(prefs.get("testing.int"), 42);
    Assert.equal(prefs.get("testing.string"), "im in ur prefs");
    Assert.equal(prefs.get("testing.bool"), false);
    Assert.equal(prefs.get("testing.deleteme"), undefined);
    Assert.equal(prefs.get("testing.dont.change"), "Please don't change me.");
    Assert.equal(prefs.get("testing.somepref"), "im a new pref from other device");
    Assert.equal(prefs.get("testing.synced.url"), "https://www.example.com");
    Assert.equal(prefs.get("testing.unsynced.url"), "https://www.example.com/2");
    Assert.equal(Svc.Prefs.get("prefs.sync.testing.somepref"), true);

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
