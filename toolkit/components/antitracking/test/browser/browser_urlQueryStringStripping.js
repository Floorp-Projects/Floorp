/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

requestLongerTimeout(6);

const TEST_THIRD_PARTY_DOMAIN = TEST_DOMAIN_2;
const TEST_THIRD_PARTY_SUB_DOMAIN = "http://sub1.xn--exmple-cua.test/";

const TEST_URI = TEST_DOMAIN + TEST_PATH + "file_stripping.html";
const TEST_THIRD_PARTY_URI =
  TEST_THIRD_PARTY_DOMAIN + TEST_PATH + "file_stripping.html";
const TEST_REDIRECT_URI = TEST_DOMAIN + TEST_PATH + "redirect.sjs";

const TEST_CASES = [
  { testQueryString: "paramToStrip1=123", strippedQueryString: "" },
  {
    testQueryString: "PARAMTOSTRIP1=123&paramToStrip2=456",
    strippedQueryString: "",
  },
  {
    testQueryString: "paramToStrip1=123&paramToKeep=456",
    strippedQueryString: "paramToKeep=456",
  },
  {
    testQueryString: "paramToStrip1=123&paramToKeep=456&paramToStrip2=abc",
    strippedQueryString: "paramToKeep=456",
  },
  {
    testQueryString: "paramToKeep=123",
    strippedQueryString: "paramToKeep=123",
  },
  // Test to make sure we don't encode the unstripped parameters.
  {
    testQueryString: "paramToStrip1=123&paramToKeep=?$!%",
    strippedQueryString: "paramToKeep=?$!%",
  },
];

let listService;

function observeChannel(uri, expected) {
  return TestUtils.topicObserved("http-on-modify-request", (subject, data) => {
    let channel = subject.QueryInterface(Ci.nsIHttpChannel);
    let channelURI = channel.URI;

    if (channelURI.spec.startsWith(uri)) {
      is(
        channelURI.query,
        expected,
        "The loading channel has the expected query string."
      );
      return true;
    }

    return false;
  });
}

async function verifyQueryString(browser, expected) {
  await SpecialPowers.spawn(browser, [expected], expected => {
    // Strip the first question mark.
    let search = content.location.search.slice(1);

    is(search, expected, "The query string is correct.");
  });
}

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["privacy.query_stripping.strip_list", "paramToStrip1 paramToStrip2"],
      ["privacy.query_stripping.redirect", true],
      ["privacy.query_stripping.listService.logLevel", "Debug"],
      ["privacy.query_stripping.strip_on_share.enabled", false],
    ],
  });

  // Get the list service so we can wait for it to be fully initialized before running tests.
  listService = Cc["@mozilla.org/query-stripping-list-service;1"].getService(
    Ci.nsIURLQueryStrippingListService
  );
  // Here we don't care about the actual enabled state, we just want any init to be done so we get reliable starting conditions.
  await listService.testWaitForInit();
});

async function waitForListServiceInit(strippingEnabled) {
  info("Waiting for nsIURLQueryStrippingListService to be initialized.");
  let isInitialized = await listService.testWaitForInit();
  is(
    isInitialized,
    strippingEnabled,
    "nsIURLQueryStrippingListService should be initialized when the feature is enabled."
  );
}

add_task(async function doTestsForTabOpen() {
  info("Start testing query stripping for tab open.");
  for (const strippingEnabled of [false, true]) {
    await SpecialPowers.pushPrefEnv({
      set: [["privacy.query_stripping.enabled", strippingEnabled]],
    });
    await waitForListServiceInit(strippingEnabled);

    for (const test of TEST_CASES) {
      let testURI = TEST_URI + "?" + test.testQueryString;

      let expected = strippingEnabled
        ? test.strippedQueryString
        : test.testQueryString;

      // Observe the channel and check if the query string is expected.
      let networkPromise = observeChannel(TEST_URI, expected);

      // Open a new tab.
      await BrowserTestUtils.withNewTab(testURI, async browser => {
        // Verify if the query string is expected in the new tab.
        await verifyQueryString(browser, expected);
      });

      await networkPromise;
    }

    await SpecialPowers.popPrefEnv();
  }
});

