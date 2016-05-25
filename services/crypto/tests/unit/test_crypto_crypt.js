Cu.import("resource://services-crypto/WeaveCrypto.js");
Cu.importGlobalProperties(['crypto']);

var cryptoSvc = new WeaveCrypto();

function run_test() {

  if ("makeSECItem" in cryptoSvc)   // Only for js-ctypes WeaveCrypto.
    test_makeSECItem();

  if (this.gczeal) {
    _("Running crypto tests with gczeal(2).");
    gczeal(2);
  }
  test_bug_617650();
  test_encrypt_decrypt();
  test_key_memoization();
  if (this.gczeal)
    gczeal(0);
}

function test_key_memoization() {
  let cryptoGlobal = cryptoSvc._getCrypto();
  let oldImport = cryptoGlobal.subtle.importKey;
  if (!oldImport) {
    _("Couldn't swizzle crypto.subtle.importKey; returning.");
    return;
  }

  let iv  = cryptoSvc.generateRandomIV();
  let key = cryptoSvc.generateRandomKey();
  let c   = 0;
  cryptoGlobal.subtle.importKey = function(format, keyData, algo, extractable, usages) {
    c++;
    return oldImport.call(cryptoGlobal.subtle, format, keyData, algo, extractable, usages);
  }

  // Encryption should cause a single counter increment.
  do_check_eq(c, 0);
  let cipherText = cryptoSvc.encrypt("Hello, world.", key, iv);
  do_check_eq(c, 1);
  cipherText = cryptoSvc.encrypt("Hello, world.", key, iv);
  do_check_eq(c, 1);

  // ... as should decryption.
  cryptoSvc.decrypt(cipherText, key, iv);
  cryptoSvc.decrypt(cipherText, key, iv);
  cryptoSvc.decrypt(cipherText, key, iv);
  do_check_eq(c, 2);

  // Un-swizzle.
  cryptoGlobal.subtle.importKey = oldImport;
}

function multiple_decrypts(iterations) {
  let iv = cryptoSvc.generateRandomIV();
  let key = cryptoSvc.generateRandomKey();
  let cipherText = cryptoSvc.encrypt("Hello, world.", key, iv);

  for (let i = 0; i < iterations; ++i) {
    let clearText = cryptoSvc.decrypt(cipherText, key, iv);
    do_check_eq(clearText + " " + i, "Hello, world. " + i);
  }
  _("Done with multiple_decrypts.");
}

function test_bug_617650() {
  if (this.gczeal) {
    gczeal(2);
    // Few iterations, because gczeal(2) is expensive... and makes it fail much faster!
    _("gczeal set to 2; attempting 10 iterations of multiple_decrypts.");
    multiple_decrypts(10);
    gczeal(0);
  } else {
    // We can't use gczeal on non-debug builds, so try lots of reps instead.
    _("No gczeal (non-debug build?); attempting 10,000 iterations of multiple_decrypts.");
    multiple_decrypts(10000);
  }
}

// Just verify that it gets populated with the correct bytes.
function test_makeUint8Array() {
  Components.utils.import("resource://gre/modules/ctypes.jsm");

  let item1 = cryptoSvc.makeUint8Array("abcdefghi", false);
  do_check_true(item1);
  for (let i = 0; i < 8; ++i)
    do_check_eq(item1[i], "abcdefghi".charCodeAt(i));
}

function test_encrypt_decrypt() {

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
  key = "St1tFCor7vQEJNug/465dQ==";
  iv  = "oLjkfrLIOnK2bDRvW4kXYA==";

  _("Testing small IV.");
  mySecret = "YWJjZGVmZ2hpamtsbW5vcHFyc3R1dnd4eXo=";
  let shortiv  = "YWJj";
  let err;
  try {
    cryptoSvc.encrypt(mySecret, key, shortiv);
  } catch (ex) {
    err = ex;
  }
  do_check_true(!!err);

  _("Testing long IV.");
  let longiv  = "gsgLRDaxWvIfKt75RjuvFWERt83FFsY2A0TW+0b2iVk=";
  try {
    cryptoSvc.encrypt(mySecret, key, longiv);
  } catch (ex) {
    err = ex;
  }
  do_check_true(!!err);

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


  key = "iz35tuIMq4/H+IYw2KTgow==";
  iv  = "TJYrvva2KxvkM8hvOIvWp3==";
  mySecret = "i like pie";

  cipherText = cryptoSvc.encrypt(mySecret, key, iv);
  clearText = cryptoSvc.decrypt(cipherText, key, iv);
  do_check_eq(cipherText, "DLGx8BWqSCLGG7i/xwvvxg==");
  do_check_eq(clearText, mySecret);

  key = "c5hG3YG+NC61FFy8NOHQak1ZhMEWO79bwiAfar2euzI=";
  iv  = "gsgLRDaxWvIfKt75RjuvFW==";
  mySecret = "i like pie";

  cipherText = cryptoSvc.encrypt(mySecret, key, iv);
  clearText = cryptoSvc.decrypt(cipherText, key, iv);
  do_check_eq(cipherText, "o+ADtdMd8ubzNWurS6jt0Q==");
  do_check_eq(clearText, mySecret);

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
