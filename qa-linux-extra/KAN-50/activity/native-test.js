(() => {
  // Floorp-pip-activity/browser-features/chrome/test/utils/test_harness.ts
  function assert(condition, message) {
    if (!condition) {
      throw new Error(message);
    }
  }
  function assertEquals(actual, expected, message) {
    if (actual !== expected) {
      throw new Error(`${message} (expected: ${String(expected)}, actual: ${String(actual)})`);
    }
  }
  async function runTests(moduleName, tests) {
    const failures = [];
    for (const [index, test] of tests.entries()) {
      globalThis.__NORA_TEST_PROGRESS__ = {
        moduleName,
        testName: test.name,
        status: "running",
        index: index + 1,
        total: tests.length,
        startedAtMs: Date.now()
      };
      try {
        await test.fn();
        const startedAtMs = globalThis.__NORA_TEST_PROGRESS__?.startedAtMs ?? Date.now();
        globalThis.__NORA_TEST_PROGRESS__ = {
          moduleName,
          testName: test.name,
          status: "passed",
          index: index + 1,
          total: tests.length,
          startedAtMs
        };
      } catch (error) {
        const message = error instanceof Error ? error.message : String(error);
        failures.push(`${test.name}: ${message}`);
        const startedAtMs = globalThis.__NORA_TEST_PROGRESS__?.startedAtMs ?? Date.now();
        globalThis.__NORA_TEST_PROGRESS__ = {
          moduleName,
          testName: test.name,
          status: "failed",
          index: index + 1,
          total: tests.length,
          startedAtMs
        };
      }
    }
    globalThis.__NORA_TEST_PROGRESS__ = {
      moduleName,
      testName: "",
      status: "done",
      index: tests.length,
      total: tests.length,
      startedAtMs: Date.now()
    };
    if (failures.length > 0) {
      throw new Error(`${moduleName} failures: ${failures.join(" | ")}`);
    }
  }

  // Floorp-pip-activity/browser-features/chrome/common/panel-sidebar/test/pictureInPictureActivity.test.ts
  async function waitFor(check) {
    for (let attempt = 0; attempt < 200; attempt++) {
      if (check()) return;
      await new Promise((resolve) => setTimeout(resolve, 50));
    }
    throw new Error("Timed out preparing native embedded chrome document");
  }
  async function testSharedAppWindow(panelFirst) {
    const pip = ChromeUtils.importESModule("moz-src:///toolkit/components/pictureinpicture/PictureInPicture.sys.mjs").PictureInPicture;
    const open = globalThis;
    const app = open.OpenBrowserWindow();
    let outer = null;
    const registered = [];
    try {
      await waitFor(() => !!app.gBrowser?.selectedBrowser);
      outer = app.document.createXULElement("browser");
      outer.setAttribute("src", "about:blank");
      app.document.documentElement.appendChild(outer);
      await waitFor(() => !!outer?.contentDocument?.body);
      const embedded = outer.contentWindow;
      assert(embedded !== app, "fixture has a real nested chrome window");
      assert(embedded.browsingContext.topChromeWindow === app, "nested chrome belongs to the containing app window");
      const inner = app.document.createXULElement("browser");
      outer.contentDocument.body.appendChild(inner);
      const normal = app.gBrowser.selectedBrowser;
      const browsers = panelFirst ? [
        inner,
        normal
      ] : [
        normal,
        inner
      ];
      for (const browser of browsers) {
        pip.addOriginatingWinToWeakMap(browser);
        registered.push(browser);
      }
      assertEquals(pip.originatingWinWeakMap.get(app), 2, "one AppWindow owns both players");
      assertEquals(app.browsingContext.forceAppWindowActive, true, "AppWindow is kept active");
      assertEquals(embedded.browsingContext.forceAppWindowActive, false, "embedded context is not incorrectly forced as an AppWindow");
      pip.removeOriginatingWinFromWeakMap(browsers[0]);
      registered.shift();
      assertEquals(pip.originatingWinWeakMap.get(app), 1, "first close retains the other player");
      assertEquals(app.browsingContext.forceAppWindowActive, true, "first close retains activity");
      pip.removeOriginatingWinFromWeakMap(browsers[1]);
      registered.shift();
      assertEquals(pip.originatingWinWeakMap.has(app), false, "last close releases the count");
      assertEquals(app.browsingContext.forceAppWindowActive, false, "last close releases activity");
    } finally {
      for (const browser of registered) {
        pip.removeOriginatingWinFromWeakMap(browser);
      }
      outer?.remove();
      app.close();
    }
  }
  async function runAllTests() {
    await runTests("pictureInPictureActivity.test.ts", [
      {
        name: "normal then embedded player share AppWindow activity",
        fn: () => testSharedAppWindow(false)
      },
      {
        name: "embedded then normal player share AppWindow activity",
        fn: () => testSharedAppWindow(true)
      }
    ]);
  }

  // qa-linux-22/C-extra/KAN-50/activity/native-test-entry.ts
  globalThis.qaRunPipActivityTests = runAllTests;
})();
