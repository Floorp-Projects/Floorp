/**
 * Tests for downloads.
 */

"use strict";

const { Downloads } = ChromeUtils.importESModule(
  "resource://gre/modules/Downloads.sys.mjs"
);
const { FileTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/FileTestUtils.sys.mjs"
);

const TEST_TARGET_FILE_NAME = "test-download.txt";
let fileURL;
let downloadList;

function createFileURL() {
  if (!fileURL) {
    const file = Services.dirsvc.get("TmpD", Ci.nsIFile);
    file.append("foo.txt");
    file.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, 0o600);

    fileURL = Services.io.newFileURI(file);
  }

  return fileURL;
}

async function createDownloadList() {
  if (!downloadList) {
    Downloads._promiseListsInitialized = null;
    Downloads._lists = {};
    Downloads._summaries = {};

    downloadList = await Downloads.getList(Downloads.ALL);
  }

  return downloadList;
}

add_task(async function test_all_downloads() {
  const url = createFileURL();
  const list = await createDownloadList();

  // First download.
  let download = await Downloads.createDownload({
    source: { url: url.spec, isPrivate: false },
    target: { path: FileTestUtils.getTempFile(TEST_TARGET_FILE_NAME).path },
  });
  Assert.ok(!!download);
  list.add(download);

  let view;
  let removePromise = new Promise(resolve => {
    view = {
      onDownloadAdded() {},
      onDownloadChanged() {},
      onDownloadRemoved() {
        resolve();
      },
    };
  });

  await list.addView(view);

  let items = await list.getAll();
  Assert.equal(items.length, 1);

  await new Promise(resolve => {
    Services.clearData.deleteData(
      Ci.nsIClearDataService.CLEAR_DOWNLOADS,
      value => {
        Assert.equal(value, 0);
        resolve();
      }
    );
  });

  await removePromise;

  items = await list.getAll();
  Assert.equal(items.length, 0);
});

add_task(async function test_range_downloads() {
  const url = createFileURL();
  const list = await createDownloadList();

  let download = await Downloads.createDownload({
    source: { url: url.spec, isPrivate: false },
    target: { path: FileTestUtils.getTempFile(TEST_TARGET_FILE_NAME).path },
  });
  Assert.ok(!!download);
  list.add(download);

  // Start + cancel. I need to have a startTime value.
  await download.start();
  await download.cancel();

  let items = await list.getAll();
  Assert.equal(items.length, 1);

  let view;
  let removePromise = new Promise(resolve => {
    view = {
      onDownloadAdded() {},
      onDownloadChanged() {},
      onDownloadRemoved() {
        resolve();
      },
    };
  });

  await list.addView(view);

  await new Promise(resolve => {
    Services.clearData.deleteDataInTimeRange(
      download.startTime.getTime() * 1000,
      download.startTime.getTime() * 1000,
      true /* user request */,
      Ci.nsIClearDataService.CLEAR_DOWNLOADS,
      value => {
        Assert.equal(value, 0);
        resolve();
      }
    );
  });

  await removePromise;

  items = await list.getAll();
  Assert.equal(items.length, 0);
});

add_task(async function test_principal_downloads() {
  const list = await createDownloadList();

  let download = await Downloads.createDownload({
    source: { url: "http://example.net", isPrivate: false },
    target: { path: FileTestUtils.getTempFile(TEST_TARGET_FILE_NAME).path },
  });
  Assert.ok(!!download);
  list.add(download);

  download = await Downloads.createDownload({
    source: { url: "http://example.com", isPrivate: false },
    target: { path: FileTestUtils.getTempFile(TEST_TARGET_FILE_NAME).path },
  });
  Assert.ok(!!download);
  list.add(download);

  let items = await list.getAll();
  Assert.equal(items.length, 2);

  let view;
  let removePromise = new Promise(resolve => {
    view = {
      onDownloadAdded() {},
      onDownloadChanged() {},
      onDownloadRemoved() {
        resolve();
      },
    };
  });

  await list.addView(view);

  let uri = Services.io.newURI("http://example.com");
  let principal = Services.scriptSecurityManager.createContentPrincipal(
    uri,
    {}
  );

  await new Promise(resolve => {
    Services.clearData.deleteDataFromPrincipal(
      principal,
      true /* user request */,
      Ci.nsIClearDataService.CLEAR_DOWNLOADS,
      value => {
        Assert.equal(value, 0);
        resolve();
      }
    );
  });

  await removePromise;

  items = await list.getAll();
  Assert.equal(items.length, 1);

  removePromise = new Promise(resolve => {
    view = {
      onDownloadAdded() {},
      onDownloadChanged() {},
      onDownloadRemoved() {
        resolve();
      },
    };
  });

  await list.addView(view);

  await new Promise(resolve => {
    Services.clearData.deleteData(
      Ci.nsIClearDataService.CLEAR_DOWNLOADS,
      value => {
        Assert.equal(value, 0);
        resolve();
      }
    );
  });

  await removePromise;

  items = await list.getAll();
  Assert.equal(items.length, 0);
});

add_task(async function test_basedomain_downloads() {
  const list = await createDownloadList();

  let download = await Downloads.createDownload({
    source: { url: "http://example.net", isPrivate: false },
    target: { path: FileTestUtils.getTempFile(TEST_TARGET_FILE_NAME).path },
  });
  Assert.ok(!!download);
  list.add(download);

  download = await Downloads.createDownload({
    source: { url: "http://test.example.net", isPrivate: false },
    target: { path: FileTestUtils.getTempFile(TEST_TARGET_FILE_NAME).path },
  });
  Assert.ok(!!download);
  list.add(download);

  download = await Downloads.createDownload({
    source: { url: "https://foo.bar.example.net", isPrivate: true },
    target: { path: FileTestUtils.getTempFile(TEST_TARGET_FILE_NAME).path },
  });
  Assert.ok(!!download);
  list.add(download);

  download = await Downloads.createDownload({
    source: { url: "http://example.com", isPrivate: false },
    target: { path: FileTestUtils.getTempFile(TEST_TARGET_FILE_NAME).path },
  });
  Assert.ok(!!download);
  list.add(download);

  let items = await list.getAll();
  Assert.equal(items.length, 4);

  let view;
  let removePromise = new Promise(resolve => {
    view = {
      onDownloadAdded() {},
      onDownloadChanged() {},
      onDownloadRemoved() {
        resolve();
      },
    };
  });

  await list.addView(view);

  await new Promise(resolve => {
    Services.clearData.deleteDataFromBaseDomain(
      "example.net",
      true /* user request */,
      Ci.nsIClearDataService.CLEAR_DOWNLOADS,
      value => {
        Assert.equal(value, 0);
        resolve();
      }
    );
  });

  await removePromise;

  items = await list.getAll();
  Assert.equal(items.length, 1);

  removePromise = new Promise(resolve => {
    view = {
      onDownloadAdded() {},
      onDownloadChanged() {},
      onDownloadRemoved() {
        resolve();
      },
    };
  });

  await list.addView(view);

  await new Promise(resolve => {
    Services.clearData.deleteData(
      Ci.nsIClearDataService.CLEAR_DOWNLOADS,
      value => {
        Assert.equal(value, 0);
        resolve();
      }
    );
  });

  await removePromise;

  items = await list.getAll();
  Assert.equal(items.length, 0);
});
