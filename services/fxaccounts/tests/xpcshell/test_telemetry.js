/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { fxAccounts } = ChromeUtils.import(
  "resource://gre/modules/FxAccounts.jsm"
);

const { PREF_ACCOUNT_ROOT } = ChromeUtils.import(
  "resource://gre/modules/FxAccountsCommon.js"
);

_("Misc tests for FxAccounts.telemetry");

const MOCK_HASHED_UID = "00112233445566778899aabbccddeeff";
const MOCK_DEVICE_ID = "ffeeddccbbaa99887766554433221100";

add_task(function test_sanitized_uid() {
  Services.prefs.deleteBranch(
    "identity.fxaccounts.account.telemetry.sanitized_uid"
  );

  // Returns `null` by default.
  Assert.equal(fxAccounts.telemetry.getSanitizedUID(), null);

  // Returns provided value if set.
  fxAccounts.telemetry._setHashedUID(MOCK_HASHED_UID);
  Assert.equal(fxAccounts.telemetry.getSanitizedUID(), MOCK_HASHED_UID);

  // Reverts to unset for falsey values.
  fxAccounts.telemetry._setHashedUID("");
  Assert.equal(fxAccounts.telemetry.getSanitizedUID(), null);
});

add_task(function test_sanitize_device_id() {
  Services.prefs.deleteBranch(
    "identity.fxaccounts.account.telemetry.sanitized_uid"
  );

  // Returns `null` by default.
  Assert.equal(fxAccounts.telemetry.sanitizeDeviceId(MOCK_DEVICE_ID), null);

  // Hashes with the sanitized UID if set.
  // (test value here is SHA256(MOCK_DEVICE_ID + MOCK_HASHED_UID))
  fxAccounts.telemetry._setHashedUID(MOCK_HASHED_UID);
  Assert.equal(
    fxAccounts.telemetry.sanitizeDeviceId(MOCK_DEVICE_ID),
    "dd7c845006df9baa1c6d756926519c8ce12f91230e11b6057bf8ec65f9b55c1a"
  );

  // Reverts to unset for falsey values.
  fxAccounts.telemetry._setHashedUID("");
  Assert.equal(fxAccounts.telemetry.sanitizeDeviceId(MOCK_DEVICE_ID), null);
});
