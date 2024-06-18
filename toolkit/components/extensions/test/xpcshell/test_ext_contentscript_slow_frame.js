"use strict";

// This test exercises various scenarios involving slow or never-loading frames,
// and confirms that the content script scheduling logic behaves as expected.
// In particular any scheduled execution should eventually settle, and not get
// stuck like: https://bugzilla.mozilla.org/show_bug.cgi?id=1900222#c6

// ExtensionContent.sys.mjs needs to know when it's running from xpcshell,
// to use the right timeout for content scripts executed at document_idle.
ExtensionTestUtils.mockAppInfo();

const server = createHttpServer({ hosts: ["example.com"] });
server.registerPathHandler("/empty", () => {});
server.registerPathHandler("/parser_inserted.html", (request, response) => {
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.write(`<iframe src="/slow_frame.html"></iframe>`);
});
server.registerPathHandler("/js_inserted.html", (request, response) => {
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.write(`<body><script>
    let f = document.createElement("iframe");
    f.src = "/slow_frame.html";
    document.body.append(f);
    // Reach into the frame's content window, in case it could potentially
    // affect the initialization of the JS execution context there. In the unit
    // tests, we want to verify that content scripts do NOT execute in initial
    // about:blank. This would trivially be true if the initial about:blank
    // document did not exist. For example, on Android the initial about:blank
    // document is not created until touched by JavaScript.
    f.contentWindow.dump("Hello world sync\\n");
    // Although not strictly necessary, do it again async. The 100ms timer here
    // is slower than the 500ms timer of slow_frame.html, so this should still
    // be touching the (initial) about:blank document.
    setTimeout(() => f.contentWindow.dump("Hello world async\\n"), 100);
  </script>`);
});
server.registerPathHandler("/slow_frame.html", (request, response) => {
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.setHeader("Cache-Control", "no-cache");
  response.processAsync();
  info("[server] received request for slow_frame.html");
  // Wait for a bit to make sure that any other pending content script tasks,
  // if any, have had a chance to execute.
  // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
  setTimeout(() => {
    info("[server] sending response for slow_frame.html");
    response.write("Response of slow_frame");
    response.finish();
  }, 500);
});
const responsesToFinish = [];
server.registerPathHandler("/neverloading.html", (request, response) => {
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.setHeader("Cache-Control", "no-cache");
  response.processAsync();
  // No response.finish() call - server never responds during test.
  responsesToFinish.push(response);
});
registerCleanupFunction(() => {
  // Need to finish eventually, otherwise the test won't quit.
  responsesToFinish.forEach(response => response.finish());
});

// Set up detector to detect content script injections in iframes.
async function setupScriptInjectionDetector(contentPage) {
  await contentPage.spawn([], () => {
    info(`setupScriptInjectionDetector: pid=${Services.appinfo.processID}`);
    const { ExtensionProcessScript } = ChromeUtils.importESModule(
      "resource://gre/modules/ExtensionProcessScript.sys.mjs"
    );
    if (ExtensionProcessScript._test_only_pendingInjections) {
      // Already run before. Just reset the state.
      ExtensionProcessScript._test_only_pendingInjections.length = 0;
      return;
    }
    const pendingInjections = [];

    let callCounter = 0;

    // Note: we overwrite the ExtensionProcessScript methods without a way to
    // undo that in this test.
    const { loadContentScript } = ExtensionProcessScript;
    ExtensionProcessScript.loadContentScript = (contentScript, window) => {
      // This logging is not strictly needed, but debugging test failures and
      // timeouts is near-impossible without this logging.
      const logPrefix = `loadContentScript #${++callCounter} pid=${
        Services.appinfo.processID
      }`;
      dump(
        `${logPrefix} START runAt=${contentScript.runAt} readyState=${window.document?.readyState} origin=${window.origin} URL=${window.document?.URL} frameHTML=${window.frameElement?.outerHTML} isInitialDocument=${window.document?.isInitialDocument}\n`
      );

      let promise = loadContentScript(contentScript, window);
      // In this test we are only interested in iframes. Test coverage for
      // top-level about:blank is already provided by:
      // toolkit/components/extensions/test/mochitest/test_ext_contentscript_about_blank.html
      if (window.parent !== window) {
        let url = window.document?.URL ?? "(no document???)";
        let runAt = contentScript.runAt;
        let injectionDescription = { returnStatus: "pending", runAt, url };
        pendingInjections.push(injectionDescription);
        promise.then(
          () => {
            injectionDescription.returnStatus = "resolved";
            dump(`${logPrefix} IFRAME RESOLVED\n`);
          },
          e => {
            injectionDescription.returnStatus = "rejected";
            dump(`${logPrefix} IFRAME REJECTED with: ${e}\n`);
          }
        );
      } else {
        promise.finally(() => {
          dump(`${logPrefix} TOP SETTLED\n`);
        });
      }
      return promise;
    };

    // To export the value later, we need a process-global object where we can
    // store the value.
    ExtensionProcessScript._test_only_pendingInjections = pendingInjections;
  });
}

