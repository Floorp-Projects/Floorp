/* eslint-disable mozilla/no-arbitrary-setTimeout */
"use strict";

const { E10SUtils } = ChromeUtils.import(
  "resource://gre/modules/E10SUtils.jsm"
);

const PREF_NAME = "browser.tabs.remote.useCrossOriginOpenerPolicy";

function httpURL(filename, host = "https://example.com") {
  let root = getRootDirectory(gTestPath).replace(
    "chrome://mochitests/content",
    host
  );
  return root + filename;
}

async function performLoad(browser, opts, action) {
  let loadedPromise = BrowserTestUtils.browserStopped(
    browser,
    opts.url,
    opts.maybeErrorPage
  );
  await action();
  await loadedPromise;
}

async function test_coop(start, target, expectedProcessSwitch) {
  return BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: start,
      waitForStateStop: true,
    },
    async function(_browser) {
      info(`test_coop: Test tab ready: ${start}`);

      await new Promise(resolve => setTimeout(resolve, 20));
      let browser = gBrowser.selectedBrowser;
      let firstProcessID = await ContentTask.spawn(browser, null, () => {
        return Services.appinfo.processID;
      });

      info(`firstProcessID: ${firstProcessID}`);

      await performLoad(
        browser,
        {
          url: target,
          maybeErrorPage: false,
        },
        async () => {
          BrowserTestUtils.loadURI(browser, target);
          if (expectedProcessSwitch) {
            await BrowserTestUtils.waitForEvent(
              gBrowser.getTabForBrowser(browser),
              "SSTabRestored"
            );
          }
        }
      );

      info(`Navigated to: ${target}`);
      await new Promise(resolve => setTimeout(resolve, 20));
      browser = gBrowser.selectedBrowser;
      let secondProcessID = await ContentTask.spawn(browser, null, () => {
        return Services.appinfo.processID;
      });

      info(`secondProcessID: ${secondProcessID}`);
      if (expectedProcessSwitch) {
        Assert.notEqual(
          firstProcessID,
          secondProcessID,
          `from: ${start} to ${target}`
        );
      } else {
        Assert.equal(
          firstProcessID,
          secondProcessID,
          `from: ${start} to ${target}`
        );
      }
    }
  );
}

function waitForDownloadWindow() {
  return new Promise(resolve => {
    var listener = {
      onOpenWindow: aXULWindow => {
        info("Download window shown...");
        Services.wm.removeListener(listener);

        function downloadOnLoad() {
          domwindow.removeEventListener("load", downloadOnLoad, true);

          is(
            domwindow.document.location.href,
            "chrome://mozapps/content/downloads/unknownContentType.xul",
            "Download page appeared"
          );
          resolve(domwindow);
        }

        var domwindow = aXULWindow.docShell.domWindow;
        domwindow.addEventListener("load", downloadOnLoad, true);
      },
      onCloseWindow: aXULWindow => {},
    };

    Services.wm.addListener(listener);
  });
}

async function test_download_from(initCoop, downloadCoop) {
  return BrowserTestUtils.withNewTab("about:blank", async function(_browser) {
    info(`test_download: Test tab ready`);

    let start = httpURL(
      "coop_header.sjs?downloadPage&" + initCoop,
      "https://example.com"
    );
    await performLoad(
      _browser,
      {
        url: start,
        maybeErrorPage: false,
      },
      async () => {
        BrowserTestUtils.loadURI(_browser, start);
        info(`test_download: Loading download page ${start}`);

        // Wait for process switch even the page is load from a new tab.
        if (initCoop != "") {
          await BrowserTestUtils.waitForEvent(
            gBrowser.getTabForBrowser(_browser),
            "SSTabRestored"
          );
        }
      }
    );

    info(`test_download: Download page ready ${start}`);

    await new Promise(resolve => setTimeout(resolve, 20));

    info(`Downloading ${downloadCoop}`);

    let winPromise = waitForDownloadWindow();
    let browser = gBrowser.selectedBrowser;
    ContentTask.spawn(browser, downloadCoop, downloadCoop => {
      content.document.getElementById(downloadCoop).click();
    });

    // if the download page doesn't appear, the promise leads a timeout.
    let win = await winPromise;

    await BrowserTestUtils.closeWindow(win);
  });
}

