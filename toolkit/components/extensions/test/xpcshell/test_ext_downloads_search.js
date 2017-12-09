/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

Cu.import("resource://gre/modules/Downloads.jsm");

const server = createHttpServer();
server.registerDirectory("/data/", do_get_file("data"));

const BASE = `http://localhost:${server.identity.primaryPort}/data`;
const TXT_FILE = "file_download.txt";
const TXT_URL = BASE + "/" + TXT_FILE;
const TXT_LEN = 46;
const HTML_FILE = "file_download.html";
const HTML_URL = BASE + "/" + HTML_FILE;
const HTML_LEN = 117;
const BIG_LEN = 1000; // something bigger both TXT_LEN and HTML_LEN

function backgroundScript() {
  let complete = new Map();

  function waitForComplete(id) {
    if (complete.has(id)) {
      return complete.get(id).promise;
    }

    let promise = new Promise(resolve => {
      complete.set(id, {resolve});
    });
    complete.get(id).promise = promise;
    return promise;
  }

  browser.downloads.onChanged.addListener(change => {
    if (change.state && change.state.current == "complete") {
      // Make sure we have a promise.
      waitForComplete(change.id);
      complete.get(change.id).resolve();
    }
  });

  browser.test.onMessage.addListener(async (msg, ...args) => {
    if (msg == "download.request") {
      try {
        let id = await browser.downloads.download(args[0]);
        browser.test.sendMessage("download.done", {status: "success", id});
      } catch (error) {
        browser.test.sendMessage("download.done", {status: "error", errmsg: error.message});
      }
    } else if (msg == "search.request") {
      try {
        let downloads = await browser.downloads.search(args[0]);
        browser.test.sendMessage("search.done", {status: "success", downloads});
      } catch (error) {
        browser.test.sendMessage("search.done", {status: "error", errmsg: error.message});
      }
    } else if (msg == "waitForComplete.request") {
      await waitForComplete(args[0]);
      browser.test.sendMessage("waitForComplete.done");
    }
  });

  browser.test.sendMessage("ready");
}

async function clearDownloads(callback) {
  let list = await Downloads.getList(Downloads.ALL);
  let downloads = await list.getAll();

  await Promise.all(downloads.map(download => list.remove(download)));

  return downloads;
}

