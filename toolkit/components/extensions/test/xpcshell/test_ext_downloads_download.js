/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

Cu.import("resource://gre/modules/osfile.jsm");
Cu.import("resource://gre/modules/Downloads.jsm");

const gServer = createHttpServer();
gServer.registerDirectory("/data/", do_get_file("data"));

gServer.registerPathHandler("/dir/", (_, res) => res.write("length=8"));

const WINDOWS = AppConstants.platform == "win";

const BASE = `http://localhost:${gServer.identity.primaryPort}/`;
const FILE_NAME = "file_download.txt";
const FILE_URL = BASE + "data/" + FILE_NAME;
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
  browser.test.onMessage.addListener(async (msg, ...args) => {
    if (msg == "download.request") {
      let options = args[0];

      if (options.blobme) {
        let blob = new Blob(options.blobme);
        delete options.blobme;
        blobUrl = options.url = window.URL.createObjectURL(blob);
      }

      try {
        let id = await browser.downloads.download(options);
        browser.test.sendMessage("download.done", {status: "success", id});
      } catch (error) {
        browser.test.sendMessage("download.done", {status: "error", errmsg: error.message});
      }
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
async function waitForDownloads() {
  let list = await Downloads.getList(Downloads.ALL);
  let downloads = await list.getAll();

  let inprogress = downloads.filter(dl => !dl.stopped);
  return Promise.all(inprogress.map(dl => dl.whenSucceeded()));
}

// Create a file in the downloads directory.
function touch(filename) {
  let file = downloadDir.clone();
  file.append(filename);
  file.create(Ci.nsIFile.NORMAL_FILE_TYPE, FileUtils.PERMS_FILE);
}

// Remove a file in the downloads directory.
function remove(filename, recursive = false) {
  let file = downloadDir.clone();
  file.append(filename);
  file.remove(recursive);
}

add_task(async function test_downloads() {
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

  async function testDownload(options, localFile, expectedSize, description) {
    let msg = await download(options);
    equal(msg.status, "success", `downloads.download() works with ${description}`);

    await waitForDownloads();

    let localPath = downloadDir.clone();
    let parts = Array.isArray(localFile) ? localFile : [localFile];

    parts.map(p => localPath.append(p));
    equal(localPath.fileSize, expectedSize, "Downloaded file has expected size");
    localPath.remove(false);
  }

  await extension.startup();
  await extension.awaitMessage("ready");
  do_print("extension started");

  // Call download() with just the url property.
  await testDownload({url: FILE_URL}, FILE_NAME, FILE_LEN, "just source");

  // Call download() with a filename property.
  await testDownload({
    url: FILE_URL,
    filename: "newpath.txt",
  }, "newpath.txt", FILE_LEN, "source and filename");

  // Call download() with a filename with subdirs.
  await testDownload({
    url: FILE_URL,
    filename: "sub/dir/file",
  }, ["sub", "dir", "file"], FILE_LEN, "source and filename with subdirs");

  // Call download() with a filename with existing subdirs.
  await testDownload({
    url: FILE_URL,
    filename: "sub/dir/file2",
  }, ["sub", "dir", "file2"], FILE_LEN, "source and filename with existing subdirs");

  // Only run Windows path separator test on Windows.
  if (WINDOWS) {
    // Call download() with a filename with Windows path separator.
    await testDownload({
      url: FILE_URL,
      filename: "sub\\dir\\file3",
    }, ["sub", "dir", "file3"], FILE_LEN, "filename with Windows path separator");
  }
  remove("sub", true);

  // Call download(), filename with subdir, skipping parts.
  await testDownload({
    url: FILE_URL,
    filename: "skip//part",
  }, ["skip", "part"], FILE_LEN, "source, filename, with subdir, skipping parts");
  remove("skip", true);

  // Check conflictAction of "uniquify".
  touch(FILE_NAME);
  await testDownload({
    url: FILE_URL,
    conflictAction: "uniquify",
  }, FILE_NAME_UNIQUE, FILE_LEN, "conflictAction=uniquify");
  // todo check that preexisting file was not modified?
  remove(FILE_NAME);

  // Check conflictAction of "overwrite".
  touch(FILE_NAME);
  await testDownload({
    url: FILE_URL,
    conflictAction: "overwrite",
  }, FILE_NAME, FILE_LEN, "conflictAction=overwrite");

  // Try to download in invalid url
  await download({url: "this is not a valid URL"}).then(msg => {
    equal(msg.status, "error", "downloads.download() fails with invalid url");
    ok(/not a valid URL/.test(msg.errmsg), "error message for invalid url is correct");
  });

  // Try to download to an empty path.
  await download({
    url: FILE_URL,
    filename: "",
  }).then(msg => {
    equal(msg.status, "error", "downloads.download() fails with empty filename");
    equal(msg.errmsg, "filename must not be empty", "error message for empty filename is correct");
  });

  // Try to download to an absolute path.
  const absolutePath = OS.Path.join(WINDOWS ? "\\tmp" : "/tmp", "file_download.txt");
  await download({
    url: FILE_URL,
    filename: absolutePath,
  }).then(msg => {
    equal(msg.status, "error", "downloads.download() fails with absolute filename");
    equal(msg.errmsg, "filename must not be an absolute path", `error message for absolute path (${absolutePath}) is correct`);
  });

  if (WINDOWS) {
    await download({
      url: FILE_URL,
      filename: "C:\\file_download.txt",
    }).then(msg => {
      equal(msg.status, "error", "downloads.download() fails with absolute filename");
      equal(msg.errmsg, "filename must not be an absolute path", "error message for absolute path with drive letter is correct");
    });
  }

  // Try to download to a relative path containing ..
  await download({
    url: FILE_URL,
    filename: OS.Path.join("..", "file_download.txt"),
  }).then(msg => {
    equal(msg.status, "error", "downloads.download() fails with back-references");
    equal(msg.errmsg, "filename must not contain back-references (..)", "error message for back-references is correct");
  });

  // Try to download to a long relative path containing ..
  await download({
    url: FILE_URL,
    filename: OS.Path.join("foo", "..", "..", "file_download.txt"),
  }).then(msg => {
    equal(msg.status, "error", "downloads.download() fails with back-references");
    equal(msg.errmsg, "filename must not contain back-references (..)", "error message for back-references is correct");
  });

  // Test illegal characters on Windows.
  if (WINDOWS) {
    await download({
      url: FILE_URL,
      filename: "like:this",
    }).then(msg => {
      equal(msg.status, "error", "downloads.download() fails with illegal chars");
      equal(msg.errmsg, "filename must not contain illegal characters", "error message correct");
    });
  }

  // Try to download a blob url
  const BLOB_STRING = "Hello, world";
  await testDownload({
    blobme: [BLOB_STRING],
    filename: FILE_NAME,
  }, FILE_NAME, BLOB_STRING.length, "blob url");
  extension.sendMessage("killTheBlob");

  // Try to download a blob url without a given filename
  await testDownload({
    blobme: [BLOB_STRING],
  }, "download", BLOB_STRING.length, "blob url with no filename");
  extension.sendMessage("killTheBlob");

  // Download a normal URL with an empty filename part.
  await testDownload({
    url: BASE + "dir/",
  }, "download", 8, "normal url with empty filename");

  // Check that the "incognito" property is supported.
  await testDownload({
    url: FILE_URL,
    incognito: false,
  }, FILE_NAME, FILE_LEN, "incognito=false");

  await testDownload({
    url: FILE_URL,
    incognito: true,
  }, FILE_NAME, FILE_LEN, "incognito=true");

  await extension.unload();
});

add_task(async function test_download_post() {
  const server = createHttpServer();
  const url = `http://localhost:${server.identity.primaryPort}/post-log`;

  let received;
  server.registerPathHandler("/post-log", request => {
    received = request;
  });

  // Confirm received vs. expected values.
  function confirm(method, headers = {}, body) {
    equal(received.method, method, "method is correct");

    for (let name in headers) {
      ok(received.hasHeader(name), `header ${name} received`);
      equal(received.getHeader(name), headers[name], `header ${name} is correct`);
    }

    if (body) {
      const str = NetUtil.readInputStreamToString(
        received.bodyInputStream,
        received.bodyInputStream.available());
      equal(str, body, "body is correct");
    }
  }

  function background() {
    browser.test.onMessage.addListener(async options => {
      try {
        await browser.downloads.download(options);
      } catch (err) {
        browser.test.sendMessage("done", {err: err.message});
      }
    });
    browser.downloads.onChanged.addListener(({state}) => {
      if (state && state.current === "complete") {
        browser.test.sendMessage("done", {ok: true});
      }
    });
  }

  const manifest = {permissions: ["downloads"]};
  const extension = ExtensionTestUtils.loadExtension({background, manifest});
  await extension.startup();

  function download(options) {
    options.url = url;
    options.conflictAction = "overwrite";

    extension.sendMessage(options);
    return extension.awaitMessage("done");
  }

  // Test method option.
  let result = await download({});
  ok(result.ok, "download works without the method option, defaults to GET");
  confirm("GET");

  result = await download({method: "PUT"});
  ok(!result.ok, "download rejected with PUT method");
  ok(/method: Invalid enumeration/.test(result.err), "descriptive error message");

  result = await download({method: "POST"});
  ok(result.ok, "download works with POST method");
  confirm("POST");

  // Test body option values.
  result = await download({body: []});
  ok(!result.ok, "download rejected because of non-string body");
  ok(/body: Expected string/.test(result.err), "descriptive error message");

  result = await download({method: "POST", body: "of work"});
  ok(result.ok, "download works with POST method and body");
  confirm("POST", {"Content-Length": 7}, "of work");

  // Test custom headers.
  result = await download({headers: [{name: "X-Custom"}]});
  ok(!result.ok, "download rejected because of missing header value");
  ok(/"value" is required/.test(result.err), "descriptive error message");

  result = await download({headers: [{name: "X-Custom", value: "13"}]});
  ok(result.ok, "download works with a custom header");
  confirm("GET", {"X-Custom": "13"});

  // Test forbidden headers.
  result = await download({headers: [{name: "DNT", value: "1"}]});
  ok(!result.ok, "download rejected because of forbidden header name DNT");
  ok(/Forbidden request header/.test(result.err), "descriptive error message");

  result = await download({headers: [{name: "Proxy-Connection", value: "keep"}]});
  ok(!result.ok, "download rejected because of forbidden header name prefix Proxy-");
  ok(/Forbidden request header/.test(result.err), "descriptive error message");

  result = await download({headers: [{name: "Sec-ret", value: "13"}]});
  ok(!result.ok, "download rejected because of forbidden header name prefix Sec-");
  ok(/Forbidden request header/.test(result.err), "descriptive error message");

  remove("post-log");
  await extension.unload();
});
