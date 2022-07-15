async function runTests(topPage, limitForeignContexts) {
  info("Creating a new tab");
  let tab = BrowserTestUtils.addTab(gBrowser, topPage);
  gBrowser.selectedTab = tab;

  let browser = gBrowser.getBrowserForTab(tab);
  await BrowserTestUtils.browserLoaded(browser);

  info("Loading scripts and images");
  await SpecialPowers.spawn(browser, [], async function() {
    // Let's load the script twice here.
    {
      let src = content.document.createElement("script");
      let p = new content.Promise(resolve => {
        src.onload = resolve;
      });
      content.document.body.appendChild(src);
      src.src =
        "https://example.org/browser/toolkit/components/antitracking/test/browser/subResources.sjs?what=script";
      await p;
    }
    {
      let src = content.document.createElement("script");
      let p = new content.Promise(resolve => {
        src.onload = resolve;
      });
      content.document.body.appendChild(src);
      src.src =
        "https://example.org/browser/toolkit/components/antitracking/test/browser/subResources.sjs?what=script";
      await p;
    }

    // Let's load an image twice here.
    {
      let img = content.document.createElement("img");
      let p = new content.Promise(resolve => {
        img.onload = resolve;
      });
      content.document.body.appendChild(img);
      img.src =
        "https://example.org/browser/toolkit/components/antitracking/test/browser/subResources.sjs?what=image";
      await p;
    }
    {
      let img = content.document.createElement("img");
      let p = new content.Promise(resolve => {
        img.onload = resolve;
      });
      content.document.body.appendChild(img);
      img.src =
        "https://example.org/browser/toolkit/components/antitracking/test/browser/subResources.sjs?what=image";
      await p;
    }
  });

  await fetch(
    "https://example.org/browser/toolkit/components/antitracking/test/browser/subResources.sjs?result&what=image"
  )
    .then(r => r.text())
    .then(text => {
      if (limitForeignContexts) {
        is(text, "0", "No cookie received for images.");
      } else {
        is(text, "1", "One cookie received for images.");
      }
    });

  await fetch(
    "https://example.org/browser/toolkit/components/antitracking/test/browser/subResources.sjs?result&what=script"
  )
    .then(r => r.text())
    .then(text => {
      if (limitForeignContexts) {
        is(text, "0", "No cookie received received for scripts.");
      } else {
        is(text, "1", "One cookie received received for scripts.");
      }
    });

  info("Creating a 3rd party content");
  await SpecialPowers.spawn(
    browser,
    [
      {
        page: TEST_3RD_PARTY_PAGE_WO,
        blockingCallback: (async _ => {}).toString(),
        nonBlockingCallback: (async _ => {}).toString(),
      },
    ],
    async function(obj) {
      await new content.Promise(resolve => {
        let ifr = content.document.createElement("iframe");
        ifr.onload = function() {
          info("Sending code to the 3rd party content");
          ifr.contentWindow.postMessage(obj, "*");
        };

        content.addEventListener("message", function msg(event) {
          if (event.data.type == "finish") {
            content.removeEventListener("message", msg);
            resolve();
            return;
          }

          if (event.data.type == "ok") {
            ok(event.data.what, event.data.msg);
            return;
          }

          if (event.data.type == "info") {
            info(event.data.msg);
            return;
          }

          ok(false, "Unknown message");
        });

        content.document.body.appendChild(ifr);
        ifr.src = obj.page;
      });
    }
  );

  info("Loading scripts and images again");
  await SpecialPowers.spawn(browser, [], async function() {
    // Let's load the script twice here.
    {
      let src = content.document.createElement("script");
      let p = new content.Promise(resolve => {
        src.onload = resolve;
      });
      content.document.body.appendChild(src);
      src.src =
        "https://example.com/browser/toolkit/components/antitracking/test/browser/subResources.sjs?what=script";
      await p;
    }
    {
      let src = content.document.createElement("script");
      let p = new content.Promise(resolve => {
        src.onload = resolve;
      });
      content.document.body.appendChild(src);
      src.src =
        "https://example.com/browser/toolkit/components/antitracking/test/browser/subResources.sjs?what=script";
      await p;
    }

    // Let's load an image twice here.
    {
      let img = content.document.createElement("img");
      let p = new content.Promise(resolve => {
        img.onload = resolve;
      });
      content.document.body.appendChild(img);
      img.src =
        "https://example.com/browser/toolkit/components/antitracking/test/browser/subResources.sjs?what=image";
      await p;
    }
    {
      let img = content.document.createElement("img");
      let p = new content.Promise(resolve => {
        img.onload = resolve;
      });
      content.document.body.appendChild(img);
      img.src =
        "https://example.com/browser/toolkit/components/antitracking/test/browser/subResources.sjs?what=image";
      await p;
    }
  });

  await fetch(
    "https://example.com/browser/toolkit/components/antitracking/test/browser/subResources.sjs?result&what=image"
  )
    .then(r => r.text())
    .then(text => {
      if (limitForeignContexts) {
        is(text, "0", "No cookie received for images.");
      } else {
        is(text, "1", "One cookie received for images.");
      }
    });

  await fetch(
    "https://example.com/browser/toolkit/components/antitracking/test/browser/subResources.sjs?result&what=script"
  )
    .then(r => r.text())
    .then(text => {
      if (limitForeignContexts) {
        is(text, "0", "No cookie received received for scripts.");
      } else {
        is(text, "1", "One cookie received received for scripts.");
      }
    });

  let expectTrackerBlocked = (item, blocked) => {
    is(
      item[0],
      Ci.nsIWebProgressListener.STATE_COOKIES_BLOCKED_TRACKER,
      "Correct blocking type reported"
    );
    is(item[1], blocked, "Correct blocking status reported");
    ok(item[2] >= 1, "Correct repeat count reported");
  };

  let expectCookiesLoaded = item => {
    is(
      item[0],
      Ci.nsIWebProgressListener.STATE_COOKIES_LOADED,
      "Correct blocking type reported"
    );
    is(item[1], true, "Correct blocking status reported");
    ok(item[2] >= 1, "Correct repeat count reported");
  };

  let expectCookiesBlockedForeign = item => {
    is(
      item[0],
      Ci.nsIWebProgressListener.STATE_COOKIES_BLOCKED_FOREIGN,
      "Correct blocking type reported"
    );
    is(item[1], true, "Correct blocking status reported");
    ok(item[2] >= 1, "Correct repeat count reported");
  };

  let log = JSON.parse(await browser.getContentBlockingLog());
  for (let trackerOrigin in log) {
    let originLog = log[trackerOrigin];
    info(trackerOrigin);
    switch (trackerOrigin) {
      case "https://example.org":
      case "https://example.com":
        let numEntries = 1;
        if (limitForeignContexts) {
          ++numEntries;
        }
        is(
          originLog.length,
          numEntries,
          `We should have ${numEntries} entries in the compressed log`
        );
        expectCookiesLoaded(originLog[0]);
        if (limitForeignContexts) {
          expectCookiesBlockedForeign(originLog[1]);
        }
        break;
      case "https://tracking.example.org":
        is(
          originLog.length,
          1,
          "We should have 1 entries in the compressed log"
        );
        expectTrackerBlocked(originLog[0], false);
        break;
    }
  }

  info("Removing the tab");
  BrowserTestUtils.removeTab(tab);
}

