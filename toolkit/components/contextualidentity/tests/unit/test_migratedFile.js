"use strict";

const profileDir = do_get_profile();

const { ContextualIdentityService } = ChromeUtils.import(
  "resource://gre/modules/ContextualIdentityService.jsm"
);
const { OS } = ChromeUtils.import("resource://gre/modules/osfile.jsm");

const TEST_STORE_FILE_PATH = OS.Path.join(
  profileDir.path,
  "test-containers.json"
);

// Test the containers JSON file migrations.
add_task(async function migratedFile() {
  // Let's create a file that has to be migrated.
  const oldFileData = {
    version: 2,
    lastUserContextId: 6,
    identities: [
      {
        userContextId: 1,
        public: true,
        icon: "fingerprint",
        color: "blue",
        l10nID: "userContextPersonal.label",
        accessKey: "userContextPersonal.accesskey",
        telemetryId: 1,
      },
      {
        userContextId: 2,
        public: true,
        icon: "briefcase",
        color: "orange",
        l10nID: "userContextWork.label",
        accessKey: "userContextWork.accesskey",
        telemetryId: 2,
      },
      {
        userContextId: 3,
        public: true,
        icon: "dollar",
        color: "green",
        l10nID: "userContextBanking.label",
        accessKey: "userContextBanking.accesskey",
        telemetryId: 3,
      },
      {
        userContextId: 4,
        public: true,
        icon: "cart",
        color: "pink",
        l10nID: "userContextShopping.label",
        accessKey: "userContextShopping.accesskey",
        telemetryId: 4,
      },
      {
        userContextId: 5,
        public: false,
        icon: "",
        color: "",
        name: "userContextIdInternal.thumbnail",
        accessKey: "",
      },
      {
        userContextId: 6,
        public: true,
        icon: "cart",
        color: "ping",
        name: "Custom user-created identity",
      },
    ],
  };

  await OS.File.writeAtomic(TEST_STORE_FILE_PATH, JSON.stringify(oldFileData), {
    tmpPath: TEST_STORE_FILE_PATH + ".tmp",
  });

  let cis = ContextualIdentityService.createNewInstanceForTesting(
    TEST_STORE_FILE_PATH
  );
  ok(!!cis, "We have our instance of ContextualIdentityService");

  // Check that the custom user-created identity exists.

  const expectedPublicLength = oldFileData.identities.filter(
    identity => identity.public
  ).length;
  const publicIdentities = cis.getPublicIdentities();
  const oldLastIdentity =
    oldFileData.identities[oldFileData.identities.length - 1];
  const customUserCreatedIdentity = publicIdentities
    .filter(identity => identity.name === oldLastIdentity.name)
    .pop();

  equal(
    publicIdentities.length,
    expectedPublicLength,
    "We should have the expected number of public identities"
  );
  ok(!!customUserCreatedIdentity, "Got the custom user-created identity");

  // Check that the reserved userContextIdInternal.webextStorageLocal identity exists.

  const webextStorageLocalPrivateId = ContextualIdentityService._defaultIdentities
    .filter(
      identity => identity.name === "userContextIdInternal.webextStorageLocal"
    )
    .pop().userContextId;

  const privWebExtStorageLocal = cis.getPrivateIdentity(
    "userContextIdInternal.webextStorageLocal"
  );
  equal(
    privWebExtStorageLocal && privWebExtStorageLocal.userContextId,
    webextStorageLocalPrivateId,
    "We should have the default userContextIdInternal.webextStorageLocal private identity"
  );
});
