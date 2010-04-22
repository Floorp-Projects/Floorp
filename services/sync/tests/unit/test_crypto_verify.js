function run_test() {
  var cryptoSvc = Cc[cryptoContractID].
                  getService(Ci.IWeaveCrypto);

  var salt = cryptoSvc.generateRandomBytes(16);
  var iv = cryptoSvc.generateRandomIV();

  // Tests with a 2048 bit key (the default)
  do_check_eq(cryptoSvc.keypairBits, 2048)
  var privOut = {};
  cryptoSvc.generateKeypair("passphrase", salt, iv, {}, privOut);
  var privKey = privOut.value;

  // Check with correct passphrase
  var shouldBeTrue = cryptoSvc.verifyPassphrase(privKey, "passphrase",
                                                salt, iv);
  do_check_eq(shouldBeTrue, true);

  // Check with incorrect passphrase
  var shouldBeFalse = cryptoSvc.verifyPassphrase(privKey, "NotPassphrase",
                                                 salt, iv);
  do_check_eq(shouldBeFalse, false);
}
