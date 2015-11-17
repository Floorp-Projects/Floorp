const DSV = Ci.nsIDataSignatureVerifier;

var keys = [
// RSA key
"MIGfMA0GCSqGSIb3DQEBAQUAA4GNADCBiQKBgQDK426erD/H3XtsjvaB5+PJqbhj" +
"Zc9EDI5OCJS8R3FIObJ9ZHJK1TXeaE7JWqt9WUmBWTEFvwS+FI9vWu8058N9CHhD" +
"NyeP6i4LuUYjTURnn7Yw/IgzyIJ2oKsYa32RuxAyteqAWqPT/J63wBixIeCxmysf" +
"awB/zH4KaPiY3vnrzQIDAQAB",

// RSA key
"MIGfMA0GCSqGSIb3DQEBAQUAA4GNADCBiQKBgQDHe69VsjNCwnGF9YGZLgnobp3D" +
"c1KgWzpbT+f12vVKkV3YBpA9KMVMy6wpxlDjvLJjfp/0HvaH7aaz/7kgxZw70Y60" +
"LaJtkAcl1ZVAxS2lQKRTAzZ0RhoTSI1xVqGTjiQakgVdUeghtnqqwp5o1inZv3Qh" +
"nUOMNPyAV8zGt+ZQHQIDAQAB",

// Invalid key data ("foobar" base 64 encoded)
"Zm9vYmFy"
];

var data = [
"Test data for data signature verifier",
"The quick brown fox jumps over the lazy dog..."
];

