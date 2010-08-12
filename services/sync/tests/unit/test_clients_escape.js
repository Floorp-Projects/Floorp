Cu.import("resource://services-sync/base_records/crypto.js");
Cu.import("resource://services-sync/base_records/keys.js");
Cu.import("resource://services-sync/engines/clients.js");
Cu.import("resource://services-sync/identity.js");
Cu.import("resource://services-sync/util.js");
Cu.import("resource://services-sync/identity.js");

function run_test() {
  _("Set up test fixtures.");
  ID.set('WeaveID', new Identity('Some Identity', 'foo'));
  Svc.Prefs.set("clusterURL", "http://fakebase/");
  let baseUri = "http://fakebase/1.0/foo/storage/";
  let pubUri = baseUri + "keys/pubkey";
  let privUri = baseUri + "keys/privkey";

  let passphrase = ID.set("WeaveCryptoID", new Identity());
  passphrase.password = "passphrase";

  _("Setting up fake pub/priv keypair and symkey for encrypt/decrypt");
  PubKeys.defaultKeyUri = baseUri + "keys/pubkey";
  let {pubkey, privkey} = PubKeys.createKeypair(passphrase, pubUri, privUri);
  PubKeys.set(pubUri, pubkey);
  PrivKeys.set(privUri, privkey);

  let cryptoMeta = new CryptoMeta(Clients.cryptoMetaURL);
  cryptoMeta.addUnwrappedKey(pubkey, Svc.Crypto.generateRandomKey());
  CryptoMetas.set(Clients.cryptoMetaURL, cryptoMeta);

  try {
    _("Test that serializing client records results in uploadable ascii");
    Clients.localID = "ascii";
    Clients.localName = "wéävê";

    _("Make sure we have the expected record");
    let record = Clients._createRecord("ascii");
    do_check_eq(record.id, "ascii");
    do_check_eq(record.name, "wéävê");

    record.encrypt(passphrase);
    let serialized = JSON.stringify(record);
    let checkCount = 0;
    _("Checking for all ASCII:", serialized);
    Array.forEach(serialized, function(ch) {
      let code = ch.charCodeAt(0);
      _("Checking asciiness of '", ch, "'=", code);
      do_check_true(code < 128);
      checkCount++;
    });

    _("Processed", checkCount, "characters out of", serialized.length);
    do_check_eq(checkCount, serialized.length);

    _("Making sure the record still looks like it did before");
    record.decrypt(passphrase);
    do_check_eq(record.id, "ascii");
    do_check_eq(record.name, "wéävê");

    _("Sanity check that creating the record also gives the same");
    record = Clients._createRecord("ascii");
    do_check_eq(record.id, "ascii");
    do_check_eq(record.name, "wéävê");
  } finally {
    Svc.Prefs.resetBranch("");
  }
}