add_task(async function doTestsForWindowOpen() {
  info("Start testing query stripping for window.open().");
  for (const strippingEnabled of [false, true]) {
    await SpecialPowers.pushPrefEnv({
      set: [["privacy.query_stripping.enabled", strippingEnabled]],
    });
    await waitForListServiceInit(strippingEnabled);

    for (const test of TEST_CASES) {
      let testFirstPartyURI = TEST_URI + "?" + test.testQueryString;
      let testThirdPartyURI = TEST_THIRD_PARTY_URI + "?" + test.testQueryString;

      let originalQueryString = test.testQueryString;
      let expectedQueryString = strippingEnabled
        ? test.strippedQueryString
        : test.testQueryString;

      await BrowserTestUtils.withNewTab(TEST_URI, async browser => {
        // Observe the channel and check if the query string is intact when open
        // a same-origin URI.
        let networkPromise = observeChannel(TEST_URI, originalQueryString);

        // Create the promise to wait for the opened tab.
        let newTabPromise = BrowserTestUtils.waitForNewTab(gBrowser, url => {
          return url.startsWith(TEST_URI);
        });

        // Call window.open() to open the same-origin URI where the query string
        // won't be stripped.
        await SpecialPowers.spawn(browser, [testFirstPartyURI], async url => {
          content.postMessage({ type: "window-open", url }, "*");
        });

        await networkPromise;
        let newTab = await newTabPromise;

        // Verify if the query string is expected in the new opened tab.
        await verifyQueryString(newTab.linkedBrowser, originalQueryString);

        BrowserTestUtils.removeTab(newTab);

        // Observe the channel and check if the query string is expected for
        // cross-origin URI.
        networkPromise = observeChannel(
          TEST_THIRD_PARTY_URI,
          expectedQueryString
        );

        newTabPromise = BrowserTestUtils.waitForNewTab(gBrowser, url => {
          return url.startsWith(TEST_THIRD_PARTY_URI);
        });

        // Call window.open() to open the cross-site URI where the query string
        // could be stripped.
        await SpecialPowers.spawn(browser, [testThirdPartyURI], async url => {
          content.postMessage({ type: "window-open", url }, "*");
        });

        await networkPromise;
        newTab = await newTabPromise;

        // Verify if the query string is expected in the new opened tab.
        await verifyQueryString(newTab.linkedBrowser, expectedQueryString);

        BrowserTestUtils.removeTab(newTab);
      });
    }

    await SpecialPowers.popPrefEnv();
  }
});

add_task(async function doTestsForLinkClick() {
  info("Start testing query stripping for link navigation.");
  for (const strippingEnabled of [false, true]) {
    await SpecialPowers.pushPrefEnv({
      set: [["privacy.query_stripping.enabled", strippingEnabled]],
    });
    await waitForListServiceInit(strippingEnabled);

    for (const test of TEST_CASES) {
      let testFirstPartyURI = TEST_URI + "?" + test.testQueryString;
      let testThirdPartyURI = TEST_THIRD_PARTY_URI + "?" + test.testQueryString;

      let originalQueryString = test.testQueryString;
      let expectedQueryString = strippingEnabled
        ? test.strippedQueryString
        : test.testQueryString;

      await BrowserTestUtils.withNewTab(TEST_URI, async browser => {
        // Observe the channel and check if the query string is intact when
        // click a same-origin link.
        let networkPromise = observeChannel(TEST_URI, originalQueryString);

        // Create the promise to wait for the location change.
        let locationChangePromise = BrowserTestUtils.waitForLocationChange(
          gBrowser,
          testFirstPartyURI
        );

        // Create a link and click it to navigate.
        await SpecialPowers.spawn(browser, [testFirstPartyURI], async uri => {
          let link = content.document.createElement("a");
          link.setAttribute("href", uri);
          link.textContent = "Link";
          content.document.body.appendChild(link);
          link.click();
        });

        await networkPromise;
        await locationChangePromise;

        // Verify the query string in the content window.
        await verifyQueryString(browser, originalQueryString);

        // Second, create a link to a cross-origin site to see if the query
        // string is stripped as expected.

        // Observe the channel and check if the query string is expected when
        // click a cross-origin link.
        networkPromise = observeChannel(
          TEST_THIRD_PARTY_URI,
          expectedQueryString
        );

        let targetURI = expectedQueryString
          ? `${TEST_THIRD_PARTY_URI}?${expectedQueryString}`
          : TEST_THIRD_PARTY_URI;
        // Create the promise to wait for the location change.
        locationChangePromise = BrowserTestUtils.waitForLocationChange(
          gBrowser,
          targetURI
        );

        // Create a cross-origin link and click it to navigate.
        await SpecialPowers.spawn(browser, [testThirdPartyURI], async uri => {
          let link = content.document.createElement("a");
          link.setAttribute("href", uri);
          link.textContent = "Link";
          content.document.body.appendChild(link);
          link.click();
        });

        await networkPromise;
        await locationChangePromise;

        // Verify the query string in the content window.
        await verifyQueryString(browser, expectedQueryString);
      });
    }

    await SpecialPowers.popPrefEnv();
  }
});

