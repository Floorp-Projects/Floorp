/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const { Downloads } = ChromeUtils.import(
  "resource://gre/modules/Downloads.jsm"
);

const { OS } = ChromeUtils.import("resource://gre/modules/osfile.jsm");

const { TestUtils } = ChromeUtils.import(
  "resource://testing-common/TestUtils.jsm"
);

const server = createHttpServer();
server.registerDirectory("/data/", do_get_file("data"));

const ROOT = `http://localhost:${server.identity.primaryPort}`;
const BASE = `${ROOT}/data`;
const TXT_FILE = "file_download.txt";
const TXT_URL = BASE + "/" + TXT_FILE;

// Keep these in sync with code in interruptible.sjs
const INT_PARTIAL_LEN = 15;
const INT_TOTAL_LEN = 31;

const TEST_DATA = "This is 31 bytes of sample data";
const TOTAL_LEN = TEST_DATA.length;
const PARTIAL_LEN = 15;

// A handler to let us systematically test pausing/resuming/canceling
// of downloads.  This target represents a small text file but a simple
// GET will stall after sending part of the data, to give the test code
// a chance to pause or do other operations on an in-progress download.
// A resumed download (ie, a GET with a Range: header) will allow the
// download to complete.
function handleRequest(request, response) {
  response.setHeader("Content-Type", "text/plain", false);

  if (request.hasHeader("Range")) {
    let start, end;
    let matches = request
      .getHeader("Range")
      .match(/^\s*bytes=(\d+)?-(\d+)?\s*$/);
    if (matches != null) {
      start = matches[1] ? parseInt(matches[1], 10) : 0;
      end = matches[2] ? parseInt(matches[2], 10) : TOTAL_LEN - 1;
    }

    if (end == undefined || end >= TOTAL_LEN) {
      response.setStatusLine(
        request.httpVersion,
        416,
        "Requested Range Not Satisfiable"
      );
      response.setHeader("Content-Range", `*/${TOTAL_LEN}`, false);
      response.finish();
      return;
    }

    response.setStatusLine(request.httpVersion, 206, "Partial Content");
    response.setHeader("Content-Range", `${start}-${end}/${TOTAL_LEN}`, false);
    response.write(TEST_DATA.slice(start, end + 1));
  } else if (request.queryString.includes("stream")) {
    response.processAsync();
    response.setHeader("Content-Length", "10000", false);
    response.write("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
    setInterval(() => {
      response.write("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
    }, 50);
  } else {
    response.processAsync();
    response.setHeader("Content-Length", `${TOTAL_LEN}`, false);
    response.write(TEST_DATA.slice(0, PARTIAL_LEN));
  }

  registerCleanupFunction(() => {
    try {
      response.finish();
    } catch (e) {
      // This will throw, but we don't care at this point.
    }
  });
}

server.registerPrefixHandler("/interruptible/", handleRequest);

let interruptibleCount = 0;
function getInterruptibleUrl(filename = "interruptible.html") {
  let n = interruptibleCount++;
  return `${ROOT}/interruptible/${filename}?count=${n}`;
}

function backgroundScript() {
  let events = new Set();
  let eventWaiter = null;

  browser.downloads.onCreated.addListener(data => {
    events.add({ type: "onCreated", data });
    if (eventWaiter) {
      eventWaiter();
    }
  });

  browser.downloads.onChanged.addListener(data => {
    events.add({ type: "onChanged", data });
    if (eventWaiter) {
      eventWaiter();
    }
  });

  browser.downloads.onErased.addListener(data => {
    events.add({ type: "onErased", data });
    if (eventWaiter) {
      eventWaiter();
    }
  });

  // Returns a promise that will resolve when the given list of expected
  // events have all been seen.  By default, succeeds only if the exact list
  // of expected events is seen in the given order.  options.exact can be
  // set to false to allow other events and options.inorder can be set to
  // false to allow the events to arrive in any order.
  function waitForEvents(expected, options = {}) {
    function compare(a, b) {
      if (typeof b == "object" && b != null) {
        if (typeof a != "object") {
          return false;
        }
        return Object.keys(b).every(fld => compare(a[fld], b[fld]));
      }
      return a == b;
    }

    const exact = "exact" in options ? options.exact : true;
    const inorder = "inorder" in options ? options.inorder : true;
    return new Promise((resolve, reject) => {
      function check() {
        function fail(msg) {
          browser.test.fail(msg);
          reject(new Error(msg));
        }
        if (events.size < expected.length) {
          return;
        }
        if (exact && expected.length < events.size) {
          fail(
            `Got ${events.size} events but only expected ${expected.length}`
          );
          return;
        }

        let remaining = new Set(events);
        if (inorder) {
          for (let event of events) {
            if (compare(event, expected[0])) {
              expected.shift();
              remaining.delete(event);
            }
          }
        } else {
          expected = expected.filter(val => {
            for (let remainingEvent of remaining) {
              if (compare(remainingEvent, val)) {
                remaining.delete(remainingEvent);
                return false;
              }
            }
            return true;
          });
        }

        // Events that did occur have been removed from expected so if
        // expected is empty, we're done.  If we didn't see all the
        // expected events and we're not looking for an exact match,
        // then we just may not have seen the event yet, so return without
        // failing and check() will be called again when a new event arrives.
        if (!expected.length) {
          events = remaining;
          eventWaiter = null;
          resolve();
        } else if (exact) {
          fail(
            `Mismatched event: expecting ${JSON.stringify(
              expected[0]
            )} but got ${JSON.stringify(Array.from(remaining)[0])}`
          );
        }
      }
      eventWaiter = check;
      check();
    });
  }

  browser.test.onMessage.addListener(async (msg, ...args) => {
    let match = msg.match(/(\w+).request$/);
    if (!match) {
      return;
    }

    let what = match[1];
    if (what == "waitForEvents") {
      try {
        await waitForEvents(...args);
        browser.test.sendMessage("waitForEvents.done", { status: "success" });
      } catch (error) {
        browser.test.sendMessage("waitForEvents.done", {
          status: "error",
          errmsg: error.message,
        });
      }
    } else if (what == "clearEvents") {
      events = new Set();
      browser.test.sendMessage("clearEvents.done", { status: "success" });
    } else {
      try {
        let result = await browser.downloads[what](...args);
        browser.test.sendMessage(`${what}.done`, { status: "success", result });
      } catch (error) {
        browser.test.sendMessage(`${what}.done`, {
          status: "error",
          errmsg: error.message,
        });
      }
    }
  });

  browser.test.sendMessage("ready");
}

let downloadDir;
let extension;

async function waitForCreatedPartFile(baseFilename = "interruptible.html") {
  const partFilePath = `${downloadDir.path}/${baseFilename}.part`;

  info(`Wait for ${partFilePath} to be created`);
  let lastError;
  await TestUtils.waitForCondition(
    async () =>
      OS.File.stat(partFilePath).then(
        () => true,
        err => {
          lastError = err;
          return false;
        }
      ),
    `Wait for the ${partFilePath} to exists before pausing the download`
  ).catch(err => {
    if (lastError) {
      throw lastError;
    }
    throw err;
  });
}

async function clearDownloads(callback) {
  let list = await Downloads.getList(Downloads.ALL);
  let downloads = await list.getAll();

  await Promise.all(downloads.map(download => list.remove(download)));

  return downloads;
}

function runInExtension(what, ...args) {
  extension.sendMessage(`${what}.request`, ...args);
  return extension.awaitMessage(`${what}.done`);
}

// This is pretty simplistic, it looks for a progress update for a
// download of the given url in which the total bytes are exactly equal
// to the given value.  Unless you know exactly how data will arrive from
// the server (eg see interruptible.sjs), it probably isn't very useful.
async function waitForProgress(url, testFn) {
  let list = await Downloads.getList(Downloads.ALL);

  return new Promise(resolve => {
    const view = {
      onDownloadChanged(download) {
        if (download.source.url == url && testFn(download.currentBytes)) {
          list.removeView(view);
          resolve(download.currentBytes);
        }
      },
    };
    list.addView(view);
  });
}

add_task(async function setup() {
  const nsIFile = Ci.nsIFile;
  downloadDir = FileUtils.getDir("TmpD", ["downloads"]);
  downloadDir.createUnique(nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);
  info(`downloadDir ${downloadDir.path}`);

  Services.prefs.setIntPref("browser.download.folderList", 2);
  Services.prefs.setComplexValue("browser.download.dir", nsIFile, downloadDir);

  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("browser.download.folderList");
    Services.prefs.clearUserPref("browser.download.dir");
    downloadDir.remove(true);

    return clearDownloads();
  });

  await clearDownloads().then(downloads => {
    info(`removed ${downloads.length} pre-existing downloads from history`);
  });

  extension = ExtensionTestUtils.loadExtension({
    background: backgroundScript,
    manifest: {
      permissions: ["downloads"],
    },
  });

  await extension.startup();
  await extension.awaitMessage("ready");
});

