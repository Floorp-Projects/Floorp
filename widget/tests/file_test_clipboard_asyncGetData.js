/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/* import-globals-from clipboard_helper.js */

"use strict";

clipboardTypes.forEach(function (type) {
  if (!clipboard.isClipboardTypeSupported(type)) {
    add_task(async function test_clipboard_asyncGetData_not_support() {
      info(`Test asyncGetData request throwing on ${type}`);
      SimpleTest.doesThrow(
        () => clipboard.asyncGetData(["text/plain"], type, {}),
        "Passing unsupported clipboard type should throw"
      );
    });
    return;
  }

  add_task(async function test_clipboard_asyncGetData_throw() {
    info(`Test asyncGetData request throwing on ${type}`);
    SimpleTest.doesThrow(
      () => clipboard.asyncGetData([], type, {}),
      "Passing empty flavor list should throw"
    );

    SimpleTest.doesThrow(
      () => clipboard.asyncGetData(["text/plain"], type, null),
      "Passing no callback should throw"
    );
  });

  add_task(async function test_clipboard_asyncGetData_no_matched_flavor() {
    info(`Test asyncGetData have no matched flavor on ${type}`);
    cleanupAllClipboard();
    is(getClipboardData("text/plain", type), null, "ensure clipboard is empty");

    writeRandomStringToClipboard("text/plain", type);
    let request = await new Promise(resolve => {
      clipboard.asyncGetData(
        ["text/html"],
        type,
        null,
        SpecialPowers.Services.scriptSecurityManager.getSystemPrincipal(),
        {
          QueryInterface: SpecialPowers.ChromeUtils.generateQI([
            "nsIAsyncClipboardGetCallback",
          ]),
          // nsIAsyncClipboardGetCallback
          onSuccess: SpecialPowers.wrapCallback(function (
            aAsyncGetClipboardData
          ) {
            resolve(aAsyncGetClipboardData);
          }),
        }
      );
    });
    isDeeply(request.flavorList, [], "Check flavorList");
  });

  add_task(async function test_empty_data() {
    info(`Test asyncGetData request with empty data on ${type}`);
    cleanupAllClipboard();
    is(getClipboardData("text/plain", type), null, "ensure clipboard is empty");

    let request = await asyncGetClipboardData(type);
    isDeeply(request.flavorList, [], "Check flavorList");
    await asyncClipboardRequestGetData(request, "text/plain", true).catch(
      () => {}
    );
  });

  add_task(async function test_clipboard_asyncGetData_after_write() {
    info(`Test asyncGetData request after write on ${type}`);

    let str = writeRandomStringToClipboard("text/plain", type);
    let request = await asyncGetClipboardData(type);
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
      e => {
        ok(true, "asyncClipboardRequestGetData should reject");
      }
    );
    ok(!request.valid, "request should no longer be valid");

    info(`check clipboard data again`);
    request = await asyncGetClipboardData(type);
    isDeeply(request.flavorList, ["text/plain"], "Check flavorList");
    is(
      await asyncClipboardRequestGetData(request, "text/plain"),
      str,
      "Check data"
    );

    cleanupAllClipboard();
  });

  add_task(async function test_clipboard_asyncGetData_after_empty() {
    info(`Test asyncGetData request after empty on ${type}`);

    let str = writeRandomStringToClipboard("text/plain", type);
    let request = await asyncGetClipboardData(type);
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
      e => {
        ok(true, "asyncClipboardRequestGetData should reject");
      }
    );
    ok(!request.valid, "request should no longer be valid");

    info(`check clipboard data again`);
    request = await asyncGetClipboardData(type);
    isDeeply(request.flavorList, [], "Check flavorList");

    cleanupAllClipboard();
  });
});

add_task(async function test_html_data() {
  info(`Test asyncGetData request with html data`);

  const html_str = `<img src="https://example.com/oops">`;
  writeStringToClipboard(html_str, "text/html", clipboard.kGlobalClipboard);

  let request = await asyncGetClipboardData(clipboard.kGlobalClipboard);
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
