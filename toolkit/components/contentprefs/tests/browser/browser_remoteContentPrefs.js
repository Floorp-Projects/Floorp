"use strict";

const childFrameURL =
  "data:text/html,<!DOCTYPE HTML><html><body></body></html>";

async function runTestsForFrame(browser, isPrivate) {
  let loadContext = Cu.createLoadContext();
  let privateLoadContext = Cu.createPrivateLoadContext();

  await SpecialPowers.spawn(browser, [], async () => {
    var cps = Cc["@mozilla.org/content-pref/service;1"].getService(
      Ci.nsIContentPrefService2
    );
    Assert.ok(cps !== null, "got the content pref service");

    await new Promise(resolve => {
      cps.setGlobal("testing", 42, null, {
        handleCompletion(reason) {
          Assert.equal(reason, 0, "set a pref?");
          resolve();
        },
      });
    });

    let numResults = 0;
    await new Promise(resolve => {
      cps.getGlobal("testing", null, {
        handleResult(pref) {
          numResults++;
          Assert.equal(pref.name, "testing", "pref has the right name");
          Assert.equal(pref.value, 42, "pref has the right value");
        },

        handleCompletion(reason) {
          Assert.equal(reason, 0, "get a pref?");
          Assert.equal(numResults, 1, "got the right number of prefs");
          resolve();
        },
      });
    });
  });

  await SpecialPowers.spawn(browser, [isPrivate], async isFramePrivate => {
    var cps = Cc["@mozilla.org/content-pref/service;1"].getService(
      Ci.nsIContentPrefService2
    );

    if (!content._result) {
      content._result = [];
    }

    let observer;
    cps.addObserverForName(
      "testName",
      (observer = {
        onContentPrefSet(group, name, value, isPrivate) {
          Assert.equal(group, null, "group should be null");
          Assert.equal(name, "testName", "should only see testName");
          Assert.equal(value, 42, "value should be correct");
          Assert.equal(isPrivate, isFramePrivate, "privacy should match");

          content._result.push("set");
        },

        onContentPrefRemoved(group, name, isPrivate) {
          Assert.equal(group, null, "group should be null");
          Assert.equal(name, "testName", "name should match");
          Assert.equal(isPrivate, isFramePrivate, "privacy should match");

          content._result.push("removed");
        },
      })
    );
    content._observer = observer;
  });

  for (let expectedResponse of ["set", "removed"]) {
    var cps = Cc["@mozilla.org/content-pref/service;1"].getService(
      Ci.nsIContentPrefService2
    );

    if (expectedResponse == "set") {
      cps.setGlobal(
        "testName",
        42,
        isPrivate ? privateLoadContext : loadContext
      );
    } else {
      cps.removeGlobal(
        "testName",
        isPrivate ? privateLoadContext : loadContext
      );
    }

    let response = await SpecialPowers.spawn(
      browser,
      [expectedResponse],
      async expected => {
        var cps = Cc["@mozilla.org/content-pref/service;1"].getService(
          Ci.nsIContentPrefService2
        );
        await ContentTaskUtils.waitForCondition(
          () => content._result.length == 1,
          "expecting one notification"
        );

        if (expected == "removed") {
          cps.removeObserverForName("testName", content._observer);
        }

        Assert.equal(
          content._result.length,
          1,
          "correct number of notifications"
        );
        let result = content._result[0];
        content._result = [];
        return result;
      }
    );

    is(response, expectedResponse, "got correct observer notification");
  }

  await SpecialPowers.spawn(browser, [], async () => {
    var cps = Cc["@mozilla.org/content-pref/service;1"].getService(
      Ci.nsIContentPrefService2
    );

    await new Promise(resolve => {
      cps.setGlobal("testName", 42, null, {
        handleCompletion(reason) {
          Assert.equal(reason, 0, "set a pref");
          cps.set("http://mochi.test", "testpref", "str", null, {
            /* eslint no-shadow: 0 */
            handleCompletion(reason) {
              Assert.equal(reason, 0, "set a pref");
              resolve();
            },
          });
        },
      });
    });

    await new Promise(resolve => {
      cps.removeByDomain("http://mochi.test", null, {
        handleCompletion(reason) {
          Assert.equal(reason, 0, "remove succeeded");
          cps.getByDomainAndName("http://mochi.test", "testpref", null, {
            handleResult() {
              throw new Error("got removed pref in test3");
            },
            handleCompletion() {
              resolve();
            },
            handleError(rv) {
              throw new Error(`got a pref error ${rv}`);
            },
          });
        },
      });
    });
  });

  await SpecialPowers.spawn(browser, [], async () => {
    var cps = Cc["@mozilla.org/content-pref/service;1"].getService(
      Ci.nsIContentPrefService2
    );

    let event = await new Promise((resolve, reject) => {
      let prefObserver = {
        onContentPrefSet(group, name, value, isPrivate) {
          resolve({ group, name, value, isPrivate });
        },
        onContentPrefRemoved(group, name, isPrivate) {
          reject("got unexpected notification");
        },
      };

      cps.addObserverForName("test", prefObserver);

      let privateLoadContext = Cu.createPrivateLoadContext();
      cps.set("http://mochi.test", "test", 42, privateLoadContext);

      cps.addObserverForName("test", prefObserver);
    });

    Assert.equal(event.name, "test", "got the right event");
    Assert.equal(event.isPrivate, true, "the event was for an isPrivate pref");
  });

  await SpecialPowers.spawn(browser, [], async () => {
    var cps = Cc["@mozilla.org/content-pref/service;1"].getService(
      Ci.nsIContentPrefService2
    );

    let results = [];
    await new Promise(resolve => {
      cps.getByDomainAndName("http://mochi.test", "test", null, {
        handleResult(pref) {
          info("received handleResult");
          results.push(pref);
        },
        handleCompletion(reason) {
          resolve();
        },
        handleError(rv) {
          ok(false, `failed to get pref ${rv}`);
          throw new Error("got unexpected error");
        },
      });
    });

    Assert.equal(results.length, 0, "should not have seen the pb pref");
  });
}

async function runTest(isPrivate) {
  /* globals Services */
  info("testing with isPrivate=" + isPrivate);

  let newWindow = await BrowserTestUtils.openNewBrowserWindow({
    private: isPrivate,
  });

  const system = Services.scriptSecurityManager.getSystemPrincipal();

  let browser = newWindow.gBrowser.selectedBrowser;
  let loadedPromise = BrowserTestUtils.browserLoaded(browser);
  browser.loadURI(childFrameURL, { triggeringPrincipal: system });
  await loadedPromise;

  await runTestsForFrame(browser, isPrivate);

  newWindow.close();
}

add_task(async function() {
  await runTest(false);
  await runTest(true);
});
