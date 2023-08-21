"use strict";

const { Management } = ChromeUtils.importESModule(
  "resource://gre/modules/Extension.sys.mjs"
);

const server = AddonTestUtils.createHttpServer({
  hosts: ["example.com"],
});

// One test below relies on a slow-loading stylesheet. This function and promise
// enables the script to control exactly when the stylesheet load should finish.
let allowStylesheetToLoad;
let stylesheetBlockerPromise = new Promise(resolve => {
  allowStylesheetToLoad = resolve;
});
server.registerPathHandler("/slow.css", (request, response) => {
  response.setHeader("Cache-Control", "no-cache", false);
  response.setHeader("Content-Type", "text/css", false);

  response.processAsync();

  stylesheetBlockerPromise.then(() => {
    response.write("body { color: rgb(1, 2, 3); }");
    response.finish();
  });
});

// Test helper to keep track of the number of background context loads, from
// any extension.
class BackgroundWatcher {
  constructor() {
    // Number of background page loads observed.
    this.bgBrowserCount = 0;
    // Number of top-level background context loads observed.
    this.bgViewCount = 0;
    this.observing = false;

    this.onBrowserInserted = this.onBrowserInserted.bind(this);
    this.onBackgroundViewLoaded = this.onBackgroundViewLoaded.bind(this);

    this.startObserving();
  }

  startObserving() {
    this.observing = true;
    Management.on("extension-browser-inserted", this.onBrowserInserted);
  }

  stopObserving() {
    this.observing = false;
    Management.off("extension-browser-inserted", this.onBrowserInserted);
    // Note: onBrowserInserted adds message listeners to the message manager
    // of background contexts, but we do not explicitly unregister these here
    // because the caller should only stop observing after knowing for sure
    // that there are no new or pending loads of background pages.
  }

  onBrowserInserted(eventName, browser) {
    Assert.equal(eventName, "extension-browser-inserted", "Seen bg browser");
    if (!browser.getAttribute("webextension-view-type") === "background") {
      return;
    }
    this.bgBrowserCount++;
    browser.messageManager.addMessageListener(
      "Extension:BackgroundViewLoaded",
      this.onBackgroundViewLoaded
    );
  }

  onBackgroundViewLoaded({ data }) {
    if (!this.observing) {
      // We shouldn't receive this event - see comment in stopObserving.
      Assert.ok(false, "Got onBackgroundViewLoaded while !observing");
    }
    this.bgViewCount++;
    Assert.ok(data.childId, "childId passed to Extension:BackgroundViewLoaded");
  }
}

