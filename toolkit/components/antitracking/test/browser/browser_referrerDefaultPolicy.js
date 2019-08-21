requestLongerTimeout(8);

const CHROME_BASE =
  "chrome://mochitests/content/browser/browser/base/content/test/general/";
Services.scriptloader.loadSubScript(CHROME_BASE + "head.js", this);
/* import-globals-from ../../../../../browser/base/content/test/general/head.js */

async function openAWindow(private) {
  info("Creating a new " + (private ? "private" : "normal") + " window");
  let win = OpenBrowserWindow({ private });
  await TestUtils.topicObserved(
    "browser-delayed-startup-finished",
    subject => subject == win
  ).then(() => win);
  await BrowserTestUtils.firstBrowserLoaded(win);
  return win;
}

async function testOnWindowBody(win, expectedReferrer, rp) {
  let browser = win.gBrowser;
  let tab = browser.selectedTab;
  let b = browser.getBrowserForTab(tab);
  await promiseTabLoadEvent(tab, TEST_TOP_PAGE);

  info("Loading tracking scripts and tracking images");
  await ContentTask.spawn(b, { rp }, async function({ rp }) {
    {
      let src = content.document.createElement("script");
      let p = new content.Promise(resolve => {
        src.onload = resolve;
      });
      content.document.body.appendChild(src);
      if (rp) {
        src.referrerPolicy = rp;
      }
      src.src =
        "https://tracking.example.org/browser/toolkit/components/antitracking/test/browser/referrer.sjs?what=script";
      await p;
    }

    {
      let img = content.document.createElement("img");
      let p = new content.Promise(resolve => {
        img.onload = resolve;
      });
      content.document.body.appendChild(img);
      if (rp) {
        img.referrerPolicy = rp;
      }
      img.src =
        "https://tracking.example.org/browser/toolkit/components/antitracking/test/browser/referrer.sjs?what=image";
      await p;
    }
  });

  await fetch(
    "https://tracking.example.org/browser/toolkit/components/antitracking/test/browser/referrer.sjs?result&what=script"
  )
    .then(r => r.text())
    .then(text => {
      is(text, expectedReferrer, "We sent the correct Referer header");
    });

  await fetch(
    "https://tracking.example.org/browser/toolkit/components/antitracking/test/browser/referrer.sjs?result&what=image"
  )
    .then(r => r.text())
    .then(text => {
      is(text, expectedReferrer, "We sent the correct Referer header");
    });
}

async function closeAWindow(win) {
  await BrowserTestUtils.closeWindow(win);
}

let gRecording = true;
let gScenarios = [];
let gRPs = [];
let gTests = { private: [], nonPrivate: [] };
const kPBPref = "network.http.referer.defaultPolicy.trackers.pbmode";
const kNonPBPref = "network.http.referer.defaultPolicy.trackers";

function recordScenario(private, expectedReferrer, rp) {
  if (!gRPs.includes(rp)) {
    gRPs.push(rp);
  }
  gScenarios.push({
    private,
    expectedReferrer,
    rp,
    pbPref: Services.prefs.getIntPref(kPBPref),
    nonPBPref: Services.prefs.getIntPref(kNonPBPref),
  });
}

async function testOnWindow(private, expectedReferrer, rp) {
  if (gRecording) {
    recordScenario(private, expectedReferrer, rp);
  }
}

function compileScenarios() {
  let keys = { false: [], true: [] };
  for (let s of gScenarios) {
    let key = {
      rp: s.rp,
      pbPref: s.pbPref,
      nonPBPref: s.nonPBPref,
    };
    let skip = false;
    for (let k of keys[s.private]) {
      if (
        key.rp == k.rp &&
        key.pbPref == k.pbPref &&
        key.nonPBPref == k.nonPBPref
      ) {
        skip = true;
        break;
      }
    }
    if (!skip) {
      keys[s.private].push(key);
      gTests[s.private ? "private" : "nonPrivate"].push({
        rp: s.rp,
        pbPref: s.pbPref,
        nonPBPref: s.nonPBPref,
        expectedReferrer: s.expectedReferrer,
      });
    }
  }

  // Verify that all scenarios are checked
  let counter = 1;
  for (let s of gScenarios) {
    let checked = false;
    for (let tt in gTests) {
      let private = tt == "private";
      for (let t of gTests[tt]) {
        if (
          private == s.private &&
          t.rp == s.rp &&
          t.pbPref == s.pbPref &&
          t.nonPBPref == s.nonPBPref &&
          t.expectedReferrer == s.expectedReferrer
        ) {
          checked = true;
          break;
        }
      }
    }
    ok(checked, `Scenario number ${counter++} checked`);
  }
}

