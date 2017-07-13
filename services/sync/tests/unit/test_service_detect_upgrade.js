/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://gre/modules/Log.jsm");
Cu.import("resource://services-sync/constants.js");
Cu.import("resource://services-sync/keys.js");
Cu.import("resource://services-sync/engines/tabs.js");
Cu.import("resource://services-sync/engines.js");
Cu.import("resource://services-sync/record.js");
Cu.import("resource://services-sync/service.js");
Cu.import("resource://services-sync/util.js");
Cu.import("resource://testing-common/services/sync/utils.js");

add_task(async function v4_upgrade() {
  enableValidationPrefs();

  let clients = new ServerCollection();
  let meta_global = new ServerWBO("global");

  // Tracking info/collections.
  let collectionsHelper = track_collections_helper();
  let upd = collectionsHelper.with_updated_collection;
  let collections = collectionsHelper.collections;

  let keysWBO = new ServerWBO("keys");
  let server = httpd_setup({
    // Special.
    "/1.1/johndoe/info/collections": collectionsHelper.handler,
    "/1.1/johndoe/storage/crypto/keys": upd("crypto", keysWBO.handler()),
    "/1.1/johndoe/storage/meta/global": upd("meta", meta_global.handler()),

    // Track modified times.
    "/1.1/johndoe/storage/clients": upd("clients", clients.handler()),
    "/1.1/johndoe/storage/tabs": upd("tabs", new ServerCollection().handler()),

    // Just so we don't get 404s in the logs.
    "/1.1/johndoe/storage/bookmarks": new ServerCollection().handler(),
    "/1.1/johndoe/storage/forms": new ServerCollection().handler(),
    "/1.1/johndoe/storage/history": new ServerCollection().handler(),
    "/1.1/johndoe/storage/passwords": new ServerCollection().handler(),
    "/1.1/johndoe/storage/prefs": new ServerCollection().handler()
  });

  try {

    Service.status.resetSync();

    _("Logging in.");

    await configureIdentity({ "username": "johndoe" }, server);

    await Service.login();
    do_check_true(Service.isLoggedIn);
    await Service.verifyAndFetchSymmetricKeys();
    do_check_true((await Service._remoteSetup()));

    async function test_out_of_date() {
      _("Old meta/global: " + JSON.stringify(meta_global));
      meta_global.payload = JSON.stringify({"syncID": "foooooooooooooooooooooooooo",
                                            "storageVersion": STORAGE_VERSION + 1});
      collections.meta = Date.now() / 1000;
      _("New meta/global: " + JSON.stringify(meta_global));
      Service.recordManager.set(Service.metaURL, meta_global);
      try {
        await Service.sync();
      } catch (ex) {
      }
      do_check_eq(Service.status.sync, VERSION_OUT_OF_DATE);
    }

    // See what happens when we bump the storage version.
    _("Syncing after server has been upgraded.");
    await test_out_of_date();

    // Same should happen after a wipe.
    _("Syncing after server has been upgraded and wiped.");
    await Service.wipeServer();
    await test_out_of_date();

    // Now's a great time to test what happens when keys get replaced.
    _("Syncing afresh...");
    Service.logout();
    Service.collectionKeys.clear();
    meta_global.payload = JSON.stringify({"syncID": "foooooooooooooobbbbbbbbbbbb",
                                          "storageVersion": STORAGE_VERSION});
    collections.meta = Date.now() / 1000;
    Service.recordManager.set(Service.metaURL, meta_global);
    await Service.login();
    do_check_true(Service.isLoggedIn);
    await Service.sync();
    do_check_true(Service.isLoggedIn);

    let serverDecrypted;
    let serverKeys;
    let serverResp;


    async function retrieve_server_default() {
      serverKeys = serverResp = serverDecrypted = null;

      serverKeys = new CryptoWrapper("crypto", "keys");
      serverResp = (await serverKeys.fetch(Service.resource(Service.cryptoKeysURL))).response;
      do_check_true(serverResp.success);

      serverDecrypted = serverKeys.decrypt(Service.identity.syncKeyBundle);
      _("Retrieved WBO:       " + JSON.stringify(serverDecrypted));
      _("serverKeys:          " + JSON.stringify(serverKeys));

      return serverDecrypted.default;
    }

    async function retrieve_and_compare_default(should_succeed) {
      let serverDefault = await retrieve_server_default();
      let localDefault = Service.collectionKeys.keyForCollection().keyPairB64;

      _("Retrieved keyBundle: " + JSON.stringify(serverDefault));
      _("Local keyBundle:     " + JSON.stringify(localDefault));

      if (should_succeed)
        do_check_eq(JSON.stringify(serverDefault), JSON.stringify(localDefault));
      else
        do_check_neq(JSON.stringify(serverDefault), JSON.stringify(localDefault));
    }

    // Uses the objects set above.
    async function set_server_keys(pair) {
      serverDecrypted.default = pair;
      serverKeys.cleartext = serverDecrypted;
      serverKeys.encrypt(Service.identity.syncKeyBundle);
      await serverKeys.upload(Service.resource(Service.cryptoKeysURL));
    }

    _("Checking we have the latest keys.");
    await retrieve_and_compare_default(true);

    _("Update keys on server.");
    await set_server_keys(["KaaaaaaaaaaaHAtfmuRY0XEJ7LXfFuqvF7opFdBD/MY=",
                           "aaaaaaaaaaaapxMO6TEWtLIOv9dj6kBAJdzhWDkkkis="]);

    _("Checking that we no longer have the latest keys.");
    await retrieve_and_compare_default(false);

    _("Indeed, they're what we set them to...");
    do_check_eq("KaaaaaaaaaaaHAtfmuRY0XEJ7LXfFuqvF7opFdBD/MY=",
                (await retrieve_server_default())[0]);

    _("Sync. Should download changed keys automatically.");
    let oldClientsModified = collections.clients;
    let oldTabsModified = collections.tabs;

    await Service.login();
    await Service.sync();
    _("New key should have forced upload of data.");
    _("Tabs: " + oldTabsModified + " < " + collections.tabs);
    _("Clients: " + oldClientsModified + " < " + collections.clients);
    do_check_true(collections.clients > oldClientsModified);
    do_check_true(collections.tabs > oldTabsModified);

    _("... and keys will now match.");
    await retrieve_and_compare_default(true);

    // Clean up.
    await Service.startOver();

  } finally {
    Svc.Prefs.resetBranch("");
    await promiseStopServer(server);
  }
});

