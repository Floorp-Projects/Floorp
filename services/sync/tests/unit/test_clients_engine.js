Cu.import("resource://services-sync/constants.js");
Cu.import("resource://services-sync/record.js");
Cu.import("resource://services-sync/identity.js");
Cu.import("resource://services-sync/util.js");
Cu.import("resource://services-sync/engines/clients.js");

const MORE_THAN_CLIENTS_TTL_REFRESH = 691200; // 8 days
const LESS_THAN_CLIENTS_TTL_REFRESH = 86400; // 1 day

function test_properties() {
  try {
    _("Test lastRecordUpload property");
    do_check_eq(Svc.Prefs.get("clients.lastRecordUpload"), undefined);
    do_check_eq(Clients.lastRecordUpload, 0);

    let now = Date.now();
    Clients.lastRecordUpload = now / 1000;
    do_check_eq(Clients.lastRecordUpload, Math.floor(now / 1000));
  } finally {
    Svc.Prefs.resetBranch("");
  }
}

function test_sync() {
  _("Ensure that Clients engine uploads a new client record once a week.");
  Svc.Prefs.set("clusterURL", "http://localhost:8080/");
  Svc.Prefs.set("username", "foo");
  new SyncTestingInfrastructure();

  CollectionKeys.generateNewKeys();

  let global = new ServerWBO('global',
                             {engines: {clients: {version: Clients.version,
                                                  syncID: Clients.syncID}}});
  let coll = new ServerCollection();
  let clientwbo = coll.wbos[Clients.localID] = new ServerWBO(Clients.localID);
  let server = httpd_setup({
      "/1.0/foo/storage/meta/global": global.handler(),
      "/1.0/foo/storage/clients": coll.handler()
  });
  do_test_pending();

  try {

    _("First sync, client record is uploaded");
    do_check_eq(clientwbo.payload, undefined);
    do_check_eq(Clients.lastRecordUpload, 0);
    Clients.sync();
    do_check_true(!!clientwbo.payload);
    do_check_true(Clients.lastRecordUpload > 0);

    _("Let's time travel more than a week back, new record should've been uploaded.");
    Clients.lastRecordUpload -= MORE_THAN_CLIENTS_TTL_REFRESH;
    let lastweek = Clients.lastRecordUpload;
    clientwbo.payload = undefined;
    Clients.sync();
    do_check_true(!!clientwbo.payload);
    do_check_true(Clients.lastRecordUpload > lastweek);

    _("Time travel one day back, no record uploaded.");
    Clients.lastRecordUpload -= LESS_THAN_CLIENTS_TTL_REFRESH;
    let yesterday = Clients.lastRecordUpload;
    clientwbo.payload = undefined;
    Clients.sync();
    do_check_eq(clientwbo.payload, undefined);
    do_check_eq(Clients.lastRecordUpload, yesterday);

  } finally {
    server.stop(do_test_finished);
    Svc.Prefs.resetBranch("");
    Records.clearCache();
  }
}


function run_test() {
  test_properties();
  test_sync();
}