add_task(async function test_events() {
  let msg = await runInExtension("download", { url: TXT_URL });
  equal(msg.status, "success", "download() succeeded");
  const id = msg.result;

  msg = await runInExtension("waitForEvents", [
    { type: "onCreated", data: { id, url: TXT_URL } },
    {
      type: "onChanged",
      data: {
        id,
        state: {
          previous: "in_progress",
          current: "complete",
        },
      },
    },
  ]);
  equal(msg.status, "success", "got onCreated and onChanged events");
});

add_task(async function test_cancel() {
  let url = getInterruptibleUrl();
  info(url);
  let msg = await runInExtension("download", { url });
  equal(msg.status, "success", "download() succeeded");
  const id = msg.result;

  let progressPromise = waitForProgress(url, bytes => bytes == INT_PARTIAL_LEN);

  msg = await runInExtension("waitForEvents", [
    { type: "onCreated", data: { id } },
  ]);
  equal(msg.status, "success", "got created and changed events");

  await progressPromise;
  info(`download reached ${INT_PARTIAL_LEN} bytes`);

  msg = await runInExtension("cancel", id);
  equal(msg.status, "success", "cancel() succeeded");

  // TODO bug 1256243: This sequence of events is bogus
  msg = await runInExtension("waitForEvents", [
    {
      type: "onChanged",
      data: {
        state: {
          previous: "in_progress",
          current: "interrupted",
        },
        paused: {
          previous: false,
          current: true,
        },
      },
    },
    {
      type: "onChanged",
      data: {
        id,
        error: {
          previous: null,
          current: "USER_CANCELED",
        },
      },
    },
    {
      type: "onChanged",
      data: {
        id,
        paused: {
          previous: true,
          current: false,
        },
      },
    },
  ]);
  equal(
    msg.status,
    "success",
    "got onChanged events corresponding to cancel()"
  );

  msg = await runInExtension("search", { error: "USER_CANCELED" });
  equal(msg.status, "success", "search() succeeded");
  equal(msg.result.length, 1, "search() found 1 download");
  equal(msg.result[0].id, id, "download.id is correct");
  equal(msg.result[0].state, "interrupted", "download.state is correct");
  equal(msg.result[0].paused, false, "download.paused is correct");
  equal(
    msg.result[0].estimatedEndTime,
    null,
    "download.estimatedEndTime is correct"
  );
  equal(msg.result[0].canResume, false, "download.canResume is correct");
  equal(msg.result[0].error, "USER_CANCELED", "download.error is correct");
  equal(
    msg.result[0].totalBytes,
    INT_TOTAL_LEN,
    "download.totalBytes is correct"
  );
  equal(msg.result[0].exists, false, "download.exists is correct");

  msg = await runInExtension("pause", id);
  equal(msg.status, "error", "cannot pause a canceled download");

  msg = await runInExtension("resume", id);
  equal(msg.status, "error", "cannot resume a canceled download");
});

