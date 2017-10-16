Cu.import("resource://services-crypto/WeaveCrypto.js", this);

var cryptoSvc = new WeaveCrypto();

add_task(async function test_crypto_random() {
  if (this.gczeal) {
    _("Running crypto random tests with gczeal(2).");
    gczeal(2);
  }

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

  salt = cryptoSvc.generateRandomBytes(1024);
  do_check_eq(salt.length, 1368);
  salt = cryptoSvc.generateRandomBytes(16);
  do_check_eq(salt.length, 24);


  // Test random key generation
  var keydata, keydata2, iv;

  keydata  = await cryptoSvc.generateRandomKey();
  do_check_eq(keydata.length, 44);
  keydata2 = await cryptoSvc.generateRandomKey();
  do_check_neq(keydata, keydata2); // sanity check for randomness
  iv = cryptoSvc.generateRandomIV();
  do_check_eq(iv.length, 24);

  if (this.gczeal)
    gczeal(0);
});
