Cu.import("resource://services-sync/engines/prefs.js");
Cu.import("resource://services-sync/type_records/prefs.js");
Cu.import("resource://services-sync/util.js");
Cu.import("resource://services-sync/ext/Preferences.js");

function run_test() {
  let store = new PrefsEngine()._store;
  let prefs = new Preferences();
  try {

    _("Test fixtures.");
    Svc.Prefs.set("prefs.sync.testing.int", true);
    Svc.Prefs.set("prefs.sync.testing.string", true);
    Svc.Prefs.set("prefs.sync.testing.bool", true);
    Svc.Prefs.set("prefs.sync.testing.dont.change", true);
    Svc.Prefs.set("prefs.sync.testing.turned.off", false);
    Svc.Prefs.set("prefs.sync.testing.nonexistent", true);

    prefs.set("testing.int", 123);
    prefs.set("testing.string", "ohai");
    prefs.set("testing.bool", true);
    prefs.set("testing.dont.change", "Please don't change me.");
    prefs.set("testing.turned.off", "I won't get synced.");
    prefs.set("testing.not.turned.on", "I won't get synced either!");

    _("The GUID corresponds to XUL App ID.");
    let allIDs = store.getAllIDs();
    let ids = [id for (id in allIDs)];
    do_check_eq(ids.length, 1);
    do_check_eq(ids[0], Svc.AppInfo.ID);
    do_check_true(allIDs[Svc.AppInfo.ID], true);

    do_check_true(store.itemExists(Svc.AppInfo.ID));
    do_check_false(store.itemExists("random-gibberish"));

    _("Unknown prefs record is created as deleted.");
    let record = store.createRecord("random-gibberish", "http://fake/uri");
    do_check_true(record.deleted);

    _("Prefs record contains only prefs that should be synced.");
    record = store.createRecord(Svc.AppInfo.ID, "http://fake/uri");
    do_check_eq(record.value["testing.int"], 123);
    do_check_eq(record.value["testing.string"], "ohai");
    do_check_eq(record.value["testing.bool"], true);
    do_check_eq(record.value["testing.nonexistent"], null);
    do_check_false("testing.turned.off" in record.value);
    do_check_false("testing.not.turned.on" in record.value);

    _("Prefs record contains pref sync prefs too.");
    do_check_eq(record.value["services.sync.prefs.sync.testing.int"], true);
    do_check_eq(record.value["services.sync.prefs.sync.testing.string"], true);
    do_check_eq(record.value["services.sync.prefs.sync.testing.bool"], true);
    do_check_eq(record.value["services.sync.prefs.sync.testing.dont.change"], true);
    do_check_eq(record.value["services.sync.prefs.sync.testing.turned.off"], false);
    do_check_eq(record.value["services.sync.prefs.sync.testing.nonexistent"], true);

    _("Update some prefs, including one that's to be reset/deleted.");
    Svc.Prefs.set("testing.deleteme", "I'm going to be deleted!"); 
    record = new PrefRec("http://fake/uri");
    record.value = {
      "testing.int": 42,
      "testing.string": "im in ur prefs",
      "testing.bool": false,
      "testing.deleteme": null,
      "services.sync.prefs.sync.testing.somepref": true
    };
    store.update(record);
    do_check_eq(prefs.get("testing.int"), 42);
    do_check_eq(prefs.get("testing.string"), "im in ur prefs");
    do_check_eq(prefs.get("testing.bool"), false);
    do_check_eq(prefs.get("testing.deleteme"), undefined);
    do_check_eq(prefs.get("testing.dont.change"), "Please don't change me.");
    do_check_eq(Svc.Prefs.get("prefs.sync.testing.somepref"), true);

  } finally {
    prefs.resetBranch("");
  }
}