add_task(async function doTestsForLinkClickInIframe() {
  info("Start testing query stripping for link navigation in iframe.");
  for (const strippingEnabled of [false, true]) {
    await SpecialPowers.pushPrefEnv({
      set: [["privacy.query_stripping.enabled", strippingEnabled]],
    });
    await waitForListServiceInit(strippingEnabled);

    for (const test of TEST_CASES) {
      let testFirstPartyURI = TEST_URI + "?" + test.testQueryString;
      let testThirdPartyURI = TEST_THIRD_PARTY_URI + "?" + test.testQueryString;

      let originalQueryString = test.testQueryString;
      let expectedQueryString = strippingEnabled
        ? test.strippedQueryString
        : test.testQueryString;

      await BrowserTestUtils.withNewTab(TEST_URI, async browser => {
        // Create an iframe and wait until it has been loaded.
        let iframeBC = await SpecialPowers.spawn(
          browser,
          [TEST_URI],
          async url => {
            let frame = content.document.createElement("iframe");
            content.document.body.appendChild(frame);

            await new Promise(done => {
              frame.addEventListener(
                "load",
                function () {
                  done();
                },
                { capture: true, once: true }
              );

              frame.setAttribute("src", url);
            });

            return frame.browsingContext;
          }
        );

        // Observe the channel and check if the query string is intact when
        // click a same-origin link.
        let networkPromise = observeChannel(TEST_URI, originalQueryString);

        // Create the promise to wait for the new tab.
        let newTabPromise = BrowserTestUtils.waitForNewTab(
          gBrowser,
          testFirstPartyURI
        );

        // Create a same-site link which has '_blank' as target in the iframe
        // and click it to navigate.
        await SpecialPowers.spawn(iframeBC, [testFirstPartyURI], async uri => {
          let link = content.document.createElement("a");
          link.setAttribute("href", uri);
          link.setAttribute("target", "_blank");
          link.textContent = "Link";
          content.document.body.appendChild(link);
          link.click();
        });

        await networkPromise;
        let newOpenedTab = await newTabPromise;

        // Verify the query string in the content window.
        await verifyQueryString(
          newOpenedTab.linkedBrowser,
          originalQueryString
        );
        BrowserTestUtils.removeTab(newOpenedTab);

        // Second, create a link to a cross-origin site in the iframe to see if
        // the query string is stripped as expected.

        // Observe the channel and check if the query string is expected when
        // click a cross-origin link.
        networkPromise = observeChannel(
          TEST_THIRD_PARTY_URI,
          expectedQueryString
        );

        let targetURI = expectedQueryString
          ? `${TEST_THIRD_PARTY_URI}?${expectedQueryString}`
          : TEST_THIRD_PARTY_URI;
        // Create the promise to wait for the new tab.
        newTabPromise = BrowserTestUtils.waitForNewTab(gBrowser, targetURI);

        // Create a cross-origin link which has '_blank' as target in the iframe
        // and click it to navigate.
        await SpecialPowers.spawn(iframeBC, [testThirdPartyURI], async uri => {
          let link = content.document.createElement("a");
          link.setAttribute("href", uri);
          link.setAttribute("target", "_blank");
          link.textContent = "Link";
          content.document.body.appendChild(link);
          link.click();
        });

        await networkPromise;
        newOpenedTab = await newTabPromise;

        // Verify the query string in the content window.
        await verifyQueryString(
          newOpenedTab.linkedBrowser,
          expectedQueryString
        );
        BrowserTestUtils.removeTab(newOpenedTab);
      });
    }

    await SpecialPowers.popPrefEnv();
  }
});

