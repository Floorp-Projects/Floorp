/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const { Downloads } = ChromeUtils.import(
  "resource://gre/modules/Downloads.jsm"
);

const server = createHttpServer();
server.registerDirectory("/data/", do_get_file("data"));

const BASE = `http://localhost:${server.identity.primaryPort}/data`;
const TEST_FILE = "file_download.txt";
const TEST_URL = BASE + "/" + TEST_FILE;

// We use different cookieBehaviors so that we can verify if we use the correct
// cookieBehavior if option.incognito is set. Note that we need to set a
// non-default value to the private cookieBehavior because the private
// cookieBehavior will mirror the regular cookieBehavior if the private pref is
// default value and the regular pref is non-default value. To avoid affecting
// the test by mirroring, we set the private cookieBehavior to a non-default
// value.
const TEST_REGULAR_COOKIE_BEHAVIOR =
  Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER;
const TEST_PRIVATE_COOKIE_BEHAVIOR = Ci.nsICookieService.BEHAVIOR_LIMIT_FOREIGN;

let downloadDir;

function observeDownloadChannel(uri, partitionKey, isPrivate) {
  return new Promise(resolve => {
    let observer = {
      observe(subject, topic, data) {
        if (topic === "http-on-modify-request") {
          let httpChannel = subject.QueryInterface(Ci.nsIHttpChannel);
          if (httpChannel.URI.spec != uri) {
            return;
          }

          let reqLoadInfo = httpChannel.loadInfo;
          let cookieJarSettings = reqLoadInfo.cookieJarSettings;

          // Check the partitionKey of the cookieJarSettings.
          equal(
            cookieJarSettings.partitionKey,
            partitionKey,
            "The loadInfo has the correct paritionKey"
          );

          // Check the cookieBehavior of the cookieJarSettings.
          equal(
            cookieJarSettings.cookieBehavior,
            isPrivate
              ? TEST_PRIVATE_COOKIE_BEHAVIOR
              : TEST_REGULAR_COOKIE_BEHAVIOR,
            "The loadInfo has the correct cookieBehavior"
          );

          Services.obs.removeObserver(observer, "http-on-modify-request");
          resolve();
        }
      },
    };

    Services.obs.addObserver(observer, "http-on-modify-request");
  });
}

async function waitForDownloads() {
  let list = await Downloads.getList(Downloads.ALL);
  let downloads = await list.getAll();

  let inprogress = downloads.filter(dl => !dl.stopped);
  return Promise.all(inprogress.map(dl => dl.whenSucceeded()));
}

function backgroundScript() {
  browser.test.onMessage.addListener(async (msg, ...args) => {
    if (msg == "download.request") {
      let options = args[0];

      try {
        let id = await browser.downloads.download(options);
        browser.test.sendMessage("download.done", { status: "success", id });
      } catch (error) {
        browser.test.sendMessage("download.done", {
          status: "error",
          errmsg: error.message,
        });
      }
    }
  });

  browser.test.sendMessage("ready");
}

// Remove a file in the downloads directory.
function remove(filename, recursive = false) {
  let file = downloadDir.clone();
  file.append(filename);
  file.remove(recursive);
}

add_task(function setup() {
  downloadDir = FileUtils.getDir("TmpD", ["downloads"]);
  downloadDir.createUnique(
    Ci.nsIFile.DIRECTORY_TYPE,
    FileUtils.PERMS_DIRECTORY
  );
  info(`Using download directory ${downloadDir.path}`);

  Services.prefs.setIntPref("browser.download.folderList", 2);
  Services.prefs.setComplexValue(
    "browser.download.dir",
    Ci.nsIFile,
    downloadDir
  );
  Services.prefs.setBoolPref("privacy.partition.network_state", true);

  Services.prefs.setIntPref(
    "network.cookie.cookieBehavior",
    TEST_REGULAR_COOKIE_BEHAVIOR
  );
  Services.prefs.setIntPref(
    "network.cookie.cookieBehavior.pbmode",
    TEST_PRIVATE_COOKIE_BEHAVIOR
  );

  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("browser.download.folderList");
    Services.prefs.clearUserPref("browser.download.dir");
    Services.prefs.clearUserPref("privacy.partition.network_state");
    Services.prefs.clearUserPref("network.cookie.cookieBehavior");
    Services.prefs.clearUserPref("network.cookie.cookieBehavior.pbmode");

    let entries = downloadDir.directoryEntries;
    while (entries.hasMoreElements()) {
      let entry = entries.nextFile;
      info(`Leftover file ${entry.path} in download directory`);
      ok(false, `Leftover file ${entry.path} in download directory`);
      entry.remove(false);
    }

    downloadDir.remove(false);
  });
});

add_task(async function test() {
  let extension = ExtensionTestUtils.loadExtension({
    background: `(${backgroundScript})()`,
    manifest: {
      permissions: ["downloads"],
    },
    incognitoOverride: "spanning",
  });

  function download(options) {
    extension.sendMessage("download.request", options);
    return extension.awaitMessage("download.done");
  }

  async function testDownload(url, partitionKey, isPrivate) {
    let options = { url, incognito: isPrivate };

    let promiseObserveDownloadChannel = observeDownloadChannel(
      url,
      partitionKey,
      isPrivate
    );

    let msg = await download(options);
    equal(msg.status, "success", `downloads.download() works`);

    await promiseObserveDownloadChannel;
    await waitForDownloads();
  }

  await extension.startup();
  await extension.awaitMessage("ready");
  info("extension started");

  // Call download() to check partitionKey of the download channel for the
  // regular browsing mode.
  await testDownload(
    TEST_URL,
    `(http,localhost,${server.identity.primaryPort})`,
    false
  );
  remove(TEST_FILE);

  // Call download again for the private browsing mode.
  await testDownload(
    TEST_URL,
    `(http,localhost,${server.identity.primaryPort})`,
    true
  );
  remove(TEST_FILE);

  await extension.unload();
});
