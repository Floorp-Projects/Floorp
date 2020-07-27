function log(test) {
  if ("iteration" in test) {
    info(
      `Running test ${
        test.withStoragePrincipalEnabled
          ? "with storage principal enabled"
          : "without storage principal"
      } with prefValue: ${test.prefValue} (Test #${test.iteration + 1})`
    );
    test.iteration++;
  } else {
    test.iteration = 0;
    log(test);
  }
}

function runAllTests(withStoragePrincipalEnabled, prefValue) {
  const storagePrincipalTest =
    prefValue == Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER;
  const dynamicFPITest =
    prefValue ==
    Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN;

  if (dynamicFPITest && withStoragePrincipalEnabled) {
    // This isn't a meaningful configuration, ignore it.
    return;
  }

  const test = { withStoragePrincipalEnabled, dynamicFPITest, prefValue };

  // For dynamic FPI tests, we want to test the conditions as if
  // storage principal was enabled, so from now on we set this variable to
  // true.
  if (dynamicFPITest) {
    withStoragePrincipalEnabled = true;
  }

  let thirdPartyDomain;
  if (storagePrincipalTest) {
    thirdPartyDomain = TEST_3RD_PARTY_DOMAIN;
  }
  if (dynamicFPITest) {
    thirdPartyDomain = TEST_4TH_PARTY_DOMAIN;
  }
  ok(thirdPartyDomain, "Sanity check");

  let storagePrincipalPrefValue;
  if (storagePrincipalTest) {
    storagePrincipalPrefValue = withStoragePrincipalEnabled;
  }
  if (dynamicFPITest) {
    storagePrincipalPrefValue = false;
  }

  // A same origin (and same-process via setting "dom.ipc.processCount" to 1)
  // top-level window with access to real localStorage does not share storage
  // with an ePartitionOrDeny iframe that should have PartitionedLocalStorage and
  // no storage events are received in either direction.  (Same-process in order
  // to avoid having to worry about any e10s propagation issues.)
  add_task(async _ => {
    log(test);

    await SpecialPowers.pushPrefEnv({
      set: [
        ["dom.ipc.processCount", 1],
        ["network.cookie.cookieBehavior", prefValue],
        ["privacy.trackingprotection.enabled", false],
        ["privacy.trackingprotection.pbmode.enabled", false],
        ["privacy.trackingprotection.annotate_channels", true],
        [
          "privacy.restrict3rdpartystorage.partitionedHosts",
          "tracking.example.org,not-tracking.example.com",
        ],
        [
          "privacy.storagePrincipal.enabledForTrackers",
          storagePrincipalPrefValue,
        ],
      ],
    });

    await UrlClassifierTestUtils.addTestTrackers();

    info("Creating a non-tracker top-level context");
    let normalTab = BrowserTestUtils.addTab(gBrowser, TEST_TOP_PAGE);
    let normalBrowser = gBrowser.getBrowserForTab(normalTab);
    await BrowserTestUtils.browserLoaded(normalBrowser);

    info("Creating a tracker top-level context");
    let trackerTab = BrowserTestUtils.addTab(
      gBrowser,
      thirdPartyDomain + TEST_PATH + "page.html"
    );
    let trackerBrowser = gBrowser.getBrowserForTab(trackerTab);
    await BrowserTestUtils.browserLoaded(trackerBrowser);

    info("The non-tracker page opens a tracker iframe");
    await SpecialPowers.spawn(
      normalBrowser,
      [
        {
          page: thirdPartyDomain + TEST_PATH + "localStorageEvents.html",
        },
      ],
      async obj => {
        let ifr = content.document.createElement("iframe");
        ifr.setAttribute("id", "ifr");
        ifr.setAttribute("src", obj.page);

        info("Iframe loading...");
        await new content.Promise(resolve => {
          ifr.onload = resolve;
          content.document.body.appendChild(ifr);
        });

        info("Setting localStorage value...");
        ifr.contentWindow.postMessage("setValue", "*");

        info("Getting the value...");
        let value = await new Promise(resolve => {
          content.addEventListener(
            "message",
            e => {
              resolve(e.data);
            },
            { once: true }
          );
          ifr.contentWindow.postMessage("getValue", "*");
        });

        ok(
          value.startsWith("tracker-"),
          "The value is correctly set by the tracker"
        );
      }
    );

    info("The tracker page should not have received events");
    await SpecialPowers.spawn(trackerBrowser, [], async _ => {
      is(content.localStorage.foo, undefined, "Undefined value!");
      content.localStorage.foo = "normal-" + Math.random();
    });

    info("Let's see if non-tracker page has received events");
    await SpecialPowers.spawn(normalBrowser, [], async _ => {
      let ifr = content.document.getElementById("ifr");

      info("Getting the value...");
      let value = await new Promise(resolve => {
        content.addEventListener(
          "message",
          e => {
            resolve(e.data);
          },
          { once: true }
        );
        ifr.contentWindow.postMessage("getValue", "*");
      });

      ok(
        value.startsWith("tracker-"),
        "The value is correctly set by the tracker"
      );

      info("Getting the events...");
      let events = await new Promise(resolve => {
        content.addEventListener(
          "message",
          e => {
            resolve(e.data);
          },
          { once: true }
        );
        ifr.contentWindow.postMessage("getEvents", "*");
      });

      is(events, 0, "No events");
    });

    BrowserTestUtils.removeTab(trackerTab);
    BrowserTestUtils.removeTab(normalTab);

    UrlClassifierTestUtils.cleanupTestTrackers();
  });

  // Two ePartitionOrDeny iframes in the same tab in the same origin see
  // the same localStorage values but no storage events are received from each
  // other if storage principal and dFPI are disbled.
  add_task(async _ => {
    log(test);

    await SpecialPowers.pushPrefEnv({
      set: [
        ["dom.ipc.processCount", 1],
        ["network.cookie.cookieBehavior", prefValue],
        ["privacy.trackingprotection.enabled", false],
        ["privacy.trackingprotection.pbmode.enabled", false],
        ["privacy.trackingprotection.annotate_channels", true],
        [
          "privacy.restrict3rdpartystorage.partitionedHosts",
          "tracking.example.org,not-tracking.example.com",
        ],
        [
          "privacy.storagePrincipal.enabledForTrackers",
          storagePrincipalPrefValue,
        ],
      ],
    });

    await UrlClassifierTestUtils.addTestTrackers();

    info("Creating a non-tracker top-level context");
    let normalTab = BrowserTestUtils.addTab(gBrowser, TEST_TOP_PAGE);
    let normalBrowser = gBrowser.getBrowserForTab(normalTab);
    await BrowserTestUtils.browserLoaded(normalBrowser);

    info("The non-tracker page opens a tracker iframe");
    await SpecialPowers.spawn(
      normalBrowser,
      [
        {
          page: thirdPartyDomain + TEST_PATH + "localStorageEvents.html",
          withStoragePrincipalEnabled: test.withStoragePrincipalEnabled,
          dynamicFPITest: test.dynamicFPITest,
        },
      ],
      async obj => {
        let ifr1 = content.document.createElement("iframe");
        ifr1.setAttribute("id", "ifr1");
        ifr1.setAttribute("src", obj.page);

        info("Iframe 1 loading...");
        await new content.Promise(resolve => {
          ifr1.onload = resolve;
          content.document.body.appendChild(ifr1);
        });

        let ifr2 = content.document.createElement("iframe");
        ifr2.setAttribute("id", "ifr2");
        ifr2.setAttribute("src", obj.page);

        info("Iframe 2 loading...");
        await new content.Promise(resolve => {
          ifr2.onload = resolve;
          content.document.body.appendChild(ifr2);
        });

        info("Setting localStorage value in ifr1...");
        ifr1.contentWindow.postMessage("setValue", "*");

        info("Getting the value from ifr1...");
        let value = await new Promise(resolve => {
          content.addEventListener(
            "message",
            e => {
              resolve(e.data);
            },
            { once: true }
          );
          ifr1.contentWindow.postMessage("getValue", "*");
        });

        ok(value.startsWith("tracker-"), "The value is correctly set in ifr1");

        info("Getting the value from ifr2...");
        value = await new Promise(resolve => {
          content.addEventListener(
            "message",
            e => {
              resolve(e.data);
            },
            { once: true }
          );
          ifr2.contentWindow.postMessage("getValue", "*");
        });

        if (obj.withStoragePrincipalEnabled || obj.dynamicFPITest) {
          ok(
            value.startsWith("tracker-"),
            "The value is correctly set in ifr2"
          );
        } else {
          is(value, null, "The value is not set in ifr2");
        }

        info("Getting the events received by ifr2...");
        let events = await new Promise(resolve => {
          content.addEventListener(
            "message",
            e => {
              resolve(e.data);
            },
            { once: true }
          );
          ifr2.contentWindow.postMessage("getEvents", "*");
        });

        if (obj.withStoragePrincipalEnabled || obj.dynamicFPITest) {
          is(events, 1, "one event");
        } else {
          is(events, 0, "No events");
        }
      }
    );

    BrowserTestUtils.removeTab(normalTab);

    UrlClassifierTestUtils.cleanupTestTrackers();
  });

  // Same as the previous test but with a cookie behavior of BEHAVIOR_ACCEPT
  // instead of BEHAVIOR_REJECT_TRACKER so the iframes get real, persistent
  // localStorage instead of partitioned localStorage.
  add_task(async _ => {
    log(test);

    await SpecialPowers.pushPrefEnv({
      set: [
        ["dom.ipc.processCount", 1],
        ["network.cookie.cookieBehavior", Ci.nsICookieService.BEHAVIOR_ACCEPT],
        ["privacy.trackingprotection.enabled", false],
        ["privacy.trackingprotection.pbmode.enabled", false],
        ["privacy.trackingprotection.annotate_channels", true],
        [
          "privacy.restrict3rdpartystorage.partitionedHosts",
          "tracking.example.org,not-tracking.example.com",
        ],
        [
          "privacy.storagePrincipal.enabledForTrackers",
          storagePrincipalPrefValue,
        ],
      ],
    });

    await UrlClassifierTestUtils.addTestTrackers();

    info("Creating a non-tracker top-level context");
    let normalTab = BrowserTestUtils.addTab(gBrowser, TEST_TOP_PAGE);
    let normalBrowser = gBrowser.getBrowserForTab(normalTab);
    await BrowserTestUtils.browserLoaded(normalBrowser);

    info("The non-tracker page opens a tracker iframe");
    await SpecialPowers.spawn(
      normalBrowser,
      [
        {
          page: thirdPartyDomain + TEST_PATH + "localStorageEvents.html",
        },
      ],
      async obj => {
        let ifr1 = content.document.createElement("iframe");
        ifr1.setAttribute("id", "ifr1");
        ifr1.setAttribute("src", obj.page);

        info("Iframe 1 loading...");
        await new content.Promise(resolve => {
          ifr1.onload = resolve;
          content.document.body.appendChild(ifr1);
        });

        let ifr2 = content.document.createElement("iframe");
        ifr2.setAttribute("id", "ifr2");
        ifr2.setAttribute("src", obj.page);

        info("Iframe 2 loading...");
        await new content.Promise(resolve => {
          ifr2.onload = resolve;
          content.document.body.appendChild(ifr2);
        });

        info("Setting localStorage value in ifr1...");
        ifr1.contentWindow.postMessage("setValue", "*");

        info("Getting the value from ifr1...");
        let value1 = await new Promise(resolve => {
          content.addEventListener(
            "message",
            e => {
              resolve(e.data);
            },
            { once: true }
          );
          ifr1.contentWindow.postMessage("getValue", "*");
        });

        ok(value1.startsWith("tracker-"), "The value is correctly set in ifr1");

        info("Getting the value from ifr2...");
        let value2 = await new Promise(resolve => {
          content.addEventListener(
            "message",
            e => {
              resolve(e.data);
            },
            { once: true }
          );
          ifr2.contentWindow.postMessage("getValue", "*");
        });

        is(value2, value1, "The values match");

        info("Getting the events received by ifr2...");
        let events = await new Promise(resolve => {
          content.addEventListener(
            "message",
            e => {
              resolve(e.data);
            },
            { once: true }
          );
          ifr2.contentWindow.postMessage("getEvents", "*");
        });

        is(events, 1, "One event");
      }
    );

    BrowserTestUtils.removeTab(normalTab);

    UrlClassifierTestUtils.cleanupTestTrackers();
  });

  // An ePartitionOrDeny iframe navigated between two distinct pages on the same
  // origin does not see the values stored by the previous iframe.
  add_task(async _ => {
    log(test);

    await SpecialPowers.pushPrefEnv({
      set: [
        ["dom.ipc.processCount", 1],
        ["network.cookie.cookieBehavior", prefValue],
        ["privacy.trackingprotection.enabled", false],
        ["privacy.trackingprotection.pbmode.enabled", false],
        ["privacy.trackingprotection.annotate_channels", true],
        [
          "privacy.restrict3rdpartystorage.partitionedHosts",
          "tracking.example.org,not-tracking.example.com",
        ],
        [
          "privacy.storagePrincipal.enabledForTrackers",
          storagePrincipalPrefValue,
        ],
      ],
    });

    await UrlClassifierTestUtils.addTestTrackers();

    info("Creating a non-tracker top-level context");
    let normalTab = BrowserTestUtils.addTab(gBrowser, TEST_TOP_PAGE);
    let normalBrowser = gBrowser.getBrowserForTab(normalTab);
    await BrowserTestUtils.browserLoaded(normalBrowser);

    info("The non-tracker page opens a tracker iframe");
    await SpecialPowers.spawn(
      normalBrowser,
      [
        {
          page: thirdPartyDomain + TEST_PATH + "localStorageEvents.html",
          withStoragePrincipalEnabled: test.withStoragePrincipalEnabled,
          dynamicFPITest: test.dynamicFPITest,
        },
      ],
      async obj => {
        let ifr = content.document.createElement("iframe");
        ifr.setAttribute("id", "ifr");
        ifr.setAttribute("src", obj.page);

        info("Iframe loading...");
        await new content.Promise(resolve => {
          ifr.onload = resolve;
          content.document.body.appendChild(ifr);
        });

        info("Setting localStorage value in ifr...");
        ifr.contentWindow.postMessage("setValue", "*");

        info("Getting the value from ifr...");
        let value = await new Promise(resolve => {
          content.addEventListener(
            "message",
            e => {
              resolve(e.data);
            },
            { once: true }
          );
          ifr.contentWindow.postMessage("getValue", "*");
        });

        ok(value.startsWith("tracker-"), "The value is correctly set in ifr");

        info("Navigate...");
        await new content.Promise(resolve => {
          ifr.onload = resolve;
          ifr.setAttribute("src", obj.page + "?" + Math.random());
        });

        info("Getting the value from ifr...");
        let value2 = await new Promise(resolve => {
          content.addEventListener(
            "message",
            e => {
              resolve(e.data);
            },
            { once: true }
          );
          ifr.contentWindow.postMessage("getValue", "*");
        });

        if (obj.withStoragePrincipalEnabled || obj.dynamicFPITest) {
          is(value, value2, "The value is received");
        } else {
          is(value2, null, "The value is undefined");
        }
      }
    );

    BrowserTestUtils.removeTab(normalTab);

    UrlClassifierTestUtils.cleanupTestTrackers();
  });

  // Like the previous test, but accepting trackers
  add_task(async _ => {
    log(test);

    await SpecialPowers.pushPrefEnv({
      set: [
        ["dom.ipc.processCount", 1],
        ["network.cookie.cookieBehavior", Ci.nsICookieService.BEHAVIOR_ACCEPT],
        ["privacy.trackingprotection.enabled", false],
        ["privacy.trackingprotection.pbmode.enabled", false],
        ["privacy.trackingprotection.annotate_channels", true],
        [
          "privacy.restrict3rdpartystorage.partitionedHosts",
          "tracking.example.org,not-tracking.example.com",
        ],
        [
          "privacy.storagePrincipal.enabledForTrackers",
          storagePrincipalPrefValue,
        ],
      ],
    });

    await UrlClassifierTestUtils.addTestTrackers();

    info("Creating a non-tracker top-level context");
    let normalTab = BrowserTestUtils.addTab(gBrowser, TEST_TOP_PAGE);
    let normalBrowser = gBrowser.getBrowserForTab(normalTab);
    await BrowserTestUtils.browserLoaded(normalBrowser);

    info("The non-tracker page opens a tracker iframe");
    await SpecialPowers.spawn(
      normalBrowser,
      [
        {
          page: thirdPartyDomain + TEST_PATH + "localStorageEvents.html",
        },
      ],
      async obj => {
        let ifr = content.document.createElement("iframe");
        ifr.setAttribute("id", "ifr");
        ifr.setAttribute("src", obj.page);

        info("Iframe loading...");
        await new content.Promise(resolve => {
          ifr.onload = resolve;
          content.document.body.appendChild(ifr);
        });

        info("Setting localStorage value in ifr...");
        ifr.contentWindow.postMessage("setValue", "*");

        info("Getting the value from ifr...");
        let value = await new Promise(resolve => {
          content.addEventListener(
            "message",
            e => {
              resolve(e.data);
            },
            { once: true }
          );
          ifr.contentWindow.postMessage("getValue", "*");
        });

        ok(value.startsWith("tracker-"), "The value is correctly set in ifr");

        info("Navigate...");
        await new content.Promise(resolve => {
          ifr.onload = resolve;
          ifr.setAttribute("src", obj.page + "?" + Math.random());
        });

        info("Getting the value from ifr...");
        let value2 = await new Promise(resolve => {
          content.addEventListener(
            "message",
            e => {
              resolve(e.data);
            },
            { once: true }
          );
          ifr.contentWindow.postMessage("getValue", "*");
        });

        is(value, value2, "The value is received");
      }
    );

    BrowserTestUtils.removeTab(normalTab);

    UrlClassifierTestUtils.cleanupTestTrackers();
  });

  // An ePartitionOrDeny iframe on the same origin that is navigated to itself
  // via window.location.reload() or equivalent does not see the values stored
  // by its previous self.
  add_task(async _ => {
    log(test);

    await SpecialPowers.pushPrefEnv({
      set: [
        ["dom.ipc.processCount", 1],
        ["network.cookie.cookieBehavior", prefValue],
        ["privacy.trackingprotection.enabled", false],
        ["privacy.trackingprotection.pbmode.enabled", false],
        ["privacy.trackingprotection.annotate_channels", true],
        [
          "privacy.restrict3rdpartystorage.partitionedHosts",
          "tracking.example.org,not-tracking.example.com",
        ],
        [
          "privacy.storagePrincipal.enabledForTrackers",
          storagePrincipalPrefValue,
        ],
      ],
    });

    await UrlClassifierTestUtils.addTestTrackers();

    info("Creating a non-tracker top-level context");
    let normalTab = BrowserTestUtils.addTab(gBrowser, TEST_TOP_PAGE);
    let normalBrowser = gBrowser.getBrowserForTab(normalTab);
    await BrowserTestUtils.browserLoaded(normalBrowser);

    info("The non-tracker page opens a tracker iframe");
    await SpecialPowers.spawn(
      normalBrowser,
      [
        {
          page: thirdPartyDomain + TEST_PATH + "localStorageEvents.html",
          withStoragePrincipalEnabled: test.withStoragePrincipalEnabled,
          dynamicFPITest: test.dynamicFPITest,
        },
      ],
      async obj => {
        let ifr = content.document.createElement("iframe");
        ifr.setAttribute("id", "ifr");
        ifr.setAttribute("src", obj.page);

        info("Iframe loading...");
        await new content.Promise(resolve => {
          ifr.onload = resolve;
          content.document.body.appendChild(ifr);
        });

        info("Setting localStorage value in ifr...");
        ifr.contentWindow.postMessage("setValue", "*");

        info("Getting the value from ifr...");
        let value = await new Promise(resolve => {
          content.addEventListener(
            "message",
            e => {
              resolve(e.data);
            },
            { once: true }
          );
          ifr.contentWindow.postMessage("getValue", "*");
        });

        ok(value.startsWith("tracker-"), "The value is correctly set in ifr");

        info("Reload...");
        await new content.Promise(resolve => {
          ifr.onload = resolve;
          ifr.contentWindow.postMessage("reload", "*");
        });

        info("Getting the value from ifr...");
        let value2 = await new Promise(resolve => {
          content.addEventListener(
            "message",
            e => {
              resolve(e.data);
            },
            { once: true }
          );
          ifr.contentWindow.postMessage("getValue", "*");
        });

        if (obj.withStoragePrincipalEnabled || obj.dynamicFPITest) {
          is(value, value2, "The value is equal");
        } else {
          is(value2, null, "The value is undefined");
        }
      }
    );

    BrowserTestUtils.removeTab(normalTab);

    UrlClassifierTestUtils.cleanupTestTrackers();
  });

  // Like the previous test, but accepting trackers
  add_task(async _ => {
    log(test);

    await SpecialPowers.pushPrefEnv({
      set: [
        ["dom.ipc.processCount", 1],
        ["network.cookie.cookieBehavior", Ci.nsICookieService.BEHAVIOR_ACCEPT],
        ["privacy.trackingprotection.enabled", false],
        ["privacy.trackingprotection.pbmode.enabled", false],
        ["privacy.trackingprotection.annotate_channels", true],
        [
          "privacy.restrict3rdpartystorage.partitionedHosts",
          "tracking.example.org,not-tracking.example.com",
        ],
        [
          "privacy.storagePrincipal.enabledForTrackers",
          storagePrincipalPrefValue,
        ],
      ],
    });

    await UrlClassifierTestUtils.addTestTrackers();

    info("Creating a non-tracker top-level context");
    let normalTab = BrowserTestUtils.addTab(gBrowser, TEST_TOP_PAGE);
    let normalBrowser = gBrowser.getBrowserForTab(normalTab);
    await BrowserTestUtils.browserLoaded(normalBrowser);

    info("The non-tracker page opens a tracker iframe");
    await SpecialPowers.spawn(
      normalBrowser,
      [
        {
          page: thirdPartyDomain + TEST_PATH + "localStorageEvents.html",
        },
      ],
      async obj => {
        let ifr = content.document.createElement("iframe");
        ifr.setAttribute("id", "ifr");
        ifr.setAttribute("src", obj.page);

        info("Iframe loading...");
        await new content.Promise(resolve => {
          ifr.onload = resolve;
          content.document.body.appendChild(ifr);
        });

        info("Setting localStorage value in ifr...");
        ifr.contentWindow.postMessage("setValue", "*");

        info("Getting the value from ifr...");
        let value = await new Promise(resolve => {
          content.addEventListener(
            "message",
            e => {
              resolve(e.data);
            },
            { once: true }
          );
          ifr.contentWindow.postMessage("getValue", "*");
        });

        ok(value.startsWith("tracker-"), "The value is correctly set in ifr");

        info("Reload...");
        await new content.Promise(resolve => {
          ifr.onload = resolve;
          ifr.contentWindow.postMessage("reload", "*");
        });

        info("Getting the value from ifr...");
        let value2 = await new Promise(resolve => {
          content.addEventListener(
            "message",
            e => {
              resolve(e.data);
            },
            { once: true }
          );
          ifr.contentWindow.postMessage("getValue", "*");
        });

        is(value, value2, "The value is equal");
      }
    );

    BrowserTestUtils.removeTab(normalTab);

    UrlClassifierTestUtils.cleanupTestTrackers();
  });

  // An ePartitionOrDeny iframe on different top-level domain tabs
  add_task(async _ => {
    log(test);

    await SpecialPowers.pushPrefEnv({
      set: [
        ["dom.ipc.processCount", 1],
        ["network.cookie.cookieBehavior", prefValue],
        ["privacy.firstparty.isolate", false],
        ["privacy.trackingprotection.enabled", false],
        ["privacy.trackingprotection.pbmode.enabled", false],
        ["privacy.trackingprotection.annotate_channels", true],
        [
          "privacy.restrict3rdpartystorage.partitionedHosts",
          "tracking.example.org,not-tracking.example.com",
        ],
        [
          "privacy.storagePrincipal.enabledForTrackers",
          storagePrincipalPrefValue,
        ],
      ],
    });

    await UrlClassifierTestUtils.addTestTrackers();

    info("Creating a non-tracker top-level context");
    let normalTab = BrowserTestUtils.addTab(gBrowser, TEST_TOP_PAGE);
    let normalBrowser = gBrowser.getBrowserForTab(normalTab);
    await BrowserTestUtils.browserLoaded(normalBrowser);

    info("The non-tracker page opens a tracker iframe");
    let result1 = await SpecialPowers.spawn(
      normalBrowser,
      [
        {
          page: thirdPartyDomain + TEST_PATH + "localStorageEvents.html",
        },
      ],
      async obj => {
        let ifr = content.document.createElement("iframe");
        ifr.setAttribute("id", "ifr");
        ifr.setAttribute("src", obj.page);

        info("Iframe loading...");
        await new content.Promise(resolve => {
          ifr.onload = resolve;
          content.document.body.appendChild(ifr);
        });

        info("Setting localStorage value in ifr...");
        ifr.contentWindow.postMessage("setValue", "*");

        info("Getting the value from ifr...");
        let value = await new Promise(resolve => {
          content.addEventListener(
            "message",
            e => {
              resolve(e.data);
            },
            { once: true }
          );
          ifr.contentWindow.postMessage("getValue", "*");
        });

        ok(value.startsWith("tracker-"), "The value is correctly set in ifr");
        return value;
      }
    );
    ok(result1.startsWith("tracker-"), "The value is correctly set in tab1");

    info("Creating a non-tracker top-level context");
    let normalTab2 = BrowserTestUtils.addTab(gBrowser, TEST_TOP_PAGE_2);
    let normalBrowser2 = gBrowser.getBrowserForTab(normalTab2);
    await BrowserTestUtils.browserLoaded(normalBrowser2);

    info("The non-tracker page opens a tracker iframe");
    let result2 = await SpecialPowers.spawn(
      normalBrowser2,
      [
        {
          page: thirdPartyDomain + TEST_PATH + "localStorageEvents.html",
        },
      ],
      async obj => {
        let ifr = content.document.createElement("iframe");
        ifr.setAttribute("id", "ifr");
        ifr.setAttribute("src", obj.page);

        info("Iframe loading...");
        await new content.Promise(resolve => {
          ifr.onload = resolve;
          content.document.body.appendChild(ifr);
        });

        info("Getting the value from ifr...");
        let value = await new Promise(resolve => {
          content.addEventListener(
            "message",
            e => {
              resolve(e.data);
            },
            { once: true }
          );
          ifr.contentWindow.postMessage("getValue", "*");
        });
        return value;
      }
    );

    ok(!result2, "The value is null");

    BrowserTestUtils.removeTab(normalTab);
    BrowserTestUtils.removeTab(normalTab2);

    UrlClassifierTestUtils.cleanupTestTrackers();
  });

  // Like the previous test, but accepting trackers
  add_task(async _ => {
    log(test);

    await SpecialPowers.pushPrefEnv({
      set: [
        ["dom.ipc.processCount", 1],
        ["network.cookie.cookieBehavior", Ci.nsICookieService.BEHAVIOR_ACCEPT],
        ["privacy.firstparty.isolate", false],
        ["privacy.trackingprotection.enabled", false],
        ["privacy.trackingprotection.pbmode.enabled", false],
        ["privacy.trackingprotection.annotate_channels", true],
        [
          "privacy.restrict3rdpartystorage.partitionedHosts",
          "tracking.example.org,not-tracking.example.com",
        ],
        [
          "privacy.storagePrincipal.enabledForTrackers",
          storagePrincipalPrefValue,
        ],
      ],
    });

    await UrlClassifierTestUtils.addTestTrackers();

    info("Creating a non-tracker top-level context");
    let normalTab = BrowserTestUtils.addTab(gBrowser, TEST_TOP_PAGE);
    let normalBrowser = gBrowser.getBrowserForTab(normalTab);
    await BrowserTestUtils.browserLoaded(normalBrowser);

    info("The non-tracker page opens a tracker iframe");
    let result1 = await SpecialPowers.spawn(
      normalBrowser,
      [
        {
          page: thirdPartyDomain + TEST_PATH + "localStorageEvents.html",
        },
      ],
      async obj => {
        let ifr = content.document.createElement("iframe");
        ifr.setAttribute("id", "ifr");
        ifr.setAttribute("src", obj.page);

        info("Iframe loading...");
        await new content.Promise(resolve => {
          ifr.onload = resolve;
          content.document.body.appendChild(ifr);
        });

        info("Setting localStorage value in ifr...");
        ifr.contentWindow.postMessage("setValue", "*");

        info("Getting the value from ifr...");
        let value = await new Promise(resolve => {
          content.addEventListener(
            "message",
            e => {
              resolve(e.data);
            },
            { once: true }
          );
          ifr.contentWindow.postMessage("getValue", "*");
        });

        ok(value.startsWith("tracker-"), "The value is correctly set in ifr");
        return value;
      }
    );
    ok(result1.startsWith("tracker-"), "The value is correctly set in tab1");

    info("Creating a non-tracker top-level context");
    let normalTab2 = BrowserTestUtils.addTab(gBrowser, TEST_TOP_PAGE_2);
    let normalBrowser2 = gBrowser.getBrowserForTab(normalTab2);
    await BrowserTestUtils.browserLoaded(normalBrowser2);

    info("The non-tracker page opens a tracker iframe");
    let result2 = await SpecialPowers.spawn(
      normalBrowser2,
      [
        {
          page: thirdPartyDomain + TEST_PATH + "localStorageEvents.html",
        },
      ],
      async obj => {
        let ifr = content.document.createElement("iframe");
        ifr.setAttribute("id", "ifr");
        ifr.setAttribute("src", obj.page);

        info("Iframe loading...");
        await new content.Promise(resolve => {
          ifr.onload = resolve;
          content.document.body.appendChild(ifr);
        });

        info("Getting the value from ifr...");
        let value = await new Promise(resolve => {
          content.addEventListener(
            "message",
            e => {
              resolve(e.data);
            },
            { once: true }
          );
          ifr.contentWindow.postMessage("getValue", "*");
        });
        return value;
      }
    );

    is(result1, result2, "The value is undefined");

    BrowserTestUtils.removeTab(normalTab);
    BrowserTestUtils.removeTab(normalTab2);

    UrlClassifierTestUtils.cleanupTestTrackers();
  });

  // Cleanup data.
  add_task(async _ => {
    await new Promise(resolve => {
      Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value =>
        resolve()
      );
    });
  });
}

for (let pref of [
  Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER,
  Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN,
]) {
  runAllTests(false, pref);
  runAllTests(true, pref);
}
