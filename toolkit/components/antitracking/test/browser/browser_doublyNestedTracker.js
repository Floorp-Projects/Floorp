add_task(async function() {
  info("Starting doubly nested tracker test");

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

  let tab = BrowserTestUtils.addTab(gBrowser, TEST_3RD_PARTY_PAGE);
  gBrowser.selectedTab = tab;

  let browser = gBrowser.getBrowserForTab(tab);
  await BrowserTestUtils.browserLoaded(browser);

  async function loadSubpage() {
    async function runChecks() {
      is(document.cookie, "", "No cookies for me");
      document.cookie = "name=value";
      is(document.cookie, "name=value", "I have the cookies!");
    }

    await new Promise(resolve => {
      let ifr = document.createElement("iframe");
      ifr.onload = _ => {
        info("Sending code to the 3rd party content");
        ifr.contentWindow.postMessage(runChecks.toString(), "*");
      };

      addEventListener("message", function msg(event) {
        if (event.data.type == "finish") {
          removeEventListener("message", msg);
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

      document.body.appendChild(ifr);
      ifr.src =
        "https://tracking.example.org/browser/toolkit/components/antitracking/test/browser/3rdParty.html";
    });
  }

  await ContentTask.spawn(
    browser,
    { page: TEST_ANOTHER_3RD_PARTY_PAGE, callback: loadSubpage.toString() },
    async function(obj) {
      await new content.Promise(resolve => {
        let ifr = content.document.createElement("iframe");
        ifr.onload = _ => {
          info("Sending code to the 3rd party content");
          ifr.contentWindow.postMessage(obj.callback, "*");
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

  BrowserTestUtils.removeTab(tab);

  UrlClassifierTestUtils.cleanupTestTrackers();
});

add_task(async function() {
  info("Cleaning up.");
  await new Promise(resolve => {
    Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value =>
      resolve()
    );
  });
});