// Check that multiple navigations of the same tab will only switch processes
// when it's expected.
add_task(async function test_multiple_nav_process_switches() {
  await SpecialPowers.pushPrefEnv({ set: [[PREF_NAME, true]] });
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: httpURL("coop_header.sjs", "https://example.org"),
      waitForStateStop: true,
    },
    async function(browser) {
      await new Promise(resolve => setTimeout(resolve, 20));
      let prevPID = await ContentTask.spawn(browser, null, () => {
        return Services.appinfo.processID;
      });

      let target = httpURL("coop_header.sjs?.", "https://example.org");
      await performLoad(
        browser,
        {
          url: target,
          maybeErrorPage: false,
        },
        async () => {
          BrowserTestUtils.loadURI(browser, target);
        }
      );

      let currentPID = await ContentTask.spawn(browser, null, () => {
        return Services.appinfo.processID;
      });

      Assert.equal(prevPID, currentPID);
      prevPID = currentPID;

      target = httpURL("coop_header.sjs?same-origin", "https://example.org");
      await performLoad(
        browser,
        {
          url: target,
          maybeErrorPage: false,
        },
        async () => {
          BrowserTestUtils.loadURI(browser, target);
          await BrowserTestUtils.waitForEvent(
            gBrowser.getTabForBrowser(browser),
            "SSTabRestored"
          );
        }
      );

      await new Promise(resolve => setTimeout(resolve, 20));
      currentPID = await ContentTask.spawn(browser, null, () => {
        return Services.appinfo.processID;
      });

      Assert.notEqual(prevPID, currentPID);
      prevPID = currentPID;

      target = httpURL("coop_header.sjs?same-origin", "https://example.com");
      await performLoad(
        browser,
        {
          url: target,
          maybeErrorPage: false,
        },
        async () => {
          BrowserTestUtils.loadURI(browser, target);
          await BrowserTestUtils.waitForEvent(
            gBrowser.getTabForBrowser(browser),
            "SSTabRestored"
          );
        }
      );

      await new Promise(resolve => setTimeout(resolve, 20));
      currentPID = await ContentTask.spawn(browser, null, () => {
        return Services.appinfo.processID;
      });

      Assert.notEqual(prevPID, currentPID);
      prevPID = currentPID;

      target = httpURL("coop_header.sjs?same-origin.#4", "https://example.com");
      await performLoad(
        browser,
        {
          url: target,
          maybeErrorPage: false,
        },
        async () => {
          BrowserTestUtils.loadURI(browser, target);
        }
      );

      await new Promise(resolve => setTimeout(resolve, 20));
      currentPID = await ContentTask.spawn(browser, null, () => {
        return Services.appinfo.processID;
      });

      Assert.equal(prevPID, currentPID);
      prevPID = currentPID;
    }
  );
});

add_task(async function test_disabled() {
  await SpecialPowers.pushPrefEnv({ set: [[PREF_NAME, false]] });
  await test_coop(
    httpURL("coop_header.sjs", "https://example.com"),
    httpURL("coop_header.sjs", "https://example.com"),
    false
  );
  await test_coop(
    httpURL("coop_header.sjs?same-origin", "http://example.com"),
    httpURL("coop_header.sjs", "http://example.com"),
    false
  );
  await test_coop(
    httpURL("coop_header.sjs", "http://example.com"),
    httpURL("coop_header.sjs?same-origin", "http://example.com"),
    false
  );
  await test_coop(
    httpURL("coop_header.sjs?same-origin", "http://example.com"),
    httpURL("coop_header.sjs?same-site", "http://example.com"),
    false
  ); // assuming we don't have fission yet :)
});

add_task(async function test_enabled() {
  await SpecialPowers.pushPrefEnv({ set: [[PREF_NAME, true]] });
  await test_coop(
    httpURL("coop_header.sjs", "https://example.com"),
    httpURL("coop_header.sjs", "https://example.com"),
    false
  );
  await test_coop(
    httpURL("coop_header.sjs", "https://example.com"),
    httpURL("coop_header.sjs?same-origin", "https://example.org"),
    true
  );
  await test_coop(
    httpURL("coop_header.sjs?same-origin#1", "https://example.com"),
    httpURL("coop_header.sjs?same-origin#1", "https://example.org"),
    true
  );
  await test_coop(
    httpURL("coop_header.sjs?same-origin#2", "https://example.com"),
    httpURL("coop_header.sjs?same-site#2", "https://example.org"),
    true
  );
});

add_task(async function test_download() {
  requestLongerTimeout(4);
  await SpecialPowers.pushPrefEnv({ set: [[PREF_NAME, true]] });

  let initCoopArray = ["", "same-site", "same-origin"];

  let downloadCoopArray = [
    "no-coop",
    "same-site",
    "same-origin",
    "same-site%20unsafe-allow-outgoing",
    "same-origin%20unsafe-allow-outgoing",
  ];

  // If the coop mismatch between current page and download link, clicking the
  // download link will make the page empty and popup the download window. That
  // forces us to reload the page every time.
  for (var initCoop of initCoopArray) {
    for (var downloadCoop of downloadCoopArray) {
      await test_download_from(initCoop, downloadCoop);
    }
  }
});
