"use strict";

const messages = [
  "The quick brown fox jumps over the lazy dog",
  ""
];
const hashes = {
  md2: [
    "03d85a0d629d2c442e987525319fc471",
    "8350e5a3e24c153df2275c9f80692773"
  ],
  md5: [
    "9e107d9d372bb6826bd81d3542a419d6",
    "d41d8cd98f00b204e9800998ecf8427e"
  ],
  sha1: [
    "2fd4e1c67a2d28fced849ee1bb76e7391b93eb12",
    "da39a3ee5e6b4b0d3255bfef95601890afd80709"
  ],
  sha256: [
    "d7a8fbb307d7809469ca9abcb0082e4f8d5651e46d3cdb762d02d0bf37c9e592",
    "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"
  ],
  sha384: [
   "ca737f1014a48f4c0b6dd43cb177b0afd9e5169367544c494011e3317dbf9a509cb1e5dc1e85a941bbee3d7f2afbc9b1",
   "38b060a751ac96384cd9327eb1b1e36a21fdb71114be07434c0cc7bf63f6e1da274edebfe76f65fbd51ad2f14898b95b"
  ],
  sha512: [
    "07e547d9586f6a73f73fbac0435ed76951218fb7d0c8d788a309d785436bbb642e93a252a954f23912547d1e8a3b5ed6e1bfd7097821233fa0538f3db854fee6",
    "cf83e1357eefb8bdf1542850d66d8007d620e4050b5715dc83f4a921d36ce9ce47d0d13c5d85f2b0ff8318d2877eec2f63b931bd47417a81a538327af927da3e"
  ]
};

function hexdigest(data) {
  // |slice(-2)| chomps off the last two characters of a string.
  //
  // Therefore, if the Unicode value is < 10, we have a single-character hex
  // string when we want one that's two characters, and unconditionally
  // prepending a "0" solves the problem.
  return Array.from(data, (c, i) => ("0" + data.charCodeAt(i).toString(16)).slice(-2)).join("");
}

function doHash(algo, value, cmp) {
  let hash = Cc["@mozilla.org/security/hash;1"].createInstance(Ci.nsICryptoHash);
  hash.initWithString(algo);

  let converter = Cc["@mozilla.org/intl/scriptableunicodeconverter"]
                    .createInstance(Ci.nsIScriptableUnicodeConverter);
  converter.charset = 'utf8';
  value = converter.convertToByteArray(value);
  hash.update(value, value.length);
  equal(hexdigest(hash.finish(false)), cmp,
        `Actual and expected hash for ${algo} should match`);

  hash.initWithString(algo);
  hash.update(value, value.length);
  equal(hexdigest(hash.finish(false)), cmp,
        `Actual and expected hash for ${algo} should match after re-init`);
}

function doHashStream(algo, value, cmp) {
  let hash = Cc["@mozilla.org/security/hash;1"].createInstance(Ci.nsICryptoHash);
  hash.initWithString(algo);

  let converter = Cc["@mozilla.org/intl/scriptableunicodeconverter"]
                    .createInstance(Ci.nsIScriptableUnicodeConverter);
  converter.charset = 'utf8';
  let stream = converter.convertToInputStream(value);
  hash.updateFromStream(stream, stream.available());
  equal(hexdigest(hash.finish(false)), cmp,
        `Actual and expected hash for ${algo} should match updating from stream`);
}

function run_test() {
  for (let algo in hashes) {
    hashes[algo].forEach(
      function(e, i) {
        doHash(algo, messages[i], e);

        if (messages[i].length) {
          // this test doesn't work for empty string/stream
          doHashStream(algo, messages[i], e);
        }
      }
    );
  }
}
