/**
 * Tests of OSKeyStore.jsm
 */

"use strict";

var { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
var { TestUtils } = ChromeUtils.import(
  "resource://testing-common/TestUtils.jsm"
);

let OSKeyStoreTestUtils;
add_task(async function os_key_store_setup() {
  ({ OSKeyStoreTestUtils } = ChromeUtils.import(
    "resource://testing-common/OSKeyStoreTestUtils.jsm"
  ));
  OSKeyStoreTestUtils.setup();
  registerCleanupFunction(async function cleanup() {
    await OSKeyStoreTestUtils.cleanup();
  });
});

let OSKeyStore;
add_task(async function setup() {
  ({ OSKeyStore } = ChromeUtils.import(
    "resource://gre/modules/OSKeyStore.jsm"
  ));
});

// Ensure that the appropriate initialization has happened.
do_get_profile();

const testText = "test string";
let cipherText;

add_task(async function test_encrypt_decrypt() {
  Assert.equal(
    (await OSKeyStore.ensureLoggedIn()).authenticated,
    true,
    "Started logged in."
  );

  cipherText = await OSKeyStore.encrypt(testText);
  Assert.notEqual(testText, cipherText);

  let plainText = await OSKeyStore.decrypt(cipherText);
  Assert.equal(testText, plainText);
});

add_task(async function test_reauth() {
  let canTest = OSKeyStoreTestUtils.canTestOSKeyStoreLogin();
  if (!canTest) {
    todo_check_true(
      canTest,
      "test_reauth: Cannot test OS key store login on this build. See OSKeyStoreTestUtils.canTestOSKeyStoreLogin for details"
    );
    return;
  }

  let reauthObserved = OSKeyStoreTestUtils.waitForOSKeyStoreLogin(false);
  await new Promise(resolve => TestUtils.executeSoon(resolve));
  try {
    await OSKeyStore.decrypt(cipherText, "prompt message text");
    throw new Error("Not receiving canceled OS unlock error");
  } catch (ex) {
    Assert.equal(ex.message, "User canceled OS unlock entry");
    Assert.equal(ex.result, Cr.NS_ERROR_ABORT);
  }
  await reauthObserved;

  reauthObserved = OSKeyStoreTestUtils.waitForOSKeyStoreLogin(false);
  await new Promise(resolve => TestUtils.executeSoon(resolve));
  Assert.equal(
    (await OSKeyStore.ensureLoggedIn("test message")).authenticated,
    false,
    "Reauth cancelled."
  );
  await reauthObserved;

  reauthObserved = OSKeyStoreTestUtils.waitForOSKeyStoreLogin(true);
  await new Promise(resolve => TestUtils.executeSoon(resolve));
  let plainText2 = await OSKeyStore.decrypt(cipherText, "prompt message text");
  await reauthObserved;
  Assert.equal(testText, plainText2);

  reauthObserved = OSKeyStoreTestUtils.waitForOSKeyStoreLogin(true);
  await new Promise(resolve => TestUtils.executeSoon(resolve));
  Assert.equal(
    (await OSKeyStore.ensureLoggedIn("test message")).authenticated,
    true,
    "Reauth logged in."
  );
  await reauthObserved;
});

add_task(async function test_decryption_failure() {
  try {
    await OSKeyStore.decrypt("Malformed cipher text");
    throw new Error("Not receiving decryption error");
  } catch (ex) {
    Assert.notEqual(ex.result, Cr.NS_ERROR_ABORT);
  }
});