add_task(async function() {
  info("Starting subResources test");

  await SpecialPowers.flushPrefEnv();
  await SpecialPowers.pushPrefEnv({
    set: [
      [
        "network.cookie.cookieBehavior",
        Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN,
      ],
      [
        "network.cookie.cookieBehavior.pbmode",
        Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN,
      ],
      ["privacy.trackingprotection.enabled", false],
      ["privacy.trackingprotection.pbmode.enabled", false],
      ["privacy.trackingprotection.annotate_channels", true],
      // Bug 1617611: Fix all the tests broken by "cookies SameSite=lax by default"
      ["network.cookie.sameSite.laxByDefault", false],
      [
        "privacy.partition.always_partition_third_party_non_cookie_storage",
        false,
      ],
    ],
  });

  for (let limitForeignContexts of [false, true]) {
    SpecialPowers.setBoolPref(
      "privacy.dynamic_firstparty.limitForeign",
      limitForeignContexts
    );
    for (let page of [TEST_TOP_PAGE, TEST_TOP_PAGE_2, TEST_TOP_PAGE_3]) {
      await runTests(page, limitForeignContexts);
    }
  }

  SpecialPowers.clearUserPref("privacy.dynamic_firstparty.limitForeign");
  SpecialPowers.clearUserPref("network.cookie.sameSite.laxByDefault");
});

add_task(async function() {
  info("Cleaning up.");
  await new Promise(resolve => {
    Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value =>
      resolve()
    );
  });
});