async function getPendingScriptInjections(contentPage) {
  return contentPage.spawn([], () => {
    info(`getPendingScriptInjections: pid=${Services.appinfo.processID}`);
    const { ExtensionProcessScript } = ChromeUtils.importESModule(
      "resource://gre/modules/ExtensionProcessScript.sys.mjs"
    );
    // Defined by setupScriptInjectionDetector.
    return ExtensionProcessScript._test_only_pendingInjections.slice();
  });
}

function loadTestExtensionWithContentScripts() {
  return ExtensionTestUtils.loadExtension({
    manifest: {
      content_scripts: [
        {
          js: ["document_start.js"],
          run_at: "document_start",
          match_about_blank: true,
          all_frames: true,
          matches: ["*://example.com/*"],
        },
        {
          js: ["document_end.js"],
          run_at: "document_end",
          match_about_blank: true,
          all_frames: true,
          matches: ["*://example.com/*"],
        },
        {
          js: ["document_idle.js"],
          run_at: "document_idle",
          match_about_blank: true,
          all_frames: true,
          matches: ["*://example.com/*"],
        },
      ],
    },
    files: {
      "document_start.js": () => {
        if (top !== window) {
          browser.test.sendMessage("got_frame", "document_start", document.URL);
        }
      },
      "document_end.js": () => {
        if (top !== window) {
          browser.test.sendMessage("got_frame", "document_end", document.URL);
        }
      },
      "document_idle.js": () => {
        if (top !== window) {
          browser.test.sendMessage("got_frame", "document_idle", document.URL);
        } else {
          browser.test.sendMessage("contentscript_in_top");
        }
      },
    },
  });
}

// This test serves the following purposes:
// - Primarily, confirm that there are no lingering content script injection
//   attempts after loading a page and executing a content script. This is to
//   check that declarative content scripts are not affected by bugs like
//   https://bugzilla.mozilla.org/show_bug.cgi?id=1900222#c6
// - Checks the behavior of match_about_blank:true (or equivalently,
//   match_origin_as_fallback:true) when there is a slow-loading frame, because
//   its initial document is about:blank. The current behavior is to not run
//   content scripts there (reported as bug 1415539 and bug 1486036).
async function testLoadContentScripts(urlThatEmbedsSlowFrame) {
  let extension = loadTestExtensionWithContentScripts();
  const seenScripts = [];
  const donePromise = new Promise(resolve => {
    extension.onMessage("got_frame", (runAt, url) => {
      seenScripts.push({ runAt, url });
      if (runAt === "document_idle" && url.endsWith("/slow_frame.html")) {
        resolve();
      }
    });
  });

  await extension.startup();
  const URL_INITIAL = "http://example.com/empty";
  const URL_TOP = urlThatEmbedsSlowFrame;
  const URL_FRAME = "http://example.com/slow_frame.html";
  let contentPage = await ExtensionTestUtils.loadContentPage(URL_INITIAL);
  await extension.awaitMessage("contentscript_in_top");
  await setupScriptInjectionDetector(contentPage);

  info(`Navigating to ${URL_TOP} which embeds slow-loading ${URL_FRAME}`);
  await contentPage.loadURL(URL_TOP);
  await extension.awaitMessage("contentscript_in_top");

  info("Waiting for the last content script to execute...");
  await donePromise;

  Assert.deepEqual(
    seenScripts,
    [
      { runAt: "document_start", url: URL_FRAME },
      { runAt: "document_end", url: URL_FRAME },
      { runAt: "document_idle", url: URL_FRAME },
    ],
    "Expected content scripts in frames"
  );

  Assert.deepEqual(
    await getPendingScriptInjections(contentPage),
    [
      // NOTE: The absence of entries with "about:blank" shows that declared
      // content_scripts won't inject in an initial about:blank document, as
      // reported in bug 1415539, bug 1486036
      { returnStatus: "resolved", runAt: "document_start", url: URL_FRAME },
      { returnStatus: "resolved", runAt: "document_end", url: URL_FRAME },
      { returnStatus: "resolved", runAt: "document_idle", url: URL_FRAME },
    ],
    "There should not be any unexpected pending content script injections"
  );

  await contentPage.close();
  await extension.unload();
}

