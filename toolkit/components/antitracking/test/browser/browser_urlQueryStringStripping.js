/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from head.js */

"use strict";

requestLongerTimeout(6);

const TEST_URI = TEST_DOMAIN + TEST_PATH + "file_stripping.html";
const TEST_THIRD_PARTY_URI = TEST_DOMAIN_2 + TEST_PATH + "file_stripping.html";

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
];

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

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["privacy.query_stripping.strip_list", "paramToStrip1 paramToStrip2"],
    ],
  });
});

add_task(async function doTestsForTabOpen() {
  info("Start testing query stripping for tab open.");
  for (const strippingEnabled of [false, true]) {
    await SpecialPowers.pushPrefEnv({
      set: [["privacy.query_stripping.enabled", strippingEnabled]],
    });

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
  }
});

add_task(async function doTestsForWindowOpen() {
  info("Start testing query stripping for window.open().");
  for (const strippingEnabled of [false, true]) {
    await SpecialPowers.pushPrefEnv({
      set: [["privacy.query_stripping.enabled", strippingEnabled]],
    });

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
  }
});

add_task(async function doTestsForLinkClick() {
  info("Start testing query stripping for link navigation.");
  for (const strippingEnabled of [false, true]) {
    await SpecialPowers.pushPrefEnv({
      set: [["privacy.query_stripping.enabled", strippingEnabled]],
    });

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
  }
});

add_task(async function doTestsForLinkClickInIframe() {
  info("Start testing query stripping for link navigation in iframe.");
  for (const strippingEnabled of [false, true]) {
    await SpecialPowers.pushPrefEnv({
      set: [["privacy.query_stripping.enabled", strippingEnabled]],
    });

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
                function() {
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
  }
});

add_task(async function doTestsForScriptNavigation() {
  info("Start testing query stripping for script navigation.");
  for (const strippingEnabled of [false, true]) {
    await SpecialPowers.pushPrefEnv({
      set: [["privacy.query_stripping.enabled", strippingEnabled]],
    });

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
  }
});

add_task(async function doTestsForNoStrippingForIframeNavigation() {
  info("Start testing no query stripping for iframe navigation.");

  for (const strippingEnabled of [false, true]) {
    await SpecialPowers.pushPrefEnv({
      set: [["privacy.query_stripping.enabled", strippingEnabled]],
    });

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
                function() {
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
  }
});
