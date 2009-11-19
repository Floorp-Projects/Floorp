var ScriptableUnicodeConverter =
  Components.Constructor("@mozilla.org/intl/scriptableunicodeconverter",
                         "nsIScriptableUnicodeConverter");
var CryptoHash =
  Components.Constructor("@mozilla.org/security/hash;1",
                         "nsICryptoHash",
                         "initWithString");

var messages = [
  "The quick brown fox jumps over the lazy dog",
  ""
];
var hashes = {
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
  /*
   * Coment taken from bug 383390:
   *  
   * First, |data| is the final string value produced by the cryptohash.  |for (i in
   * data)| uses the Mozilla JS extension that iterating over a string iterates over
   * its character indexes, so that's 0..length-1 over the hash string.
   * 
   * Returning to the left, the |charCodeAt| gets the value of the character at that
   * index in the string.
   * 
   * |slice(-2)| is equivalent to |slice(length of this string - 2)| as a convenient
   * way of wrapping around to the other end of a string without doing the actual
   * calculation.  When provided with only one argument, slice selects to the end of
   * the string, so this chomps off the last two characters of the string.
   * 
   * The last-two-characters part clarifies the |"0" +| -- if the Unicode value is
   * <10, we have a single-character hex string when we want one that's two
   * characters, and unconditionally prepending a "0" solves the problem.
   * 
   * The array comprehension just creates an array whose elements are these
   * two-character strings.
   */
  return [("0" + data.charCodeAt(i).toString(16)).slice(-2) for (i in data)].join("");
}

function doHash(algo, value, cmp) {
  var converter = new ScriptableUnicodeConverter();
  var hash = new CryptoHash(algo);

  converter.charset = 'utf8';
  value = converter.convertToByteArray(value);
  hash.update(value, value.length);
  var hash1 = hexdigest(hash.finish(false));
  if (cmp != hash1) {
    do_throw("Hash mismatch!\n" +
             "  Expected: " + cmp + "\n" +
             "  Actual: " + hash1 + "\n" +
             "  Algo: " + algo);
  }                                                                                                                                                                                                                                  

  hash.initWithString(algo);
  hash.update(value, value.length);
  var hash2 = hexdigest(hash.finish(false));
  if (cmp != hash2) {
    do_throw("Hash mismatch after crypto hash re-init!\n" +
             "  Expected: " + cmp + "\n" +
             "  Actual: " + hash2 + "\n" +
             "  Algo: " + algo);
  }
}

function doHashStream(algo, value, cmp) {
  var converter = new ScriptableUnicodeConverter();
  var hash = new CryptoHash(algo);

  converter.charset = 'utf8';
  var stream = converter.convertToInputStream(value);
  hash.updateFromStream(stream, stream.available());
  hash = hexdigest(hash.finish(false));
  if (cmp != hash) {
    do_throw("Hash mismatch!\n" +
             "  Expected: " + cmp + "\n" +
             "  Actual: " + hash + "\n" +
             "  Algo: " + algo);
  }
}

function run_test() {
  for (algo in hashes) {
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
