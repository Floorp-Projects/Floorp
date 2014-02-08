/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// XXX until bug 937114 is fixed
Cu.importGlobalProperties(['btoa']);
Cu.import("resource://services-crypto/utils.js");
Cu.import("resource://services-common/utils.js");

let {bytesAsHex: b2h} = CommonUtils;

function run_test() {
  run_next_test();
}

add_task(function test_pbkdf2() {
  let symmKey16 = CryptoUtils.pbkdf2Generate("secret phrase", "DNXPzPpiwn", 4096, 16);
  do_check_eq(symmKey16.length, 16);
  do_check_eq(btoa(symmKey16), "d2zG0d2cBfXnRwMUGyMwyg==");
  do_check_eq(CommonUtils.encodeBase32(symmKey16), "O5WMNUO5TQC7LZ2HAMKBWIZQZI======");
  let symmKey32 = CryptoUtils.pbkdf2Generate("passphrase", "salt", 4096, 32);
  do_check_eq(symmKey32.length, 32);
});

// http://tools.ietf.org/html/rfc6070
// PBKDF2 HMAC-SHA1 Test Vectors
add_task(function test_pbkdf2_hmac_sha1() {
  let pbkdf2 = CryptoUtils.pbkdf2Generate;
  let vectors = [
    {P: "password",                    // (8 octets)
     S: "salt",                        // (4 octets)
     c: 1,
     dkLen: 20,
     DK: h("0c 60 c8 0f 96 1f 0e 71"+
           "f3 a9 b5 24 af 60 12 06"+
           "2f e0 37 a6"),             // (20 octets)
    },

    {P: "password",                    // (8 octets)
     S: "salt",                        // (4 octets)
     c: 2,
     dkLen: 20,
     DK: h("ea 6c 01 4d c7 2d 6f 8c"+
           "cd 1e d9 2a ce 1d 41 f0"+
           "d8 de 89 57"),             // (20 octets)
    },

    {P: "password",                    // (8 octets)
     S: "salt",                        // (4 octets)
     c: 4096,
     dkLen: 20,
     DK: h("4b 00 79 01 b7 65 48 9a"+
           "be ad 49 d9 26 f7 21 d0"+
           "65 a4 29 c1"),             // (20 octets)
    },

    // XXX Uncomment the following test after Bug 968567 lands
    //
    // XXX As it stands, I estimate that the CryptoUtils implementation will
    // take approximately 16 hours in my 2.3GHz MacBook to perform this many
    // rounds.
    //
    // {P: "password",                     // (8 octets)
    //  S: "salt"                          // (4 octets)
    //  c: 16777216,
    //  dkLen = 20,
    //  DK: h("ee fe 3d 61 cd 4d a4 e4"+
    //        "e9 94 5b 3d 6b a2 15 8c"+
    //        "26 34 e9 84"),             // (20 octets)
    // },

    {P: "passwordPASSWORDpassword",    // (24 octets)
     S: "saltSALTsaltSALTsaltSALTsaltSALTsalt", // (36 octets)
     c: 4096,
     dkLen: 25,
     DK: h("3d 2e ec 4f e4 1c 84 9b"+
           "80 c8 d8 36 62 c0 e4 4a"+
           "8b 29 1a 96 4c f2 f0 70"+
           "38"),                      // (25 octets)

    },

    {P: "pass\0word",                  // (9 octets)
     S: "sa\0lt",                      // (5 octets)
     c: 4096,
     dkLen: 16,
     DK: h("56 fa 6a a7 55 48 09 9d"+
           "cc 37 d7 f0 34 25 e0 c3"), // (16 octets)
    },
  ];

  for (let v of vectors) {
    do_check_eq(v.DK, b2h(pbkdf2(v.P, v.S, v.c, v.dkLen)));
  }

  run_next_test();
});

// I can't find any normative ietf test vectors for pbkdf2 hmac-sha256.
// The following vectors are derived with the same inputs as above (the sha1
// test).  Results verified by users here:
// https://stackoverflow.com/questions/5130513/pbkdf2-hmac-sha2-test-vectors
add_task(function test_pbkdf2_hmac_sha256() {
  let pbkdf2 = CryptoUtils.pbkdf2Generate;
  let vectors = [
    {P: "password",                    // (8 octets)
     S: "salt",                        // (4 octets)
     c: 1,
     dkLen: 32,
     DK: h("12 0f b6 cf fc f8 b3 2c"+
           "43 e7 22 52 56 c4 f8 37"+
           "a8 65 48 c9 2c cc 35 48"+
           "08 05 98 7c b7 0b e1 7b"), // (32 octets)
    },

    {P: "password",                    // (8 octets)
     S: "salt",                        // (4 octets)
     c: 2,
     dkLen: 32,
     DK: h("ae 4d 0c 95 af 6b 46 d3"+
           "2d 0a df f9 28 f0 6d d0"+
           "2a 30 3f 8e f3 c2 51 df"+
           "d6 e2 d8 5a 95 47 4c 43"), // (32 octets)
    },

    {P: "password",                    // (8 octets)
     S: "salt",                        // (4 octets)
     c: 4096,
     dkLen: 32,
     DK: h("c5 e4 78 d5 92 88 c8 41"+
           "aa 53 0d b6 84 5c 4c 8d"+
           "96 28 93 a0 01 ce 4e 11"+
           "a4 96 38 73 aa 98 13 4a"), // (32 octets)
    },

    {P: "passwordPASSWORDpassword",    // (24 octets)
     S: "saltSALTsaltSALTsaltSALTsaltSALTsalt", // (36 octets)
     c: 4096,
     dkLen: 40,
     DK: h("34 8c 89 db cb d3 2b 2f"+
           "32 d8 14 b8 11 6e 84 cf"+
           "2b 17 34 7e bc 18 00 18"+
           "1c 4e 2a 1f b8 dd 53 e1"+
           "c6 35 51 8c 7d ac 47 e9"), // (40 octets)
    },

    {P: "pass\0word",                  // (9 octets)
     S: "sa\0lt",                      // (5 octets)
     c: 4096,
     dkLen: 16,
     DK: h("89 b6 9d 05 16 f8 29 89"+
           "3c 69 62 26 65 0a 86 87"), // (16 octets)
    },
  ];

  for (let v of vectors) {
    do_check_eq(v.DK,
        b2h(pbkdf2(v.P, v.S, v.c, v.dkLen, Ci.nsICryptoHMAC.SHA256, 32)));
  }

  run_next_test();
});

// turn formatted test vectors into normal hex strings
function h(hexStr) {
  return hexStr.replace(/\s+/g, "");
}
