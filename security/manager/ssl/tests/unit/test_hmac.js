"use strict";

var ScriptableUnicodeConverter =
  Components.Constructor("@mozilla.org/intl/scriptableunicodeconverter",
                         "nsIScriptableUnicodeConverter");

function getHMAC(data, key, alg) {
  let converter = new ScriptableUnicodeConverter();
  converter.charset = "utf8";
  let dataArray = converter.convertToByteArray(data);

  let keyObject = Cc["@mozilla.org/security/keyobjectfactory;1"]
                    .getService(Ci.nsIKeyObjectFactory)
                    .keyFromString(Ci.nsIKeyObject.HMAC, key);

  let cryptoHMAC = Cc["@mozilla.org/security/hmac;1"]
                     .createInstance(Ci.nsICryptoHMAC);

  cryptoHMAC.init(alg, keyObject);
  cryptoHMAC.update(dataArray, dataArray.length);
  let digest1 = cryptoHMAC.finish(false);

  cryptoHMAC.reset();
  cryptoHMAC.update(dataArray, dataArray.length);
  let digest2 = cryptoHMAC.finish(false);

  equal(digest1, digest2,
        "Initial digest and digest after calling reset() should match");

  return digest1;
}

function testHMAC(alg) {
  const key1 = 'MyKey_ABCDEFGHIJKLMN';
  const key2 = 'MyKey_01234567890123';

  const dataA = "Secret message";
  const dataB = "Secres message";

  let digest1a = getHMAC(key1, dataA, alg);
  let digest2 = getHMAC(key2, dataA, alg);
  let digest1b = getHMAC(key1, dataA, alg);

  equal(digest1a, digest1b,
        "The digests for the same key, data and algorithm should match");
  notEqual(digest1a, digest2, "The digests for different keys should not match");

  let digest1 = getHMAC(key1, dataA, alg);
  digest2 = getHMAC(key1, dataB, alg);

  notEqual(digest1, digest2, "The digests for different data should not match");
}

function hexdigest(data) {
  return Array.from(data, (c, i) => ("0" + data.charCodeAt(i).toString(16)).slice(-2)).join("");
}

function testVectors() {
  // These are test vectors taken from RFC 4231, section 4.3. (Test Case 2)
  const keyTestVector = "Jefe";
  const dataTestVector = "what do ya want for nothing?";

  let digest = hexdigest(getHMAC(dataTestVector, keyTestVector,
                                 Ci.nsICryptoHMAC.SHA256));
  equal(digest,
        "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843",
        "Actual and expected SHA-256 digests should match");

  digest = hexdigest(getHMAC(dataTestVector, keyTestVector,
                             Ci.nsICryptoHMAC.SHA384));
  equal(digest,
        "af45d2e376484031617f78d2b58a6b1b9c7ef464f5a01b47e42ec3736322445e8e2240ca5e69e2c78b3239ecfab21649",
        "Actual and expected SHA-384 digests should match");

  digest = hexdigest(getHMAC(dataTestVector, keyTestVector,
                             Ci.nsICryptoHMAC.SHA512));
  equal(digest,
        "164b7a7bfcf819e2e395fbe73b56e0a387bd64222e831fd610270cd7ea2505549758bf75c05a994a6d034f65f8f0e6fdcaeab1a34d4a6b4b636e070a38bce737",
        "Actual and expected SHA-512 digests should match");
}

function run_test() {
  testVectors();

  testHMAC(Ci.nsICryptoHMAC.SHA1);
  testHMAC(Ci.nsICryptoHMAC.SHA512);
  testHMAC(Ci.nsICryptoHMAC.MD5);
}
