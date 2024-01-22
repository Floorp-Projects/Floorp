/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

do_get_profile();

add_task(async function test_clear_fingerprinting_protection_state() {
  info("Enabling fingerprinting randomization");
  Services.prefs.setBoolPref("privacy.resistFingerprinting", true);

  let uri = Services.io.newURI("https://example.com");
  let principal = Services.scriptSecurityManager.createContentPrincipal(
    uri,
    {}
  );
  let channel = Services.io.newChannelFromURI(
    uri,
    null, // aLoadingNode
    principal,
    null, // aTriggeringPrincipal
    Ci.nsILoadInfo.SEC_ALLOW_CROSS_ORIGIN_SEC_CONTEXT_IS_NULL,
    Ci.nsIContentPolicy.TYPE_DOCUMENT
  );

  // Test nsIClearDataService.deleteDataFromHost
  let key = Services.rfp.testGenerateRandomKey(channel);
  let keyStr = key.map(bytes => bytes.toString(16).padStart(2, "0")).join("");

  // Verify that the key remains the same without clearing.
  key = Services.rfp.testGenerateRandomKey(channel);
  let keyStrAgain = key
    .map(bytes => bytes.toString(16).padStart(2, "0"))
    .join("");

  Assert.equal(
    keyStr,
    keyStrAgain,
    "The fingerprinting randomization key remain the same without clearing."
  );

  info("Trigger the deleteDataFromHost");
  await new Promise(resolve => {
    Services.clearData.deleteDataFromHost(
      "example.com",
      true /* user request */,
      Ci.nsIClearDataService.CLEAR_FINGERPRINTING_PROTECTION_STATE,
      _ => {
        resolve();
      }
    );
  });

  key = Services.rfp.testGenerateRandomKey(channel);
  let newKeyStr = key
    .map(bytes => bytes.toString(16).padStart(2, "0"))
    .join("");

  Assert.notEqual(
    keyStr,
    newKeyStr,
    "The fingerprinting randomization key is reset properly."
  );

  // Test nsIClearDataService.deleteDataFromBaseDomain
  keyStr = newKeyStr;

  info("Trigger the deleteDataFromBaseDomain");
  await new Promise(resolve => {
    Services.clearData.deleteDataFromBaseDomain(
      "example.com",
      true /* user request */,
      Ci.nsIClearDataService.CLEAR_FINGERPRINTING_PROTECTION_STATE,
      _ => {
        resolve();
      }
    );
  });

  key = Services.rfp.testGenerateRandomKey(channel);
  newKeyStr = key.map(bytes => bytes.toString(16).padStart(2, "0")).join("");

  Assert.notEqual(
    keyStr,
    newKeyStr,
    "The fingerprinting randomization key is reset properly."
  );

  // Test nsIClearDataService.deleteDataFromPrincipal
  keyStr = newKeyStr;

  await new Promise(resolve => {
    Services.clearData.deleteDataFromPrincipal(
      principal,
      true /* user request */,
      Ci.nsIClearDataService.CLEAR_FINGERPRINTING_PROTECTION_STATE,
      _ => {
        resolve();
      }
    );
  });

  key = Services.rfp.testGenerateRandomKey(channel);
  newKeyStr = key.map(bytes => bytes.toString(16).padStart(2, "0")).join("");

  Assert.notEqual(
    keyStr,
    newKeyStr,
    "The fingerprinting randomization key is reset properly."
  );

  // Test nsIClearDataService.deleteData
  keyStr = newKeyStr;

  // Generate a key for another site.
  uri = Services.io.newURI("https://example.org");
  principal = Services.scriptSecurityManager.createContentPrincipal(uri, {});
  let channelAnother = Services.io.newChannelFromURI(
    uri,
    null, // aLoadingNode
    principal,
    null, // aTriggeringPrincipal
    Ci.nsILoadInfo.SEC_ALLOW_CROSS_ORIGIN_SEC_CONTEXT_IS_NULL,
    Ci.nsIContentPolicy.TYPE_DOCUMENT
  );
  key = Services.rfp.testGenerateRandomKey(channelAnother);
  let keyStrAnother = key
    .map(bytes => bytes.toString(16).padStart(2, "0"))
    .join("");

  info("Trigger the deleteData");
  await new Promise(resolve => {
    Services.clearData.deleteData(
      Ci.nsIClearDataService.CLEAR_FINGERPRINTING_PROTECTION_STATE,
      _ => {
        resolve();
      }
    );
  });

  key = Services.rfp.testGenerateRandomKey(channel);
  newKeyStr = key.map(bytes => bytes.toString(16).padStart(2, "0")).join("");

  Assert.notEqual(
    keyStr,
    newKeyStr,
    "The fingerprinting randomization key is reset properly."
  );

  // Verify whether deleteData clears another site as well.
  key = Services.rfp.testGenerateRandomKey(channelAnother);
  newKeyStr = key.map(bytes => bytes.toString(16).padStart(2, "0")).join("");

  Assert.notEqual(
    keyStrAnother,
    newKeyStr,
    "The fingerprinting randomization key is reset properly for another site."
  );

  Services.prefs.clearUserPref("privacy.resistFingerprinting");
});
