"use strict";

const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);
const { FileUtils } = ChromeUtils.import(
  "resource://gre/modules/FileUtils.jsm"
);

add_task(
  {
    skip_if: () => !AppConstants.MOZ_NEW_XULSTORE,
  },
  async function test_get_values() {
    // Import XULStore before getting the profile to ensure that the new store
    // is initialized, as the purpose of this test is to confirm that the old
    // store data gets migrated if the profile change happens post-initialization.
    const { XULStore } = ChromeUtils.import(
      "resource://gre/modules/XULStore.jsm"
    );

    // We haven't migrated any data yet (nor even changed to a profile), so there
    // shouldn't be a value in the store.
    Assert.equal(XULStore.getValue("doc1", "id1", "attr1"), "");

    // Register an observer before the XULStore service registers its observer,
    // so we can observe the profile-after-change notification first and create
    // an old store for it to migrate.  We need to write synchronously to avoid
    // racing XULStore, so we use FileUtils instead of IOUtils.
    Services.obs.addObserver(
      {
        observe() {
          const file = FileUtils.getFile("ProfD", ["xulstore.json"]);
          const xulstoreJSON = JSON.stringify({
            doc1: {
              id1: {
                attr1: "value1",
              },
            },
          });
          let stream = FileUtils.openAtomicFileOutputStream(file);
          stream.write(xulstoreJSON, xulstoreJSON.length);
          FileUtils.closeAtomicFileOutputStream(stream);
        },
      },
      "profile-after-change"
    );

    // This creates a profile and changes to it, triggering first our
    // profile-after-change observer above and then XULStore's equivalent.
    do_get_profile(true);

    // XULStore should now have migrated the value from the old store.
    Assert.equal(XULStore.getValue("doc1", "id1", "attr1"), "value1");
  }
);
