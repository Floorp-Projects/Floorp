Cu.import("resource://services-sync/util.js");
Cu.import("resource://services-sync/base_records/crypto.js");
Cu.import("resource://services-sync/base_records/keys.js");
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

function createAndUploadKeypair() {
  let keys = PubKeys.createKeypair(ID.get("WeaveCryptoID"),
                                   PubKeys.defaultKeyUri,
                                   PrivKeys.defaultKeyUri);
  PubKeys.uploadKeypair(keys);
}

function createAndUploadSymKey(url) {
  let symkey = Svc.Crypto.generateRandomKey();
  let pubkey = PubKeys.getDefaultKey();
  let meta = new CryptoMeta(url);
  meta.addUnwrappedKey(pubkey, symkey);
  let res = new Resource(meta.uri);
  res.put(meta);
  CryptoMetas.set(url, meta);
}

function setUpTestFixtures() {
  let cryptoService = new FakeCryptoService();

  Service.clusterURL = "http://localhost:8080/";
  Service.username = "johndoe";
  Service.passphrase = "secret";

  createAndUploadKeypair();
  createAndUploadSymKey("http://localhost:8080/1.0/johndoe/storage/crypto/steam");
  createAndUploadSymKey("http://localhost:8080/1.0/johndoe/storage/crypto/petrol");
  createAndUploadSymKey("http://localhost:8080/1.0/johndoe/storage/crypto/diesel");
}

function test_withCollectionList_failOnCrypto() {
  _("Service.wipeServer() deletes collections given as argument and aborts if a collection delete fails.");

  let steam_coll = new FakeCollection();
  let petrol_coll = new FakeCollection();
  let diesel_coll = new FakeCollection();
  let crypto_steam = new ServerWBO('steam');
  let crypto_diesel = new ServerWBO('diesel');

  let server = httpd_setup({
    "/1.0/johndoe/storage/keys/pubkey": (new ServerWBO('pubkey')).handler(),
    "/1.0/johndoe/storage/keys/privkey": (new ServerWBO('privkey')).handler(),
    "/1.0/johndoe/storage/steam": steam_coll.handler(),
    "/1.0/johndoe/storage/petrol": petrol_coll.handler(),
    "/1.0/johndoe/storage/diesel": diesel_coll.handler(),
    "/1.0/johndoe/storage/crypto/steam": crypto_steam.handler(),
    "/1.0/johndoe/storage/crypto/petrol": serviceUnavailable,
    "/1.0/johndoe/storage/crypto/diesel": crypto_diesel.handler()
  });
  do_test_pending();

  try {
    setUpTestFixtures();

    _("Confirm initial environment.");
    do_check_false(steam_coll.deleted);
    do_check_false(petrol_coll.deleted);
    do_check_false(diesel_coll.deleted);

    do_check_true(crypto_steam.payload != undefined);
    do_check_true(crypto_diesel.payload != undefined);

    do_check_true(CryptoMetas.contains("http://localhost:8080/1.0/johndoe/storage/crypto/steam"));
    do_check_true(CryptoMetas.contains("http://localhost:8080/1.0/johndoe/storage/crypto/petrol"));
    do_check_true(CryptoMetas.contains("http://localhost:8080/1.0/johndoe/storage/crypto/diesel"));

    _("wipeServer() will happily ignore the non-existent collection, delete the 'steam' collection and abort after an receiving an error on the 'petrol' collection's symkey.");
    let error;
    try {
      Service.wipeServer(["non-existent", "steam", "petrol", "diesel"]);
    } catch(ex) {
      error = ex;
    }
    _("wipeServer() threw this exception: " + error);
    do_check_true(error != undefined);

    _("wipeServer stopped deleting after encountering an error with the 'petrol' collection's symkey, thus only 'steam' and 'petrol' have been deleted.");
    do_check_true(steam_coll.deleted);
    do_check_true(petrol_coll.deleted);
    do_check_false(diesel_coll.deleted);

    do_check_true(crypto_steam.payload == undefined);
    do_check_true(crypto_diesel.payload != undefined);

    do_check_false(CryptoMetas.contains("http://localhost:8080/1.0/johndoe/storage/crypto/steam"));
    do_check_false(CryptoMetas.contains("http://localhost:8080/1.0/johndoe/storage/crypto/petrol"));
    do_check_true(CryptoMetas.contains("http://localhost:8080/1.0/johndoe/storage/crypto/diesel"));

  } finally {
    server.stop(do_test_finished);
    Svc.Prefs.resetBranch("");
    CryptoMetas.clearCache();
  }
}

function test_withCollectionList_failOnCollection() {
  _("Service.wipeServer() deletes collections given as argument.");

  let steam_coll = new FakeCollection();
  let diesel_coll = new FakeCollection();
  let crypto_steam = new ServerWBO('steam');
  let crypto_petrol = new ServerWBO('petrol');
  let crypto_diesel = new ServerWBO('diesel');

  let server = httpd_setup({
    "/1.0/johndoe/storage/keys/pubkey": (new ServerWBO('pubkey')).handler(),
    "/1.0/johndoe/storage/keys/privkey": (new ServerWBO('privkey')).handler(),
    "/1.0/johndoe/storage/steam": steam_coll.handler(),
    "/1.0/johndoe/storage/petrol": serviceUnavailable,
    "/1.0/johndoe/storage/diesel": diesel_coll.handler(),
    "/1.0/johndoe/storage/crypto/steam": crypto_steam.handler(),
    "/1.0/johndoe/storage/crypto/petrol": crypto_petrol.handler(),
    "/1.0/johndoe/storage/crypto/diesel": crypto_diesel.handler()
  });
  do_test_pending();

  try {
    setUpTestFixtures();

    _("Confirm initial environment.");
    do_check_false(steam_coll.deleted);
    do_check_false(diesel_coll.deleted);

    do_check_true(crypto_steam.payload != undefined);
    do_check_true(crypto_petrol.payload != undefined);
    do_check_true(crypto_diesel.payload != undefined);

    do_check_true(CryptoMetas.contains("http://localhost:8080/1.0/johndoe/storage/crypto/steam"));
    do_check_true(CryptoMetas.contains("http://localhost:8080/1.0/johndoe/storage/crypto/petrol"));
    do_check_true(CryptoMetas.contains("http://localhost:8080/1.0/johndoe/storage/crypto/diesel"));

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

    do_check_true(crypto_steam.payload == undefined);
    do_check_true(crypto_petrol.payload != undefined);
    do_check_true(crypto_diesel.payload != undefined);

    do_check_false(CryptoMetas.contains("http://localhost:8080/1.0/johndoe/storage/crypto/steam"));
    do_check_true(CryptoMetas.contains("http://localhost:8080/1.0/johndoe/storage/crypto/petrol"));
    do_check_true(CryptoMetas.contains("http://localhost:8080/1.0/johndoe/storage/crypto/diesel"));

  } finally {
    server.stop(do_test_finished);
    Svc.Prefs.resetBranch("");
    CryptoMetas.clearCache();
  }
}

function run_test() {
  test_withCollectionList_failOnCollection();
  test_withCollectionList_failOnCrypto();
}
