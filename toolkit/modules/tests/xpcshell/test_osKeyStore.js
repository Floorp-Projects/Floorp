/**
 * Tests of OSKeyStore.sys.mjs
 */

"use strict";

var { TestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/TestUtils.sys.mjs"
);

let OSKeyStoreTestUtils;
add_task(async function os_key_store_setup() {
  ({ OSKeyStoreTestUtils } = ChromeUtils.importESModule(
    "resource://testing-common/OSKeyStoreTestUtils.sys.mjs"
  ));
  OSKeyStoreTestUtils.setup();
  registerCleanupFunction(async function cleanup() {
    await OSKeyStoreTestUtils.cleanup();
  });
});

let OSKeyStore;
add_task(async function setup() {
  ({ OSKeyStore } = ChromeUtils.importESModule(
    "resource://gre/modules/OSKeyStore.sys.mjs"
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

add_task(async function test_exportRecoveryPhrase() {
  // First, we need to get the original recovery phrase returned by generating
  // the secret key for the native key store the first time. We'll do this by
  // first destroying the secret, then generating it again, both with the
  // underlying native OSKeyStore mechanism.
  let nativeOSKeyStore = Cc["@mozilla.org/security/oskeystore;1"].getService(
    Ci.nsIOSKeyStore
  );

  // Just to make sure we're not about to blow away any _real_ secrets
  // associated with the OSKeyStore, we'll check that the OSKeyStoreTestUtils
  // has written the original store label to ORIGINAL_STORE_LABEL, and that
  // this doesn't match OSKeyStore.STORE_LABEL.
  Assert.ok(
    OSKeyStoreTestUtils.ORIGINAL_STORE_LABEL.length,
    "OSKeyStoreTestUtils.ORIGINAL_STORE_LABEL must have some length"
  );
  Assert.notEqual(
    OSKeyStoreTestUtils.ORIGINAL_STORE_LABEL,
    OSKeyStore.STORE_LABEL
  );

  // If we got this far, we can destroy the secret key, since it's just the
  // one used in tests.
  await nativeOSKeyStore.asyncDeleteSecret(OSKeyStore.STORE_LABEL);
  const expectedRecoveryPhrase = await nativeOSKeyStore.asyncGenerateSecret(
    OSKeyStore.STORE_LABEL
  );

  const ORIGINAL_STRING = "I'm a string that will be encrypted.";
  let encryptedString = await OSKeyStore.encrypt(ORIGINAL_STRING);

  let recoveryPhrase = await OSKeyStore.exportRecoveryPhrase();
  Assert.ok(recoveryPhrase, "Got a recovery phrase back");
  Assert.equal(
    recoveryPhrase,
    expectedRecoveryPhrase,
    "The recovery phrase matches the one initially returned when " +
      "generating the secret"
  );

  // Now delete the secret key (again)...
  await nativeOSKeyStore.asyncDeleteSecret(OSKeyStore.STORE_LABEL);

  // Now try to regenerate the secret using the recovery phrase.
  await nativeOSKeyStore.asyncRecoverSecret(
    OSKeyStore.STORE_LABEL,
    recoveryPhrase
  );

  let decryptedString = await OSKeyStore.decrypt(encryptedString);

  Assert.equal(
    decryptedString,
    ORIGINAL_STRING,
    "Decrypted string matches the original string"
  );
});

/**
 * Tests the behaviour of exporting the recovery phrase when there is no
 * store secret.
 */
add_task(async function test_exportRecoveryPhrase_no_secret() {
  let nativeOSKeyStore = Cc["@mozilla.org/security/oskeystore;1"].getService(
    Ci.nsIOSKeyStore
  );

  // Just to make sure we're not about to blow away any _real_ secrets
  // associated with the OSKeyStore, we'll check that the OSKeyStoreTestUtils
  // has written the original store label to ORIGINAL_STORE_LABEL, and that
  // this doesn't match OSKeyStore.STORE_LABEL.
  Assert.ok(
    OSKeyStoreTestUtils.ORIGINAL_STORE_LABEL.length,
    "OSKeyStoreTestUtils.ORIGINAL_STORE_LABEL must have some length"
  );
  Assert.notEqual(
    OSKeyStoreTestUtils.ORIGINAL_STORE_LABEL,
    OSKeyStore.STORE_LABEL
  );

  // If we got this far, we can destroy the secret key.
  await nativeOSKeyStore.asyncDeleteSecret(OSKeyStore.STORE_LABEL);

  // This should cause a new secret key to be automatically generated and
  // then returned.
  let recoveryPhrase = await OSKeyStore.exportRecoveryPhrase();
  Assert.ok(recoveryPhrase, "Got a recovery phrase back");
});
