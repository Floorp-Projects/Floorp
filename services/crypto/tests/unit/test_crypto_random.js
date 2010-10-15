let cryptoSvc;
try {
  Components.utils.import("resource://services-crypto/threaded.js");
  cryptoSvc = new ThreadedCrypto();
} catch (ex) {
  // Fallback to binary WeaveCrypto
  cryptoSvc = Cc["@labs.mozilla.com/Weave/Crypto;1"]
                .getService(Ci.IWeaveCrypto);
}

function run_test() {
  // Test salt generation.
  var salt;

  salt = cryptoSvc.generateRandomBytes(0);
  do_check_eq(salt.length, 0);
  salt = cryptoSvc.generateRandomBytes(1);
  do_check_eq(salt.length, 4);
  salt = cryptoSvc.generateRandomBytes(2);
  do_check_eq(salt.length, 4);
  salt = cryptoSvc.generateRandomBytes(3);
  do_check_eq(salt.length, 4);
  salt = cryptoSvc.generateRandomBytes(4);
  do_check_eq(salt.length, 8);
  salt = cryptoSvc.generateRandomBytes(8);
  do_check_eq(salt.length, 12);

  // sanity check to make sure salts seem random
  var salt2 = cryptoSvc.generateRandomBytes(8);
  do_check_eq(salt2.length, 12);
  do_check_neq(salt, salt2);

  salt = cryptoSvc.generateRandomBytes(16);
  do_check_eq(salt.length, 24);
  salt = cryptoSvc.generateRandomBytes(1024);
  do_check_eq(salt.length, 1368);


  // Test random key generation
  var keydata, keydata2, iv;

  keydata  = cryptoSvc.generateRandomKey();
  do_check_eq(keydata.length, 44);
  keydata2 = cryptoSvc.generateRandomKey();
  do_check_neq(keydata, keydata2); // sanity check for randomness
  iv = cryptoSvc.generateRandomIV();
  do_check_eq(iv.length, 24);

  cryptoSvc.algorithm = Ci.IWeaveCrypto.AES_256_CBC;
  keydata  = cryptoSvc.generateRandomKey();
  do_check_eq(keydata.length, 44);
  keydata2 = cryptoSvc.generateRandomKey();
  do_check_neq(keydata, keydata2); // sanity check for randomness
  iv = cryptoSvc.generateRandomIV();
  do_check_eq(iv.length, 24);

  cryptoSvc.algorithm = Ci.IWeaveCrypto.AES_192_CBC;
  keydata  = cryptoSvc.generateRandomKey();
  do_check_eq(keydata.length, 32);
  keydata2 = cryptoSvc.generateRandomKey();
  do_check_neq(keydata, keydata2); // sanity check for randomness
  iv = cryptoSvc.generateRandomIV();
  do_check_eq(iv.length, 24);

  cryptoSvc.algorithm = Ci.IWeaveCrypto.AES_128_CBC;
  keydata  = cryptoSvc.generateRandomKey();
  do_check_eq(keydata.length, 24);
  keydata2 = cryptoSvc.generateRandomKey();
  do_check_neq(keydata, keydata2); // sanity check for randomness
  iv = cryptoSvc.generateRandomIV();
  do_check_eq(iv.length, 24);
}