add_task(async function test_pauseresume() {
  const filename = "pauseresume.html";
  let url = getInterruptibleUrl(filename);
  let msg = await runInExtension("download", { url });
  equal(msg.status, "success", "download() succeeded");
  const id = msg.result;

  let progressPromise = waitForProgress(url, bytes => bytes == INT_PARTIAL_LEN);

  msg = await runInExtension("waitForEvents", [
    { type: "onCreated", data: { id } },
  ]);
  equal(msg.status, "success", "got created and changed events");

  await progressPromise;
  info(`download reached ${INT_PARTIAL_LEN} bytes`);

  // Prevent intermittent timeouts due to the part file not yet created
  // (e.g. see Bug 1573360).
  await waitForCreatedPartFile(filename);

  info("Pause the download item");
  msg = await runInExtension("pause", id);
  equal(msg.status, "success", "pause() succeeded");

  msg = await runInExtension("waitForEvents", [
    {
      type: "onChanged",
      data: {
        id,
        state: {
          previous: "in_progress",
          current: "interrupted",
        },
        paused: {
          previous: false,
          current: true,
        },
        canResume: {
          previous: false,
          current: true,
        },
      },
    },
    {
      type: "onChanged",
      data: {
        id,
        error: {
          previous: null,
          current: "USER_CANCELED",
        },
      },
    },
  ]);
  equal(msg.status, "success", "got onChanged event corresponding to pause");

  msg = await runInExtension("search", { paused: true });
  equal(msg.status, "success", "search() succeeded");
  equal(msg.result.length, 1, "search() found 1 download");
  equal(msg.result[0].id, id, "download.id is correct");
  equal(msg.result[0].state, "interrupted", "download.state is correct");
  equal(msg.result[0].paused, true, "download.paused is correct");
  equal(
    msg.result[0].estimatedEndTime,
    null,
    "download.estimatedEndTime is correct"
  );
  equal(msg.result[0].canResume, true, "download.canResume is correct");
  equal(msg.result[0].error, "USER_CANCELED", "download.error is correct");
  equal(
    msg.result[0].bytesReceived,
    INT_PARTIAL_LEN,
    "download.bytesReceived is correct"
  );
  equal(
    msg.result[0].totalBytes,
    INT_TOTAL_LEN,
    "download.totalBytes is correct"
  );
  equal(msg.result[0].exists, false, "download.exists is correct");

  msg = await runInExtension("search", { error: "USER_CANCELED" });
  equal(msg.status, "success", "search() succeeded");
  let found = msg.result.filter(item => item.id == id);
  equal(found.length, 1, "search() by error found the paused download");

  msg = await runInExtension("pause", id);
  equal(msg.status, "error", "cannot pause an already paused download");

  msg = await runInExtension("resume", id);
  equal(msg.status, "success", "resume() succeeded");

  msg = await runInExtension("waitForEvents", [
    {
      type: "onChanged",
      data: {
        id,
        state: {
          previous: "interrupted",
          current: "in_progress",
        },
        paused: {
          previous: true,
          current: false,
        },
        canResume: {
          previous: true,
          current: false,
        },
        error: {
          previous: "USER_CANCELED",
          current: null,
        },
      },
    },
    {
      type: "onChanged",
      data: {
        id,
        state: {
          previous: "in_progress",
          current: "complete",
        },
      },
    },
  ]);
  equal(msg.status, "success", "got onChanged events for resume and complete");

  msg = await runInExtension("search", { id });
  equal(msg.status, "success", "search() succeeded");
  equal(msg.result.length, 1, "search() found 1 download");
  equal(msg.result[0].state, "complete", "download.state is correct");
  equal(msg.result[0].paused, false, "download.paused is correct");
  equal(
    msg.result[0].estimatedEndTime,
    null,
    "download.estimatedEndTime is correct"
  );
  equal(msg.result[0].canResume, false, "download.canResume is correct");
  equal(msg.result[0].error, null, "download.error is correct");
  equal(
    msg.result[0].bytesReceived,
    INT_TOTAL_LEN,
    "download.bytesReceived is correct"
  );
  equal(
    msg.result[0].totalBytes,
    INT_TOTAL_LEN,
    "download.totalBytes is correct"
  );
  equal(msg.result[0].exists, true, "download.exists is correct");

  msg = await runInExtension("pause", id);
  equal(msg.status, "error", "cannot pause a completed download");

  msg = await runInExtension("resume", id);
  equal(msg.status, "error", "cannot resume a completed download");
});