add_task(async function test_first_extension_api_call_in_iframe() {
  // In this test we test what happens when an extension API call happens in
  // an iframe before the top-level document observes DOMContentLoaded.
  //
  // 1. Because DOMContentLoaded is blocked on the execution on <script defer>,
  //    and <script defer> is blocked on stylesheet load completion, we embed
  //    both elements in the top-level document to postpone DOMContentLoaded.
  // 2. We load an iframe with a moz-extension document and call an extension
  //    API in the context of that frame at iframe.onload.
  //    Because there is no <script> in the iframe, iframe.onload dispatches
  //    before DOMContentLoaded of the top-level document.
  // 3. We add several sanity checks to verify that the order of execution is
  //    as expected.
  //
  function backgroundScript() {
    // Note: no extension API calls until window.onload to avoid side effects.
    // Saving any relevant state in global variables to assert later.
    const readyStateAtTopLevelScriptExecution = document.readyState;
    globalThis.readyStateAtFrameLoad = "(to be set in iframe.onload)";
    globalThis.readyStateInScriptDefer = "(to be set in deferred script)";
    globalThis.styleSheetStateAtFrameLoad = "(to be set in iframe.onload)";
    globalThis.styleSheetStateInScriptDefer = "(to be set in deferred script)";
    globalThis.bodyAtFrameLoad = "(to be set in iframe.onload)";
    globalThis.scriptRunInFrame = "(to be set in iframe.onload)";
    globalThis.scriptDeferRunAfterFrameLoad = "(to be set in deferred script)";

    // We use a slow-loading stylesheet to block `<script defer>` execution,
    // which in turn postpones DOMContentLoaded. This method checks whether
    // the stylesheet has been loaded.
    globalThis.getStyleSheetState = () => {
      if (getComputedStyle(document.body).color === "rgb(1, 2, 3)") {
        return "slow.css loaded";
      }
      return "slow.css not loaded";
    };

    const handle_iframe_onload = event => {
      const iframe = event.target;
      // Note: using dump instead of browser.test.log because we want to avoid
      // extension API calls until we've completed the extension API call in
      // the iframe below.
      dump(`iframe.onload triggered\n`);
      globalThis.styleSheetStateAtFrameLoad = globalThis.getStyleSheetState();
      globalThis.readyStateAtFrameLoad = document.readyState;
      globalThis.bodyAtFrameLoad = iframe.contentDocument.body.textContent;
      // Now, call the extension API in the context of the iframe. We cannot
      // use <script> inside the iframe, because its execution is also
      // postponed by the slow style trick that we use to postpone
      // DOMContentLoaded.
      // The exact API call does not matter, as any extension API call will
      // ensure that ProxyContextParent is initialized if it was not before:
      // https://searchfox.org/mozilla-central/rev/892475f3ba2b959aeaef19d1d8602494e3f2ae32/toolkit/components/extensions/ExtensionPageChild.sys.mjs#221,223,227-228
      iframe.contentWindow.browser.runtime.getPlatformInfo().then(info => {
        dump(`Extension API call made a roundtrip through the parent\n`);
        globalThis.scriptRunInFrame = true;
        // By now, we have run an extension API call in the iframe, so we
        // don't need to be careful with avoiding extension API calls in the
        // top-level context. Thus we can now use browser.test APIs here.
        browser.test.assertTrue("os" in info, "extension API called in iframe");

        browser.test.assertTrue(
          iframe.contentWindow.browser.extension.getBackgroundPage() === window,
          "extension.getBackgroundPage() returns the top context"
        );

        // Allow stylesheet load to complete, which unblocks DOMContentLoaded
        // and window.onload.
        browser.test.sendMessage("allowStylesheetToLoad");
      });
    };

    // background.js runs before <iframe> is inserted. This capturing listener
    // should detect iframe.onload once it fires, through event bubbling.
    document.addEventListener(
      "load",
      function listener(event) {
        if (event.target.id === "iframe") {
          document.removeEventListener("load", listener, true);
          handle_iframe_onload(event);
        }
      },
      true
    );

    window.onload = () => {
      // First, several sanity checks to verify that the timing of script
      // execution in the iframe vs DOMContentLoaded is as expected.
      browser.test.assertEq(
        "loading",
        readyStateAtTopLevelScriptExecution,
        "Top-level script should run immediately while DOM is still loading"
      );
      function assertBeforeDOMContentLoaded(actualReadyState, message) {
        browser.test.assertTrue(
          actualReadyState === "interactive" || actualReadyState === "loading",
          `${message}, actual readyState=${actualReadyState}`
        );
      }
      assertBeforeDOMContentLoaded(
        globalThis.readyStateAtFrameLoad,
        "frame.onload + script should run before DOMContentLoaded fires"
      );
      assertBeforeDOMContentLoaded(
        globalThis.readyStateInScriptDefer,
        "<script defer> should run right before DOMContentLoaded fires"
      );
      browser.test.assertEq(
        "complete",
        document.readyState,
        "Sanity check: DOMContentLoaded has triggerd before window.onload"
      );

      // Sanity check: Verify that the stylesheet was still loading while
      // frame.onload was executing. This should be true because the style
      // sheet loads very slowly.
      browser.test.assertEq(
        "slow.css not loaded",
        globalThis.styleSheetStateAtFrameLoad,
        "Sanity check: stylesheet load pending during frame.onload"
      );
      browser.test.assertEq(
        "slow.css loaded",
        globalThis.styleSheetStateInScriptDefer,
        "Sanity check: stylesheet loaded before deferred script"
      );

      // Sanity check: The deferred script should execute after iframe.onload,
      // because `<script defer>` execution is blocked on stylesheet load
      // completion, and we have a slow-loading stylesheet.
      browser.test.assertEq(
        globalThis.scriptDeferRunAfterFrameLoad,
        true,
        "Sanity check: iframe.onload should run before <script defer>"
      );

      // Sanity check: Verify that the moz-extension:-document was loaded in
      // the frame at iframe.onload.
      browser.test.assertEq(
        "body_of_iframe",
        globalThis.bodyAtFrameLoad,
        "Sanity check: iframe.onload runs when document in frame was ready"
      );

      browser.test.sendMessage("top_and_frame_done");
    };
    dump(`background.js ran. Waiting for iframe.onload to continue.\n`);
  }
  function backgroundScriptDeferred() {
    globalThis.scriptDeferRunAfterFrameLoad = globalThis.scriptRunInFrame;
    globalThis.styleSheetStateInScriptDefer = globalThis.getStyleSheetState();
    globalThis.readyStateInScriptDefer = document.readyState;
    dump(`background-deferred.js ran. Expecting window.onload to run next.\n`);
  }
  const extension = ExtensionTestUtils.loadExtension({
    manifest: {
      background: {
        page: "background.html",
      },
    },
    files: {
      "background.html": `<!DOCTYPE html>
         <meta charset="utf-8">
         <!-- background.js first, to never miss the iframe.onload event -->
         <script src="background.js"></script>
         <!-- iframe should start loading before slow.css blocks the DOM -->
         <iframe src="background-subframe.html" id="iframe"></iframe>
         <!--
         Load a slow stylesheet AND add <script defer> to intentionally postpone
         the DOMContentLoaded notification until well after background-top.js
         has run and extension API calls have been processed, if any.
         -->
         <link rel="stylesheet" href="http://example.com/slow.css">
         <script src="background-deferred.js" defer></script>
      `,
      "background-subframe.html": `<!DOCTYPE html>
         <meta charset="utf-8">
         <!-- No <script> here because their presence blocks iframe.onload -->
         <body>body_of_iframe</body>`,
      "background.js": backgroundScript,
      "background-deferred.js": backgroundScriptDeferred,
    },
  });

  const bgWatcher = new BackgroundWatcher();
  // No "await extension.startup();" because extension.startup() in tests is
  // currently blocked on background startup (due to TEST_NO_DELAYED_STARTUP
  // defaulting to true). Because the background startup completion is blocked
  // on the DOMContentLoaded of the background, extension.startup() does not
  // resolve until we've unblocked the DOMContentLoaded notification.
  const startupPromise = extension.startup();
  await extension.awaitMessage("allowStylesheetToLoad");
  Assert.equal(bgWatcher.bgBrowserCount, 1, "Got background page");
  Assert.equal(bgWatcher.bgViewCount, 0, "Background view still loading");
  info("frame loaded; allowing slow.css to load to unblock DOMContentLoaded");
  allowStylesheetToLoad();

  info("Waiting for extension.startup() to resolve (background completion)");
  await startupPromise;
  info("extension.startup() resolved. Waiting for top_and_frame_done...");

  await extension.awaitMessage("top_and_frame_done");
  Assert.equal(
    extension.extension.backgroundContext?.uri?.spec,
    `moz-extension://${extension.uuid}/background.html`,
    `extension.backgroundContext should exist and point to the main background`
  );
  Assert.equal(bgWatcher.bgViewCount, 1, "Background has loaded once");
  Assert.equal(
    extension.extension.views.size,
    2,
    "Got ProxyContextParent instances for background and iframe"
  );

  await extension.unload();
  bgWatcher.stopObserving();
});