var signatures = [
// Key 0, Data 0, MD2 hash algorithm
"MIGTMA0GCSqGSIb3DQEBAgUAA4GBALe3hO76UCpI8b1/oJUCIPmC6AbnMAMlAqo7" +
"pc3TaWmU9wISWmXSrwNmr/QQNjWDn4nzQn8/K/Ac+tszaXib6fVLKA1a6e+/E0qE" +
"OIKFwUiDWCkGDgxM8aYiTgoSZub/5rokgse+ivuCRSVTv9mSxRzKwj+Cvp1EjKCT" +
"iIl3nnTh",

// Key 0, Data 0, MD5 hash algorithm
"MIGTMA0GCSqGSIb3DQEBBAUAA4GBAGGb2QkA8LcA+QZj1SoVFmMpVTd9P5Ac0Rjb" +
"ldouMmngztMV/dxymVKCpknqelhsxTQ/zaqGZ4KKzcIffJa9jXi5XUD8XzDIKcFE" +
"dQvti8oUNDPb6l1ybETI8LKh2ywyBCSZ/Q5BwUeS9nfx+4cAnu9YZf29SGljit+Y" +
"XLPKZ+of",

// Key 0, Data 0, SHA1 hash algorithm
"MIGTMA0GCSqGSIb3DQEBBQUAA4GBAHcl6tqR5yjTDUJ5NiLOdIB4I6/C/rS2WW5a" +
"7MVhYYhBgaZyGaem4LLT/REloOOLwPNMcmSEpwexqMSATbl9AAoJPq/wO3l4cZpO" +
"tDkLlafsiwnqK156YvHp50zUq5Os28d6Bq/Nl2qjH1yizwlIEo0o9qo8Cq6QK3j2" +
"MdFCopgk",

// Key 0, Data 0, SHA256 hash algorithm
"MIGTMA0GCSqGSIb3DQEBCwUAA4GBAAOtDvzWwbYK7zLUz0F3e3wmkL1YWXuD/GzQ" +
"LgwOg6VWtn9v54M9nfv/+iX+m4udzIwHZU7utYM31vtwqRC36l0QKo2VMbiLySX+" +
"uHnSK40Kk4SfBvMF1wuz6BQ8ia8uTjPPfC764dB1N7gQdgdRevLTrh2EU6+DLmSS" +
"Sm1QJm9/",

// Key 0, Data 0, SHA384 hash algorithm
"MIGTMA0GCSqGSIb3DQEBDAUAA4GBAMWB3DskcGuGXpi3TO2Pm0g915EIvm5aOeXQ" +
"sbs0ZGOwPyzYN1kKOmEpGHMeRhcIIBcF80ZC5N6dsTxeMGkFGOqhvB/HNl7gXMqF" +
"OA8mG9vAcwfMeJlY4H5WbYD8bUn72UbIuS+sURLvWVhuIFBYPHHU7KVUaGAWl0rp" +
"hCa4Dk37",

// Key 0, Data 0, SHA512 hash algorithm
"MIGTMA0GCSqGSIb3DQEBDQUAA4GBAFkm61zH8Y0J5PA4GtOvoZA6w/SzHg5jFP11" +
"gmXki81VgKwLF7Gyj4wRBX7Q9c8cNrNeoGZb12NUUxlR+u6J6VxcosVPKrCz7Xfj" +
"LPi6+A1dNV5eH2B6tZR6wIiEatAWNIXxQZEJbj9BWefRFq6NnKhR5A/MEPnrZyoR" +
"Da3YsDV3",

// Key 0, Data 1, MD2 hash algorithm
"MIGTMA0GCSqGSIb3DQEBAgUAA4GBAJjwosJK6jV9Bt6HhrFn7+48LRhamjWjzs7a" +
"cf5D/GTuul6aQQvQJ4Lt26KTyh3VglaQJFToH0Ik/fR1lOJS3tCPr1RRH06cKZgK" +
"haoUaGR8rmtn678wX067q7ACmKPeqmgj71pHm7O5YgN3z45iAazbUHP4erdbFUf9" +
"4rOr3L2f",

// Key 0, Data 1, MD5 hash algorithm
"MIGTMA0GCSqGSIb3DQEBBAUAA4GBAC0EAoHWTb4CC+gw7fs5zMaZw7PWoDH1rXMD" +
"dKoMBDmAW1EXZTfUGUTv0ga3VzuPJKuHHZOFVyFDnt4qFrefzzWs17LiPpN+yVgo" +
"6vBnpXLeIp7D9n94bz56gv9NZZmy02XQVKDaRc3E4JBC7Ze7RAHuKtWuZRTUKF86" +
"VXakwW3a",

// Key 0, Data 1, SHA1 hash algorithm
"MIGTMA0GCSqGSIb3DQEBBQUAA4GBABkClr0Ot3aXBKYIiARdwpX8etDQO/Eqjxe8" +
"pJyaqwj/P+x8j9/CbtJKJJTxvYmV9AhdgLPgoWjcTkfvqKdb1vpKKbV30QC/TEnu" +
"ON+66MJgkwrZw6NCDyBRgMTjf4FWR75Ot1DLuu3+7uCswKCJe8la0HMm/AcqUzu1" +
"SKOPMseI",

// Key 0, Data 1, SHA256 hash algorithm
"MIGTMA0GCSqGSIb3DQEBCwUAA4GBAE2cIr6Uzo7RISkGgCA5m4K8s9+0iHwzr2u/" +
"3ICUrTPe4RY2g9RLd6qkwaHD101LW5TQw71fhePIxfWHEhWtTCLS5DnGiucxfGKW" +
"47gOBJIYf0DG7o5N4lA99j2Zuj+V+yjAcLfq7Su5FwASbD30KqCue1/F03qdXUxj" +
"ciJeGo2b",

// Key 0, Data 1, SHA384 hash algorithm
"MIGTMA0GCSqGSIb3DQEBDAUAA4GBAK+JfKJNBjw2d2iXT/pe9xMXPkLSkf+gjG2i" +
"F7B0kBvMGyOVxuvQ4TCx+NFhAUevQugDHgBMkmAieg8mApcWWFWQ+8rbdUFv/hD7" +
"fHW+QukMgcfLMquh0GtDuoM8ZKFBBvwnPGLLUh+ZMy8fEOjjH+s6bQQSpf072SSJ" +
"K+Uo8DG2",

// Key 0, Data 1, SHA512 hash algorithm
"MIGTMA0GCSqGSIb3DQEBDQUAA4GBAEEvDpyBssG0qFfRamNwkwSjhqYRVFdIa6+E" +
"xfxdRqW/nxN5HuzFA8aajgSMXX0YFWPXV7OuVjCCJfZWhq7kQpTy96AmI/04rVdr" +
"9gc5mc2tdLl3Yk/Qd+Xq8WYcQIZ5Ewyo7sr8eKtVhtEM8OtPR54FO2s1pkZwJdVf" +
"ymMzHBoE",

// Key 1, Data 0, MD5 hash algorithm
"MIGTMA0GCSqGSIb3DQEBBAUAA4GBAAIzLho2i5jfJ5SPPV/u2gUuefzhjEAsUhlL" +
"Nir4FKhNzB2DZNbME9DtgNvdmZd00IjydYlaJ0dnLiMigXIaRJsyncYazULZdY6i" +
"i7oP6llbXbszSTbHGolr5kQ+6cZPBBATOkJ+wekDdlvh5cZ+B0Lux4LevUDlGWAy" +
"uR7bqrc5",

// Key 1, Data 0, SHA512 hash algorithm
"MIGTMA0GCSqGSIb3DQEBDQUAA4GBABsjF8K/SIaY3KTeIGpPEwl1+ZXLKBaHxlRT" +
"b37PhorSfqW1KFjquCEASUUeFwCQ14uUIBaRQV2haRGA0dRpuWr4zrWZMcDKOsmX" +
"r5XRvcti9/lNqoIID/Mq0tKtS6aVFZpoHIrwbXpV4hV+BRGhaPxV3RBzEIzM7bWJ" +
"tN3JY9+1",

// Key 1, Data 1, MD5 hash algorithm
"MIGTMA0GCSqGSIb3DQEBBAUAA4GBAIAxRPXDAT2LBcOo7yTNr5uIZbPW9rkSX0Ba" +
"h4sq6YRcxlnohaE2VO0CLGXFNwaBihhkkp+2dA76EvbMo/+O9XTWwroqtWWwvmxs" +
"tWK6HvwYKnGFKOOZMOjNjmXjk446UVvxYCbU+NPM1LZTewT1/UpPWWRowF/lwX7m" +
"SnT8d2ds",

// Key 1, Data 1, SHA512 hash algorithm
"MIGTMA0GCSqGSIb3DQEBDQUAA4GBAF0+XYD/r0Annz1GJ24GTkAlWY/OixCSV6Ix" +
"OMM7P2d/jgOP+ICKIpxqaSE0CbkLiegUiidIOWvFqDxQJWlAAukDUWISGFfJMFxX" +
"3jzJ0bBfeNY/1Qo8jMQopcNco/NlNgoSKAUOBtk31aFgNoVC3kWUk6pO97KEiJ+e" +
"bQp9Z2/M",

// Invalid signature data ("foobar" base 64 encoded)
"Zm9vYmFy"
];

