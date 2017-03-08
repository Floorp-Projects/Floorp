"use strict";

// This file tests various aspects of the nsICryptoHMAC implementation for all
// of the supported algorithms.

function getHMAC(data, key, alg, returnBase64) {
  let converter = Cc["@mozilla.org/intl/scriptableunicodeconverter"]
                    .createInstance(Ci.nsIScriptableUnicodeConverter);
  converter.charset = "utf8";
  let dataArray = converter.convertToByteArray(data);

  let keyObject = Cc["@mozilla.org/security/keyobjectfactory;1"]
                    .getService(Ci.nsIKeyObjectFactory)
                    .keyFromString(Ci.nsIKeyObject.HMAC, key);

  let cryptoHMAC = Cc["@mozilla.org/security/hmac;1"]
                     .createInstance(Ci.nsICryptoHMAC);

  cryptoHMAC.init(alg, keyObject);
  cryptoHMAC.update(dataArray, dataArray.length);
  let digest1 = cryptoHMAC.finish(returnBase64);

  cryptoHMAC.reset();
  cryptoHMAC.update(dataArray, dataArray.length);
  let digest2 = cryptoHMAC.finish(returnBase64);

  let stream = converter.convertToInputStream(data);
  cryptoHMAC.reset();
  cryptoHMAC.updateFromStream(stream, stream.available());
  let digestFromStream = cryptoHMAC.finish(returnBase64);

  equal(digest1, digest2,
        "Initial digest and digest after calling reset() should match");

  equal(digest1, digestFromStream,
        "Digest from buffer and digest from stream should match");

  return digest1;
}

function testHMAC(alg) {
  const key1 = "MyKey_ABCDEFGHIJKLMN";
  const key2 = "MyKey_01234567890123";

  const dataA = "Secret message";
  const dataB = "Secres message";

  let digest1a = getHMAC(key1, dataA, alg, false);
  let digest2 = getHMAC(key2, dataA, alg, false);
  let digest1b = getHMAC(key1, dataA, alg, false);

  equal(digest1a, digest1b,
        "The digests for the same key, data and algorithm should match");
  notEqual(digest1a, digest2, "The digests for different keys should not match");

  let digest1 = getHMAC(key1, dataA, alg, false);
  digest2 = getHMAC(key1, dataB, alg, false);

  notEqual(digest1, digest2, "The digests for different data should not match");
}

function testVectors() {
  const keyTestVector = "Jefe";
  const dataTestVector = "what do ya want for nothing?";

  // The base 64 values aren't in either of the RFCs below; they were just
  // calculated using the hex vectors.
  const vectors = [
    // RFC 2202 section 2 test case 2.
    {
      algoID: Ci.nsICryptoHMAC.MD5, algoName: "MD5",
      expectedDigest: "750c783e6ab0b503eaa86e310a5db738",
      expectedBase64: "dQx4PmqwtQPqqG4xCl23OA==",
    },
    // RFC 2202 section 2 test case 3.
    {
      algoID: Ci.nsICryptoHMAC.SHA1, algoName: "SHA-1",
      expectedDigest: "effcdf6ae5eb2fa2d27416d5f184df9c259a7c79",
      expectedBase64: "7/zfauXrL6LSdBbV8YTfnCWafHk=",
    },
    // RFC 4231 section 4.3.
    {
      algoID: Ci.nsICryptoHMAC.SHA256, algoName: "SHA-256",
      expectedDigest: "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843",
      expectedBase64: "W9zBRr9gdU5qBCQmCJV1x1oAPwidJzmDnexYuWTsOEM=",
    },
    {
      algoID: Ci.nsICryptoHMAC.SHA384, algoName: "SHA-384",
      expectedDigest: "af45d2e376484031617f78d2b58a6b1b9c7ef464f5a01b47e42ec3736322445e8e2240ca5e69e2c78b3239ecfab21649",
      expectedBase64: "r0XS43ZIQDFhf3jStYprG5x+9GT1oBtH5C7Dc2MiRF6OIkDKXmnix4syOez6shZJ",
    },
    {
      algoID: Ci.nsICryptoHMAC.SHA512, algoName: "SHA-512",
      expectedDigest: "164b7a7bfcf819e2e395fbe73b56e0a387bd64222e831fd610270cd7ea2505549758bf75c05a994a6d034f65f8f0e6fdcaeab1a34d4a6b4b636e070a38bce737",
      // TODO(Bug 1338897): Stop inserting CRLFs every 64 characters.
      expectedBase64: "Fkt6e/z4GeLjlfvnO1bgo4e9ZCIugx/WECcM1+olBVSXWL91wFqZSm0DT2X48Ob9\r\nyuqxo01Ka0tjbgcKOLznNw==",
    },
  ];

  for (let vector of vectors) {
    let digest = getHMAC(dataTestVector, keyTestVector, vector.algoID, false);
    equal(hexify(digest), vector.expectedDigest,
          `Actual and expected ${vector.algoName} digests should match`);
    let b64Digest = getHMAC(dataTestVector, keyTestVector, vector.algoID, true);
    equal(b64Digest, vector.expectedBase64,
          `Actual and expected ${vector.algoName} base64 digest should match`);
  }
}

function run_test() {
  testVectors();

  testHMAC(Ci.nsICryptoHMAC.MD5);
  testHMAC(Ci.nsICryptoHMAC.SHA1);
  testHMAC(Ci.nsICryptoHMAC.SHA256);
  testHMAC(Ci.nsICryptoHMAC.SHA384);
  testHMAC(Ci.nsICryptoHMAC.SHA512);

  // Our buffer size for working with streams is 4096 bytes. This tests we
  // handle larger inputs.
  let digest = getHMAC(" ".repeat(4100), "test", Ci.nsICryptoHMAC.MD5, false);
  equal(hexify(digest), "befbc875f73a088cf04e77f2b1286010",
        "Actual and expected digest for large stream should match");
}
