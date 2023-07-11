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

add_task(async function () {
  await delete_all_secrets();
  await encrypt_decrypt_test();
  await delete_all_secrets();
});

// Test that using a recovery phrase works.
add_task(async function () {
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
add_task(async function () {
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
add_task(async function () {
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
add_task(async function () {
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
add_task(async function () {
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