add_task(async function test_search() {
  const nsIFile = Ci.nsIFile;
  let downloadDir = FileUtils.getDir("TmpD", ["downloads"]);
  downloadDir.createUnique(nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);
  do_print(`downloadDir ${downloadDir.path}`);

  function downloadPath(filename) {
    let path = downloadDir.clone();
    path.append(filename);
    return path.path;
  }

  Services.prefs.setIntPref("browser.download.folderList", 2);
  Services.prefs.setComplexValue("browser.download.dir", nsIFile, downloadDir);

  do_register_cleanup(async () => {
    Services.prefs.clearUserPref("browser.download.folderList");
    Services.prefs.clearUserPref("browser.download.dir");
    await cleanupDir(downloadDir);
    await clearDownloads();
  });

  await clearDownloads().then(downloads => {
    do_print(`removed ${downloads.length} pre-existing downloads from history`);
  });

  let extension = ExtensionTestUtils.loadExtension({
    background: backgroundScript,
    manifest: {
      permissions: ["downloads"],
    },
  });

  async function download(options) {
    extension.sendMessage("download.request", options);
    let result = await extension.awaitMessage("download.done");

    if (result.status == "success") {
      do_print(`wait for onChanged event to indicate ${result.id} is complete`);
      extension.sendMessage("waitForComplete.request", result.id);

      await extension.awaitMessage("waitForComplete.done");
    }

    return result;
  }

  function search(query) {
    extension.sendMessage("search.request", query);
    return extension.awaitMessage("search.done");
  }

  await extension.startup();
  await extension.awaitMessage("ready");

  // Do some downloads...
  const time1 = new Date();

  let downloadIds = {};
  let msg = await download({url: TXT_URL});
  equal(msg.status, "success", "download() succeeded");
  downloadIds.txt1 = msg.id;

  const TXT_FILE2 = "NewFile.txt";
  msg = await download({url: TXT_URL, filename: TXT_FILE2});
  equal(msg.status, "success", "download() succeeded");
  downloadIds.txt2 = msg.id;

  const time2 = new Date();

  msg = await download({url: HTML_URL});
  equal(msg.status, "success", "download() succeeded");
  downloadIds.html1 = msg.id;

  const HTML_FILE2 = "renamed.html";
  msg = await download({url: HTML_URL, filename: HTML_FILE2});
  equal(msg.status, "success", "download() succeeded");
  downloadIds.html2 = msg.id;

  const time3 = new Date();

  // Search for each individual download and check
  // the corresponding DownloadItem.
  async function checkDownloadItem(id, expect) {
    let item = await search({id});
    equal(item.status, "success", "search() succeeded");
    equal(item.downloads.length, 1, "search() found exactly 1 download");

    Object.keys(expect).forEach(function(field) {
      equal(item.downloads[0][field], expect[field], `DownloadItem.${field} is correct"`);
    });
  }
  await checkDownloadItem(downloadIds.txt1, {
    url: TXT_URL,
    filename: downloadPath(TXT_FILE),
    mime: "text/plain",
    state: "complete",
    bytesReceived: TXT_LEN,
    totalBytes: TXT_LEN,
    fileSize: TXT_LEN,
    exists: true,
  });

  await checkDownloadItem(downloadIds.txt2, {
    url: TXT_URL,
    filename: downloadPath(TXT_FILE2),
    mime: "text/plain",
    state: "complete",
    bytesReceived: TXT_LEN,
    totalBytes: TXT_LEN,
    fileSize: TXT_LEN,
    exists: true,
  });

  await checkDownloadItem(downloadIds.html1, {
    url: HTML_URL,
    filename: downloadPath(HTML_FILE),
    mime: "text/html",
    state: "complete",
    bytesReceived: HTML_LEN,
    totalBytes: HTML_LEN,
    fileSize: HTML_LEN,
    exists: true,
  });

  await checkDownloadItem(downloadIds.html2, {
    url: HTML_URL,
    filename: downloadPath(HTML_FILE2),
    mime: "text/html",
    state: "complete",
    bytesReceived: HTML_LEN,
    totalBytes: HTML_LEN,
    fileSize: HTML_LEN,
    exists: true,
  });

  async function checkSearch(query, expected, description, exact) {
    let item = await search(query);
    equal(item.status, "success", "search() succeeded");
    equal(item.downloads.length, expected.length, `search() for ${description} found exactly ${expected.length} downloads`);

    let receivedIds = item.downloads.map(i => i.id);
    if (exact) {
      receivedIds.forEach((id, idx) => {
        equal(id, downloadIds[expected[idx]], `search() for ${description} returned ${expected[idx]} in position ${idx}`);
      });
    } else {
      Object.keys(downloadIds).forEach(key => {
        const id = downloadIds[key];
        const thisExpected = expected.includes(key);
        equal(receivedIds.includes(id), thisExpected,
              `search() for ${description} ${thisExpected ? "includes" : "does not include"} ${key}`);
      });
    }
  }

  // Check that search with an invalid id returns nothing.
  // NB: for now ids are not persistent and we start numbering them at 1
  //     so a sufficiently large number will be unused.
  const INVALID_ID = 1000;
  await checkSearch({id: INVALID_ID}, [], "invalid id");

  // Check that search on url works.
  await checkSearch({url: TXT_URL}, ["txt1", "txt2"], "url");

  // Check that regexp on url works.
  const HTML_REGEX = "[downlad]{8}\.html+$";
  await checkSearch({urlRegex: HTML_REGEX}, ["html1", "html2"], "url regexp");

  // Check that compatible url+regexp works
  await checkSearch({url: HTML_URL, urlRegex: HTML_REGEX}, ["html1", "html2"], "compatible url+urlRegex");

  // Check that incompatible url+regexp works
  await checkSearch({url: TXT_URL, urlRegex: HTML_REGEX}, [], "incompatible url+urlRegex");

  // Check that search on filename works.
  await checkSearch({filename: downloadPath(TXT_FILE)}, ["txt1"], "filename");

  // Check that regexp on filename works.
  await checkSearch({filenameRegex: HTML_REGEX}, ["html1"], "filename regex");

  // Check that compatible filename+regexp works
  await checkSearch({filename: downloadPath(HTML_FILE), filenameRegex: HTML_REGEX}, ["html1"], "compatible filename+filename regex");

  // Check that incompatible filename+regexp works
  await checkSearch({filename: downloadPath(TXT_FILE), filenameRegex: HTML_REGEX}, [], "incompatible filename+filename regex");

  // Check that simple positive search terms work.
  await checkSearch({query: ["file_download"]}, ["txt1", "txt2", "html1", "html2"],
                    "term file_download");
  await checkSearch({query: ["NewFile"]}, ["txt2"], "term NewFile");

  // Check that positive search terms work case-insensitive.
  await checkSearch({query: ["nEwfILe"]}, ["txt2"], "term nEwfiLe");

  // Check that negative search terms work.
  await checkSearch({query: ["-txt"]}, ["html1", "html2"], "term -txt");

  // Check that positive and negative search terms together work.
  await checkSearch({query: ["html", "-renamed"]}, ["html1"], "postive and negative terms");

  async function checkSearchWithDate(query, expected, description) {
    const fields = Object.keys(query);
    if (fields.length != 1 || !(query[fields[0]] instanceof Date)) {
      throw new Error("checkSearchWithDate expects exactly one Date field");
    }
    const field = fields[0];
    const date = query[field];

    let newquery = {};

    // Check as a Date
    newquery[field] = date;
    await checkSearch(newquery, expected, `${description} as Date`);

    // Check as numeric milliseconds
    newquery[field] = date.valueOf();
    await checkSearch(newquery, expected, `${description} as numeric ms`);

    // Check as stringified milliseconds
    newquery[field] = date.valueOf().toString();
    await checkSearch(newquery, expected, `${description} as string ms`);

    // Check as ISO string
    newquery[field] = date.toISOString();
    await checkSearch(newquery, expected, `${description} as iso string`);
  }

  // Check startedBefore
  await checkSearchWithDate({startedBefore: time1}, [], "before time1");
  await checkSearchWithDate({startedBefore: time2}, ["txt1", "txt2"], "before time2");
  await checkSearchWithDate({startedBefore: time3}, ["txt1", "txt2", "html1", "html2"], "before time3");

  // Check startedAfter
  await checkSearchWithDate({startedAfter: time1}, ["txt1", "txt2", "html1", "html2"], "after time1");
  await checkSearchWithDate({startedAfter: time2}, ["html1", "html2"], "after time2");
  await checkSearchWithDate({startedAfter: time3}, [], "after time3");

  // Check simple search on totalBytes
  await checkSearch({totalBytes: TXT_LEN}, ["txt1", "txt2"], "totalBytes");
  await checkSearch({totalBytes: HTML_LEN}, ["html1", "html2"], "totalBytes");

  // Check simple test on totalBytes{Greater,Less}
  // (NB: TXT_LEN < HTML_LEN < BIG_LEN)
  await checkSearch({totalBytesGreater: 0}, ["txt1", "txt2", "html1", "html2"], "totalBytesGreater than 0");
  await checkSearch({totalBytesGreater: TXT_LEN}, ["html1", "html2"], `totalBytesGreater than ${TXT_LEN}`);
  await checkSearch({totalBytesGreater: HTML_LEN}, [], `totalBytesGreater than ${HTML_LEN}`);
  await checkSearch({totalBytesLess: TXT_LEN}, [], `totalBytesLess than ${TXT_LEN}`);
  await checkSearch({totalBytesLess: HTML_LEN}, ["txt1", "txt2"], `totalBytesLess than ${HTML_LEN}`);
  await checkSearch({totalBytesLess: BIG_LEN}, ["txt1", "txt2", "html1", "html2"], `totalBytesLess than ${BIG_LEN}`);

  // Check good combinations of totalBytes*.
  await checkSearch({totalBytes: HTML_LEN, totalBytesGreater: TXT_LEN}, ["html1", "html2"], "totalBytes and totalBytesGreater");
  await checkSearch({totalBytes: TXT_LEN, totalBytesLess: HTML_LEN}, ["txt1", "txt2"], "totalBytes and totalBytesGreater");
  await checkSearch({totalBytes: HTML_LEN, totalBytesLess: BIG_LEN, totalBytesGreater: 0}, ["html1", "html2"], "totalBytes and totalBytesLess and totalBytesGreater");

  // Check bad combination of totalBytes*.
  await checkSearch({totalBytesLess: TXT_LEN, totalBytesGreater: HTML_LEN}, [], "bad totalBytesLess, totalBytesGreater combination");
  await checkSearch({totalBytes: TXT_LEN, totalBytesGreater: HTML_LEN}, [], "bad totalBytes, totalBytesGreater combination");
  await checkSearch({totalBytes: HTML_LEN, totalBytesLess: TXT_LEN}, [], "bad totalBytes, totalBytesLess combination");

  // Check mime.
  await checkSearch({mime: "text/plain"}, ["txt1", "txt2"], "mime text/plain");
  await checkSearch({mime: "text/html"}, ["html1", "html2"], "mime text/htmlplain");
  await checkSearch({mime: "video/webm"}, [], "mime video/webm");

  // Check fileSize.
  await checkSearch({fileSize: TXT_LEN}, ["txt1", "txt2"], "fileSize");
  await checkSearch({fileSize: HTML_LEN}, ["html1", "html2"], "fileSize");

  // Fields like bytesReceived, paused, state, exists are meaningful
  // for downloads that are in progress but have not yet completed.
  // todo: add tests for these when we have better support for in-progress
  // downloads (e.g., after pause(), resume() and cancel() are implemented)

  // Check multiple query properties.
  // We could make this testing arbitrarily complicated...
  // We already tested combining fields with obvious interactions above
  // (e.g., filename and filenameRegex or startTime and startedBefore/After)
  // so now just throw as many fields as we can at a single search and
  // make sure a simple case still works.
  await checkSearch({
    url: TXT_URL,
    urlRegex: "download",
    filename: downloadPath(TXT_FILE),
    filenameRegex: "download",
    query: ["download"],
    startedAfter: time1.valueOf().toString(),
    startedBefore: time2.valueOf().toString(),
    totalBytes: TXT_LEN,
    totalBytesGreater: 0,
    totalBytesLess: BIG_LEN,
    mime: "text/plain",
    fileSize: TXT_LEN,
  }, ["txt1"], "many properties");

  // Check simple orderBy (forward and backward).
  await checkSearch({orderBy: ["startTime"]}, ["txt1", "txt2", "html1", "html2"], "orderBy startTime", true);
  await checkSearch({orderBy: ["-startTime"]}, ["html2", "html1", "txt2", "txt1"], "orderBy -startTime", true);

  // Check orderBy with multiple fields.
  // NB: TXT_URL and HTML_URL differ only in extension and .html precedes .txt
  await checkSearch({orderBy: ["url", "-startTime"]}, ["html2", "html1", "txt2", "txt1"], "orderBy with multiple fields", true);

  // Check orderBy with limit.
  await checkSearch({orderBy: ["url"], limit: 1}, ["html1"], "orderBy with limit", true);

  // Check bad arguments.
  async function checkBadSearch(query, pattern, description) {
    let item = await search(query);
    equal(item.status, "error", "search() failed");
    ok(pattern.test(item.errmsg), `error message for ${description} was correct (${item.errmsg}).`);
  }

  await checkBadSearch("myquery", /Incorrect argument type/, "query is not an object");
  await checkBadSearch({bogus: "boo"}, /Unexpected property/, "query contains an unknown field");
  await checkBadSearch({query: "query string"}, /Expected array/, "query.query is a string");
  await checkBadSearch({startedBefore: "i am not a time"}, /Type error/, "query.startedBefore is not a valid time");
  await checkBadSearch({startedAfter: "i am not a time"}, /Type error/, "query.startedAfter is not a valid time");
  await checkBadSearch({endedBefore: "i am not a time"}, /Type error/, "query.endedBefore is not a valid time");
  await checkBadSearch({endedAfter: "i am not a time"}, /Type error/, "query.endedAfter is not a valid time");
  await checkBadSearch({urlRegex: "["}, /Invalid urlRegex/, "query.urlRegexp is not a valid regular expression");
  await checkBadSearch({filenameRegex: "["}, /Invalid filenameRegex/, "query.filenameRegexp is not a valid regular expression");
  await checkBadSearch({orderBy: "startTime"}, /Expected array/, "query.orderBy is not an array");
  await checkBadSearch({orderBy: ["bogus"]}, /Invalid orderBy field/, "query.orderBy references a non-existent field");

  await extension.unload();
});
