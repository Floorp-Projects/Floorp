/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const server = createHttpServer();
server.registerDirectory("/data/", do_get_file("data"));

const BASE_URL = `http://localhost:${server.identity.primaryPort}/data`;

function background() {
  browser.runtime.onPerformanceWarning.addListener(details => {
    browser.test.assertDeepEq(
      {
        category: "content_script",
        severity: "high",
        description:
          "Slow extension content script caused a page hang, user was warned.",
      },
      details,
      "Performance warning event contains expected details."
    );
    browser.test.notifyPass("performance-warning");
  });
}

function otherBackground() {
  browser.runtime.onPerformanceWarning.addListener(() => {
    browser.test.fail(
      "Unexpected onPerformanceWarning fired for other extension"
    );
  });
}

function contentScript() {
  function hang(hangSeconds) {
    const start = performance.now();
    while (performance.now() - start < hangSeconds * 1000) {
      /* no-op */
    }
  }

  browser.test.onMessage.addListener(message => {
    if (message === "trigger-hang") {
      hang(3);
    }
  });
}

add_task(async function test_should_fire_on_content_script_hang() {
  // Ensure hang reports are triggered after a one second hang, even without
  // user input and for DEBUG builds.
  Services.prefs.setIntPref("dom.max_ext_content_script_run_time", 1);
  Services.prefs.setBoolPref(
    "dom.max_script_run_time.require_critical_input",
    false
  );
  Services.prefs.setBoolPref("dom.ipc.reportProcessHangs", true);

  // First extension that triggers hang reports.
  const extension = ExtensionTestUtils.loadExtension({
    background,
    files: {
      "contentscript.js": contentScript,
    },
    manifest: {
      content_scripts: [
        {
          js: ["contentscript.js"],
          matches: ["http://localhost/*/dummy_page.html"],
          run_at: "document_start",
        },
      ],
    },
  });
  await extension.startup();

  // Second extension that does not trigger hang reports.
  const otherExtension = ExtensionTestUtils.loadExtension({
    background: otherBackground,
  });
  await otherExtension.startup();

  // Page for the first extension to run its content script.
  const contentPage = await ExtensionTestUtils.loadContentPage(
    `${BASE_URL}/dummy_page.html`
  );
  await contentPage.browserReady;

  // Wait for the warning to fire and be caught.
  await extension.sendMessage("trigger-hang");
  await extension.awaitFinish("performance-warning");

  // Tidy up.
  await extension.unload();
  await otherExtension.unload();
  await contentPage.close();

  Services.prefs.clearUserPref("dom.max_ext_content_script_run_time");
  Services.prefs.clearUserPref(
    "dom.max_script_run_time.require_critical_input"
  );
  Services.prefs.clearUserPref("dom.ipc.reportProcessHangs");
});
