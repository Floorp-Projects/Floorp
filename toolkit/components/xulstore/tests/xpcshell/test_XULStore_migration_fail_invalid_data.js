"use strict";

const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);

function run_test() {
  do_get_profile();
  run_next_test();
}

add_task(
  {
    skip_if: () => !AppConstants.MOZ_NEW_XULSTORE,
  },
  async function test_create_old_datastore() {
    const path = PathUtils.join(PathUtils.profileDir, "xulstore.json");

    // Valid JSON, but invalid data: attr1's value is a number, not a string.
    const xulstoreJSON = {
      doc1: {
        id1: {
          attr1: 1,
        },
      },
      doc2: {
        id2: {
          attr2: "value2",
        },
      },
    };

    await IOUtils.writeJSON(path, xulstoreJSON);
  }
);

add_task(
  {
    skip_if: () => !AppConstants.MOZ_NEW_XULSTORE,
  },
  async function test_get_values() {
    // We wait until now to import XULStore to ensure we've created
    // the old store, as importing that module will initiate the attempt
    // to migrate the old store to the new one.
    const { XULStore } = ChromeUtils.import(
      "resource://gre/modules/XULStore.jsm"
    );

    // XULStore should *not* have migrated the values from the old store,
    // so it should return empty strings when we try to retrieve them.
    // That's true for both values, even though one of them is valid,
    // because the migrator uses a typed parser that requires the entire
    // JSON file to conform to the XULStore format.
    Assert.equal(await XULStore.getValue("doc1", "id1", "attr1"), "");
    Assert.equal(await XULStore.getValue("doc2", "id2", "attr2"), "");
  }
);