async function executeTests() {
  compileScenarios();

  gRecording = false;
  for (let mode in gTests) {
    info(`Open a ${mode} window`);
    let win = await openAWindow(mode == "private");

    while (gTests[mode].length) {
      let test = gTests[mode].shift();
      info(`Running test ${test.toSource()}`);

      await SpecialPowers.pushPrefEnv({
        set: [
          ["network.http.referer.defaultPolicy.trackers", test.nonPBPref],
          ["network.http.referer.defaultPolicy.trackers.pbmode", test.pbPref],
        ],
      });
      await testOnWindowBody(win, test.expectedReferrer, test.rp);
    }

    await closeAWindow(win);
  }

  Services.prefs.clearUserPref(kPBPref);
  Services.prefs.clearUserPref(kNonPBPref);
}

function pn(name, private) {
  return private ? name + ".pbmode" : name;
}

async function testOnNoReferrer(private) {
  // no-referrer pref when no-referrer is forced
  await SpecialPowers.pushPrefEnv({
    set: [[pn("network.http.referer.defaultPolicy.trackers", private), 3]],
  });
  await testOnWindow(private, "", "no-referrer");

  // strict-origin-when-cross-origin pref when no-referrer is forced
  await SpecialPowers.pushPrefEnv({
    set: [[pn("network.http.referer.defaultPolicy.trackers", private), 2]],
  });
  await testOnWindow(private, "", "no-referrer");

  // same-origin pref when no-referrer is forced
  await SpecialPowers.pushPrefEnv({
    set: [[pn("network.http.referer.defaultPolicy.trackers", private), 1]],
  });
  await testOnWindow(private, "", "no-referrer");

  // no-referrer pref when no-referrer is forced
  await SpecialPowers.pushPrefEnv({
    set: [[pn("network.http.referer.defaultPolicy.trackers", private), 0]],
  });
  await testOnWindow(private, "", "no-referrer");
}

async function testOnSameOrigin(private) {
  // same-origin pref when same-origin is forced
  await SpecialPowers.pushPrefEnv({
    set: [[pn("network.http.referer.defaultPolicy.trackers", private), 3]],
  });
  await testOnWindow(private, "", "same-origin");

  // strict-origin-when-cross-origin pref when same-origin is forced
  await SpecialPowers.pushPrefEnv({
    set: [[pn("network.http.referer.defaultPolicy.trackers", private), 2]],
  });
  await testOnWindow(private, "", "same-origin");

  // same-origin pref when same-origin is forced
  await SpecialPowers.pushPrefEnv({
    set: [[pn("network.http.referer.defaultPolicy.trackers", private), 1]],
  });
  await testOnWindow(private, "", "same-origin");

  // same-origin pref when same-origin is forced
  await SpecialPowers.pushPrefEnv({
    set: [[pn("network.http.referer.defaultPolicy.trackers", private), 0]],
  });
  await testOnWindow(private, "", "same-origin");
}

async function testOnNoReferrerWhenDowngrade(private) {
  // no-referrer-when-downgrade pref when no-referrer-when-downgrade is forced
  await SpecialPowers.pushPrefEnv({
    set: [[pn("network.http.referer.defaultPolicy.trackers", private), 3]],
  });
  await testOnWindow(private, TEST_TOP_PAGE, "no-referrer-when-downgrade");

  // strict-origin-when-cross-origin pref when no-referrer-when-downgrade is forced
  await SpecialPowers.pushPrefEnv({
    set: [[pn("network.http.referer.defaultPolicy.trackers", private), 2]],
  });
  await testOnWindow(private, TEST_TOP_PAGE, "no-referrer-when-downgrade");

  // same-origin pref when no-referrer-when-downgrade is forced
  await SpecialPowers.pushPrefEnv({
    set: [[pn("network.http.referer.defaultPolicy.trackers", private), 1]],
  });
  await testOnWindow(private, TEST_TOP_PAGE, "no-referrer-when-downgrade");

  // no-referrer pref when no-referrer-when-downgrade is forced
  await SpecialPowers.pushPrefEnv({
    set: [[pn("network.http.referer.defaultPolicy.trackers", private), 0]],
  });
  await testOnWindow(private, TEST_TOP_PAGE, "no-referrer-when-downgrade");
}

async function testOnOrigin(private) {
  // origin pref when origin is forced
  await SpecialPowers.pushPrefEnv({
    set: [[pn("network.http.referer.defaultPolicy.trackers", private), 3]],
  });
  await testOnWindow(private, TEST_DOMAIN, "origin");

  // strict-origin pref when origin is forced
  await SpecialPowers.pushPrefEnv({
    set: [[pn("network.http.referer.defaultPolicy.trackers", private), 2]],
  });
  await testOnWindow(private, TEST_DOMAIN, "origin");

  // same-origin pref when origin is forced
  await SpecialPowers.pushPrefEnv({
    set: [[pn("network.http.referer.defaultPolicy.trackers", private), 1]],
  });
  await testOnWindow(private, TEST_DOMAIN, "origin");

  // no-referrer pref when origin is forced
  await SpecialPowers.pushPrefEnv({
    set: [[pn("network.http.referer.defaultPolicy.trackers", private), 0]],
  });
  await testOnWindow(private, TEST_DOMAIN, "origin");
}

