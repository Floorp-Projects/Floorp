/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// This is a kinto-specific test...
Services.prefs.setBoolPref("webextensions.storage.sync.kinto", true);

const {
  KintoStorageTestUtils: { EncryptionRemoteTransformer },
} = ChromeUtils.importESModule(
  "resource://gre/modules/ExtensionStorageSyncKinto.sys.mjs"
);
const { CryptoUtils } = ChromeUtils.importESModule(
  "resource://services-crypto/utils.sys.mjs"
);
const { Utils } = ChromeUtils.importESModule(
  "resource://services-sync/util.sys.mjs"
);

/**
 * Like Assert.throws, but for generators.
 *
 * @param {string | object | Function} constraint
 *        What to use to check the exception.
 * @param {Function} f
 *        The function to call.
 */
async function throwsGen(constraint, f) {
  let threw = false;
  let exception;
  try {
    await f();
  } catch (e) {
    threw = true;
    exception = e;
  }

  ok(threw, "did not throw an exception");

  const debuggingMessage = `got ${exception}, expected ${constraint}`;

  if (typeof constraint === "function") {
    ok(constraint(exception), debuggingMessage);
  } else {
    let message = exception;
    if (typeof exception === "object") {
      message = exception.message;
    }
    ok(constraint === message, debuggingMessage);
  }
}

/**
 * An EncryptionRemoteTransformer that uses a fixed key bundle,
 * suitable for testing.
 */
class StaticKeyEncryptionRemoteTransformer extends EncryptionRemoteTransformer {
  constructor(keyBundle) {
    super();
    this.keyBundle = keyBundle;
  }

  getKeys() {
    return Promise.resolve(this.keyBundle);
  }
}
const BORING_KB =
  "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
let transformer;
add_task(async function setup() {
  const STRETCHED_KEY = await CryptoUtils.hkdfLegacy(
    BORING_KB,
    undefined,
    `testing storage.sync encryption`,
    2 * 32
  );
  const KEY_BUNDLE = {
    hmacKey: STRETCHED_KEY.slice(0, 32),
    encryptionKeyB64: btoa(STRETCHED_KEY.slice(32, 64)),
  };
  transformer = new StaticKeyEncryptionRemoteTransformer(KEY_BUNDLE);
});

add_task(async function test_encryption_transformer_roundtrip() {
  const POSSIBLE_DATAS = [
    "string",
    2, // number
    [1, 2, 3], // array
    { key: "value" }, // object
  ];

  for (let data of POSSIBLE_DATAS) {
    const record = { data, id: "key-some_2D_key", key: "some-key" };

    deepEqual(
      record,
      await transformer.decode(await transformer.encode(record))
    );
  }
});

add_task(async function test_refuses_to_decrypt_tampered() {
  const encryptedRecord = await transformer.encode({
    data: [1, 2, 3],
    id: "key-some_2D_key",
    key: "some-key",
  });
  const tamperedHMAC = Object.assign({}, encryptedRecord, {
    hmac: "0000000000000000000000000000000000000000000000000000000000000001",
  });
  await throwsGen(Utils.isHMACMismatch, async function () {
    await transformer.decode(tamperedHMAC);
  });

  const tamperedIV = Object.assign({}, encryptedRecord, {
    IV: "aaaaaaaaaaaaaaaaaaaaaa==",
  });
  await throwsGen(Utils.isHMACMismatch, async function () {
    await transformer.decode(tamperedIV);
  });
});
