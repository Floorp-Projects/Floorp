"use strict";

// This file tests various aspects of the nsICryptoHash implementation for all
// of the supported algorithms.

const messages = ["The quick brown fox jumps over the lazy dog", ""];
const ALGORITHMS = [
  {
    initString: "md5",
    initConstant: Ci.nsICryptoHash.MD5,
    hexHashes: [
      "9e107d9d372bb6826bd81d3542a419d6",
      "d41d8cd98f00b204e9800998ecf8427e",
    ],
    b64Hashes: ["nhB9nTcrtoJr2B01QqQZ1g==", "1B2M2Y8AsgTpgAmY7PhCfg=="],
  },
  {
    initString: "sha1",
    initConstant: Ci.nsICryptoHash.SHA1,
    hexHashes: [
      "2fd4e1c67a2d28fced849ee1bb76e7391b93eb12",
      "da39a3ee5e6b4b0d3255bfef95601890afd80709",
    ],
    b64Hashes: ["L9ThxnotKPzthJ7hu3bnORuT6xI=", "2jmj7l5rSw0yVb/vlWAYkK/YBwk="],
  },
  {
    initString: "sha256",
    initConstant: Ci.nsICryptoHash.SHA256,
    hexHashes: [
      "d7a8fbb307d7809469ca9abcb0082e4f8d5651e46d3cdb762d02d0bf37c9e592",
      "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
    ],
    b64Hashes: [
      "16j7swfXgJRpypq8sAguT41WUeRtPNt2LQLQvzfJ5ZI=",
      "47DEQpj8HBSa+/TImW+5JCeuQeRkm5NMpJWZG3hSuFU=",
    ],
  },
  {
    initString: "sha384",
    initConstant: Ci.nsICryptoHash.SHA384,
    hexHashes: [
      "ca737f1014a48f4c0b6dd43cb177b0afd9e5169367544c494011e3317dbf9a509cb1e5dc1e85a941bbee3d7f2afbc9b1",
      "38b060a751ac96384cd9327eb1b1e36a21fdb71114be07434c0cc7bf63f6e1da274edebfe76f65fbd51ad2f14898b95b",
    ],
    b64Hashes: [
      "ynN/EBSkj0wLbdQ8sXewr9nlFpNnVExJQBHjMX2/mlCcseXcHoWpQbvuPX8q+8mx",
      "OLBgp1GsljhM2TJ+sbHjaiH9txEUvgdDTAzHv2P24donTt6/529l+9Ua0vFImLlb",
    ],
  },
  {
    initString: "sha512",
    initConstant: Ci.nsICryptoHash.SHA512,
    hexHashes: [
      "07e547d9586f6a73f73fbac0435ed76951218fb7d0c8d788a309d785436bbb642e93a252a954f23912547d1e8a3b5ed6e1bfd7097821233fa0538f3db854fee6",
      "cf83e1357eefb8bdf1542850d66d8007d620e4050b5715dc83f4a921d36ce9ce47d0d13c5d85f2b0ff8318d2877eec2f63b931bd47417a81a538327af927da3e",
    ],
    b64Hashes: [
      "B+VH2VhvanP3P7rAQ17XaVEhj7fQyNeIownXhUNru2Quk6JSqVTyORJUfR6KO17W4b/XCXghIz+gU489uFT+5g==",
      "z4PhNX7vuL3xVChQ1m2AB9Yg5AULVxXcg/SpIdNs6c5H0NE8XYXysP+DGNKHfuwvY7kxvUdBeoGlODJ6+SfaPg==",
    ],
  },
];

function doHash(algo, value, cmp) {
  let hash = Cc["@mozilla.org/security/hash;1"].createInstance(
    Ci.nsICryptoHash
  );
  hash.initWithString(algo);

  let converter = Cc[
    "@mozilla.org/intl/scriptableunicodeconverter"
  ].createInstance(Ci.nsIScriptableUnicodeConverter);
  converter.charset = "utf8";
  value = converter.convertToByteArray(value);
  hash.update(value, value.length);
  equal(
    hexify(hash.finish(false)),
    cmp,
    `Actual and expected hash for ${algo} should match`
  );

  hash.initWithString(algo);
  hash.update(value, value.length);
  equal(
    hexify(hash.finish(false)),
    cmp,
    `Actual and expected hash for ${algo} should match after re-init`
  );
}

function doHashStream(algo, value, cmp) {
  // TODO(Bug 459835): Make updateFromStream() accept zero length streams.
  if (!value.length) {
    return;
  }

  let hash = Cc["@mozilla.org/security/hash;1"].createInstance(
    Ci.nsICryptoHash
  );
  hash.initWithString(algo);

  let converter = Cc[
    "@mozilla.org/intl/scriptableunicodeconverter"
  ].createInstance(Ci.nsIScriptableUnicodeConverter);
  converter.charset = "utf8";
  let stream = converter.convertToInputStream(value);
  hash.updateFromStream(stream, stream.available());
  equal(
    hexify(hash.finish(false)),
    cmp,
    `Actual and expected hash for ${algo} should match updating from stream`
  );
}

function testInitConstantAndBase64(
  initConstant,
  algoName,
  message,
  expectedOutput
) {
  let converter = Cc[
    "@mozilla.org/intl/scriptableunicodeconverter"
  ].createInstance(Ci.nsIScriptableUnicodeConverter);
  converter.charset = "utf8";
  let value = converter.convertToByteArray(message);

  let hash = Cc["@mozilla.org/security/hash;1"].createInstance(
    Ci.nsICryptoHash
  );
  hash.init(initConstant);
  hash.update(value, value.length);
  equal(
    hash.finish(true),
    expectedOutput,
    `Actual and expected base64 hash for ${algoName} should match`
  );
}

function run_test() {
  for (let algo of ALGORITHMS) {
    algo.hexHashes.forEach((hash, i) => {
      doHash(algo.initString, messages[i], hash);
      doHashStream(algo.initString, messages[i], hash);
    });
    algo.b64Hashes.forEach((hash, i) => {
      testInitConstantAndBase64(
        algo.initConstant,
        algo.initString,
        messages[i],
        hash
      );
    });
  }

  // Our buffer size for working with streams is 4096 bytes. This tests we
  // handle larger inputs.
  doHashStream("md5", " ".repeat(4100), "59f337d82f9ef5c9571bec4d78d66641");
}