async function testOnStrictOrigin(private) {
  // strict-origin pref when strict-origin is forced
  await SpecialPowers.pushPrefEnv({
    set: [[pn("network.http.referer.defaultPolicy.trackers", private), 3]],
  });
  await testOnWindow(private, TEST_DOMAIN, "strict-origin");

  // strict-origin pref when strict-origin is forced
  await SpecialPowers.pushPrefEnv({
    set: [[pn("network.http.referer.defaultPolicy.trackers", private), 2]],
  });
  await testOnWindow(private, TEST_DOMAIN, "strict-origin");

  // same-origin pref when strict-origin is forced
  await SpecialPowers.pushPrefEnv({
    set: [[pn("network.http.referer.defaultPolicy.trackers", private), 1]],
  });
  await testOnWindow(private, TEST_DOMAIN, "strict-origin");

  // no-referrer pref when strict-origin is forced
  await SpecialPowers.pushPrefEnv({
    set: [[pn("network.http.referer.defaultPolicy.trackers", private), 0]],
  });
  await testOnWindow(private, TEST_DOMAIN, "strict-origin");
}

async function testOnOriginWhenCrossOrigin(private) {
  // origin-when-cross-origin pref when origin-when-cross-origin is forced
  await SpecialPowers.pushPrefEnv({
    set: [[pn("network.http.referer.defaultPolicy.trackers", private), 3]],
  });
  await testOnWindow(private, TEST_DOMAIN, "origin-when-cross-origin");

  // strict-origin-when-cross-origin pref when origin-when-cross-origin is forced
  await SpecialPowers.pushPrefEnv({
    set: [[pn("network.http.referer.defaultPolicy.trackers", private), 2]],
  });
  await testOnWindow(private, TEST_DOMAIN, "origin-when-cross-origin");

  // same-origin pref when origin-when-cross-origin is forced
  await SpecialPowers.pushPrefEnv({
    set: [[pn("network.http.referer.defaultPolicy.trackers", private), 1]],
  });
  await testOnWindow(private, TEST_DOMAIN, "origin-when-cross-origin");

  // no-referrer pref when origin-when-cross-origin is forced
  await SpecialPowers.pushPrefEnv({
    set: [[pn("network.http.referer.defaultPolicy.trackers", private), 0]],
  });
  await testOnWindow(private, TEST_DOMAIN, "origin-when-cross-origin");
}

async function testOnStrictOriginWhenCrossOrigin(private) {
  // origin-when-cross-origin pref when strict-origin-when-cross-origin is forced
  await SpecialPowers.pushPrefEnv({
    set: [[pn("network.http.referer.defaultPolicy.trackers", private), 3]],
  });
  await testOnWindow(private, TEST_DOMAIN, "strict-origin-when-cross-origin");

  // strict-origin-when-cross-origin pref when strict-origin-when-cross-origin is forced
  await SpecialPowers.pushPrefEnv({
    set: [[pn("network.http.referer.defaultPolicy.trackers", private), 2]],
  });
  await testOnWindow(private, TEST_DOMAIN, "strict-origin-when-cross-origin");

  // same-origin pref when strict-origin-when-cross-origin is forced
  await SpecialPowers.pushPrefEnv({
    set: [[pn("network.http.referer.defaultPolicy.trackers", private), 1]],
  });
  await testOnWindow(private, TEST_DOMAIN, "strict-origin-when-cross-origin");

  // no-referrer pref when strict-origin-when-cross-origin is forced
  await SpecialPowers.pushPrefEnv({
    set: [[pn("network.http.referer.defaultPolicy.trackers", private), 0]],
  });
  await testOnWindow(private, TEST_DOMAIN, "strict-origin-when-cross-origin");
}

async function testOnUnsafeUrl(private) {
  // no-referrer-when-downgrade pref when unsafe-url is forced
  await SpecialPowers.pushPrefEnv({
    set: [[pn("network.http.referer.defaultPolicy.trackers", private), 3]],
  });
  await testOnWindow(private, TEST_TOP_PAGE, "unsafe-url");

  // strict-origin-when-cross-origin pref when unsafe-url is forced
  await SpecialPowers.pushPrefEnv({
    set: [[pn("network.http.referer.defaultPolicy.trackers", private), 2]],
  });
  await testOnWindow(private, TEST_TOP_PAGE, "unsafe-url");

  // same-origin pref when unsafe-url is forced
  await SpecialPowers.pushPrefEnv({
    set: [[pn("network.http.referer.defaultPolicy.trackers", private), 1]],
  });
  await testOnWindow(private, TEST_TOP_PAGE, "unsafe-url");

  // no-referrer pref when unsafe-url is forced
  await SpecialPowers.pushPrefEnv({
    set: [[pn("network.http.referer.defaultPolicy.trackers", private), 0]],
  });
  await testOnWindow(private, TEST_TOP_PAGE, "unsafe-url");
}