var tests = [
// Data   Signature  Key   Expected   Throws
// Pass cases
  [0,     0,         0,    true,      false], //0
  [0,     1,         0,    true,      false], //1
  [0,     2,         0,    true,      false], //2
  [0,     3,         0,    true,      false], //3
  [0,     4,         0,    true,      false], //4
  [0,     5,         0,    true,      false], //5
  [1,     6,         0,    true,      false], //6
  [1,     7,         0,    true,      false], //7
  [1,     8,         0,    true,      false], //8
  [1,     9,         0,    true,      false], //9
  [1,     10,        0,    true,      false], //10
  [1,     11,        0,    true,      false], //11
  [0,     12,        1,    true,      false], //12
  [0,     13,        1,    true,      false], //13
  [1,     14,        1,    true,      false], //14
  [1,     15,        1,    true,      false], //15
// Incorrect data cases
  [1,     0,         0,    false,     false], //16
  [1,     1,         0,    false,     false], //17
  [1,     2,         0,    false,     false], //18
  [1,     3,         0,    false,     false], //19
  [1,     4,         0,    false,     false], //20
  [1,     5,         0,    false,     false], //21
  [0,     6,         0,    false,     false], //22
  [0,     7,         0,    false,     false], //23
  [0,     8,         0,    false,     false], //24
  [0,     9,         0,    false,     false], //25
  [0,     10,        0,    false,     false], //26
  [0,     11,        0,    false,     false], //27
// Incorrect key cases
  [0,     1,         1,    false,     false], //28
  [0,     5,         1,    false,     false], //29
  [1,     7,         1,    false,     false], //30
  [1,     11,        1,    false,     false], //31
  [0,     12,        0,    false,     false], //32
  [0,     13,        0,    false,     false], //33
  [1,     14,        0,    false,     false], //34
  [1,     15,        0,    false,     false], //35
// Invalid data cases
  [0,     0,         2,    false,     true],  //36
  [0,     1,         2,    false,     true],  //37
  [0,     16,        0,    false,     true],  //38
  [1,     16,        0,    false,     true],  //39
];

function run_test() {
  var verifier = Cc["@mozilla.org/security/datasignatureverifier;1"].
                 createInstance(Ci.nsIDataSignatureVerifier);

  for (var t = 0; t < tests.length; t++) {
    let testShouldThrow = tests[t][4];
    try {
      var result = verifier.verifyData(data[tests[t][0]],
                                       signatures[tests[t][1]],
                                       keys[tests[t][2]]);
      ok(!testShouldThrow,
         `Test ${t} should reach here only if not expected to throw`);
      equal(result, tests[t][3],
            `Actual and expected result should match for test ${t}`);
    }
    catch (e) {
      ok(testShouldThrow, `Test ${t} should throw only if expected to: ${e}`);
    }
  }
}
