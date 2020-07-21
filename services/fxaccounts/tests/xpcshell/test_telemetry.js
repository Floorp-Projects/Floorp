/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { fxAccounts, FxAccounts } = ChromeUtils.import(
  "resource://gre/modules/FxAccounts.jsm"
);

const { PREF_ACCOUNT_ROOT } = ChromeUtils.import(
  "resource://gre/modules/FxAccountsCommon.js"
);

const { FxAccountsProfile } = ChromeUtils.import(
  "resource://gre/modules/FxAccountsProfile.jsm"
);

const { FxAccountsTelemetry } = ChromeUtils.import(
  "resource://gre/modules/FxAccountsTelemetry.jsm"
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

add_task(async function test_getEcosystemAnonId() {
  let ecosystemAnonId = "aaaaaaaaaaaaaaa";
  let testCases = [
    {
      // testing retrieving the ecosystemAnonId when the profile contains it
      throw: false,
      profileObj: { ecosystemAnonId },
      expectedEcosystemAnonId: ecosystemAnonId,
    },
    {
      // testing retrieving the ecosystemAnonId when the profile doesn't contain it
      throw: false,
      profileObj: {},
      expectedEcosystemAnonId: null,
    },
    {
      // testing retrieving the ecosystemAnonId when the profile is null
      throw: true,
      profileObj: null,
      expectedEcosystemAnonId: null,
    },
  ];

  for (const tc of testCases) {
    let profile = new FxAccountsProfile({ profileServerUrl: "http://testURL" });
    let telemetry = new FxAccountsTelemetry({});
    telemetry._internal = { profile };
    const mockProfile = sinon.mock(profile);
    const mockTelemetry = sinon.mock(telemetry);

    if (tc.throw) {
      mockProfile
        .expects("getProfile")
        .once()
        .throws(Error);
    } else {
      mockProfile
        .expects("getProfile")
        .once()
        .returns(tc.profileObj);
    }

    if (tc.expectedEcosystemAnonId) {
      mockTelemetry.expects("ensureEcosystemAnonId").never();
    } else {
      mockTelemetry
        .expects("ensureEcosystemAnonId")
        .once()
        .resolves("dddddddddd");
    }

    let actualEcoSystemAnonId = await telemetry.getEcosystemAnonId();
    mockProfile.verify();
    mockTelemetry.verify();
    Assert.equal(actualEcoSystemAnonId, tc.expectedEcosystemAnonId);
  }
});

add_task(async function test_ensureEcosystemAnonId() {
  let ecosystemAnonId = "bbbbbbbbbbbbbb";
  let testCases = [
    {
      profile: {
        ecosystemAnonId,
      },
      willErr: false,
    },
    {
      profile: {},
      willErr: true,
    },
  ];

  for (const tc of testCases) {
    let profile = new FxAccountsProfile({ profileServerUrl: "http://testURL" });
    let telemetry = new FxAccountsTelemetry({});
    telemetry._internal = { profile };
    let mockProfile = sinon.mock(profile);
    mockProfile
      .expects("ensureProfile")
      .once()
      .returns(tc.profile);

    if (tc.willErr) {
      const expectedError = `Profile data does not contain an 'ecosystemAnonId'`;
      try {
        await telemetry.ensureEcosystemAnonId();
      } catch (e) {
        Assert.equal(e.message, expectedError);
      }
    } else {
      let actualEcoSystemAnonId = await telemetry.ensureEcosystemAnonId();
      Assert.equal(actualEcoSystemAnonId, ecosystemAnonId);
    }
    mockProfile.verify();
  }
});
