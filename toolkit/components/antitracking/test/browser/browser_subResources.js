add_task(async function() {
  info("Starting subResources test");

  await SpecialPowers.flushPrefEnv();
  await SpecialPowers.pushPrefEnv({
    set: [
      [
        "network.cookie.cookieBehavior",
        Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER,
      ],
      ["privacy.trackingprotection.enabled", false],
      ["privacy.trackingprotection.pbmode.enabled", false],
      ["privacy.trackingprotection.annotate_channels", true],
      [
        "privacy.restrict3rdpartystorage.userInteractionRequiredForHosts",
        "tracking.example.com,tracking.example.org",
      ],
    ],
  });

  await UrlClassifierTestUtils.addTestTrackers();

  info("Creating a new tab");
  let tab = BrowserTestUtils.addTab(gBrowser, TEST_TOP_PAGE);
  gBrowser.selectedTab = tab;

  let browser = gBrowser.getBrowserForTab(tab);
  await BrowserTestUtils.browserLoaded(browser);

  info("Loading tracking scripts and tracking images");
  await ContentTask.spawn(browser, null, async function() {
    // Let's load the script twice here.
    {
      let src = content.document.createElement("script");
      let p = new content.Promise(resolve => {
        src.onload = resolve;
      });
      content.document.body.appendChild(src);
      src.src =
        "https://tracking.example.org/browser/toolkit/components/antitracking/test/browser/subResources.sjs?what=script";
      await p;
    }
    {
      let src = content.document.createElement("script");
      let p = new content.Promise(resolve => {
        src.onload = resolve;
      });
      content.document.body.appendChild(src);
      src.src =
        "https://tracking.example.org/browser/toolkit/components/antitracking/test/browser/subResources.sjs?what=script";
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
        "https://tracking.example.org/browser/toolkit/components/antitracking/test/browser/subResources.sjs?what=image";
      await p;
    }
    {
      let img = content.document.createElement("img");
      let p = new content.Promise(resolve => {
        img.onload = resolve;
      });
      content.document.body.appendChild(img);
      img.src =
        "https://tracking.example.org/browser/toolkit/components/antitracking/test/browser/subResources.sjs?what=image";
      await p;
    }
  });

  await fetch(
    "https://tracking.example.org/browser/toolkit/components/antitracking/test/browser/subResources.sjs?result&what=image"
  )
    .then(r => r.text())
    .then(text => {
      is(text, 0, "Cookies received for images");
    });

  await fetch(
    "https://tracking.example.org/browser/toolkit/components/antitracking/test/browser/subResources.sjs?result&what=script"
  )
    .then(r => r.text())
    .then(text => {
      is(text, 0, "Cookies received for scripts");
    });

  info("Creating a 3rd party content");
  await ContentTask.spawn(
    browser,
    {
      page: TEST_3RD_PARTY_PAGE_WO,
      blockingCallback: (async _ => {}).toString(),
      nonBlockingCallback: (async _ => {}).toString(),
    },
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

  info("Loading tracking scripts and tracking images again");
  await ContentTask.spawn(browser, null, async function() {
    // Let's load the script twice here.
    {
      let src = content.document.createElement("script");
      let p = new content.Promise(resolve => {
        src.onload = resolve;
      });
      content.document.body.appendChild(src);
      src.src =
        "https://tracking.example.org/browser/toolkit/components/antitracking/test/browser/subResources.sjs?what=script";
      await p;
    }
    {
      let src = content.document.createElement("script");
      let p = new content.Promise(resolve => {
        src.onload = resolve;
      });
      content.document.body.appendChild(src);
      src.src =
        "https://tracking.example.org/browser/toolkit/components/antitracking/test/browser/subResources.sjs?what=script";
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
        "https://tracking.example.org/browser/toolkit/components/antitracking/test/browser/subResources.sjs?what=image";
      await p;
    }
    {
      let img = content.document.createElement("img");
      let p = new content.Promise(resolve => {
        img.onload = resolve;
      });
      content.document.body.appendChild(img);
      img.src =
        "https://tracking.example.org/browser/toolkit/components/antitracking/test/browser/subResources.sjs?what=image";
      await p;
    }
  });

  await fetch(
    "https://tracking.example.org/browser/toolkit/components/antitracking/test/browser/subResources.sjs?result&what=image"
  )
    .then(r => r.text())
    .then(text => {
      is(text, 1, "One cookie received for images.");
    });

  await fetch(
    "https://tracking.example.org/browser/toolkit/components/antitracking/test/browser/subResources.sjs?result&what=script"
  )
    .then(r => r.text())
    .then(text => {
      is(text, 1, "One cookie received received for scripts.");
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

  let expectTrackerFound = item => {
    is(
      item[0],
      Ci.nsIWebProgressListener.STATE_LOADED_TRACKING_CONTENT,
      "Correct blocking type reported"
    );
    is(item[1], true, "Correct blocking status reported");
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

  let log = JSON.parse(await browser.getContentBlockingLog());
  for (let trackerOrigin in log) {
    is(
      trackerOrigin + "/",
      TEST_3RD_PARTY_DOMAIN,
      "Correct tracker origin must be reported"
    );
    let originLog = log[trackerOrigin];
    is(originLog.length, 4, "We should have 4 entries in the compressed log");
    expectTrackerFound(originLog[0]);
    expectCookiesLoaded(originLog[1]);
    expectTrackerBlocked(originLog[2], true);
    expectTrackerBlocked(originLog[3], false);
  }

  info("Removing the tab");
  BrowserTestUtils.removeTab(tab);
});

add_task(async function() {
  info("Cleaning up.");
  await new Promise(resolve => {
    Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value =>
      resolve()
    );
  });
});
