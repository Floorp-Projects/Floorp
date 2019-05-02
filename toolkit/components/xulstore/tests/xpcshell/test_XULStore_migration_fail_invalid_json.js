"use strict";

const {AppConstants} = ChromeUtils.import("resource://gre/modules/AppConstants.jsm");
const {OS} = ChromeUtils.import("resource://gre/modules/osfile.jsm");

function run_test() {
  do_get_profile();
  run_next_test();
}

add_task({
  skip_if: () => !AppConstants.MOZ_NEW_XULSTORE,
}, async function test_create_old_datastore() {
  const path = OS.Path.join(OS.Constants.Path.profileDir, "xulstore.json");

  // Invalid JSON: it's missing the final closing brace.
  const xulstoreJSON = '{ doc: { id: { attr: "value" } }';

  await OS.File.writeAtomic(path, xulstoreJSON);
});

add_task({
  skip_if: () => !AppConstants.MOZ_NEW_XULSTORE,
}, async function test_get_value() {
  // We wait until now to import XULStore to ensure we've created
  // the old store, as importing that module will initiate the attempt
  // to migrate the old store to the new one.
  const {XULStore} = ChromeUtils.import("resource://gre/modules/XULStore.jsm");

  // XULStore should *not* have migrated the value from the old store,
  // so it should return an empty string when we try to retrieve it.
  Assert.equal(await XULStore.getValue("doc", "id", "attr"), "");
});