add_task(async function test_pausecancel() {
  let url = getInterruptibleUrl();
  let msg = await runInExtension("download", { url });
  equal(msg.status, "success", "download() succeeded");
  const id = msg.result;

  let progressPromise = waitForProgress(url, bytes => bytes == INT_PARTIAL_LEN);

  msg = await runInExtension("waitForEvents", [
    { type: "onCreated", data: { id } },
  ]);
  equal(msg.status, "success", "got created and changed events");

  await progressPromise;
  info(`download reached ${INT_PARTIAL_LEN} bytes`);

  msg = await runInExtension("pause", id);
  equal(msg.status, "success", "pause() succeeded");

  msg = await runInExtension("waitForEvents", [
    {
      type: "onChanged",
      data: {
        id,
        state: {
          previous: "in_progress",
          current: "interrupted",
        },
        paused: {
          previous: false,
          current: true,
        },
        canResume: {
          previous: false,
          current: true,
        },
      },
    },
    {
      type: "onChanged",
      data: {
        id,
        error: {
          previous: null,
          current: "USER_CANCELED",
        },
      },
    },
  ]);
  equal(msg.status, "success", "got onChanged event corresponding to pause");

  msg = await runInExtension("search", { paused: true });
  equal(msg.status, "success", "search() succeeded");
  equal(msg.result.length, 1, "search() found 1 download");
  equal(msg.result[0].id, id, "download.id is correct");
  equal(msg.result[0].state, "interrupted", "download.state is correct");
  equal(msg.result[0].paused, true, "download.paused is correct");
  equal(
    msg.result[0].estimatedEndTime,
    null,
    "download.estimatedEndTime is correct"
  );
  equal(msg.result[0].canResume, true, "download.canResume is correct");
  equal(msg.result[0].error, "USER_CANCELED", "download.error is correct");
  equal(
    msg.result[0].bytesReceived,
    INT_PARTIAL_LEN,
    "download.bytesReceived is correct"
  );
  equal(
    msg.result[0].totalBytes,
    INT_TOTAL_LEN,
    "download.totalBytes is correct"
  );
  equal(msg.result[0].exists, false, "download.exists is correct");

  msg = await runInExtension("search", { error: "USER_CANCELED" });
  equal(msg.status, "success", "search() succeeded");
  let found = msg.result.filter(item => item.id == id);
  equal(found.length, 1, "search() by error found the paused download");

  msg = await runInExtension("cancel", id);
  equal(msg.status, "success", "cancel() succeeded");

  msg = await runInExtension("waitForEvents", [
    {
      type: "onChanged",
      data: {
        id,
        paused: {
          previous: true,
          current: false,
        },
        canResume: {
          previous: true,
          current: false,
        },
      },
    },
  ]);
  equal(msg.status, "success", "got onChanged event for cancel");

  msg = await runInExtension("search", { id });
  equal(msg.status, "success", "search() succeeded");
  equal(msg.result.length, 1, "search() found 1 download");
  equal(msg.result[0].state, "interrupted", "download.state is correct");
  equal(msg.result[0].paused, false, "download.paused is correct");
  equal(
    msg.result[0].estimatedEndTime,
    null,
    "download.estimatedEndTime is correct"
  );
  equal(msg.result[0].canResume, false, "download.canResume is correct");
  equal(msg.result[0].error, "USER_CANCELED", "download.error is correct");
  equal(
    msg.result[0].totalBytes,
    INT_TOTAL_LEN,
    "download.totalBytes is correct"
  );
  equal(msg.result[0].exists, false, "download.exists is correct");
});

