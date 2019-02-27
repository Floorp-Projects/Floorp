const CHROME_BASE = "chrome://mochitests/content/browser/browser/base/content/test/general/";
Services.scriptloader.loadSubScript(CHROME_BASE + "head.js", this);
/* import-globals-from ../../../../../browser/base/content/test/general/head.js */

async function testOnWindow(private, expectedReferrer) {
  info("Creating a new " + (private ? "private" : "normal") + " window");
  let win = await BrowserTestUtils.openNewBrowserWindow({private});
  let browser = win.gBrowser;
  let tab = browser.selectedTab;
  let b = browser.getBrowserForTab(tab);
  await promiseTabLoadEvent(tab, TEST_TOP_PAGE);

  info("Loading tracking scripts and tracking images");
  await ContentTask.spawn(b, null, async function() {
    {
      let src = content.document.createElement("script");
      let p = new content.Promise(resolve => { src.onload = resolve; });
      content.document.body.appendChild(src);
      src.src = "https://tracking.example.org/browser/toolkit/components/antitracking/test/browser/referrer.sjs?what=script";
      await p;
    }

    {
      let img = content.document.createElement("img");
      let p = new content.Promise(resolve => { img.onload = resolve; });
      content.document.body.appendChild(img);
      img.src = "https://tracking.example.org/browser/toolkit/components/antitracking/test/browser/referrer.sjs?what=image";
      await p;
    }
  });

  await fetch("https://tracking.example.org/browser/toolkit/components/antitracking/test/browser/referrer.sjs?result&what=image")
    .then(r => r.text())
    .then(text => {
      is(text, expectedReferrer, "We sent the correct Referer header");
    });

  await fetch("https://tracking.example.org/browser/toolkit/components/antitracking/test/browser/referrer.sjs?result&what=script")
    .then(r => r.text())
    .then(text => {
      is(text, expectedReferrer, "We sent the correct Referer header");
    });

  await BrowserTestUtils.closeWindow(win);
}

add_task(async function() {
  info("Starting referrer default policy test");

  await SpecialPowers.flushPrefEnv();
  await SpecialPowers.pushPrefEnv({"set": [
    ["browser.contentblocking.allowlist.annotations.enabled", true],
    ["browser.contentblocking.allowlist.storage.enabled", true],
    ["network.cookie.cookieBehavior", Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER],
    ["privacy.trackingprotection.enabled", false],
    ["privacy.trackingprotection.pbmode.enabled", false],
    ["privacy.trackingprotection.annotate_channels", true],
  ]});

  await UrlClassifierTestUtils.addTestTrackers();

  // no-referrer-when-downgrade
  await SpecialPowers.pushPrefEnv({"set":
    [["network.http.referer.defaultPolicy.trackers", 3]]});
  await testOnWindow(false, TEST_TOP_PAGE);

  // strict-origin-when-cross-origin
  await SpecialPowers.pushPrefEnv({"set":
    [["network.http.referer.defaultPolicy.trackers", 2]]});
  await testOnWindow(false, TEST_DOMAIN);

  // same-origin
  await SpecialPowers.pushPrefEnv({"set":
    [["network.http.referer.defaultPolicy.trackers", 1]]});
  await testOnWindow(false, "");

  // no-referrer
  await SpecialPowers.pushPrefEnv({"set":
    [["network.http.referer.defaultPolicy.trackers", 0]]});
  await testOnWindow(false, "");

  // Reset the pref.
  Services.prefs.clearUserPref("network.http.referer.defaultPolicy.trackers");

  // no-referrer-when-downgrade
  await SpecialPowers.pushPrefEnv({"set": [
    // Set both prefs, because if we only set the trackers pref, then the PB
    // mode default policy pref (2) would apply!
    ["network.http.referer.defaultPolicy.pbmode", 3],
    ["network.http.referer.defaultPolicy.trackers.pbmode", 3],
  ]});
  await testOnWindow(true, TEST_TOP_PAGE);

  // strict-origin-when-cross-origin
  await SpecialPowers.pushPrefEnv({"set":
    [["network.http.referer.defaultPolicy.trackers.pbmode", 2]]});
  await testOnWindow(true, TEST_DOMAIN);

  // same-origin
  await SpecialPowers.pushPrefEnv({"set":
    [["network.http.referer.defaultPolicy.trackers.pbmode", 1]]});
  await testOnWindow(true, "");

  // no-referrer
  await SpecialPowers.pushPrefEnv({"set":
    [["network.http.referer.defaultPolicy.trackers.pbmode", 0]]});
  await testOnWindow(true, "");

  // Reset the pref.
  Services.prefs.clearUserPref("network.http.referer.defaultPolicy.trackers.pbmode");
});

add_task(async function() {
  info("Cleaning up.");
  await new Promise(resolve => {
    Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value => resolve());
  });
});

