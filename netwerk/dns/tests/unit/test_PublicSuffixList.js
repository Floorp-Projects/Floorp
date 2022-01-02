/* Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { PublicSuffixList } = ChromeUtils.import(
  "resource://gre/modules/netwerk-dns/PublicSuffixList.jsm"
);
const { TestUtils } = ChromeUtils.import(
  "resource://testing-common/TestUtils.jsm"
);

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

const CLIENT = PublicSuffixList.CLIENT;
const SIGNAL = "public-suffix-list-updated";

const PAYLOAD_UPDATED_RECORDS = {
  current: [{ id: "tld-dafsa", "commit-hash": "current-commit-hash" }],
  created: [],
  updated: [
    {
      old: { id: "tld-dafsa", "commit-hash": "current-commit-hash" },
      new: { id: "tld-dafsa", "commit-hash": "new-commit-hash" },
    },
  ],
  deleted: [],
};
const PAYLOAD_CREATED_RECORDS = {
  current: [],
  created: [
    {
      id: "tld-dafsa",
      "commit-hash": "new-commit-hash",
      attachment: {},
    },
  ],
  updated: [],
  deleted: [],
};
const PAYLOAD_UPDATED_AND_CREATED_RECORDS = {
  current: [{ id: "tld-dafsa", "commit-hash": "current-commit-hash" }],
  created: [{ id: "tld-dafsa", "commit-hash": "another-commit-hash" }],
  updated: [
    {
      old: { id: "tld-dafsa", "commit-hash": "current-commit-hash" },
      new: { id: "tld-dafsa", "commit-hash": "new-commit-hash" },
    },
  ],
  deleted: [],
};

const fakeDafsaBinFile = do_get_file("data/fake_remote_dafsa.bin");
const mockedFilePath = fakeDafsaBinFile.path;

function setup() {
  Services.prefs.setBoolPref("network.psl.onUpdate_notify", true);
}
setup();
registerCleanupFunction(() => {
  Services.prefs.clearUserPref("network.psl.onUpdate_notify");
});

/**
 * downloadCalled is used by mockDownload() and resetMockDownload()
 * to keep track weather CLIENT.attachments.download is called or not
 * downloadBackup will help restore CLIENT.attachments.download to original definition
 * notifyUpdateBackup will help restore PublicSuffixList.notifyUpdate to original definition
 */
let downloadCalled = false;
const downloadBackup = CLIENT.attachments.download;

// returns a fake fileURI and sends a signal with filePath and no nsifile
const mockDownload = () => {
  downloadCalled = false;
  CLIENT.attachments.download = async rec => {
    downloadCalled = true;
    return `file://${mockedFilePath}`; // Create a fake file URI
  };
};

// resetMockDownload() must be run at the end of the test that uses mockDownload()
const resetMockDownload = () => {
  CLIENT.attachments.download = downloadBackup;
};

add_task(async () => {
  info("File path sent when record is in DB.");

  await CLIENT.db.clear(); // Make sure there's no record initially
  await CLIENT.db.create({
    id: "tld-dafsa",
    "commit-hash": "fake-commit-hash",
    attachment: {},
  });

  mockDownload();

  const promiseSignal = TestUtils.topicObserved(SIGNAL);
  await PublicSuffixList.init();
  const observed = await promiseSignal;

  Assert.equal(
    observed[1],
    mockedFilePath,
    "File path sent when record is in DB."
  );
  await CLIENT.db.clear(); // Clean up the mockDownloaded record
  resetMockDownload();
});

add_task(async () => {
  info("File path sent when record updated.");

  mockDownload();

  const promiseSignal = TestUtils.topicObserved(SIGNAL);
  await PublicSuffixList.init();
  await CLIENT.emit("sync", { data: PAYLOAD_UPDATED_RECORDS });
  const observed = await promiseSignal;

  Assert.equal(
    observed[1],
    mockedFilePath,
    "File path sent when record updated."
  );
  resetMockDownload();
});

add_task(async () => {
  info("Attachment downloaded when record created.");

  mockDownload();

  await PublicSuffixList.init();
  await CLIENT.emit("sync", { data: PAYLOAD_CREATED_RECORDS });

  Assert.equal(
    downloadCalled,
    true,
    "Attachment downloaded when record created."
  );
  resetMockDownload();
});

add_task(async () => {
  info("Attachment downloaded when record updated.");

  mockDownload();

  await PublicSuffixList.init();
  await CLIENT.emit("sync", { data: PAYLOAD_UPDATED_RECORDS });

  Assert.equal(
    downloadCalled,
    true,
    "Attachment downloaded when record updated."
  );
  resetMockDownload();
});

add_task(async () => {
  info("No download when more than one record is changed.");

  mockDownload();

  await PublicSuffixList.init();
  await CLIENT.emit("sync", { data: PAYLOAD_UPDATED_AND_CREATED_RECORDS });

  Assert.equal(
    downloadCalled,
    false,
    "No download when more than one record is changed."
  );
  resetMockDownload();
});
