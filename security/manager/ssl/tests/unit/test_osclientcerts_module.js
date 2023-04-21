/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/publicdomain/zero/1.0/
"use strict";

// Tests that the platform can load the osclientcerts module.

// Ensure that the appropriate initialization has happened.
Services.prefs.setBoolPref("security.osclientcerts.autoload", false);
do_get_profile();

const { TestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/TestUtils.sys.mjs"
);

async function check_osclientcerts_module_loaded() {
  // Loading happens asynchronously, so we have to wait for the notification.
  await TestUtils.topicObserved("psm:load-os-client-certs-module-task-ran");
  let testModule = checkPKCS11ModuleExists(
    "OS Client Cert Module",
    "osclientcerts"
  );

  // Check that listing the slots for the osclientcerts module works.
  let testModuleSlotNames = Array.from(
    testModule.listSlots(),
    slot => slot.name
  );
  testModuleSlotNames.sort();
  const expectedSlotNames = ["OS Client Cert Slot"];
  deepEqual(
    testModuleSlotNames,
    expectedSlotNames,
    "Actual and expected slot names should be equal"
  );
}

add_task(async function run_test() {
  // Check that if we haven't loaded the osclientcerts module, we don't find it
  // in the module list.
  checkPKCS11ModuleNotPresent("OS Client Cert Module", "osclientcerts");

  // Check that enabling the pref that loads the osclientcerts module makes it
  // appear in the module list.
  Services.prefs.setBoolPref("security.osclientcerts.autoload", true);
  await check_osclientcerts_module_loaded();

  // Check that disabling the pref that loads the osclientcerts module (thus
  // unloading the module) makes it disappear from the module list.
  Services.prefs.setBoolPref("security.osclientcerts.autoload", false);
  checkPKCS11ModuleNotPresent("OS Client Cert Module", "osclientcerts");

  // Check that loading the module again succeeds.
  Services.prefs.setBoolPref("security.osclientcerts.autoload", true);
  await check_osclientcerts_module_loaded();

  // And once more check that unloading succeeds.
  Services.prefs.setBoolPref("security.osclientcerts.autoload", false);
  checkPKCS11ModuleNotPresent("OS Client Cert Module", "osclientcerts");
});
