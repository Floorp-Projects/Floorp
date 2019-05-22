"use strict";

const { BlocklistClients } = ChromeUtils.import("resource://services-common/blocklist-clients.js");
const { Utils } = ChromeUtils.import("resource://services-settings/Utils.jsm");
const { RemoteSettings } = ChromeUtils.import("resource://services-settings/remote-settings.js");
const { OneCRLBlocklistClient } = BlocklistClients.initialize();
const { AppConstants } = ChromeUtils.import("resource://gre/modules/AppConstants.jsm");


add_task(async function test_uses_a_custom_signer() {
  Assert.notEqual(OneCRLBlocklistClient.signerName, RemoteSettings("not-specified").signerName);
});

add_task(async function test_has_initial_dump() {
  Assert.ok(await Utils.hasLocalDump(OneCRLBlocklistClient.bucketName, OneCRLBlocklistClient.collectionName));
});

add_task(async function test_default_jexl_filter_is_used() {
  const countInDump = (await OneCRLBlocklistClient.get()).length;

  // Create two fake records, one whose target expression is falsy (and will
  // this be filtered by `.get()`) and another one with a truthy filter.
  const collection = await OneCRLBlocklistClient.openCollection();
  await collection.create({ filter_expression: "1 == 2" }); // filtered.
  await collection.create({ filter_expression: "1 == 1" });
await collection.db.saveLastModified(42); // Fake sync state: prevent from loading JSON dump.

  Assert.equal((await OneCRLBlocklistClient.get()).length, countInDump + 1);
});

add_task({
skip_if: () => !AppConstants.MOZ_NEW_CERT_STORAGE,
}, async function test_revocations_are_updated_on_sync_with_cert_storage() {
  const certList = Cc["@mozilla.org/security/certstorage;1"]
    .getService(Ci.nsICertStorage);
  const has_revocations = () => new Promise((resolve) => {
    certList.hasPriorData(Ci.nsICertStorage.DATA_TYPE_REVOCATION, (rv, hasPriorData) => {
      if (rv == Cr.NS_OK) {
        return resolve(hasPriorData);
      }
      return resolve(false);
    });
  });

  Assert.ok(!(await has_revocations()));

  await OneCRLBlocklistClient.emit("sync", { data: {
    current: [],
    created: [{
      issuerName: "MBIxEDAOBgNVBAMMB1Rlc3QgQ0E=",
      serialNumber: "a0X7/7DlTaedpgrIJg25iBPOkIM=",
    }],
    updated: [],
    deleted: [],
  }});

  Assert.ok(await has_revocations());
});

add_task({
  skip_if: () => AppConstants.MOZ_NEW_CERT_STORAGE,
}, async function test_revocations_are_updated_on_sync() {
  const profile = do_get_profile();
  const revocations = profile.clone();
  revocations.append("revocations.txt");
  const before = revocations.lastModifiedTime;

  await OneCRLBlocklistClient.emit("sync", { data: {
    current: [{
      issuerName: "MBIxEDAOBgNVBAMMB1Rlc3QgQ0E=",
      serialNumber: "a0X7/7DlTaedpgrIJg25iBPOkIM=",
    }],
  }});

  const after = revocations.lastModifiedTime;
  Assert.notEqual(before, after, "revocation file was modified.");
});