add_task(async function doTestsForScriptNavigation() {
  info("Start testing query stripping for script navigation.");
  for (const strippingEnabled of [false, true]) {
    await SpecialPowers.pushPrefEnv({
      set: [["privacy.query_stripping.enabled", strippingEnabled]],
    });
    await waitForListServiceInit(strippingEnabled);

    for (const test of TEST_CASES) {
      let testFirstPartyURI = TEST_URI + "?" + test.testQueryString;
      let testThirdPartyURI = TEST_THIRD_PARTY_URI + "?" + test.testQueryString;

      let originalQueryString = test.testQueryString;
      let expectedQueryString = strippingEnabled
        ? test.strippedQueryString
        : test.testQueryString;

      await BrowserTestUtils.withNewTab(TEST_URI, async browser => {
        // Observe the channel and check if the query string is intact when
        // navigating to a same-origin URI via script.
        let networkPromise = observeChannel(TEST_URI, originalQueryString);

        // Create the promise to wait for the location change.
        let locationChangePromise = BrowserTestUtils.waitForLocationChange(
          gBrowser,
          testFirstPartyURI
        );

        // Trigger the navigation by script.
        await SpecialPowers.spawn(browser, [testFirstPartyURI], async url => {
          content.postMessage({ type: "script", url }, "*");
        });

        await networkPromise;
        await locationChangePromise;

        // Verify the query string in the content window.
        await verifyQueryString(browser, originalQueryString);

        // Second, trigger a cross-origin navigation through script to see if
        // the query string is stripped as expected.

        let targetURI = expectedQueryString
          ? `${TEST_THIRD_PARTY_URI}?${expectedQueryString}`
          : TEST_THIRD_PARTY_URI;

        // Observe the channel and check if the query string is expected.
        networkPromise = observeChannel(
          TEST_THIRD_PARTY_URI,
          expectedQueryString
        );

        locationChangePromise = BrowserTestUtils.waitForLocationChange(
          gBrowser,
          targetURI
        );

        // Trigger the cross-origin navigation by script.
        await SpecialPowers.spawn(browser, [testThirdPartyURI], async url => {
          content.postMessage({ type: "script", url }, "*");
        });

        await networkPromise;
        await locationChangePromise;

        // Verify the query string in the content window.
        await verifyQueryString(browser, expectedQueryString);
      });
    }

    await SpecialPowers.popPrefEnv();
  }
});

add_task(async function doTestsForNoStrippingForIframeNavigation() {
  info("Start testing no query stripping for iframe navigation.");

  for (const strippingEnabled of [false, true]) {
    await SpecialPowers.pushPrefEnv({
      set: [["privacy.query_stripping.enabled", strippingEnabled]],
    });
    await waitForListServiceInit(strippingEnabled);

    for (const test of TEST_CASES) {
      let testFirstPartyURI = TEST_URI + "?" + test.testQueryString;
      let testThirdPartyURI = TEST_THIRD_PARTY_URI + "?" + test.testQueryString;

      // There should be no query stripping for the iframe navigation.
      let originalQueryString = test.testQueryString;
      let expectedQueryString = test.testQueryString;

      await BrowserTestUtils.withNewTab(TEST_URI, async browser => {
        // Create an iframe and wait until it has been loaded.
        let iframeBC = await SpecialPowers.spawn(
          browser,
          [TEST_URI],
          async url => {
            let frame = content.document.createElement("iframe");
            content.document.body.appendChild(frame);

            await new Promise(done => {
              frame.addEventListener(
                "load",
                function () {
                  done();
                },
                { capture: true, once: true }
              );

              frame.setAttribute("src", url);
            });

            return frame.browsingContext;
          }
        );

        // Observe the channel and check if the query string is intact when
        // navigating an iframe.
        let networkPromise = observeChannel(TEST_URI, originalQueryString);

        // Create the promise to wait for the location change.
        let locationChangePromise = BrowserTestUtils.waitForLocationChange(
          gBrowser,
          testFirstPartyURI
        );

        // Trigger the iframe navigation by script.
        await SpecialPowers.spawn(iframeBC, [testFirstPartyURI], async url => {
          content.postMessage({ type: "script", url }, "*");
        });

        await networkPromise;
        await locationChangePromise;

        // Verify the query string in the iframe.
        await verifyQueryString(iframeBC, originalQueryString);

        // Second, trigger a cross-origin navigation through script to see if
        // the query string is still the same.

        let targetURI = expectedQueryString
          ? `${TEST_THIRD_PARTY_URI}?${expectedQueryString}`
          : TEST_THIRD_PARTY_URI;

        // Observe the channel and check if the query string is not stripped.
        networkPromise = observeChannel(
          TEST_THIRD_PARTY_URI,
          expectedQueryString
        );

        locationChangePromise = BrowserTestUtils.waitForLocationChange(
          gBrowser,
          targetURI
        );

        // Trigger the cross-origin iframe navigation by script.
        await SpecialPowers.spawn(iframeBC, [testThirdPartyURI], async url => {
          content.postMessage({ type: "script", url }, "*");
        });

        await networkPromise;
        await locationChangePromise;

        // Verify the query string in the content window.
        await verifyQueryString(iframeBC, expectedQueryString);
      });
    }

    await SpecialPowers.popPrefEnv();
  }
});

