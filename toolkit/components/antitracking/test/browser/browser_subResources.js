ChromeUtils.import("resource://gre/modules/Services.jsm");

add_task(async function() {
  info("Starting subResources test");

  await SpecialPowers.flushPrefEnv();
  await SpecialPowers.pushPrefEnv({"set": [
    ["privacy.restrict3rdpartystorage.enabled", true],
    ["privacy.trackingprotection.enabled", false],
    ["privacy.trackingprotection.pbmode.enabled", false],
    ["privacy.trackingprotection.annotate_channels", true],
  ]});

  await UrlClassifierTestUtils.addTestTrackers();

  info("Creating a new tab");
  let tab = BrowserTestUtils.addTab(gBrowser, TEST_TOP_PAGE);
  gBrowser.selectedTab = tab;

  let browser = gBrowser.getBrowserForTab(tab);
  await BrowserTestUtils.browserLoaded(browser);

  info("Loading a tracking scripts");
  await ContentTask.spawn(browser, null, async function() {
    // Let's load the script twice here.
    {
      let src = content.document.createElement("script");
      let p = new content.Promise(resolve => { src.onload = resolve; });
      content.document.body.appendChild(src);
      src.src = "https://tracking.example.com/browser/toolkit/components/antitracking/test/browser/script.sjs";
      await p;
    }

    {
      let src = content.document.createElement("script");
      let p = new content.Promise(resolve => { src.onload = resolve; });
      content.document.body.appendChild(src);
      src.src = "https://tracking.example.com/browser/toolkit/components/antitracking/test/browser/script.sjs";
      await p;
    }
  });

  await fetch("https://tracking.example.com/browser/toolkit/components/antitracking/test/browser/script.sjs?result")
    .then(r => r.text())
    .then(text => {
      is(text, 0, "No cookies received.");
    });

  info("Creating a 3rd party content");
  await ContentTask.spawn(browser,
                          { page: TEST_3RD_PARTY_PAGE_WO,
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
  });

  info("Loading a tracking scripts again");
  await ContentTask.spawn(browser, null, async function() {
    // Let's load the script twice here.
    {
      let src = content.document.createElement("script");
      let p = new content.Promise(resolve => { src.onload = resolve; });
      content.document.body.appendChild(src);
      src.src = "https://tracking.example.com/browser/toolkit/components/antitracking/test/browser/script.sjs";
      await p;
    }

    {
      let src = content.document.createElement("script");
      let p = new content.Promise(resolve => { src.onload = resolve; });
      content.document.body.appendChild(src);
      src.src = "https://tracking.example.com/browser/toolkit/components/antitracking/test/browser/script.sjs";
      await p;
    }
  });

  await fetch("https://tracking.example.com/browser/toolkit/components/antitracking/test/browser/script.sjs?result")
    .then(r => r.text())
    .then(text => {
      is(text, 1, "One cookie received received.");
    });

  info("Removing the tab");
  BrowserTestUtils.removeTab(tab);

  UrlClassifierTestUtils.cleanupTestTrackers();
});

add_task(async function() {
  info("Cleaning up.");
  await new Promise(resolve => {
    Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value => resolve());
  });
});

