/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/* import-globals-from clipboard_helper.js */

"use strict";

clipboardTypes.forEach(function (type) {
  if (!clipboard.isClipboardTypeSupported(type)) {
    add_task(async function test_clipboard_requestGetData_not_support() {
      info(`Test getDataSnapshotSync request throwing on ${type}`);
      SimpleTest.doesThrow(
        () => clipboard.getDataSnapshotSync(["text/plain"], type),
        "Passing unsupported clipboard type should throw"
      );
    });
    return;
  }

  add_task(async function test_clipboard_getDataSnapshotSync_throw() {
    info(`Test getDataSnapshotSync request throwing on ${type}`);
    SimpleTest.doesThrow(
      () => clipboard.getDataSnapshotSync([], type),
      "Passing empty flavor list should throw"
    );
  });

  add_task(
    async function test_clipboard_getDataSnapshotSync_no_matched_flavor() {
      info(`Test getDataSnapshotSync have no matched flavor on ${type}`);
      cleanupAllClipboard();
      is(
        getClipboardData("text/plain", type),
        null,
        "ensure clipboard is empty"
      );

      writeRandomStringToClipboard("text/plain", type);
      let request = clipboard.getDataSnapshotSync(["text/html"], type);
      isDeeply(request.flavorList, [], "Check flavorList");
    }
  );

  add_task(async function test_empty_data() {
    info(`Test getDataSnapshotSync request with empty data on ${type}`);
    cleanupAllClipboard();
    is(getClipboardData("text/plain", type), null, "ensure clipboard is empty");

    let request = getClipboardDataSnapshotSync(type);
    isDeeply(request.flavorList, [], "Check flavorList");
    await asyncClipboardRequestGetData(request, "text/plain", true).catch(
      () => {}
    );
  });

  add_task(async function test_clipboard_getDataSnapshotSync_after_write() {
    info(`Test getDataSnapshotSync request after write on ${type}`);

    let str = writeRandomStringToClipboard("text/plain", type);
    let request = getClipboardDataSnapshotSync(type);
    isDeeply(request.flavorList, ["text/plain"], "Check flavorList");
    is(
      await asyncClipboardRequestGetData(request, "text/plain"),
      str,
      "Check data"
    );
    ok(request.valid, "request should still be valid");
    // Requesting a flavor that is not in the list should throw error.
    await asyncClipboardRequestGetData(request, "text/html", true).catch(
      () => {}
    );
    ok(request.valid, "request should still be valid");

    // Writing a new data should invalid existing get request.
    str = writeRandomStringToClipboard("text/plain", type);
    await asyncClipboardRequestGetData(request, "text/plain").then(
      () => {
        ok(false, "asyncClipboardRequestGetData should not success");
      },
      () => {
        ok(true, "asyncClipboardRequestGetData should reject");
      }
    );
    ok(!request.valid, "request should no longer be valid");

    info(`check clipboard data again`);
    request = getClipboardDataSnapshotSync(type);
    isDeeply(request.flavorList, ["text/plain"], "Check flavorList");
    is(
      await asyncClipboardRequestGetData(request, "text/plain"),
      str,
      "Check data"
    );

    cleanupAllClipboard();
  });

  add_task(async function test_clipboard_getDataSnapshotSync_after_empty() {
    info(`Test getDataSnapshotSync request after empty on ${type}`);

    let str = writeRandomStringToClipboard("text/plain", type);
    let request = getClipboardDataSnapshotSync(type);
    isDeeply(request.flavorList, ["text/plain"], "Check flavorList");
    is(
      await asyncClipboardRequestGetData(request, "text/plain"),
      str,
      "Check data"
    );
    ok(request.valid, "request should still be valid");

    // Empty clipboard data
    emptyClipboardData(type);
    is(getClipboardData("text/plain", type), null, "ensure clipboard is empty");

    await asyncClipboardRequestGetData(request, "text/plain").then(
      () => {
        ok(false, "asyncClipboardRequestGetData should not success");
      },
      () => {
        ok(true, "asyncClipboardRequestGetData should reject");
      }
    );
    ok(!request.valid, "request should no longer be valid");

    info(`check clipboard data again`);
    request = getClipboardDataSnapshotSync(type);
    isDeeply(request.flavorList, [], "Check flavorList");

    cleanupAllClipboard();
  });
});

add_task(async function test_clipboard_getDataSnapshotSync_html_data() {
  info(`Test getDataSnapshotSync request with html data`);

  const html_str = `<img src="https://example.com/oops">`;
  writeStringToClipboard(html_str, "text/html", clipboard.kGlobalClipboard);

  let request = getClipboardDataSnapshotSync(clipboard.kGlobalClipboard);
  isDeeply(request.flavorList, ["text/html"], "Check flavorList");
  is(
    await asyncClipboardRequestGetData(request, "text/html"),
    // On Windows, widget adds extra data into HTML clipboard.
    navigator.platform.includes("Win")
      ? `<html><body>\n<!--StartFragment-->${html_str}<!--EndFragment-->\n</body>\n</html>`
      : html_str,
    "Check data"
  );
  // Requesting a flavor that is not in the list should throw error.
  await asyncClipboardRequestGetData(request, "text/plain", true).catch(
    () => {}
  );
});
