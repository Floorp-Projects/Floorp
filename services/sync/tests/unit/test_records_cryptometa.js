Cu.import("resource://weave/util.js");
Cu.import("resource://weave/base_records/crypto.js");
Cu.import("resource://weave/base_records/keys.js");

function run_test() {
  _("Generating keypair to encrypt/decrypt symkeys");
  let {pubkey, privkey} = PubKeys.createKeypair(
    passphrase,
    "http://site/pubkey",
    "http://site/privkey"
  );
  PubKeys.set(pubkey.uri, pubkey);
  PrivKeys.set(privkey.uri, privkey);

  _("Generating a crypto meta with a random key");
  let crypto = new CryptoMeta("http://site/crypto");
  let symkey = Svc.Crypto.generateRandomKey();
  crypto.addUnwrappedKey(pubkey, symkey);

  _("Verifying correct HMAC by getting the key");
  crypto.getKey(privkey, passphrase);

  _("Generating a new crypto meta as the previous caches the unwrapped key");
  let crypto = new CryptoMeta("http://site/crypto");
  let symkey = Svc.Crypto.generateRandomKey();
  crypto.addUnwrappedKey(pubkey, symkey);

  _("Changing the HMAC to force a mismatch");
  let goodHMAC = crypto.keyring[pubkey.uri.spec].hmac;
  crypto.keyring[pubkey.uri.spec].hmac = "failme!";
  let error = "";
  try {
    crypto.getKey(privkey, passphrase);
  }
  catch(ex) {
    error = ex;
  }
  do_check_eq(error, "Key SHA256 HMAC mismatch: failme!");

  _("Switching back to the correct HMAC and trying again");
  crypto.keyring[pubkey.uri.spec].hmac = goodHMAC;
  crypto.getKey(privkey, passphrase);
}
