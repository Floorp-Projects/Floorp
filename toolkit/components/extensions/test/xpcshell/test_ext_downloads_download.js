/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

/* global OS */

Cu.import("resource://gre/modules/osfile.jsm");
Cu.import("resource://gre/modules/Downloads.jsm");

const server = createHttpServer();
server.registerDirectory("/data/", do_get_file("data"));

const WINDOWS = AppConstants.platform == "win";

const BASE = `http://localhost:${server.identity.primaryPort}/data`;
const FILE_NAME = "file_download.txt";
const FILE_URL = BASE + "/" + FILE_NAME;
const FILE_NAME_UNIQUE = "file_download(1).txt";
const FILE_LEN = 46;

let downloadDir;

function setup() {
  downloadDir = FileUtils.getDir("TmpD", ["downloads"]);
  downloadDir.createUnique(Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);
  do_print(`Using download directory ${downloadDir.path}`);

  Services.prefs.setIntPref("browser.download.folderList", 2);
  Services.prefs.setComplexValue("browser.download.dir", Ci.nsIFile, downloadDir);

  do_register_cleanup(() => {
    Services.prefs.clearUserPref("browser.download.folderList");
    Services.prefs.clearUserPref("browser.download.dir");

    let entries = downloadDir.directoryEntries;
    while (entries.hasMoreElements()) {
      let entry = entries.getNext().QueryInterface(Ci.nsIFile);
      ok(false, `Leftover file ${entry.path} in download directory`);
      entry.remove(false);
    }

    downloadDir.remove(false);
  });
}

function backgroundScript() {
  let blobUrl;
  browser.test.onMessage.addListener((msg, ...args) => {
    if (msg == "download.request") {
      let options = args[0];

      if (options.blobme) {
        let blob = new Blob(options.blobme);
        delete options.blobme;
        blobUrl = options.url = window.URL.createObjectURL(blob);
      }

      // download() throws on bad arguments, we can remove the extra
      // promise when bug 1250223 is fixed.
      return Promise.resolve().then(() => browser.downloads.download(options))
                    .then(id => {
                      browser.test.sendMessage("download.done", {status: "success", id});
                    })
                    .catch(error => {
                      browser.test.sendMessage("download.done", {status: "error", errmsg: error.message});
                    });
    } else if (msg == "killTheBlob") {
      window.URL.revokeObjectURL(blobUrl);
      blobUrl = null;
    }
  });

  browser.test.sendMessage("ready");
}

// This function is a bit of a sledgehammer, it looks at every download
// the browser knows about and waits for all active downloads to complete.
// But we only start one at a time and only do a handful in total, so
// this lets us test download() without depending on anything else.
function waitForDownloads() {
  return Downloads.getList(Downloads.ALL)
                  .then(list => list.getAll())
                  .then(downloads => {
                    let inprogress = downloads.filter(dl => !dl.stopped);
                    return Promise.all(inprogress.map(dl => dl.whenSucceeded()));
                  });
}

// Create a file in the downloads directory.
function touch(filename) {
  let file = downloadDir.clone();
  file.append(filename);
  file.create(Ci.nsIFile.NORMAL_FILE_TYPE, FileUtils.PERMS_FILE);
}

// Remove a file in the downloads directory.
function remove(filename) {
  let file = downloadDir.clone();
  file.append(filename);
  file.remove(false);
}

add_task(function* test_downloads() {
  setup();

  let extension = ExtensionTestUtils.loadExtension({
    background: `(${backgroundScript})()`,
    manifest: {
      permissions: ["downloads"],
    },
  });

  function download(options) {
    extension.sendMessage("download.request", options);
    return extension.awaitMessage("download.done");
  }

  function testDownload(options, localFile, expectedSize, description) {
    return download(options).then(msg => {
      equal(msg.status, "success", `downloads.download() works with ${description}`);
      return waitForDownloads();
    }).then(() => {
      let localPath = downloadDir.clone();
      localPath.append(localFile);
      equal(localPath.fileSize, expectedSize, "Downloaded file has expected size");
      localPath.remove(false);
    });
  }

  yield extension.startup();
  yield extension.awaitMessage("ready");
  do_print("extension started");

  // Call download() with just the url property.
  yield testDownload({url: FILE_URL}, FILE_NAME, FILE_LEN, "just source");

  // Call download() with a filename property.
  yield testDownload({
    url: FILE_URL,
    filename: "newpath.txt",
  }, "newpath.txt", FILE_LEN, "source and filename");

  // Check conflictAction of "uniquify".
  touch(FILE_NAME);
  yield testDownload({
    url: FILE_URL,
    conflictAction: "uniquify",
  }, FILE_NAME_UNIQUE, FILE_LEN, "conflictAction=uniquify");
  // todo check that preexisting file was not modified?
  remove(FILE_NAME);

  // Check conflictAction of "overwrite".
  touch(FILE_NAME);
  yield testDownload({
    url: FILE_URL,
    conflictAction: "overwrite",
  }, FILE_NAME, FILE_LEN, "conflictAction=overwrite");

  // Try to download in invalid url
  yield download({url: "this is not a valid URL"}).then(msg => {
    equal(msg.status, "error", "downloads.download() fails with invalid url");
    ok(/not a valid URL/.test(msg.errmsg), "error message for invalid url is correct");
  });

  // Try to download to an empty path.
  yield download({
    url: FILE_URL,
    filename: "",
  }).then(msg => {
    equal(msg.status, "error", "downloads.download() fails with empty filename");
    equal(msg.errmsg, "filename must not be empty", "error message for empty filename is correct");
  });

  // Try to download to an absolute path.
  const absolutePath = OS.Path.join(WINDOWS ? "\\tmp" : "/tmp", "file_download.txt");
  yield download({
    url: FILE_URL,
    filename: absolutePath,
  }).then(msg => {
    equal(msg.status, "error", "downloads.download() fails with absolute filename");
    equal(msg.errmsg, "filename must not be an absolute path", `error message for absolute path (${absolutePath}) is correct`);
  });

  if (WINDOWS) {
    yield download({
      url: FILE_URL,
      filename: "C:\\file_download.txt",
    }).then(msg => {
      equal(msg.status, "error", "downloads.download() fails with absolute filename");
      equal(msg.errmsg, "filename must not be an absolute path", "error message for absolute path with drive letter is correct");
    });
  }

  // Try to download to a relative path containing ..
  yield download({
    url: FILE_URL,
    filename: OS.Path.join("..", "file_download.txt"),
  }).then(msg => {
    equal(msg.status, "error", "downloads.download() fails with back-references");
    equal(msg.errmsg, "filename must not contain back-references (..)", "error message for back-references is correct");
  });

  // Try to download to a long relative path containing ..
  yield download({
    url: FILE_URL,
    filename: OS.Path.join("foo", "..", "..", "file_download.txt"),
  }).then(msg => {
    equal(msg.status, "error", "downloads.download() fails with back-references");
    equal(msg.errmsg, "filename must not contain back-references (..)", "error message for back-references is correct");
  });

  // Try to download a blob url
  const BLOB_STRING = "Hello, world";
  yield testDownload({
    blobme: [BLOB_STRING],
    filename: FILE_NAME,
  }, FILE_NAME, BLOB_STRING.length, "blob url");
  extension.sendMessage("killTheBlob");

  // Try to download a blob url without a given filename
  yield testDownload({
    blobme: [BLOB_STRING],
  }, "download", BLOB_STRING.length, "blob url with no filename");
  extension.sendMessage("killTheBlob");

  yield extension.unload();
});
