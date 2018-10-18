/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

ChromeUtils.import("resource://gre/modules/CreditCard.jsm");
ChromeUtils.import("resource://formautofill/MasterPassword.jsm");

let oldGetters = {};
let gFakeLoggedIn = true;

add_task(function setup() {
  oldGetters._token = Object.getOwnPropertyDescriptor(MasterPassword, "_token").get;
  oldGetters.isEnabled = Object.getOwnPropertyDescriptor(MasterPassword, "isEnabled").get;
  oldGetters.isLoggedIn = Object.getOwnPropertyDescriptor(MasterPassword, "isLoggedIn").get;
  MasterPassword.__defineGetter__("_token", () => { return {hasPassword: true}; });
  MasterPassword.__defineGetter__("isEnabled", () => true);
  MasterPassword.__defineGetter__("isLoggedIn", () => gFakeLoggedIn);
  registerCleanupFunction(() => {
    MasterPassword.__defineGetter__("_token", oldGetters._token);
    MasterPassword.__defineGetter__("isEnabled", oldGetters.isEnabled);
    MasterPassword.__defineGetter__("isLoggedIn", oldGetters.isLoggedIn);

    // CreditCard.jsm and MasterPassword.jsm are imported into the global scope
    // -- the window -- above. If they're not deleted, they outlive the test and
    // are reported as a leak.
    delete window.MasterPassword;
    delete window.CreditCard;
  });
});

add_task(async function test_getLabel_withMasterPassword() {
  ok(MasterPassword.isEnabled, "Confirm that MasterPassword is faked and thinks it is enabled");
  ok(MasterPassword.isLoggedIn, "Confirm that MasterPassword is faked and thinks it is logged in");

  const ccNumber = "4111111111111111";
  const encryptedNumber = await MasterPassword.encrypt(ccNumber);
  const decryptedNumber = await MasterPassword.decrypt(encryptedNumber);
  is(decryptedNumber, ccNumber, "Decrypted CC number should match original");

  const name = "Foxkeh";
  const creditCard = new CreditCard({encryptedNumber, name: "Foxkeh"});
  const label = await creditCard.getLabel({showNumbers: true});
  is(label, `${ccNumber}, ${name}`);
});
