"use strict";

const COOP_PREF = "browser.tabs.remote.useCrossOriginOpenerPolicy";

async function setPref() {
  await SpecialPowers.pushPrefEnv({
    set: [[COOP_PREF, true]],
  });
}

async function unsetPref() {
  await SpecialPowers.pushPrefEnv({
    set: [[COOP_PREF, false]],
  });
}

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

async function test_coop(
  start,
  target,
  expectedProcessSwitch,
  startRemoteTypeCheck,
  targetRemoteTypeCheck
) {
  return BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: start,
      waitForStateStop: true,
    },
    async function (_browser) {
      info(`test_coop: Test tab ready: ${start}`);

      let browser = gBrowser.selectedBrowser;
      let firstRemoteType = browser.remoteType;
      let firstBC = browser.browsingContext;

      info(`firstBC: ${firstBC.id} remoteType: ${firstRemoteType}`);

      if (startRemoteTypeCheck) {
        startRemoteTypeCheck(firstRemoteType);
      }

      await performLoad(
        browser,
        {
          url: target,
          maybeErrorPage: false,
        },
        async () => BrowserTestUtils.startLoadingURIString(browser, target)
      );

      info(`Navigated to: ${target}`);
      browser = gBrowser.selectedBrowser;
      let secondRemoteType = browser.remoteType;
      let secondBC = browser.browsingContext;

      info(`secondBC: ${secondBC.id} remoteType: ${secondRemoteType}`);
      if (targetRemoteTypeCheck) {
        targetRemoteTypeCheck(secondRemoteType);
      }
      if (expectedProcessSwitch) {
        Assert.notEqual(firstBC.id, secondBC.id, `from: ${start} to ${target}`);
      } else {
        Assert.equal(firstBC.id, secondBC.id, `from: ${start} to ${target}`);
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
            "chrome://mozapps/content/downloads/unknownContentType.xhtml",
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

async function waitForDownloadUI() {
  return BrowserTestUtils.waitForEvent(DownloadsPanel.panel, "popupshown");
}

async function cleanupDownloads(downloadList) {
  info("cleaning up downloads");
  let [download] = await downloadList.getAll();
  await downloadList.remove(download);
  await download.finalize(true);

  try {
    if (Services.appinfo.OS === "WINNT") {
      // We need to make the file writable to delete it on Windows.
      await IOUtils.setPermissions(download.target.path, 0o600);
    }
    await IOUtils.remove(download.target.path);
  } catch (error) {
    info("The file " + download.target.path + " is not removed, " + error);
  }

  if (DownloadsPanel.panel.state !== "closed") {
    let hiddenPromise = BrowserTestUtils.waitForEvent(
      DownloadsPanel.panel,
      "popuphidden"
    );
    DownloadsPanel.hidePanel();
    await hiddenPromise;
  }
  is(
    DownloadsPanel.panel.state,
    "closed",
    "Check that the download panel is closed"
  );
}

async function test_download_from(initCoop, downloadCoop) {
  return BrowserTestUtils.withNewTab("about:blank", async function (_browser) {
    info(`test_download: Test tab ready`);

    let start = httpURL(
      "coop_header.sjs?downloadPage&coop=" + initCoop,
      "https://example.com"
    );
    await performLoad(
      _browser,
      {
        url: start,
        maybeErrorPage: false,
      },
      async () => {
        info(`test_download: Loading download page ${start}`);
        return BrowserTestUtils.startLoadingURIString(_browser, start);
      }
    );

    info(`test_download: Download page ready ${start}`);
    info(`Downloading ${downloadCoop}`);

    let expectDialog = Services.prefs.getBoolPref(
      "browser.download.always_ask_before_handling_new_types",
      false
    );
    let resultPromise = expectDialog
      ? waitForDownloadWindow()
      : waitForDownloadUI();
    let browser = gBrowser.selectedBrowser;
    SpecialPowers.spawn(browser, [downloadCoop], downloadCoop => {
      content.document.getElementById(downloadCoop).click();
    });

    // if the download page doesn't appear, the promise leads a timeout.
    if (expectDialog) {
      let win = await resultPromise;
      await BrowserTestUtils.closeWindow(win);
    } else {
      // verify link target will get automatically downloaded
      await resultPromise;
      let downloadList = await Downloads.getList(Downloads.PUBLIC);
      is((await downloadList.getAll()).length, 1, "Target was downloaded");
      await cleanupDownloads(downloadList);
      is((await downloadList.getAll()).length, 0, "Downloads were cleaned up");
    }
  });
}

// Check that multiple navigations of the same tab will only switch processes
// when it's expected.
add_task(async function test_multiple_nav_process_switches() {
  await setPref();
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: httpURL("coop_header.sjs", "https://example.org"),
      waitForStateStop: true,
    },
    async function (browser) {
      let prevBC = browser.browsingContext;

      let target = httpURL("coop_header.sjs?.", "https://example.org");
      await performLoad(
        browser,
        {
          url: target,
          maybeErrorPage: false,
        },
        async () => BrowserTestUtils.startLoadingURIString(browser, target)
      );

      Assert.equal(prevBC, browser.browsingContext);
      prevBC = browser.browsingContext;

      target = httpURL(
        "coop_header.sjs?coop=same-origin",
        "https://example.org"
      );
      await performLoad(
        browser,
        {
          url: target,
          maybeErrorPage: false,
        },
        async () => BrowserTestUtils.startLoadingURIString(browser, target)
      );

      Assert.notEqual(prevBC, browser.browsingContext);
      prevBC = browser.browsingContext;

      target = httpURL(
        "coop_header.sjs?coop=same-origin",
        "https://example.com"
      );
      await performLoad(
        browser,
        {
          url: target,
          maybeErrorPage: false,
        },
        async () => BrowserTestUtils.startLoadingURIString(browser, target)
      );

      Assert.notEqual(prevBC, browser.browsingContext);
      prevBC = browser.browsingContext;

      target = httpURL(
        "coop_header.sjs?coop=same-origin&index=4",
        "https://example.com"
      );
      await performLoad(
        browser,
        {
          url: target,
          maybeErrorPage: false,
        },
        async () => BrowserTestUtils.startLoadingURIString(browser, target)
      );

      Assert.equal(prevBC, browser.browsingContext);
    }
  );
});