add_task(async function test_pause_resume_cancel_badargs() {
  let BAD_ID = 1000;

  let msg = await runInExtension("pause", BAD_ID);
  equal(msg.status, "error", "pause() failed with a bad download id");
  ok(/Invalid download id/.test(msg.errmsg), "error message is descriptive");

  msg = await runInExtension("resume", BAD_ID);
  equal(msg.status, "error", "resume() failed with a bad download id");
  ok(/Invalid download id/.test(msg.errmsg), "error message is descriptive");

  msg = await runInExtension("cancel", BAD_ID);
  equal(msg.status, "error", "cancel() failed with a bad download id");
  ok(/Invalid download id/.test(msg.errmsg), "error message is descriptive");
});

add_task(async function test_file_removal() {
  let msg = await runInExtension("download", { url: TXT_URL });
  equal(msg.status, "success", "download() succeeded");
  const id = msg.result;

  msg = await runInExtension("waitForEvents", [
    { type: "onCreated", data: { id, url: TXT_URL } },
    {
      type: "onChanged",
      data: {
        id,
        state: {
          previous: "in_progress",
          current: "complete",
        },
      },
    },
  ]);

  equal(msg.status, "success", "got onCreated and onChanged events");

  msg = await runInExtension("removeFile", id);
  equal(msg.status, "success", "removeFile() succeeded");

  msg = await runInExtension("removeFile", id);
  equal(
    msg.status,
    "error",
    "removeFile() fails since the file was already removed."
  );
  ok(
    /file doesn't exist/.test(msg.errmsg),
    "removeFile() failed on removed file."
  );

  msg = await runInExtension("removeFile", 1000);
  ok(
    /Invalid download id/.test(msg.errmsg),
    "removeFile() failed due to non-existent id"
  );
});

