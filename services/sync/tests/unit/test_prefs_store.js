/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://gre/modules/LightweightThemeManager.jsm");
Cu.import("resource://gre/modules/Preferences.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://services-common/utils.js");
Cu.import("resource://services-sync/engines/prefs.js");
Cu.import("resource://services-sync/service.js");
Cu.import("resource://services-sync/util.js");

const PREFS_GUID = CommonUtils.encodeBase64URL(Services.appinfo.ID);

loadAddonTestFunctions();
startupManager();

function makePersona(id) {
  return {
    id: id || Math.random().toString(),
    name: Math.random().toString(),
    headerURL: "http://localhost:1234/a"
  };
}

function run_test() {
  _("Test fixtures.");
  // read our custom prefs file before doing anything.
  Services.prefs.readUserPrefs(do_get_file("prefs_test_prefs_store.js"));
  // Now we've read from this file, any writes the pref service makes will be
  // back to this prefs_test_prefs_store.js directly in the obj dir. This
  // upsets things in confusing ways :) We avoid this by explicitly telling the
  // pref service to use a file in our profile dir.
  let prefFile = do_get_profile();
  prefFile.append("prefs.js");
  Services.prefs.savePrefFile(prefFile);
  Services.prefs.readUserPrefs(prefFile);

  let store = Service.engineManager.get("prefs")._store;
  let prefs = new Preferences();
  try {

    _("The GUID corresponds to XUL App ID.");
    let allIDs = store.getAllIDs();
    let ids = Object.keys(allIDs);
    do_check_eq(ids.length, 1);
    do_check_eq(ids[0], PREFS_GUID);
    do_check_true(allIDs[PREFS_GUID], true);

    do_check_true(store.itemExists(PREFS_GUID));
    do_check_false(store.itemExists("random-gibberish"));

    _("Unknown prefs record is created as deleted.");
    let record = store.createRecord("random-gibberish", "prefs");
    do_check_true(record.deleted);

    _("Prefs record contains only prefs that should be synced.");
    record = store.createRecord(PREFS_GUID, "prefs");
    do_check_eq(record.value["testing.int"], 123);
    do_check_eq(record.value["testing.string"], "ohai");
    do_check_eq(record.value["testing.bool"], true);
    // non-existing prefs get null as the value
    do_check_eq(record.value["testing.nonexistent"], null);
    // as do prefs that have a default value.
    do_check_eq(record.value["testing.default"], null);
    do_check_false("testing.turned.off" in record.value);
    do_check_false("testing.not.turned.on" in record.value);

    _("Prefs record contains non-default pref sync prefs too.");
    do_check_eq(record.value["services.sync.prefs.sync.testing.int"], null);
    do_check_eq(record.value["services.sync.prefs.sync.testing.string"], null);
    do_check_eq(record.value["services.sync.prefs.sync.testing.bool"], null);
    do_check_eq(record.value["services.sync.prefs.sync.testing.dont.change"], null);
    // but this one is a user_pref so *will* be synced.
    do_check_eq(record.value["services.sync.prefs.sync.testing.turned.off"], false);
    do_check_eq(record.value["services.sync.prefs.sync.testing.nonexistent"], null);
    do_check_eq(record.value["services.sync.prefs.sync.testing.default"], null);

    _("Update some prefs, including one that's to be reset/deleted.");
    Svc.Prefs.set("testing.deleteme", "I'm going to be deleted!");
    record = new PrefRec("prefs", PREFS_GUID);
    record.value = {
      "testing.int": 42,
      "testing.string": "im in ur prefs",
      "testing.bool": false,
      "testing.deleteme": null,
      "testing.somepref": "im a new pref from other device",
      "services.sync.prefs.sync.testing.somepref": true
    };
    store.update(record);
    do_check_eq(prefs.get("testing.int"), 42);
    do_check_eq(prefs.get("testing.string"), "im in ur prefs");
    do_check_eq(prefs.get("testing.bool"), false);
    do_check_eq(prefs.get("testing.deleteme"), undefined);
    do_check_eq(prefs.get("testing.dont.change"), "Please don't change me.");
    do_check_eq(prefs.get("testing.somepref"), "im a new pref from other device");
    do_check_eq(Svc.Prefs.get("prefs.sync.testing.somepref"), true);

    _("Enable persona");
    // Ensure we don't go to the network to fetch personas and end up leaking
    // stuff.
    Services.io.offline = true;
    do_check_false(!!prefs.get("lightweightThemes.selectedThemeID"));
    do_check_eq(LightweightThemeManager.currentTheme, null);

    let persona1 = makePersona();
    let persona2 = makePersona();
    let usedThemes = JSON.stringify([persona1, persona2]);
    record.value = {
      "lightweightThemes.selectedThemeID": persona1.id,
      "lightweightThemes.usedThemes": usedThemes
    };
    store.update(record);
    do_check_eq(prefs.get("lightweightThemes.selectedThemeID"), persona1.id);
    do_check_true(Utils.deepEquals(LightweightThemeManager.currentTheme,
                  persona1));

    _("Disable persona");
    record.value = {
      "lightweightThemes.selectedThemeID": null,
      "lightweightThemes.usedThemes": usedThemes
    };
    store.update(record);
    do_check_false(!!prefs.get("lightweightThemes.selectedThemeID"));
    do_check_eq(LightweightThemeManager.currentTheme, null);

    _("Only the current app's preferences are applied.");
    record = new PrefRec("prefs", "some-fake-app");
    record.value = {
      "testing.int": 98
    };
    store.update(record);
    do_check_eq(prefs.get("testing.int"), 42);

  } finally {
    prefs.resetBranch("");
  }
}
