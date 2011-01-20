Cu.import("resource://services-sync/util.js");
Cu.import("resource://services-sync/record.js");
Cu.import("resource://services-sync/resource.js");

Svc.DefaultPrefs.set("registerEngines", "");
Cu.import("resource://services-sync/service.js");

function FakeCollection() {
  this.deleted = false;
}
FakeCollection.prototype = {
  handler: function() {
    let self = this;
    return function(request, response) {
      let body = "";
      if (request.method == "DELETE") {
          body = JSON.stringify(Date.now() / 1000);
          self.deleted = true;
      }
      response.setStatusLine(request.httpVersion, 200, "OK");
      response.bodyOutputStream.write(body, body.length);
    };
  }
};

function serviceUnavailable(request, response) {
  let body = "Service Unavailable";
  response.setStatusLine(request.httpVersion, 503, "Service Unavailable");
  response.bodyOutputStream.write(body, body.length);
}

function setUpTestFixtures() {
  let cryptoService = new FakeCryptoService();

  Service.clusterURL = "http://localhost:8080/";
  Service.username = "johndoe";
  Service.passphrase = "aabcdeabcdeabcdeabcdeabcde";
}

function test_withCollectionList_fail() {
  _("Service.wipeServer() deletes collections given as argument.");

  let steam_coll = new FakeCollection();
  let diesel_coll = new FakeCollection();

  let server = httpd_setup({
    "/1.0/johndoe/storage/steam": steam_coll.handler(),
    "/1.0/johndoe/storage/petrol": serviceUnavailable,
    "/1.0/johndoe/storage/diesel": diesel_coll.handler()
  });
  do_test_pending();

  try {
    setUpTestFixtures();

    _("Confirm initial environment.");
    do_check_false(steam_coll.deleted);
    do_check_false(diesel_coll.deleted);

    _("wipeServer() will happily ignore the non-existent collection, delete the 'steam' collection and abort after an receiving an error on the 'petrol' collection.");
    let error;
    try {
      Service.wipeServer(["non-existent", "steam", "petrol", "diesel"]);
    } catch(ex) {
      error = ex;
    }
    _("wipeServer() threw this exception: " + error);
    do_check_true(error != undefined);

    _("wipeServer stopped deleting after encountering an error with the 'petrol' collection, thus only 'steam' has been deleted.");
    do_check_true(steam_coll.deleted);
    do_check_false(diesel_coll.deleted);

  } finally {
    server.stop(do_test_finished);
    Svc.Prefs.resetBranch("");
  }
}

function test_wipeServer_leaves_collections() {
  _("Service.wipeServer() deletes everything but keys.");
  
  let steam_coll = new FakeCollection();
  let diesel_coll = new FakeCollection();
  let keys_coll = new FakeCollection();

  function info_collections(request, response) {
    let collections = {};
    let timestamp = Date.now() / 1000;
    if (!steam_coll.deleted)
      collections.steam = timestamp
    if (!diesel_coll.deleted)
      collections.diesel = timestamp;
    if (!keys_coll.deleted)
      collections.keys = timestamp;
    let body = JSON.stringify(collections);
    response.setStatusLine(request.httpVersion, 200, "OK");
    response.bodyOutputStream.write(body, body.length);
  }

  let server = httpd_setup({
    "/1.0/johndoe/storage/steam": steam_coll.handler(),
    "/1.0/johndoe/storage/diesel": diesel_coll.handler(),
    "/1.0/johndoe/storage/keys": keys_coll.handler(),
    "/1.0/johndoe/info/collections": info_collections
  });
  do_test_pending();

  try {
    setUpTestFixtures();
    _("Info URL: " + Service.infoURL);

    _("Confirm initial environment.");
    do_check_false(steam_coll.deleted);
    do_check_false(diesel_coll.deleted);
    do_check_false(keys_coll.deleted);
    
    _("Collections: " + new Resource(Service.infoURL).get());
    _("Try deletion.");
    Service.wipeServer();
    _("Collections: " + new Resource(Service.infoURL).get());
    
    _("Make sure keys is still present.");
    do_check_true(steam_coll.deleted);
    do_check_true(diesel_coll.deleted);
    do_check_false(keys_coll.deleted);
    
    _("Delete everything.");
    Service.wipeServer(null, true);
    do_check_true(steam_coll.deleted);
    do_check_true(diesel_coll.deleted);
    do_check_true(keys_coll.deleted);
    
  } finally {
    server.stop(do_test_finished);
    Svc.Prefs.resetBranch("");
  }
}

function run_test() {
  initTestLogging("Trace");
  test_withCollectionList_fail();
  test_wipeServer_leaves_collections();
}
