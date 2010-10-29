let cryptoSvc;
try {
  Components.utils.import("resource://services-crypto/WeaveCrypto.js");
  cryptoSvc = new WeaveCrypto();
} catch (ex) {
  // Fallback to binary WeaveCrypto
  cryptoSvc = Cc["@labs.mozilla.com/Weave/Crypto;1"]
                .getService(Ci.IWeaveCrypto);
}

function run_test() {
  var salt = cryptoSvc.generateRandomBytes(16);
  var iv = cryptoSvc.generateRandomIV();
  var symKey = cryptoSvc.generateRandomKey();

  // Tests with a 2048 bit key (the default)
  do_check_eq(cryptoSvc.keypairBits, 2048)
  var pubOut = {};
  var privOut = {};
  cryptoSvc.generateKeypair("old passphrase", salt, iv, pubOut, privOut);
  var pubKey = pubOut.value;
  var privKey = privOut.value;

  // do some key wrapping
  var wrappedKey = cryptoSvc.wrapSymmetricKey(symKey, pubKey);
  var unwrappedKey = cryptoSvc.unwrapSymmetricKey(wrappedKey, privKey,
                                                  "old passphrase", salt, iv);

  // Is our unwrapped key the same thing we started with?
  do_check_eq(unwrappedKey, symKey);

  // Rewrap key with a new passphrase
  var newPrivKey = cryptoSvc.rewrapPrivateKey(privKey, "old passphrase",
                                              salt, iv, "new passphrase");
  
  // Unwrap symkey with new symkey
  var newUnwrappedKey = cryptoSvc.unwrapSymmetricKey(wrappedKey, newPrivKey,
                                                     "new passphrase", salt, iv);
  
  // The acid test... Is this unwrapped symkey the same as before?
  do_check_eq(newUnwrappedKey, unwrappedKey);
}