add_task(async function v5_upgrade() {
  enableValidationPrefs();

  // Tracking info/collections.
  let collectionsHelper = track_collections_helper();
  let upd = collectionsHelper.with_updated_collection;

  let keysWBO = new ServerWBO("keys");
  let bulkWBO = new ServerWBO("bulk");
  let clients = new ServerCollection();
  let meta_global = new ServerWBO("global");

  let server = httpd_setup({
    // Special.
    "/1.1/johndoe/storage/meta/global": upd("meta", meta_global.handler()),
    "/1.1/johndoe/info/collections": collectionsHelper.handler,
    "/1.1/johndoe/storage/crypto/keys": upd("crypto", keysWBO.handler()),
    "/1.1/johndoe/storage/crypto/bulk": upd("crypto", bulkWBO.handler()),

    // Track modified times.
    "/1.1/johndoe/storage/clients": upd("clients", clients.handler()),
    "/1.1/johndoe/storage/tabs": upd("tabs", new ServerCollection().handler()),
  });

  try {
    Service.status.resetSync();

    Service.clusterURL = server.baseURI + "/";

    await configureIdentity({ "username": "johndoe" }, server);

    // Test an upgrade where the contents of the server would cause us to error
    // -- keys decrypted with a different sync key, for example.
    _("Testing v4 -> v5 (or similar) upgrade.");
    async function update_server_keys(syncKeyBundle, wboName, collWBO) {
      generateNewKeys(Service.collectionKeys);
      let serverKeys = Service.collectionKeys.asWBO("crypto", wboName);
      serverKeys.encrypt(syncKeyBundle);
      let res = Service.resource(Service.storageURL + collWBO);
      do_check_true((await serverKeys.upload(res)).success);
    }

    _("Bumping version.");
    // Bump version on the server.
    let m = new WBORecord("meta", "global");
    m.payload = {"syncID": "foooooooooooooooooooooooooo",
                 "storageVersion": STORAGE_VERSION + 1};
    await m.upload(Service.resource(Service.metaURL));

    _("New meta/global: " + JSON.stringify(meta_global));

    // Fill the keys with bad data.
    let badKeys = new SyncKeyBundle("foobar", "aaaaaaaaaaaaaaaaaaaaaaaaaa");
    await update_server_keys(badKeys, "keys", "crypto/keys");  // v4
    await update_server_keys(badKeys, "bulk", "crypto/bulk");  // v5

    _("Generating new keys.");
    generateNewKeys(Service.collectionKeys);

    // Now sync and see what happens. It should be a version fail, not a crypto
    // fail.

    _("Logging in.");
    try {
      await Service.login();
    } catch (e) {
      _("Exception: " + e);
    }
    _("Status: " + Service.status);
    do_check_false(Service.isLoggedIn);
    do_check_eq(VERSION_OUT_OF_DATE, Service.status.sync);

    // Clean up.
    await Service.startOver();

  } finally {
    Svc.Prefs.resetBranch("");
    await promiseStopServer(server);
  }
});

function run_test() {
  Log.repository.rootLogger.addAppender(new Log.DumpAppender());

  run_next_test();
}
