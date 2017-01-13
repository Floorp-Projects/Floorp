/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://services-sync/constants.js");
Cu.import("resource://services-sync/engines/history.js");
Cu.import("resource://services-sync/engines.js");
Cu.import("resource://services-sync/identity.js");
Cu.import("resource://services-sync/record.js");
Cu.import("resource://services-sync/service.js");
Cu.import("resource://services-sync/util.js");
Cu.import("resource://testing-common/services/sync/utils.js");

Service.engineManager.clear();

add_test(function test_setup() {
  PlacesTestUtils.clearHistory().then(run_next_test);
});

add_task(async function test_processIncoming_mobile_history_batched() {
  _("SyncEngine._processIncoming works on history engine.");

  let FAKE_DOWNLOAD_LIMIT = 100;

  Svc.Prefs.set("client.type", "mobile");
  Service.engineManager.register(HistoryEngine);

  // A collection that logs each GET
  let collection = new ServerCollection();
  collection.get_log = [];
  collection._get = collection.get;
  collection.get = function(options) {
    this.get_log.push(options);
    return this._get(options);
  };

  let server = sync_httpd_setup({
    "/1.1/foo/storage/history": collection.handler()
  });

  await SyncTestingInfrastructure(server);

  // Let's create some 234 server side history records. They're all at least
  // 10 minutes old.
  let visitType = Ci.nsINavHistoryService.TRANSITION_LINK;
  for (var i = 0; i < 234; i++) {
    let id = 'record-no' + ("00" + i).slice(-3);
    let modified = Date.now() / 1000 - 60 * (i + 10);
    let payload = encryptPayload({
      id,
      histUri: "http://foo/bar?" + id,
        title: id,
        sortindex: i,
        visits: [{date: (modified - 5) * 1000000, type: visitType}],
        deleted: false});

    let wbo = new ServerWBO(id, payload);
    wbo.modified = modified;
    collection.insertWBO(wbo);
  }

  let engine = Service.engineManager.get("history");
  let meta_global = Service.recordManager.set(engine.metaURL,
                                              new WBORecord(engine.metaURL));
  meta_global.payload.engines = {history: {version: engine.version,
                                           syncID: engine.syncID}};

  try {

    _("On a mobile client, we get new records from the server in batches of 50.");
    engine._syncStartup();

    // Fake a lower limit.
    engine.downloadLimit = FAKE_DOWNLOAD_LIMIT;
    _("Last modified: " + engine.lastModified);
    _("Processing...");
    engine._processIncoming();

    _("Last modified: " + engine.lastModified);
    engine._syncFinish();

    // Back to the normal limit.
    _("Running again. Should fetch none, because of lastModified");
    engine.downloadLimit = MAX_HISTORY_DOWNLOAD;
    _("Processing...");
    engine._processIncoming();

    _("Last modified: " + engine.lastModified);
    _("Running again. Expecting to pull everything");

    engine.lastModified = undefined;
    engine.lastSync     = 0;
    _("Processing...");
    engine._processIncoming();

    _("Last modified: " + engine.lastModified);

    // Verify that the right number of GET requests with the right
    // kind of parameters were made.
    do_check_eq(collection.get_log.length,
        // First try:
        1 +    // First 50...
        1 +    // 1 GUID fetch...
               // 1 fetch...
        Math.ceil((FAKE_DOWNLOAD_LIMIT - 50) / MOBILE_BATCH_SIZE) +
        // Second try: none
        // Third try:
        1 +    // First 50...
        1 +    // 1 GUID fetch...
               // 4 fetch...
        Math.ceil((234 - 50) / MOBILE_BATCH_SIZE));

    // Check the structure of each HTTP request.
    do_check_eq(collection.get_log[0].full, 1);
    do_check_eq(collection.get_log[0].limit, MOBILE_BATCH_SIZE);
    do_check_eq(collection.get_log[1].full, undefined);
    do_check_eq(collection.get_log[1].sort, "index");
    do_check_eq(collection.get_log[1].limit, FAKE_DOWNLOAD_LIMIT);
    do_check_eq(collection.get_log[2].full, 1);
    do_check_eq(collection.get_log[3].full, 1);
    do_check_eq(collection.get_log[3].limit, MOBILE_BATCH_SIZE);
    do_check_eq(collection.get_log[4].full, undefined);
    do_check_eq(collection.get_log[4].sort, "index");
    do_check_eq(collection.get_log[4].limit, MAX_HISTORY_DOWNLOAD);
    for (let i = 0; i <= Math.floor((234 - 50) / MOBILE_BATCH_SIZE); i++) {
      let j = i + 5;
      do_check_eq(collection.get_log[j].full, 1);
      do_check_eq(collection.get_log[j].limit, undefined);
      if (i < Math.floor((234 - 50) / MOBILE_BATCH_SIZE))
        do_check_eq(collection.get_log[j].ids.length, MOBILE_BATCH_SIZE);
      else
        do_check_eq(collection.get_log[j].ids.length, 234 % MOBILE_BATCH_SIZE);
    }

  } finally {
    await PlacesTestUtils.clearHistory();
    await promiseStopServer(server);
    Svc.Prefs.resetBranch("");
    Service.recordManager.clearCache();
  }
});

function run_test() {
  generateNewKeys(Service.collectionKeys);

  run_next_test();
}
