/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/publicdomain/zero/1.0/
"use strict";

// Tests the methods and attributes for interfacing with nsIOSKeyStore.

// Ensure that the appropriate initialization has happened.
do_get_profile();

const LABELS = ["mylabel1", "mylabel2", "mylabel3"];

async function delete_all_secrets() {
  let keystore = Cc["@mozilla.org/security/oskeystore;1"].getService(
    Ci.nsIOSKeyStore
  );
  for (let label of LABELS) {
    if (await keystore.asyncSecretAvailable(label)) {
      await keystore.asyncDeleteSecret(label);
      ok(
        !(await keystore.asyncSecretAvailable(label)),
        label + " should be deleted now."
      );
    }
  }
}

// Test that Firefox handles locking and unlocking of the OSKeyStore properly.
// Does so by mocking out the actual dialog and "filling in" the
// password. Also tests that providing an incorrect password will fail (well,
// technically the user will just get prompted again, but if they then cancel
// the dialog the overall operation will fail).

var gMockPrompter = {
  passwordToTry: null,
  numPrompts: 0,

  // This intentionally does not use arrow function syntax to avoid an issue
  // where in the context of the arrow function, |this != gMockPrompter| due to
  // how objects get wrapped when going across xpcom boundaries.
  promptPassword(dialogTitle, text, password, checkMsg, checkValue) {
    this.numPrompts++;
    equal(
      text,
      "Please enter your Primary Password.",
      "password prompt text should be as expected"
    );
    equal(checkMsg, null, "checkMsg should be null");
    ok(this.passwordToTry, "passwordToTry should be non-null");
    if (this.passwordToTry == "DontTryThisPassword") {
      // Cancel the prompt in this case.
      return false;
    }
    password.value = this.passwordToTry;
    return true;
  },

  QueryInterface: ChromeUtils.generateQI(["nsIPrompt"]),
};

// Mock nsIWindowWatcher. PSM calls getNewPrompter on this to get an nsIPrompt
// to call promptPassword. We return the mock one, above.
var gWindowWatcher = {
  getNewPrompter: () => gMockPrompter,
  QueryInterface: ChromeUtils.generateQI(["nsIWindowWatcher"]),
};

async function encrypt_decrypt_test() {
  let keystore = Cc["@mozilla.org/security/oskeystore;1"].getService(
    Ci.nsIOSKeyStore
  );
  ok(
    !(await keystore.asyncSecretAvailable(LABELS[0])),
    "The secret should not be available yet."
  );

  let recoveryPhrase = await keystore.asyncGenerateSecret(LABELS[0]);
  ok(recoveryPhrase, "A recovery phrase should've been created.");
  let recoveryPhrase2 = await keystore.asyncGenerateSecret(LABELS[1]);
  ok(recoveryPhrase2, "A recovery phrase should've been created.");

  let text = new Uint8Array([0x01, 0x00, 0x01]);
  let ciphertext = "";
  try {
    ciphertext = await keystore.asyncEncryptBytes(LABELS[0], text);
    ok(ciphertext, "We should have a ciphertext now.");
  } catch (e) {
    ok(false, "Error encrypting " + e);
  }

  // Decrypting should give us the plaintext bytes again.
  try {
    let plaintext = await keystore.asyncDecryptBytes(LABELS[0], ciphertext);
    ok(
      plaintext.toString() == text.toString(),
      "Decrypted plaintext should be the same as text."
    );
  } catch (e) {
    ok(false, "Error decrypting ciphertext " + e);
  }

  // Decrypting with a wrong key should throw an error.
  try {
    await keystore.asyncDecryptBytes(LABELS[1], ciphertext);
    ok(false, "Decrypting with the wrong key should fail.");
  } catch (e) {
    ok(true, "Decrypting with the wrong key should fail " + e);
  }
}

