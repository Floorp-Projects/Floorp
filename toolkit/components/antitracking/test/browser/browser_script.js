/* import-globals-from antitracking_head.js */

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

  info("Loading tracking scripts");
  await SpecialPowers.spawn(
    browser,
    [
      {
        scriptURL: TEST_DOMAIN + TEST_PATH + "tracker.js",
        page: TEST_3RD_PARTY_PAGE,
      },
    ],
    async obj => {
      info("Checking if permission is denied");
      let callbackBlocked = async _ => {
        try {
          localStorage.foo = 42;
          ok(false, "LocalStorage cannot be used!");
        } catch (e) {
          ok(true, "LocalStorage cannot be used!");
          is(e.name, "SecurityError", "We want a security error message.");
        }
      };

      let assertBlocked = () =>
        new content.Promise(resolve => {
          let ifr = content.document.createElement("iframe");
          ifr.onload = function() {
            info("Sending code to the 3rd party content");
            ifr.contentWindow.postMessage(callbackBlocked.toString(), "*");
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

      await assertBlocked();

      info("Triggering a 3rd party script...");
      let p = new content.Promise(resolve => {
        let bc = new content.BroadcastChannel("a");
        bc.onmessage = resolve;
      });

      let src = content.document.createElement("script");
      content.document.body.appendChild(src);
      src.src = obj.scriptURL;

      await p;

      info("Checking if permission is denied before interacting with tracker");
      await assertBlocked();
    }
  );

  await AntiTracking.interactWithTracker();

  info("Loading tracking scripts");
  await SpecialPowers.spawn(
    browser,
    [
      {
        scriptURL: TEST_DOMAIN + TEST_PATH + "tracker.js",
        page: TEST_3RD_PARTY_PAGE,
      },
    ],
    async obj => {
      info("Checking if permission is denied");
      let callbackBlocked = async _ => {
        try {
          localStorage.foo = 42;
          ok(false, "LocalStorage cannot be used!");
        } catch (e) {
          ok(true, "LocalStorage cannot be used!");
          is(e.name, "SecurityError", "We want a security error message.");
        }
      };

      await new content.Promise(resolve => {
        let ifr = content.document.createElement("iframe");
        ifr.onload = function() {
          info("Sending code to the 3rd party content");
          ifr.contentWindow.postMessage(callbackBlocked.toString(), "*");
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

      info("Triggering a 3rd party script...");
      let p = new content.Promise(resolve => {
        let bc = new content.BroadcastChannel("a");
        bc.onmessage = resolve;
      });

      let src = content.document.createElement("script");
      content.document.body.appendChild(src);
      src.src = obj.scriptURL;

      await p;

      info("Checking if permission is granted");
      let callbackAllowed = async _ => {
        localStorage.foo = 42;
        ok(true, "LocalStorage can be used!");
      };

      await new content.Promise(resolve => {
        let ifr = content.document.createElement("iframe");
        ifr.onload = function() {
          info("Sending code to the 3rd party content");
          ifr.contentWindow.postMessage(callbackAllowed.toString(), "*");
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

  info("Removing the tab");
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