add_task(async function test_disabled() {
  await unsetPref();
  await test_coop(
    httpURL("coop_header.sjs", "https://example.com"),
    httpURL("coop_header.sjs", "https://example.com"),
    false
  );
  await test_coop(
    httpURL("coop_header.sjs?coop=same-origin", "http://example.com"),
    httpURL("coop_header.sjs", "http://example.com"),
    false
  );
  await test_coop(
    httpURL("coop_header.sjs", "http://example.com"),
    httpURL("coop_header.sjs?coop=same-origin", "http://example.com"),
    false
  );
});

add_task(async function test_enabled() {
  await setPref();

  function checkIsCoopRemoteType(remoteType) {
    Assert.ok(
      remoteType.startsWith(E10SUtils.WEB_REMOTE_COOP_COEP_TYPE_PREFIX),
      `${remoteType} expected to be coop`
    );
  }

  function checkIsNotCoopRemoteType(remoteType) {
    if (gFissionBrowser) {
      Assert.ok(
        remoteType.startsWith("webIsolated="),
        `${remoteType} expected to start with webIsolated=`
      );
    } else {
      Assert.equal(
        remoteType,
        E10SUtils.WEB_REMOTE_TYPE,
        `${remoteType} expected to be web`
      );
    }
  }

  await test_coop(
    httpURL("coop_header.sjs", "https://example.com"),
    httpURL("coop_header.sjs", "https://example.com"),
    false,
    checkIsNotCoopRemoteType,
    checkIsNotCoopRemoteType
  );
  await test_coop(
    httpURL("coop_header.sjs", "https://example.com"),
    httpURL("coop_header.sjs?coop=same-origin", "https://example.org"),
    true,
    checkIsNotCoopRemoteType,
    checkIsNotCoopRemoteType
  );
  await test_coop(
    httpURL("coop_header.sjs?coop=same-origin&index=1", "https://example.com"),
    httpURL("coop_header.sjs?coop=same-origin&index=1", "https://example.org"),
    true,
    checkIsNotCoopRemoteType,
    checkIsNotCoopRemoteType
  );
  await test_coop(
    httpURL("coop_header.sjs", "https://example.com"),
    httpURL(
      "coop_header.sjs?coop=same-origin&coep=require-corp",
      "https://example.com"
    ),
    true,
    checkIsNotCoopRemoteType,
    checkIsCoopRemoteType
  );
  await test_coop(
    httpURL(
      "coop_header.sjs?coop=same-origin&coep=require-corp&index=2",
      "https://example.com"
    ),
    httpURL(
      "coop_header.sjs?coop=same-origin&coep=require-corp&index=3",
      "https://example.com"
    ),
    false,
    checkIsCoopRemoteType,
    checkIsCoopRemoteType
  );
  await test_coop(
    httpURL(
      "coop_header.sjs?coop=same-origin&coep=require-corp&index=4",
      "https://example.com"
    ),
    httpURL("coop_header.sjs", "https://example.com"),
    true,
    checkIsCoopRemoteType,
    checkIsNotCoopRemoteType
  );
  await test_coop(
    httpURL(
      "coop_header.sjs?coop=same-origin&coep=require-corp&index=5",
      "https://example.com"
    ),
    httpURL(
      "coop_header.sjs?coop=same-origin&coep=require-corp&index=6",
      "https://example.org"
    ),
    true,
    checkIsCoopRemoteType,
    checkIsCoopRemoteType
  );
});

add_task(async function test_download() {
  requestLongerTimeout(4);
  await setPref();

  let initCoopArray = ["", "same-origin"];

  let downloadCoopArray = [
    "no-coop",
    "same-origin",
    "same-origin-allow-popups",
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