add_task(async function test_removal_of_incomplete_download() {
  const filename = "remove-incomplete.html";
  let url = getInterruptibleUrl(filename);
  let msg = await runInExtension("download", { url });
  equal(msg.status, "success", "download() succeeded");
  const id = msg.result;

  let progressPromise = waitForProgress(url, bytes => bytes == INT_PARTIAL_LEN);

  msg = await runInExtension("waitForEvents", [
    { type: "onCreated", data: { id } },
  ]);
  equal(msg.status, "success", "got created and changed events");

  await progressPromise;
  info(`download reached ${INT_PARTIAL_LEN} bytes`);

  // Prevent intermittent timeouts due to the part file not yet created
  // (e.g. see Bug 1573360).
  await waitForCreatedPartFile(filename);

  msg = await runInExtension("pause", id);
  equal(msg.status, "success", "pause() succeeded");

  msg = await runInExtension("waitForEvents", [
    {
      type: "onChanged",
      data: {
        id,
        state: {
          previous: "in_progress",
          current: "interrupted",
        },
        paused: {
          previous: false,
          current: true,
        },
        canResume: {
          previous: false,
          current: true,
        },
      },
    },
    {
      type: "onChanged",
      data: {
        id,
        error: {
          previous: null,
          current: "USER_CANCELED",
        },
      },
    },
  ]);
  equal(msg.status, "success", "got onChanged event corresponding to pause");

  msg = await runInExtension("removeFile", id);
  equal(msg.status, "error", "removeFile() on paused download failed");

  ok(
    /Cannot remove incomplete download/.test(msg.errmsg),
    "removeFile() failed due to download being incomplete"
  );

  msg = await runInExtension("resume", id);
  equal(msg.status, "success", "resume() succeeded");

  msg = await runInExtension("waitForEvents", [
    {
      type: "onChanged",
      data: {
        id,
        state: {
          previous: "interrupted",
          current: "in_progress",
        },
        paused: {
          previous: true,
          current: false,
        },
        canResume: {
          previous: true,
          current: false,
        },
        error: {
          previous: "USER_CANCELED",
          current: null,
        },
      },
    },
    {
      type: "onChanged",
      data: {
        id,
        state: {
          previous: "in_progress",
          current: "complete",
        },
      },
    },
  ]);
  equal(msg.status, "success", "got onChanged events for resume and complete");

  msg = await runInExtension("removeFile", id);
  equal(
    msg.status,
    "success",
    "removeFile() succeeded following completion of resumed download."
  );
});

// Test erase().  We don't do elaborate testing of the query handling
// since it uses the exact same engine as search() which is tested
// more thoroughly in test_chrome_ext_downloads_search.html
add_task(async function test_erase() {
  await clearDownloads();

  await runInExtension("clearEvents");

  async function download() {
    let msg = await runInExtension("download", { url: TXT_URL });
    equal(msg.status, "success", "download succeeded");
    let id = msg.result;

    msg = await runInExtension(
      "waitForEvents",
      [
        {
          type: "onChanged",
          data: { id, state: { current: "complete" } },
        },
      ],
      { exact: false }
    );
    equal(msg.status, "success", "download finished");

    return id;
  }

  let ids = {};
  ids.dl1 = await download();
  ids.dl2 = await download();
  ids.dl3 = await download();

  let msg = await runInExtension("search", {});
  equal(msg.status, "success", "search succeeded");
  equal(msg.result.length, 3, "search found 3 downloads");

  msg = await runInExtension("clearEvents");

  msg = await runInExtension("erase", { id: ids.dl1 });
  equal(msg.status, "success", "erase by id succeeded");

  msg = await runInExtension("waitForEvents", [
    { type: "onErased", data: ids.dl1 },
  ]);
  equal(msg.status, "success", "received onErased event");

  msg = await runInExtension("search", {});
  equal(msg.status, "success", "search succeeded");
  equal(msg.result.length, 2, "search found 2 downloads");

  msg = await runInExtension("erase", {});
  equal(msg.status, "success", "erase everything succeeded");

  msg = await runInExtension(
    "waitForEvents",
    [
      { type: "onErased", data: ids.dl2 },
      { type: "onErased", data: ids.dl3 },
    ],
    { inorder: false }
  );
  equal(msg.status, "success", "received 2 onErased events");

  msg = await runInExtension("search", {});
  equal(msg.status, "success", "search succeeded");
  equal(msg.result.length, 0, "search found 0 downloads");
});

function loadImage(img, data) {
  return new Promise(resolve => {
    img.src = data;
    img.onload = resolve;
  });
}

