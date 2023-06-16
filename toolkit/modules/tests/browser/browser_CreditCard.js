/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { CreditCard } = ChromeUtils.importESModule(
  "resource://gre/modules/CreditCard.sys.mjs"
);
const { OSKeyStore } = ChromeUtils.importESModule(
  "resource://gre/modules/OSKeyStore.sys.mjs"
);
const { OSKeyStoreTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/OSKeyStoreTestUtils.sys.mjs"
);

let oldGetters = {};
let gFakeLoggedIn = true;

add_setup(function () {
  OSKeyStoreTestUtils.setup();
  oldGetters.isLoggedIn = Object.getOwnPropertyDescriptor(
    OSKeyStore,
    "isLoggedIn"
  ).get;
  OSKeyStore.__defineGetter__("isLoggedIn", () => gFakeLoggedIn);
  registerCleanupFunction(async () => {
    OSKeyStore.__defineGetter__("isLoggedIn", oldGetters.isLoggedIn);
    await OSKeyStoreTestUtils.cleanup();
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
