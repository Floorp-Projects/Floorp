/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { LoginRelatedRealmsParent } = ChromeUtils.importESModule(
  "resource://gre/modules/LoginRelatedRealms.sys.mjs"
);
const { RemoteSettings } = ChromeUtils.importESModule(
  "resource://services-settings/remote-settings.sys.mjs"
);

const REMOTE_SETTINGS_COLLECTION = "websites-with-shared-credential-backends";

add_task(async function test_related_domain_matching() {
  const client = RemoteSettings(REMOTE_SETTINGS_COLLECTION);
  const records = await client.get();
  console.log(records);

  // Assumes that the test collection is a 2D array with one subarray
  let relatedRealms = records[0].relatedRealms;
  relatedRealms = relatedRealms.flat();
  Assert.ok(relatedRealms);

  let LRR = new LoginRelatedRealmsParent();

  // We should not return unrelated realms
  let result = await LRR.findRelatedRealms("https://not-example.com");
  equal(result.length, 0, "Check that there were no related realms found");

  // We should not return unrelated realms given an unrelated subdomain
  result = await LRR.findRelatedRealms("https://sub.not-example.com");
  equal(result.length, 0, "Check that there were no related realms found");
  // We should return the related realms collection
  result = await LRR.findRelatedRealms("https://sub.example.com");
  equal(
    result.length,
    relatedRealms.length,
    "Ensure that three related realms were found"
  );

  // We should return the related realms collection minus the base domain that we searched with
  result = await LRR.findRelatedRealms("https://example.co.uk");
  equal(
    result.length,
    relatedRealms.length - 1,
    "Ensure that two related realms were found"
  );
});

add_task(async function test_newly_synced_collection() {
  // Initialize LoginRelatedRealmsParent so the sync handler is enabled
  let LRR = new LoginRelatedRealmsParent();
  await LRR.getSharedCredentialsCollection();

  const client = RemoteSettings(REMOTE_SETTINGS_COLLECTION);
  let records = await client.get();
  const record1 = {
    id: records[0].id,
    relatedRealms: records[0].relatedRealms,
  };

  // Assumes that the test collection is a 2D array with one subarray
  let originalRelatedRealms = records[0].relatedRealms;
  originalRelatedRealms = originalRelatedRealms.flat();
  Assert.ok(originalRelatedRealms);

  const updatedRelatedRealms = ["completely-different.com", "example.com"];
  const record2 = {
    id: "some-other-ID",
    relatedRealms: [updatedRelatedRealms],
  };
  const payload = {
    current: [record2],
    created: [record2],
    updated: [],
    deleted: [record1],
  };
  await RemoteSettings(REMOTE_SETTINGS_COLLECTION).emit("sync", {
    data: payload,
  });

  let [{ id, relatedRealms }] = await LRR.getSharedCredentialsCollection();
  equal(id, record2.id, "internal collection ID should be updated");
  equal(
    relatedRealms,
    record2.relatedRealms,
    "internal collection related realms should be updated"
  );

  // We should return only one result, and that result should be example.com
  // NOT other-example.com or example.co.uk
  let result = await LRR.findRelatedRealms("https://completely-different.com");
  equal(
    result.length,
    updatedRelatedRealms.length - 1,
    "Check that there is only one related realm found"
  );
  equal(
    result[0],
    "example.com",
    "Ensure that the updated collection should only match example.com"
  );
});

add_task(async function test_no_related_domains() {
  await LoginTestUtils.remoteSettings.cleanWebsitesWithSharedCredentials();

  const client = RemoteSettings(REMOTE_SETTINGS_COLLECTION);
  let records = await client.get();

  equal(records.length, 0, "Check that there are no related realms");

  let LRR = new LoginRelatedRealmsParent();

  Assert.ok(LRR.findRelatedRealms, "Ensure findRelatedRealms exists");

  let result = await LRR.findRelatedRealms("https://example.com");
  equal(result.length, 0, "Assert that there were no related realms found");
});

add_task(async function test_unrelated_subdomains() {
  await LoginTestUtils.remoteSettings.cleanWebsitesWithSharedCredentials();
  let testCollection = [
    ["slpl.bibliocommons.com", "slpl.overdrive.com"],
    ["springfield.overdrive.com", "coolcat.org"],
  ];
  await LoginTestUtils.remoteSettings.setupWebsitesWithSharedCredentials(
    testCollection
  );

  let LRR = new LoginRelatedRealmsParent();
  let result = await LRR.findRelatedRealms("https://evil.overdrive.com");
  equal(result.length, 0, "Assert that there were no related realms found");

  result = await LRR.findRelatedRealms("https://abc.slpl.bibliocommons.com");
  equal(result.length, 2, "Assert that two related realms were found");
  equal(result[0], testCollection[0][0]);
  equal(result[1], testCollection[0][1]);

  result = await LRR.findRelatedRealms("https://slpl.overdrive.com");
  console.log("what is result: " + result);
  equal(result.length, 1, "Assert that one related realm was found");
  for (let item of result) {
    notEqual(
      item,
      "coolcat.org",
      "coolcat.org is not related to slpl.overdrive.com"
    );
    notEqual(
      item,
      "springfield.overdrive.com",
      "springfield.overdrive.com is not related to slpl.overdrive.com"
    );
  }
});