add_task(async function doTestsForRedirect() {
  info("Start testing query stripping for redirects.");

  for (const strippingEnabled of [false, true]) {
    await SpecialPowers.pushPrefEnv({
      set: [["privacy.query_stripping.enabled", strippingEnabled]],
    });
    await waitForListServiceInit(strippingEnabled);

    for (const test of TEST_CASES) {
      let testFirstPartyURI =
        TEST_REDIRECT_URI + "?" + TEST_URI + "?" + test.testQueryString;
      let testThirdPartyURI = `${TEST_REDIRECT_URI}?${TEST_THIRD_PARTY_URI}?${test.testQueryString}`;

      let originalQueryString = test.testQueryString;
      let expectedQueryString = strippingEnabled
        ? test.strippedQueryString
        : test.testQueryString;

      await BrowserTestUtils.withNewTab(TEST_URI, async browser => {
        // Observe the channel and check if the query string is intact when
        // redirecting to a same-origin URI .
        let networkPromise = observeChannel(TEST_URI, originalQueryString);

        let targetURI = `${TEST_URI}?${originalQueryString}`;

        // Create the promise to wait for the location change.
        let locationChangePromise = BrowserTestUtils.waitForLocationChange(
          gBrowser,
          targetURI
        );

        // Trigger the redirect.
        await SpecialPowers.spawn(browser, [testFirstPartyURI], async url => {
          content.postMessage({ type: "script", url }, "*");
        });

        await networkPromise;
        await locationChangePromise;

        // Verify the query string in the content window.
        await verifyQueryString(browser, originalQueryString);

        // Second, trigger a redirect to a cross-origin site where the query
        // string should be stripped.

        targetURI = expectedQueryString
          ? `${TEST_THIRD_PARTY_URI}?${expectedQueryString}`
          : TEST_THIRD_PARTY_URI;

        // Observe the channel and check if the query string is expected.
        networkPromise = observeChannel(
          TEST_THIRD_PARTY_URI,
          expectedQueryString
        );

        locationChangePromise = BrowserTestUtils.waitForLocationChange(
          gBrowser,
          targetURI
        );

        // Trigger the cross-origin redirect.
        await SpecialPowers.spawn(browser, [testThirdPartyURI], async url => {
          content.postMessage({ type: "script", url }, "*");
        });

        await networkPromise;
        await locationChangePromise;

        // Verify the query string in the content window.
        await verifyQueryString(browser, expectedQueryString);
      });
    }

    await SpecialPowers.popPrefEnv();
  }
});