add_task(async function test_only_script_execution_in_iframe() {
  function backgroundSubframeScript() {
    // The exact API call does not matter, as any extension API call will
    // ensure that ProxyContextParent is initialized if it was not before:
    // https://searchfox.org/mozilla-central/rev/892475f3ba2b959aeaef19d1d8602494e3f2ae32/toolkit/components/extensions/ExtensionPageChild.sys.mjs#221,223,227-228
    browser.runtime.getPlatformInfo().then(info => {
      browser.test.assertTrue("os" in info, "extension API called in iframe");
      browser.test.assertTrue(
        browser.extension.getBackgroundPage() === top,
        "extension.getBackgroundPage() returns the top context"
      );
      browser.test.sendMessage("iframe_done");
    });
  }
  const extension = ExtensionTestUtils.loadExtension({
    manifest: {
      background: {
        page: "background.html",
      },
    },
    files: {
      "background.html": `<!DOCTYPE html>
         <meta charset="utf-8">
         <iframe src="background-subframe.html"></iframe>
      `,
      "background-subframe.html": `<!DOCTYPE html>
         <meta charset="utf-8">
         <script src="background-subframe.js"></script>
      `,
      "background-subframe.js": backgroundSubframeScript,
    },
  });
  const bgWatcher = new BackgroundWatcher();
  await extension.startup();
  await extension.awaitMessage("iframe_done");
  Assert.equal(bgWatcher.bgBrowserCount, 1, "Got background page");
  Assert.equal(bgWatcher.bgViewCount, 1, "Got background view");
  Assert.equal(
    extension.extension.views.size,
    2,
    "Got ProxyContextParent instances for background and iframe"
  );

  Assert.equal(
    extension.extension.backgroundContext?.uri?.spec,
    `moz-extension://${extension.uuid}/background.html`,
    `extension.backgroundContext should exist and point to the main background`
  );

  await extension.unload();
  bgWatcher.stopObserving();
});
