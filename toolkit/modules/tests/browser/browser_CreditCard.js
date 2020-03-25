/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { CreditCard } = ChromeUtils.import(
  "resource://gre/modules/CreditCard.jsm"
);
const { OSKeyStore } = ChromeUtils.import(
  "resource://gre/modules/OSKeyStore.jsm"
);
const { OSKeyStoreTestUtils } = ChromeUtils.import(
  "resource://testing-common/OSKeyStoreTestUtils.jsm"
);

let oldGetters = {};
let gFakeLoggedIn = true;

add_task(function setup() {
  OSKeyStoreTestUtils.setup();
  oldGetters.isLoggedIn = Object.getOwnPropertyDescriptor(
    OSKeyStore,
    "isLoggedIn"
  ).get;
  OSKeyStore.__defineGetter__("isLoggedIn", () => gFakeLoggedIn);
  registerCleanupFunction(async () => {
    OSKeyStore.__defineGetter__("isLoggedIn", oldGetters.isLoggedIn);
    await OSKeyStoreTestUtils.cleanup();

    // CreditCard.jsm, OSKeyStore.jsm, and OSKeyStoreTestUtils.jsm are imported
    // into the global scope -- the window -- above. If they're not deleted,
    // they outlive the test and are reported as a leak.
    delete window.OSKeyStore;
    delete window.CreditCard;
    delete window.OSKeyStoreTestUtils;
  });
});

add_task(async function test_getLabel_withOSKeyStore() {
  ok(
    OSKeyStore.isLoggedIn,
    "Confirm that OSKeyStore is faked and thinks it is logged in"
  );

  const ccNumber = "4111111111111111";
  const encryptedNumber = await OSKeyStore.encrypt(ccNumber);
  const decryptedNumber = await OSKeyStore.decrypt(encryptedNumber);
  is(decryptedNumber, ccNumber, "Decrypted CC number should match original");

  const name = "Foxkeh";
  const label = CreditCard.getLabel({ name: "Foxkeh", number: ccNumber });
  is(label, `**** 1111, ${name}`, "Label matches");
});