add_task(async function() {
  let keystore = Cc["@mozilla.org/security/oskeystore;1"].getService(
    Ci.nsIOSKeyStore
  );
  let windowWatcherCID;
  if (keystore.isNSSKeyStore) {
    windowWatcherCID = MockRegistrar.register(
      "@mozilla.org/embedcomp/window-watcher;1",
      gWindowWatcher
    );
    registerCleanupFunction(() => {
      MockRegistrar.unregister(windowWatcherCID);
    });
  }

  await delete_all_secrets();
  await encrypt_decrypt_test();
  await delete_all_secrets();

  if (
    AppConstants.platform == "macosx" ||
    AppConstants.platform == "win" ||
    AppConstants.platform == "linux"
  ) {
    ok(
      !keystore.isNSSKeyStore,
      "OS X, Windows, and Linux should use the non-NSS implementation"
    );
  }

  if (keystore.isNSSKeyStore) {
    // If we use the NSS key store implementation test that everything works
    // when a master password is set.
    // Set an initial password.
    let tokenDB = Cc["@mozilla.org/security/pk11tokendb;1"].getService(
      Ci.nsIPK11TokenDB
    );
    let token = tokenDB.getInternalKeyToken();
    token.initPassword("hunter2");

    // Lock the key store. This should be equivalent to token.logoutSimple()
    await keystore.asyncLock();

    // Set the correct password so that the test operations should succeed.
    gMockPrompter.passwordToTry = "hunter2";
    await encrypt_decrypt_test();
    ok(
      gMockPrompter.numPrompts == 1,
      "There should've been one password prompt."
    );
    await delete_all_secrets();
  }

  // Check lock/unlock behaviour.
  // Unfortunately we can only test this automatically for the NSS key store.
  // Uncomment the outer keystore.isNSSKeyStore to test other key stores manually.
  if (keystore.isNSSKeyStore) {
    await delete_all_secrets();
    await encrypt_decrypt_test();
    await keystore.asyncLock();
    info("Keystore should be locked. Cancel the login request.");
    try {
      if (keystore.isNSSKeyStore) {
        gMockPrompter.passwordToTry = "DontTryThisPassword";
      }
      await keystore.asyncUnlock();
      ok(false, "Unlock should've rejected.");
    } catch (e) {
      ok(
        e.result == Cr.NS_ERROR_FAILURE || e.result == Cr.NS_ERROR_ABORT,
        "Rejected login prompt."
      );
    }
    // clean up
    if (keystore.isNSSKeyStore) {
      gMockPrompter.passwordToTry = "hunter2";
    }
    await delete_all_secrets();
  }
});

// Test that if we kick off a background operation and then call a synchronous function on the
// keystore, we don't deadlock.
add_task(async function() {
  await delete_all_secrets();

  let keystore = Cc["@mozilla.org/security/oskeystore;1"].getService(
    Ci.nsIOSKeyStore
  );
  let recoveryPhrase = await keystore.asyncGenerateSecret(LABELS[0]);
  ok(recoveryPhrase, "A recovery phrase should've been created.");

  try {
    let text = new Uint8Array(8192);
    let promise = keystore.asyncEncryptBytes(LABELS[0], text);
    /* eslint-disable no-unused-expressions */
    keystore.isNSSKeyStore; // we don't care what this is - we just need to access it
    /* eslint-enable no-unused-expressions */
    let ciphertext = await promise;
    ok(ciphertext, "We should have a ciphertext now.");
  } catch (e) {
    ok(false, "Error encrypting " + e);
  }

  await delete_all_secrets();
});

// Test that using a recovery phrase works.
add_task(async function() {
  await delete_all_secrets();

  let keystore = Cc["@mozilla.org/security/oskeystore;1"].getService(
    Ci.nsIOSKeyStore
  );

  let recoveryPhrase = await keystore.asyncGenerateSecret(LABELS[0]);
  ok(recoveryPhrase, "A recovery phrase should've been created.");

  let text = new Uint8Array([0x01, 0x00, 0x01]);
  let ciphertext = await keystore.asyncEncryptBytes(LABELS[0], text);
  ok(ciphertext, "We should have a ciphertext now.");

  await keystore.asyncDeleteSecret(LABELS[0]);
  // Decrypting should fail after deleting the secret.
  await keystore
    .asyncDecryptBytes(LABELS[0], ciphertext)
    .then(() =>
      ok(false, "decrypting didn't throw as expected after deleting the secret")
    )
    .catch(() =>
      ok(true, "decrypting threw as expected after deleting the secret")
    );

  await keystore.asyncRecoverSecret(LABELS[0], recoveryPhrase);
  let plaintext = await keystore.asyncDecryptBytes(LABELS[0], ciphertext);
  ok(
    plaintext.toString() == text.toString(),
    "Decrypted plaintext should be the same as text."
  );

  await delete_all_secrets();
});

// Test that trying to use a non-base64 recovery phrase fails.
add_task(async function() {
  await delete_all_secrets();

  let keystore = Cc["@mozilla.org/security/oskeystore;1"].getService(
    Ci.nsIOSKeyStore
  );
  await keystore
    .asyncRecoverSecret(LABELS[0], "@##$^&*()#$^&*(@#%&*_")
    .then(() =>
      ok(false, "base64-decoding non-base64 should have failed but didn't")
    )
    .catch(() => ok(true, "base64-decoding non-base64 failed as expected"));

  ok(
    !(await keystore.asyncSecretAvailable(LABELS[0])),
    "we didn't recover a secret, so the secret shouldn't be available"
  );
  let recoveryPhrase = await keystore.asyncGenerateSecret(LABELS[0]);
  ok(
    recoveryPhrase && !!recoveryPhrase.length,
    "we should be able to re-use that label to generate a new secret"
  );
  await delete_all_secrets();
});

