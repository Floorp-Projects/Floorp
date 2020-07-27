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

const { FxAccountsProfileClient } = ChromeUtils.import(
  "resource://gre/modules/FxAccountsProfileClient.jsm"
);

const { FxAccountsTelemetry } = ChromeUtils.import(
  "resource://gre/modules/FxAccountsTelemetry.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  FxAccountsConfig: "resource://gre/modules/FxAccountsConfig.jsm",
  jwcrypto: "resource://services-crypto/jwcrypto.jsm",
});

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
  const ecosystemAnonId = "aaaaaaaaaaaaaaa";
  const testCases = [
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
    const profile = new FxAccountsProfile({
      profileServerUrl: "http://testURL",
    });
    const telemetry = new FxAccountsTelemetry({});
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

    const actualEcoSystemAnonId = await telemetry.getEcosystemAnonId();
    mockProfile.verify();
    mockTelemetry.verify();
    Assert.equal(actualEcoSystemAnonId, tc.expectedEcosystemAnonId);
  }
});

add_task(async function test_ensureEcosystemAnonId_fromProfile() {
  const expecteErrorMessage =
    "Profile data does not contain an 'ecosystemAnonId'";
  const expectedEcosystemAnonId = "aaaaaaaaaaa";

  const testCases = [
    {
      profile: {
        ecosystemAnonId: expectedEcosystemAnonId,
      },
      generatePlaceholder: true,
    },
    {
      profile: {},
      generatePlaceholder: false,
    },
  ];

  for (const tc of testCases) {
    const profile = new FxAccountsProfile({
      profileServerUrl: "http://testURL",
    });
    const telemetry = new FxAccountsTelemetry({
      profile,
      withCurrentAccountState: async cb => {
        return cb({});
      },
    });
    const mockProfile = sinon.mock(profile);

    mockProfile
      .expects("ensureProfile")
      .once()
      .returns(tc.profile);

    if (tc.profile.ecosystemAnonId) {
      const actualEcoSystemAnonId = await telemetry.ensureEcosystemAnonId();
      Assert.equal(actualEcoSystemAnonId, expectedEcosystemAnonId);
    } else {
      try {
        await telemetry.ensureEcosystemAnonId(tc.generatePlaceholder);
      } catch (e) {
        Assert.equal(expecteErrorMessage, e.message);
      }
    }
    mockProfile.verify();
  }
});

add_task(async function test_ensureEcosystemAnonId_failToGenerateKeys() {
  const expectedErrorMessage =
    "Unable to fetch ecosystem_anon_id_keys from FxA server";
  const testCases = [
    {
      serverConfig: {},
    },
    {
      serverConfig: {
        ecosystem_anon_id_keys: [],
      },
    },
  ];
  for (const tc of testCases) {
    const profile = new FxAccountsProfile({
      profileServerUrl: "http://testURL",
    });
    const telemetry = new FxAccountsTelemetry({
      profile,
      withCurrentAccountState: async cb => {
        return cb({});
      },
    });
    const mockProfile = sinon.mock(profile);
    const mockFxAccountsConfig = sinon.mock(FxAccountsConfig);

    mockProfile
      .expects("ensureProfile")
      .once()
      .returns({});

    mockFxAccountsConfig
      .expects("fetchConfigDocument")
      .once()
      .returns(tc.serverConfig);

    try {
      await telemetry.ensureEcosystemAnonId(true);
    } catch (e) {
      Assert.equal(expectedErrorMessage, e.message);
      mockProfile.verify();
      mockFxAccountsConfig.verify();
    }
  }
});

add_task(async function test_ensureEcosystemAnonId_clientRace() {
  const expectedEcosystemAnonId = "bbbbbbbbbbbb";
  const expectedErrrorMessage = "test error at 'setEcosystemAnonId'";

  const testCases = [
    {
      errorCode: 412,
      errorMessage: null,
    },
    {
      errorCode: 405,
      errorMessage: expectedErrrorMessage,
    },
  ];

  for (const tc of testCases) {
    const profileClient = new FxAccountsProfileClient({
      serverURL: "http://testURL",
    });
    const profile = new FxAccountsProfile({ profileClient });
    const telemetry = new FxAccountsTelemetry({
      profile,
      withCurrentAccountState: async cb => {
        return cb({});
      },
    });
    const mockProfile = sinon.mock(profile);
    const mockFxAccountsConfig = sinon.mock(FxAccountsConfig);
    const mockJwcrypto = sinon.mock(jwcrypto);
    const mockProfileClient = sinon.mock(profileClient);

    mockProfile
      .expects("ensureProfile")
      .once()
      .returns({});

    mockFxAccountsConfig
      .expects("fetchConfigDocument")
      .once()
      .returns({
        ecosystem_anon_id_keys: ["testKey"],
      });

    mockJwcrypto
      .expects("generateJWE")
      .once()
      .returns(expectedEcosystemAnonId);

    mockProfileClient
      .expects("setEcosystemAnonId")
      .once()
      .throws({
        code: tc.errorCode,
        message: tc.errorMessage,
      });

    if (tc.errorCode === 412) {
      mockProfile
        .expects("ensureProfile")
        .once()
        .returns({
          ecosystemAnonId: expectedEcosystemAnonId,
        });

      const actualEcosystemAnonId = await telemetry.ensureEcosystemAnonId(true);
      Assert.equal(expectedEcosystemAnonId, actualEcosystemAnonId);
    } else {
      try {
        await telemetry.ensureEcosystemAnonId(true);
      } catch (e) {
        Assert.equal(expectedErrrorMessage, e.message);
      }
    }

    mockProfile.verify();
    mockFxAccountsConfig.verify();
    mockJwcrypto.verify();
    mockProfileClient.verify();
  }
});

add_task(async function test_ensureEcosystemAnonId_updateAccountData() {
  const expectedEcosystemUserId = "aaaaaaaaaa";
  const expectedEcosystemAnonId = "bbbbbbbbbbbb";
  const profileClient = new FxAccountsProfileClient({
    serverURL: "http://testURL",
  });
  const profile = new FxAccountsProfile({ profileClient });
  const fxa = new FxAccounts();
  fxa.withCurrentAccountState = async cb =>
    cb({
      ecosystemUserId: expectedEcosystemUserId,
      updateUserAccountData: fields => {
        Assert.equal(expectedEcosystemUserId, fields.ecosystemUserId);
      },
    });
  fxa.profile = profile;
  const telemetry = new FxAccountsTelemetry(fxa);
  const mockProfile = sinon.mock(profile);
  const mockFxAccountsConfig = sinon.mock(FxAccountsConfig);
  const mockJwcrypto = sinon.mock(jwcrypto);
  const mockProfileClient = sinon.mock(profileClient);

  mockProfile
    .expects("ensureProfile")
    .once()
    .returns({});

  mockFxAccountsConfig
    .expects("fetchConfigDocument")
    .once()
    .returns({
      ecosystem_anon_id_keys: ["testKey"],
    });

  mockJwcrypto
    .expects("generateJWE")
    .once()
    .returns(expectedEcosystemAnonId);

  mockProfileClient
    .expects("setEcosystemAnonId")
    .once()
    .returns(null);

  const actualEcosystemAnonId = await telemetry.ensureEcosystemAnonId(true);
  Assert.equal(expectedEcosystemAnonId, actualEcosystemAnonId);

  mockProfile.verify();
  mockFxAccountsConfig.verify();
  mockJwcrypto.verify();
  mockProfileClient.verify();
});