add_task(async function() {
  info("Starting referrer default policy test");

  await SpecialPowers.flushPrefEnv();
  await SpecialPowers.pushPrefEnv({
    set: [
      [
        "network.cookie.cookieBehavior",
        Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER,
      ],
      ["network.http.referer.defaultPolicy", 3],
      ["privacy.trackingprotection.enabled", false],
      ["privacy.trackingprotection.pbmode.enabled", false],
      ["privacy.trackingprotection.annotate_channels", true],
    ],
  });

  await UrlClassifierTestUtils.addTestTrackers();

  // no-referrer-when-downgrade
  await SpecialPowers.pushPrefEnv({
    set: [["network.http.referer.defaultPolicy.trackers", 3]],
  });
  await testOnWindow(false, TEST_TOP_PAGE, null);

  // strict-origin-when-cross-origin
  await SpecialPowers.pushPrefEnv({
    set: [["network.http.referer.defaultPolicy.trackers", 2]],
  });
  await testOnWindow(false, TEST_DOMAIN, null);

  // same-origin
  await SpecialPowers.pushPrefEnv({
    set: [["network.http.referer.defaultPolicy.trackers", 1]],
  });
  await testOnWindow(false, "", null);

  // no-referrer
  await SpecialPowers.pushPrefEnv({
    set: [["network.http.referer.defaultPolicy.trackers", 0]],
  });
  await testOnWindow(false, "", null);

  // override with no-referrer
  await testOnNoReferrer(false);

  // override with same-origin
  await testOnSameOrigin(false);

  // override with no-referrer-when-downgrade
  await testOnNoReferrerWhenDowngrade(false);

  // override with origin
  await testOnOrigin(false);

  // override with strict-origin
  await testOnStrictOrigin(false);

  // override with origin-when-cross-origin
  await testOnOriginWhenCrossOrigin(false);

  // override with strict-origin-when-cross-origin
  await testOnStrictOriginWhenCrossOrigin(false);

  // override with unsafe-url
  await testOnUnsafeUrl(false);

  // Reset the pref.
  Services.prefs.clearUserPref("network.http.referer.defaultPolicy.trackers");

  // no-referrer-when-downgrade
  await SpecialPowers.pushPrefEnv({
    set: [
      // Set both prefs, because if we only set the trackers pref, then the PB
      // mode default policy pref (2) would apply!
      ["network.http.referer.defaultPolicy.pbmode", 3],
      ["network.http.referer.defaultPolicy.trackers.pbmode", 3],
    ],
  });
  await testOnWindow(true, TEST_TOP_PAGE, null);

  // strict-origin-when-cross-origin
  await SpecialPowers.pushPrefEnv({
    set: [["network.http.referer.defaultPolicy.trackers.pbmode", 2]],
  });
  await testOnWindow(true, TEST_DOMAIN, null);

  // same-origin
  await SpecialPowers.pushPrefEnv({
    set: [["network.http.referer.defaultPolicy.trackers.pbmode", 1]],
  });
  await testOnWindow(true, "", null);

  // no-referrer
  await SpecialPowers.pushPrefEnv({
    set: [["network.http.referer.defaultPolicy.trackers.pbmode", 0]],
  });
  await testOnWindow(true, "", null);

  // override with no-referrer
  await testOnNoReferrer(true);

  // override with same-origin
  await testOnSameOrigin(true);

  // override with no-referrer-when-downgrade
  await testOnNoReferrerWhenDowngrade(true);

  // override with origin
  await testOnOrigin(true);

  // override with strict-origin
  await testOnStrictOrigin(true);

  // override with origin-when-cross-origin
  await testOnOriginWhenCrossOrigin(true);

  // override with strict-origin-when-cross-origin
  await testOnStrictOriginWhenCrossOrigin(true);

  // override with unsafe-url
  await testOnUnsafeUrl(true);

  // Reset the pref.
  Services.prefs.clearUserPref(
    "network.http.referer.defaultPolicy.trackers.pbmode"
  );
});

add_task(async function() {
  await executeTests();
});

add_task(async function() {
  info("Cleaning up.");
  await new Promise(resolve => {
    Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value =>
      resolve()
    );
  });
});
