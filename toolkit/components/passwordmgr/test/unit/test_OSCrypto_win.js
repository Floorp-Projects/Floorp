/**
 * Tests the OSCrypto object.
 */

"use strict";

// Globals

ChromeUtils.defineModuleGetter(
  this,
  "OSCrypto",
  "resource://gre/modules/OSCrypto.jsm"
);

let crypto = new OSCrypto();

// Tests

add_task(function test_getIELoginHash() {
  Assert.equal(
    crypto.getIELoginHash("https://bugzilla.mozilla.org/page.cgi"),
    "4A66FE96607885790F8E67B56EEE52AB539BAFB47D"
  );

  Assert.equal(
    crypto.getIELoginHash("https://github.com/login"),
    "0112F7DCE67B8579EA01367678AA44AB9868B5A143"
  );

  Assert.equal(
    crypto.getIELoginHash("https://login.live.com/login.srf"),
    "FBF92E5D804C82717A57856533B779676D92903688"
  );

  Assert.equal(
    crypto.getIELoginHash("https://preview.c9.io/riadh/w1/pass.1.html"),
    "6935CF27628830605927F86AB53831016FC8973D1A"
  );

  Assert.equal(
    crypto.getIELoginHash("https://reviewboard.mozilla.org/account/login/"),
    "09141FD287E2E59A8B1D3BB5671537FD3D6B61337A"
  );

  Assert.equal(
    crypto.getIELoginHash("https://www.facebook.com/"),
    "EF44D3E034009CB0FD1B1D81A1FF3F3335213BD796"
  );
});

add_task(function test_decryptData_encryptData() {
  function encryptDecrypt(value, key) {
    Assert.ok(true, `Testing value='${value}' with key='${key}'`);
    let encrypted = crypto.encryptData(value, key);
    Assert.ok(!!encrypted, "Encrypted value returned");
    let decrypted = crypto.decryptData(encrypted, key);
    Assert.equal(decrypted, value, "Decrypted value matches initial value");
    return decrypted;
  }

  let values = [
    "",
    "secret",
    "https://www.mozilla.org",
    "https://reviewboard.mozilla.org",
    "https://bugzilla.mozilla.org/page.cgi",
    "新年快樂新年快樂",
  ];
  let keys = [
    null,
    "a",
    "keys",
    "abcdedf",
    "pass",
    "https://bugzilla.mozilla.org/page.cgi",
    "https://login.live.com/login.srf",
  ];
  for (let value of values) {
    for (let key of keys) {
      Assert.equal(
        encryptDecrypt(value, key),
        value,
        `'${value}' encrypted then decrypted with entropy of '${key}' should match original value.`
      );
    }
  }

  let url = "https://twitter.com/";
  let value = [
    1,
    0,
    0,
    0,
    208,
    140,
    157,
    223,
    1,
    21,
    209,
    17,
    140,
    122,
    0,
    192,
    79,
    194,
    151,
    235,
    1,
    0,
    0,
    0,
    254,
    58,
    230,
    75,
    132,
    228,
    181,
    79,
    184,
    160,
    37,
    106,
    201,
    29,
    42,
    152,
    0,
    0,
    0,
    0,
    2,
    0,
    0,
    0,
    0,
    0,
    16,
    102,
    0,
    0,
    0,
    1,
    0,
    0,
    32,
    0,
    0,
    0,
    90,
    136,
    17,
    124,
    122,
    57,
    178,
    24,
    34,
    86,
    209,
    198,
    184,
    107,
    58,
    58,
    32,
    98,
    61,
    239,
    129,
    101,
    56,
    239,
    114,
    159,
    139,
    165,
    183,
    40,
    183,
    85,
    0,
    0,
    0,
    0,
    14,
    128,
    0,
    0,
    0,
    2,
    0,
    0,
    32,
    0,
    0,
    0,
    147,
    170,
    34,
    21,
    53,
    227,
    191,
    6,
    201,
    84,
    106,
    31,
    57,
    227,
    46,
    127,
    219,
    199,
    80,
    142,
    37,
    104,
    112,
    223,
    26,
    165,
    223,
    55,
    176,
    89,
    55,
    37,
    112,
    0,
    0,
    0,
    98,
    70,
    221,
    109,
    5,
    152,
    46,
    11,
    190,
    213,
    226,
    58,
    244,
    20,
    180,
    217,
    63,
    155,
    227,
    132,
    7,
    151,
    235,
    6,
    37,
    232,
    176,
    182,
    141,
    191,
    251,
    50,
    20,
    123,
    53,
    11,
    247,
    233,
    112,
    121,
    130,
    27,
    168,
    68,
    92,
    144,
    192,
    7,
    12,
    239,
    53,
    217,
    253,
    155,
    54,
    109,
    236,
    216,
    225,
    245,
    79,
    234,
    165,
    225,
    104,
    36,
    77,
    13,
    195,
    237,
    143,
    165,
    100,
    107,
    230,
    70,
    54,
    19,
    179,
    35,
    8,
    101,
    93,
    202,
    121,
    210,
    222,
    28,
    93,
    122,
    36,
    84,
    185,
    249,
    238,
    3,
    102,
    149,
    248,
    94,
    137,
    16,
    192,
    22,
    251,
    220,
    22,
    223,
    16,
    58,
    104,
    187,
    64,
    0,
    0,
    0,
    70,
    72,
    15,
    119,
    144,
    66,
    117,
    203,
    190,
    82,
    131,
    46,
    111,
    130,
    238,
    191,
    170,
    63,
    186,
    117,
    46,
    88,
    171,
    3,
    94,
    146,
    75,
    86,
    243,
    159,
    63,
    195,
    149,
    25,
    105,
    141,
    42,
    217,
    108,
    18,
    63,
    62,
    98,
    182,
    241,
    195,
    12,
    216,
    152,
    230,
    176,
    253,
    202,
    129,
    41,
    185,
    135,
    111,
    226,
    92,
    27,
    78,
    27,
    198,
  ];

  let arr1 = crypto.arrayToString(value);
  let arr2 = crypto.stringToArray(
    crypto.decryptData(crypto.encryptData(arr1, url), url)
  );
  for (let i = 0; i < arr1.length; i++) {
    Assert.equal(arr2[i], value[i], "Checking index " + i);
  }
});

add_task(function test_decryptDataOutput() {
  const testString = "2<FPZd";
  const encrypted = crypto.encryptData(testString);

  const decryptedString = crypto.decryptData(encrypted, null, "string");
  Assert.equal(
    decryptedString,
    testString,
    "Decrypted string matches initial value"
  );

  const decryptedBytes = crypto.decryptData(encrypted, null, "bytes");
  testString.split("").forEach((c, i) => {
    const code = c.charCodeAt(0);
    Assert.equal(
      decryptedBytes[i],
      code,
      `Decrypted bytes matches ${c} charCode (${code})`
    );
  });
});