add_task(async function doTestForAllowList() {
  info("Start testing query stripping allow list.");

  // Enable the query stripping and set the allow list.
  await SpecialPowers.pushPrefEnv({
    set: [
      ["privacy.query_stripping.enabled", true],
      ["privacy.query_stripping.allow_list", "xn--exmple-cua.test"],
    ],
  });
  await waitForListServiceInit(true);

  const expected = "paramToStrip1=123";

  // Make sure the allow list works for sites, so we will test both the domain
  // and the sub domain.
  for (const domain of [TEST_THIRD_PARTY_DOMAIN, TEST_THIRD_PARTY_SUB_DOMAIN]) {
    let testURI = `${domain}${TEST_PATH}file_stripping.html`;
    let testURIWithQueryString = `${testURI}?${expected}`;

    // 1. Test the allow list for tab open.
    info("Run tab open test.");

    // Observe the channel and check if the query string is not stripped.
    let networkPromise = observeChannel(testURI, expected);

    await BrowserTestUtils.withNewTab(testURIWithQueryString, async browser => {
      // Verify if the query string is not stripped in the new tab.
      await verifyQueryString(browser, expected);
    });

    await networkPromise;

    // 2. Test the allow list for window open
    info("Run window open test.");
    await BrowserTestUtils.withNewTab(TEST_URI, async browser => {
      // Observe the channel and check if the query string is not stripped.
      let networkPromise = observeChannel(testURI, expected);

      let newTabPromise = BrowserTestUtils.waitForNewTab(gBrowser, url => {
        return url.startsWith(testURI);
      });

      await SpecialPowers.spawn(
        browser,
        [testURIWithQueryString],
        async url => {
          content.postMessage({ type: "window-open", url }, "*");
        }
      );

      await networkPromise;
      let newTab = await newTabPromise;

      // Verify if the query string is not stripped in the new opened tab.
      await verifyQueryString(newTab.linkedBrowser, expected);

      BrowserTestUtils.removeTab(newTab);
    });

    // 3. Test the allow list for link click
    info("Run link click test.");
    await BrowserTestUtils.withNewTab(TEST_URI, async browser => {
      // Observe the channel and check if the query string is not stripped.
      let networkPromise = observeChannel(testURI, expected);

      // Create the promise to wait for the location change.
      let locationChangePromise = BrowserTestUtils.waitForLocationChange(
        gBrowser,
        testURIWithQueryString
      );

      await SpecialPowers.spawn(
        browser,
        [testURIWithQueryString],
        async url => {
          let link = content.document.createElement("a");
          link.setAttribute("href", url);
          link.textContent = "Link";
          content.document.body.appendChild(link);
          link.click();
        }
      );

      await networkPromise;
      await locationChangePromise;

      // Verify the query string is not stripped in the content window.
      await verifyQueryString(browser, expected);
    });

    // 4. Test the allow list for clicking link in an iframe.
    info("Run link click in iframe test.");
    await BrowserTestUtils.withNewTab(TEST_URI, async browser => {
      // Create an iframe and wait until it has been loaded.
      let iframeBC = await SpecialPowers.spawn(
        browser,
        [TEST_URI],
        async url => {
          let frame = content.document.createElement("iframe");
          content.document.body.appendChild(frame);

          await new Promise(done => {
            frame.addEventListener(
              "load",
              function () {
                done();
              },
              { capture: true, once: true }
            );

            frame.setAttribute("src", url);
          });

          return frame.browsingContext;
        }
      );

      // Observe the channel and check if the query string is not stripped.
      let networkPromise = observeChannel(testURI, expected);

      // Create the promise to wait for the new tab.
      let newTabPromise = BrowserTestUtils.waitForNewTab(
        gBrowser,
        testURIWithQueryString
      );

      // Create a same-site link which has '_blank' as target in the iframe
      // and click it to navigate.
      await SpecialPowers.spawn(
        iframeBC,
        [testURIWithQueryString],
        async uri => {
          let link = content.document.createElement("a");
          link.setAttribute("href", uri);
          link.setAttribute("target", "_blank");
          link.textContent = "Link";
          content.document.body.appendChild(link);
          link.click();
        }
      );

      await networkPromise;
      let newOpenedTab = await newTabPromise;

      // Verify the query string is not stripped in the content window.
      await verifyQueryString(newOpenedTab.linkedBrowser, expected);
      BrowserTestUtils.removeTab(newOpenedTab);

      // 5. Test the allow list for script navigation.
      info("Run script navigation test.");
      await BrowserTestUtils.withNewTab(TEST_URI, async browser => {
        // Observe the channel and check if the query string is not stripped.
        let networkPromise = observeChannel(testURI, expected);

        // Create the promise to wait for the location change.
        let locationChangePromise = BrowserTestUtils.waitForLocationChange(
          gBrowser,
          testURIWithQueryString
        );

        await SpecialPowers.spawn(
          browser,
          [testURIWithQueryString],
          async url => {
            content.postMessage({ type: "script", url }, "*");
          }
        );

        await networkPromise;
        await locationChangePromise;

        // Verify the query string is not stripped in the content window.
        await verifyQueryString(browser, expected);
      });

      // 6. Test the allow list for redirect.
      info("Run redirect test.");
      await BrowserTestUtils.withNewTab(TEST_URI, async browser => {
        // Observe the channel and check if the query string is not stripped.
        let networkPromise = observeChannel(testURI, expected);

        // Create the promise to wait for the location change.
        let locationChangePromise = BrowserTestUtils.waitForLocationChange(
          gBrowser,
          testURIWithQueryString
        );

        let testRedirectURI = `${TEST_REDIRECT_URI}?${testURI}?${expected}`;

        // Trigger the redirect.
        await SpecialPowers.spawn(browser, [testRedirectURI], async url => {
          content.postMessage({ type: "script", url }, "*");
        });

        await networkPromise;
        await locationChangePromise;

        // Verify the query string in the content window.
        await verifyQueryString(browser, expected);
      });
    });
  }
});
