/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://services-common/utils.js");
Cu.import("resource://services-crypto/utils.js");

// Test vectors from RFC 5869

// Test case 1

let tc1 = {
   IKM:  "0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b",
   salt: "000102030405060708090a0b0c",
   info: "f0f1f2f3f4f5f6f7f8f9",
   L:    42,
   PRK:  "077709362c2e32df0ddc3f0dc47bba63" +
         "90b6c73bb50f9c3122ec844ad7c2b3e5",
   OKM:  "3cb25f25faacd57a90434f64d0362f2a" +
         "2d2d0a90cf1a5a4c5db02d56ecc4c5bf" +
         "34007208d5b887185865"
};

// Test case 2

let tc2 = {
   IKM:  "000102030405060708090a0b0c0d0e0f" +
         "101112131415161718191a1b1c1d1e1f" +
         "202122232425262728292a2b2c2d2e2f" +
         "303132333435363738393a3b3c3d3e3f" +
         "404142434445464748494a4b4c4d4e4f",
   salt: "606162636465666768696a6b6c6d6e6f" +
         "707172737475767778797a7b7c7d7e7f" +
         "808182838485868788898a8b8c8d8e8f" +
         "909192939495969798999a9b9c9d9e9f" +
         "a0a1a2a3a4a5a6a7a8a9aaabacadaeaf",
   info: "b0b1b2b3b4b5b6b7b8b9babbbcbdbebf" +
         "c0c1c2c3c4c5c6c7c8c9cacbcccdcecf" +
         "d0d1d2d3d4d5d6d7d8d9dadbdcdddedf" +
         "e0e1e2e3e4e5e6e7e8e9eaebecedeeef" +
         "f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff",
   L:    82,
   PRK:  "06a6b88c5853361a06104c9ceb35b45c" +
         "ef760014904671014a193f40c15fc244",
   OKM:  "b11e398dc80327a1c8e7f78c596a4934" +
         "4f012eda2d4efad8a050cc4c19afa97c" +
         "59045a99cac7827271cb41c65e590e09" +
         "da3275600c2f09b8367793a9aca3db71" +
         "cc30c58179ec3e87c14c01d5c1f3434f" +
         "1d87"
};

// Test case 3

let tc3 = {
   IKM:  "0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b",
   salt: "",
   info: "",
   L:    42,
   PRK:  "19ef24a32c717b167f33a91d6f648bdf" +
         "96596776afdb6377ac434c1c293ccb04",
   OKM:  "8da4e775a563c18f715f802a063c5a31" +
         "b8a11f5c5ee1879ec3454e5f3c738d2d" +
         "9d201395faa4b61a96c8"
};

function sha256HMAC(message, key) {
  let h = CryptoUtils.makeHMACHasher(Ci.nsICryptoHMAC.SHA256, key);
  return CryptoUtils.digestBytes(message, h);
}

function _hexToString(hex) {
  let ret = "";
  if (hex.length % 2 != 0) {
    return false;
  }

  for (let i = 0; i < hex.length; i += 2) {
    let cur = hex[i] + hex[i + 1];
    ret += String.fromCharCode(parseInt(cur, 16));
  }
  return ret;
}

function extract_hex(salt, ikm) {
  salt = _hexToString(salt);
  ikm = _hexToString(ikm);
  return CommonUtils.bytesAsHex(sha256HMAC(ikm, CryptoUtils.makeHMACKey(salt)));
}

function expand_hex(prk, info, len) {
  prk = _hexToString(prk);
  info = _hexToString(info);
  return CommonUtils.bytesAsHex(CryptoUtils.hkdfExpand(prk, info, len));
}

function hkdf_hex(ikm, salt, info, len) {
  ikm = _hexToString(ikm);
  if (salt)
    salt = _hexToString(salt);
  info = _hexToString(info);
  return CommonUtils.bytesAsHex(CryptoUtils.hkdf(ikm, salt, info, len));
}

function run_test() {
  _("Verifying Test Case 1");
  do_check_eq(extract_hex(tc1.salt, tc1.IKM), tc1.PRK);
  do_check_eq(expand_hex(tc1.PRK, tc1.info, tc1.L), tc1.OKM);
  do_check_eq(hkdf_hex(tc1.IKM, tc1.salt, tc1.info, tc1.L), tc1.OKM);

  _("Verifying Test Case 2");
  do_check_eq(extract_hex(tc2.salt, tc2.IKM), tc2.PRK);
  do_check_eq(expand_hex(tc2.PRK, tc2.info, tc2.L), tc2.OKM);
  do_check_eq(hkdf_hex(tc2.IKM, tc2.salt, tc2.info, tc2.L), tc2.OKM);

  _("Verifying Test Case 3");
  do_check_eq(extract_hex(tc3.salt, tc3.IKM), tc3.PRK);
  do_check_eq(expand_hex(tc3.PRK, tc3.info, tc3.L), tc3.OKM);
  do_check_eq(hkdf_hex(tc3.IKM, tc3.salt, tc3.info, tc3.L), tc3.OKM);
  do_check_eq(hkdf_hex(tc3.IKM, undefined, tc3.info, tc3.L), tc3.OKM);
}