add_task(async function test_getFileIcon() {
  let webNav = Services.appShell.createWindowlessBrowser(false);
  let docShell = webNav.docShell;

  let system = Services.scriptSecurityManager.getSystemPrincipal();
  docShell.createAboutBlankContentViewer(system, system);

  let img = webNav.document.createElement("img");

  let msg = await runInExtension("download", { url: TXT_URL });
  equal(msg.status, "success", "download() succeeded");
  const id = msg.result;

  msg = await runInExtension("getFileIcon", id);
  equal(msg.status, "success", "getFileIcon() succeeded");
  await loadImage(img, msg.result);
  equal(img.height, 32, "returns an icon with the right height");
  equal(img.width, 32, "returns an icon with the right width");

  msg = await runInExtension("waitForEvents", [
    { type: "onCreated", data: { id, url: TXT_URL } },
    { type: "onChanged" },
  ]);
  equal(msg.status, "success", "got events");

  msg = await runInExtension("getFileIcon", id);
  equal(msg.status, "success", "getFileIcon() succeeded");
  await loadImage(img, msg.result);
  equal(img.height, 32, "returns an icon with the right height after download");
  equal(img.width, 32, "returns an icon with the right width after download");

  msg = await runInExtension("getFileIcon", id + 100);
  equal(msg.status, "error", "getFileIcon() failed");
  ok(msg.errmsg.includes("Invalid download id"), "download id is invalid");

  msg = await runInExtension("getFileIcon", id, { size: 127 });
  equal(msg.status, "success", "getFileIcon() succeeded");
  await loadImage(img, msg.result);
  equal(img.height, 127, "returns an icon with the right custom height");
  equal(img.width, 127, "returns an icon with the right custom width");

  msg = await runInExtension("getFileIcon", id, { size: 1 });
  equal(msg.status, "success", "getFileIcon() succeeded");
  await loadImage(img, msg.result);
  equal(img.height, 1, "returns an icon with the right custom height");
  equal(img.width, 1, "returns an icon with the right custom width");

  msg = await runInExtension("getFileIcon", id, { size: "foo" });
  equal(msg.status, "error", "getFileIcon() fails");
  ok(msg.errmsg.includes("Error processing size"), "size is not a number");

  msg = await runInExtension("getFileIcon", id, { size: 0 });
  equal(msg.status, "error", "getFileIcon() fails");
  ok(msg.errmsg.includes("Error processing size"), "size is too small");

  msg = await runInExtension("getFileIcon", id, { size: 128 });
  equal(msg.status, "error", "getFileIcon() fails");
  ok(msg.errmsg.includes("Error processing size"), "size is too big");

  webNav.close();
});

add_task(async function test_estimatedendtime() {
  // Note we are not testing the actual value calculation of estimatedEndTime,
  // only whether it is null/non-null at the appropriate times.

  let url = `${getInterruptibleUrl()}&stream=1`;
  let msg = await runInExtension("download", { url });
  equal(msg.status, "success", "download() succeeded");
  const id = msg.result;

  let previousBytes = await waitForProgress(url, bytes => bytes > 0);
  await waitForProgress(url, bytes => bytes > previousBytes);

  msg = await runInExtension("search", { id });
  equal(msg.status, "success", "search() succeeded");
  equal(msg.result.length, 1, "search() found 1 download");
  ok(msg.result[0].estimatedEndTime, "download.estimatedEndTime is correct");
  ok(msg.result[0].bytesReceived > 0, "download.bytesReceived is correct");

  msg = await runInExtension("cancel", id);

  msg = await runInExtension("search", { id });
  equal(msg.status, "success", "search() succeeded");
  equal(msg.result.length, 1, "search() found 1 download");
  ok(!msg.result[0].estimatedEndTime, "download.estimatedEndTime is correct");
});

add_task(async function test_byExtension() {
  let msg = await runInExtension("download", { url: TXT_URL });
  equal(msg.status, "success", "download() succeeded");
  const id = msg.result;
  msg = await runInExtension("search", { id });

  equal(msg.result.length, 1, "search() found 1 download");
  equal(
    msg.result[0].byExtensionName,
    "Generated extension",
    "download.byExtensionName is correct"
  );
  equal(
    msg.result[0].byExtensionId,
    extension.id,
    "download.byExtensionId is correct"
  );
});

add_task(async function cleanup() {
  await extension.unload();
});
