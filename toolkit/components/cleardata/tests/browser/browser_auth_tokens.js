/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests for AuthTokensCleaner.
 */

const TEST_SECRET = "secret";
const TEST_PRINCIPAL = Services.scriptSecurityManager.createContentPrincipalFromOrigin(
  "https://example.com"
);
const TEST_CLEAR_DATA_FLAGS = Services.clearData.CLEAR_AUTH_TOKENS;

const pk11db = Cc["@mozilla.org/security/pk11tokendb;1"].getService(
  Ci.nsIPK11TokenDB
);

const { LoginTestUtils } = ChromeUtils.import(
  "resource://testing-common/LoginTestUtils.jsm"
);

function testLoggedIn(isLoggedIn) {
  Assert.equal(
    pk11db.getInternalKeyToken().isLoggedIn(),
    isLoggedIn,
    `Should ${isLoggedIn ? "" : "not "}be logged in`
  );
  pk11db.getInternalKeyToken().isLoggedIn();
}

function clearData({ deleteBy = "all", hasUserInput = false } = {}) {
  return new Promise(resolve => {
    if (deleteBy == "principal") {
      Services.clearData.deleteDataFromPrincipal(
        TEST_PRINCIPAL,
        hasUserInput,
        TEST_CLEAR_DATA_FLAGS,
        value => {
          Assert.equal(value, 0);
          resolve();
        }
      );
    } else if (deleteBy == "baseDomain") {
      Services.clearData.deleteDataFromBaseDomain(
        TEST_PRINCIPAL.baseDomain,
        hasUserInput,
        TEST_CLEAR_DATA_FLAGS,
        value => {
          Assert.equal(value, 0);
          resolve();
        }
      );
    } else {
      Services.clearData.deleteData(TEST_CLEAR_DATA_FLAGS, value => {
        Assert.equal(value, 0);
        resolve();
      });
    }
  });
}

function runTest({ deleteBy, hasUserInput }) {
  testLoggedIn(false);

  info("Setup primary password and login");
  LoginTestUtils.primaryPassword.enable(true);
  testLoggedIn(true);

  info(
    `Clear AuthTokensCleaner data for ${deleteBy}, hasUserInput: ${hasUserInput}`
  );
  clearData({ deleteBy, hasUserInput });

  // The auth tokens cleaner cannot delete by principal or baseDomain
  // (yet). If this method is called, it will check whether the used requested
  // the clearing. If the user requested clearing, it will over-clear (clear
  // all data). If the request didn't come from the user, for example from the
  // PurgeTrackerService, it will not clear anything to avoid clearing storage
  // unrelated to the baseDomain or principal.
  let isCleared = deleteBy == "all" || hasUserInput;
  testLoggedIn(!isCleared);

  // Cleanup
  let sdr = Cc["@mozilla.org/security/sdr;1"].getService(
    Ci.nsISecretDecoderRing
  );
  sdr.logoutAndTeardown();
  LoginTestUtils.primaryPassword.disable();
}

add_task(async function test_deleteAll() {
  runTest({ deleteBy: "all" });
});

add_task(async function test_deleteByPrincipal() {
  for (let hasUserInput of [false, true]) {
    runTest({ deleteBy: "principal", hasUserInput });
  }
});

add_task(async function test_deleteByBaseDomain() {
  for (let hasUserInput of [false, true]) {
    runTest({ deleteBy: "baseDomain", hasUserInput });
  }
});