// Test that re-using a label overwrites any previously-stored secret.
add_task(async function() {
  await delete_all_secrets();

  let keystore = Cc["@mozilla.org/security/oskeystore;1"].getService(
    Ci.nsIOSKeyStore
  );

  let recoveryPhrase = await keystore.asyncGenerateSecret(LABELS[0]);
  ok(recoveryPhrase, "A recovery phrase should've been created.");

  let text = new Uint8Array([0x66, 0x6f, 0x6f, 0x66]);
  let ciphertext = await keystore.asyncEncryptBytes(LABELS[0], text);
  ok(ciphertext, "We should have a ciphertext now.");

  let newRecoveryPhrase = await keystore.asyncGenerateSecret(LABELS[0]);
  ok(newRecoveryPhrase, "A new recovery phrase should've been created.");

  // The new secret replaced the old one so we shouldn't be able to decrypt the ciphertext now.
  await keystore
    .asyncDecryptBytes(LABELS[0], ciphertext)
    .then(() =>
      ok(false, "decrypting without the original key should have failed")
    )
    .catch(() =>
      ok(true, "decrypting without the original key failed as expected")
    );

  await keystore.asyncRecoverSecret(LABELS[0], recoveryPhrase);
  let plaintext = await keystore.asyncDecryptBytes(LABELS[0], ciphertext);
  ok(
    plaintext.toString() == text.toString(),
    "Decrypted plaintext should be the same as text (once we have the original key again)."
  );

  await delete_all_secrets();
});

// Test that re-using a label (this time using a recovery phrase) overwrites any previously-stored
// secret.
add_task(async function() {
  await delete_all_secrets();

  let keystore = Cc["@mozilla.org/security/oskeystore;1"].getService(
    Ci.nsIOSKeyStore
  );

  let recoveryPhrase = await keystore.asyncGenerateSecret(LABELS[0]);
  ok(recoveryPhrase, "A recovery phrase should've been created.");

  let newRecoveryPhrase = await keystore.asyncGenerateSecret(LABELS[0]);
  ok(newRecoveryPhrase, "A new recovery phrase should've been created.");

  let text = new Uint8Array([0x66, 0x6f, 0x6f, 0x66]);
  let ciphertext = await keystore.asyncEncryptBytes(LABELS[0], text);
  ok(ciphertext, "We should have a ciphertext now.");

  await keystore.asyncRecoverSecret(LABELS[0], recoveryPhrase);

  // We recovered the old secret, so decrypting ciphertext that had been encrypted with the newer
  // key should fail.
  await keystore
    .asyncDecryptBytes(LABELS[0], ciphertext)
    .then(() => ok(false, "decrypting without the new key should have failed"))
    .catch(() => ok(true, "decrypting without the new key failed as expected"));

  await keystore.asyncRecoverSecret(LABELS[0], newRecoveryPhrase);
  let plaintext = await keystore.asyncDecryptBytes(LABELS[0], ciphertext);
  ok(
    plaintext.toString() == text.toString(),
    "Decrypted plaintext should be the same as text (once we have the new key again)."
  );

  await delete_all_secrets();
});

// Test that trying to use recovery phrases that are the wrong size fails.
add_task(async function() {
  await delete_all_secrets();

  let keystore = Cc["@mozilla.org/security/oskeystore;1"].getService(
    Ci.nsIOSKeyStore
  );

  await keystore
    .asyncRecoverSecret(LABELS[0], "")
    .then(() => ok(false, "'recovering' with an empty key should have failed"))
    .catch(() => ok(true, "'recovering' with an empty key failed as expected"));
  ok(
    !(await keystore.asyncSecretAvailable(LABELS[0])),
    "we didn't recover a secret, so the secret shouldn't be available"
  );

  await keystore
    .asyncRecoverSecret(LABELS[0], "AAAAAA")
    .then(() =>
      ok(false, "recovering with a key that is too short should have failed")
    )
    .catch(() =>
      ok(true, "recovering with a key that is too short failed as expected")
    );
  ok(
    !(await keystore.asyncSecretAvailable(LABELS[0])),
    "we didn't recover a secret, so the secret shouldn't be available"
  );

  await keystore
    .asyncRecoverSecret(
      LABELS[0],
      "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
    )
    .then(() =>
      ok(false, "recovering with a key that is too long should have failed")
    )
    .catch(() =>
      ok(true, "recovering with a key that is too long failed as expected")
    );
  ok(
    !(await keystore.asyncSecretAvailable(LABELS[0])),
    "we didn't recover a secret, so the secret shouldn't be available"
  );

  let recoveryPhrase = await keystore.asyncGenerateSecret(LABELS[0]);
  ok(
    recoveryPhrase && !!recoveryPhrase.length,
    "we should be able to use that label to generate a new secret"
  );
  ok(
    await keystore.asyncSecretAvailable(LABELS[0]),
    "the generated secret should now be available"
  );

  await delete_all_secrets();
});
