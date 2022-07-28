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

    const xulstoreJSON = {
      doc1: {
        id1: {
          attr1: "value1",
        },
      },
      doc2: {
        id1: {
          attr2: "value2",
        },
        id2: {
          attr1: "value1",
          attr2: "value2",
          attr3: "value3",
        },
        id3: {},
      },
      doc3: {},
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
    // the old datastore, as importing that module will initiate the attempt
    // to migrate the old datastore to the new one.
    const { XULStore } = ChromeUtils.import(
      "resource://gre/modules/XULStore.jsm"
    );

    Assert.equal(await XULStore.getValue("doc1", "id1", "attr1"), "value1");
    Assert.equal(await XULStore.getValue("doc1", "id1", "attr2"), "");
    Assert.equal(await XULStore.getValue("doc1", "id1", "attr3"), "");
    Assert.equal(await XULStore.getValue("doc1", "id2", "attr1"), "");
    Assert.equal(await XULStore.getValue("doc1", "id2", "attr2"), "");
    Assert.equal(await XULStore.getValue("doc1", "id2", "attr3"), "");
    Assert.equal(await XULStore.getValue("doc1", "id3", "attr1"), "");
    Assert.equal(await XULStore.getValue("doc1", "id3", "attr2"), "");
    Assert.equal(await XULStore.getValue("doc1", "id3", "attr3"), "");

    Assert.equal(await XULStore.getValue("doc2", "id1", "attr1"), "");
    Assert.equal(await XULStore.getValue("doc2", "id1", "attr2"), "value2");
    Assert.equal(await XULStore.getValue("doc2", "id1", "attr3"), "");
    Assert.equal(await XULStore.getValue("doc2", "id2", "attr1"), "value1");
    Assert.equal(await XULStore.getValue("doc2", "id2", "attr2"), "value2");
    Assert.equal(await XULStore.getValue("doc2", "id2", "attr3"), "value3");
    Assert.equal(await XULStore.getValue("doc2", "id3", "attr1"), "");
    Assert.equal(await XULStore.getValue("doc2", "id3", "attr2"), "");
    Assert.equal(await XULStore.getValue("doc2", "id3", "attr3"), "");

    Assert.equal(await XULStore.getValue("doc3", "id1", "attr1"), "");
    Assert.equal(await XULStore.getValue("doc3", "id1", "attr2"), "");
    Assert.equal(await XULStore.getValue("doc3", "id1", "attr3"), "");
    Assert.equal(await XULStore.getValue("doc3", "id2", "attr1"), "");
    Assert.equal(await XULStore.getValue("doc3", "id2", "attr2"), "");
    Assert.equal(await XULStore.getValue("doc3", "id2", "attr3"), "");
    Assert.equal(await XULStore.getValue("doc3", "id3", "attr1"), "");
    Assert.equal(await XULStore.getValue("doc3", "id3", "attr2"), "");
    Assert.equal(await XULStore.getValue("doc3", "id3", "attr3"), "");
  }
);
