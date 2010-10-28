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
  // First, do a normal run with expected usage... Generate a random key and
  // iv, encrypt and decrypt a string.
  var iv = cryptoSvc.generateRandomIV();
  do_check_eq(iv.length, 24);

  var key = cryptoSvc.generateRandomKey();
  do_check_eq(key.length, 44);

  var mySecret = "bacon is a vegetable";
  var cipherText = cryptoSvc.encrypt(mySecret, key, iv);
  do_check_eq(cipherText.length, 44);

  var clearText = cryptoSvc.decrypt(cipherText, key, iv);
  do_check_eq(clearText.length, 20);
  
  // Did the text survive the encryption round-trip?
  do_check_eq(clearText, mySecret);
  do_check_neq(cipherText, mySecret); // just to be explicit


  // Do some more tests with a fixed key/iv, to check for reproducable results.
  cryptoSvc.algorithm = Ci.IWeaveCrypto.AES_128_CBC;
  key = "St1tFCor7vQEJNug/465dQ==";
  iv  = "oLjkfrLIOnK2bDRvW4kXYA==";

  // Test small input sizes
  mySecret = "";
  cipherText = cryptoSvc.encrypt(mySecret, key, iv);
  clearText = cryptoSvc.decrypt(cipherText, key, iv);
  do_check_eq(cipherText, "OGQjp6mK1a3fs9k9Ml4L3w==");
  do_check_eq(clearText, mySecret);

  mySecret = "x";
  cipherText = cryptoSvc.encrypt(mySecret, key, iv);
  clearText = cryptoSvc.decrypt(cipherText, key, iv);
  do_check_eq(cipherText, "96iMl4vhOxFUW/lVHHzVqg==");
  do_check_eq(clearText, mySecret);

  mySecret = "xx";
  cipherText = cryptoSvc.encrypt(mySecret, key, iv);
  clearText = cryptoSvc.decrypt(cipherText, key, iv);
  do_check_eq(cipherText, "olpPbETRYROCSqFWcH2SWg==");
  do_check_eq(clearText, mySecret);

  mySecret = "xxx";
  cipherText = cryptoSvc.encrypt(mySecret, key, iv);
  clearText = cryptoSvc.decrypt(cipherText, key, iv);
  do_check_eq(cipherText, "rRbpHGyVSZizLX/x43Wm+Q==");
  do_check_eq(clearText, mySecret);

  mySecret = "xxxx";
  cipherText = cryptoSvc.encrypt(mySecret, key, iv);
  clearText = cryptoSvc.decrypt(cipherText, key, iv);
  do_check_eq(cipherText, "HeC7miVGDcpxae9RmiIKAw==");
  do_check_eq(clearText, mySecret);

  // Test non-ascii input
  // ("testuser1" using similar-looking glyphs)
  mySecret = String.fromCharCode(355, 277, 349, 357, 533, 537, 101, 345, 185);
  cipherText = cryptoSvc.encrypt(mySecret, key, iv);
  clearText = cryptoSvc.decrypt(cipherText, key, iv);
  do_check_eq(cipherText, "Pj4ixByXoH3SU3JkOXaEKPgwRAWplAWFLQZkpJd5Kr4=");
  do_check_eq(clearText, mySecret);

  // Tests input spanning a block boundary (AES block size is 16 bytes)
  mySecret = "123456789012345";
  cipherText = cryptoSvc.encrypt(mySecret, key, iv);
  clearText = cryptoSvc.decrypt(cipherText, key, iv);
  do_check_eq(cipherText, "e6c5hwphe45/3VN/M0bMUA==");
  do_check_eq(clearText, mySecret);

  mySecret = "1234567890123456";
  cipherText = cryptoSvc.encrypt(mySecret, key, iv);
  clearText = cryptoSvc.decrypt(cipherText, key, iv);
  do_check_eq(cipherText, "V6aaOZw8pWlYkoIHNkhsP1JOIQF87E2vTUvBUQnyV04=");
  do_check_eq(clearText, mySecret);

  mySecret = "12345678901234567";
  cipherText = cryptoSvc.encrypt(mySecret, key, iv);
  clearText = cryptoSvc.decrypt(cipherText, key, iv);
  do_check_eq(cipherText, "V6aaOZw8pWlYkoIHNkhsP5GvxWJ9+GIAS6lXw+5fHTI=");
  do_check_eq(clearText, mySecret);


  // Test with 192 bit key.
  cryptoSvc.algorithm = Ci.IWeaveCrypto.AES_192_CBC;
  key = "iz35tuIMq4/H+IYw2KTgow==";
  iv  = "TJYrvva2KxvkM8hvOIvWp3xgjTXgq5Ss";
  mySecret = "i like pie";

  cipherText = cryptoSvc.encrypt(mySecret, key, iv);
  clearText = cryptoSvc.decrypt(cipherText, key, iv);
  do_check_eq(cipherText, "DLGx8BWqSCLGG7i/xwvvxg==");
  do_check_eq(clearText, mySecret);

  // Test with 256 bit key.
  cryptoSvc.algorithm = Ci.IWeaveCrypto.AES_256_CBC;
  key = "c5hG3YG+NC61FFy8NOHQak1ZhMEWO79bwiAfar2euzI=";
  iv  = "gsgLRDaxWvIfKt75RjuvFWERt83FFsY2A0TW+0b2iVk=";
  mySecret = "i like pie";

  cipherText = cryptoSvc.encrypt(mySecret, key, iv);
  clearText = cryptoSvc.decrypt(cipherText, key, iv);
  do_check_eq(cipherText, "o+ADtdMd8ubzNWurS6jt0Q==");
  do_check_eq(clearText, mySecret);


  // Test with bogus inputs
  cryptoSvc.algorithm = Ci.IWeaveCrypto.AES_128_CBC;
  key = "St1tFCor7vQEJNug/465dQ==";
  iv  = "oLjkfrLIOnK2bDRvW4kXYA==";
  mySecret = "does thunder read testcases?";
  cipherText = cryptoSvc.encrypt(mySecret, key, iv);
  do_check_eq(cipherText, "T6fik9Ros+DB2ablH9zZ8FWZ0xm/szSwJjIHZu7sjPs=");

  var badkey    = "badkeybadkeybadkeybadk==";
  var badiv     = "badivbadivbadivbadivbad=";
  var badcipher = "crapinputcrapinputcrapinputcrapinputcrapinp=";
  var failure;

  try {
    failure = false;
    clearText = cryptoSvc.decrypt(cipherText, badkey, iv);
  } catch (e) {
    failure = true;
  }
  do_check_true(failure);

  try {
    failure = false;
    clearText = cryptoSvc.decrypt(cipherText, key, badiv);
  } catch (e) {
    failure = true;
  }
  do_check_true(failure);

  try {
    failure = false;
    clearText = cryptoSvc.decrypt(cipherText, badkey, badiv);
  } catch (e) {
    failure = true;
  }
  do_check_true(failure);

  try {
    failure = false;
    clearText = cryptoSvc.decrypt(badcipher, key, iv);
  } catch (e) {
    failure = true;
  }
  do_check_true(failure);

}
