/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

_("Make sure the form store follows the Store api and correctly accesses the backend form storage");
Cu.import("resource://services-sync/engines/forms.js");
Cu.import("resource://services-sync/service.js");
Cu.import("resource://services-sync/util.js");
Cu.import("resource://gre/modules/Services.jsm");

function run_test() {
  let engine = new FormEngine(Service);
  let store = engine._store;

  function applyEnsureNoFailures(records) {
    do_check_eq(store.applyIncomingBatch(records).length, 0);
  }

  _("Remove any existing entries");
  store.wipe();
  if (store.getAllIDs().length) {
    do_throw("Shouldn't get any ids!");
  }

  _("Add a form entry");
  applyEnsureNoFailures([{
    id: Utils.makeGUID(),
    name: "name!!",
    value: "value??"
  }]);

  _("Should have 1 entry now");
  let id = "";
  for (let _id in store.getAllIDs()) {
    if (id == "")
      id = _id;
    else
      do_throw("Should have only gotten one!");
  }
  do_check_true(store.itemExists(id));

  _("Should be able to find this entry as a dupe");
  do_check_eq(engine._findDupe({name: "name!!", value: "value??"}), id);

  let rec = store.createRecord(id);
  _("Got record for id", id, rec);
  do_check_eq(rec.name, "name!!");
  do_check_eq(rec.value, "value??");

  _("Create a non-existent id for delete");
  do_check_true(store.createRecord("deleted!!").deleted);

  _("Try updating.. doesn't do anything yet");
  store.update({});

  _("Remove all entries");
  store.wipe();
  if (store.getAllIDs().length) {
    do_throw("Shouldn't get any ids!");
  }

  _("Add another entry");
  applyEnsureNoFailures([{
    id: Utils.makeGUID(),
    name: "another",
    value: "entry"
  }]);
  id = "";
  for (let _id in store.getAllIDs()) {
    if (id == "")
      id = _id;
    else
      do_throw("Should have only gotten one!");
  }

  _("Change the id of the new entry to something else");
  store.changeItemID(id, "newid");

  _("Make sure it's there");
  do_check_true(store.itemExists("newid"));

  _("Remove the entry");
  store.remove({
    id: "newid"
  });
  if (store.getAllIDs().length) {
    do_throw("Shouldn't get any ids!");
  }

  _("Removing the entry again shouldn't matter");
  store.remove({
    id: "newid"
  });
  if (store.getAllIDs().length) {
    do_throw("Shouldn't get any ids!");
  }

  _("Add another entry to delete using applyIncomingBatch");
  let toDelete = {
    id: Utils.makeGUID(),
    name: "todelete",
    value: "entry"
  };
  applyEnsureNoFailures([toDelete]);
  id = "";
  for (let _id in store.getAllIDs()) {
    if (id == "")
      id = _id;
    else
      do_throw("Should have only gotten one!");
  }
  do_check_true(store.itemExists(id));
  // mark entry as deleted
  toDelete.id = id;
  toDelete.deleted = true;
  applyEnsureNoFailures([toDelete]);
  if (store.getAllIDs().length) {
    do_throw("Shouldn't get any ids!");
  }

  _("Add an entry to wipe");
  applyEnsureNoFailures([{
    id: Utils.makeGUID(),
    name: "towipe",
    value: "entry"
  }]);

  store.wipe();

  if (store.getAllIDs().length) {
    do_throw("Shouldn't get any ids!");
  }

  _("Ensure we work if formfill is disabled.");
  Services.prefs.setBoolPref("browser.formfill.enable", false);
  try {
    // a search
    if (store.getAllIDs().length) {
      do_throw("Shouldn't get any ids!");
    }
    // an update.
    applyEnsureNoFailures([{
      id: Utils.makeGUID(),
      name: "some",
      value: "entry"
    }]);
  } finally {
    Services.prefs.clearUserPref("browser.formfill.enable");
    store.wipe();
  }
}