add_task(async function test_slow_frame_in_html() {
  await testLoadContentScripts("http://example.com/parser_inserted.html");
});

add_task(async function test_slow_frame_inserted_via_javascript() {
  await testLoadContentScripts("http://example.com/js_inserted.html");
});

// Verifies that the logic that executes scripts in existing pages upon
// extension startup (through ExtensionPolicyService::InjectContentScripts)
// won't result in never-resolving script injections, e.g. as seen in
// https://bugzilla.mozilla.org/show_bug.cgi?id=1900222#c6
add_task(async function test_frame_unload_before_execute() {
  const URL_INITIAL = "http://example.com/empty";
  let contentPage = await ExtensionTestUtils.loadContentPage(URL_INITIAL);
  await setupScriptInjectionDetector(contentPage);
  await contentPage.spawn([], () => {
    info(`Adding frame: pid=${Services.appinfo.processID}`);
    const { document } = this.content.wrappedJSObject;
    let f = document.createElement("iframe");
    f.id = "neverloading_frame";
    f.src = "/neverloading.html";
    document.body.append(f);

    // We need to dereference the content window, because otherwise the document
    // won't be created and the test gets stuck waiting for donePromise below,
    // on Android with --disable-fission (bug 1902709).
    void f.contentWindow;
  });

  const extension = loadTestExtensionWithContentScripts();
  const seenScripts = [];
  const donePromise = new Promise(resolve => {
    extension.onMessage("got_frame", (runAt, url) => {
      seenScripts.push({ runAt, url });
      if (runAt === "document_idle") {
        resolve();
      }
    });
  });
  await extension.startup();
  await extension.awaitMessage("contentscript_in_top");
  info("Detected content script attempt in top doc, waiting for frame");
  await donePromise;
  info("Detected content script execution in frame, going to remove the frame");
  await contentPage.spawn([], async () => {
    info(`Removing frame: pid=${Services.appinfo.processID}`);
    const { TestUtils } = ChromeUtils.importESModule(
      "resource://testing-common/TestUtils.sys.mjs"
    );
    let f = this.content.document.querySelector(`#neverloading_frame`);
    let innerWindowId = f.contentWindow.windowGlobalChild.innerWindowId;
    let frameNukedPromise = TestUtils.topicObserved(
      "inner-window-nuked",
      subject =>
        innerWindowId === subject.QueryInterface(Ci.nsISupportsPRUint64).data
    );
    f.remove();
    info("Removed frame, waiting to ensure that it was fully destroyed");
    await frameNukedPromise;
    info("Frame has been fully destroyed");
  });

  Assert.deepEqual(
    seenScripts,
    [
      // Note: this is unexpectedly inconsistent with content script execution
      // in the usual case, which is tested by testLoadContentScripts. See
      // the comment about "about:blank" and bug 1415539 + bug 1486036.
      { runAt: "document_start", url: "about:blank" },
      { runAt: "document_end", url: "about:blank" },
      { runAt: "document_idle", url: "about:blank" },
    ],
    "Accounted for all content script executions"
  );
  Assert.deepEqual(
    await getPendingScriptInjections(contentPage),
    [
      // Regression test for bug 1900222: returnStatus should not be "pending".
      { returnStatus: "resolved", runAt: "document_start", url: "about:blank" },
      { returnStatus: "resolved", runAt: "document_end", url: "about:blank" },
      { returnStatus: "resolved", runAt: "document_idle", url: "about:blank" },
      // NOTE: The appearance of initial about:blank above is inconsistent with
      // the usual injection behavior (see testLoadContentScripts, the mention
      // of "about:blank" and bug 1415539 + bug 1486036 ).
      //
      // We haven't loaded any other frame, so we should not have seen any
      // other content script execution attempt.
    ],
    "There should not be any pending content script injections"
  );
  await contentPage.close();
  await extension.unload();
});
