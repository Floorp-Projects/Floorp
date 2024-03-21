/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  PlacesTestUtils: "resource://testing-common/PlacesTestUtils.sys.mjs",
  TestUtils: "resource://testing-common/TestUtils.sys.mjs",
});

add_task(async function test_basic() {
  // Make sure places visit chains are saved correctly with a redirect
  // transitions.

  // Part 1: observe history events that fire when a visit occurs.
  // Make sure visits appear in order, and that the visit chain is correct.
  const expectedUrls = [
    "http://example.com/tests/toolkit/components/places/tests/browser/begin.html",
    "http://example.com/tests/toolkit/components/places/tests/browser/redirect_twice.sjs",
    "http://example.com/tests/toolkit/components/places/tests/browser/redirect_once.sjs",
    "http://test1.example.com/tests/toolkit/components/places/tests/browser/final.html",
  ];

  let currentIndex = 0;
  let visitUriPromise = lazy.TestUtils.topicObserved(
    "uri-visit-saved",
    async subject => {
      let uri = subject.QueryInterface(Ci.nsIURI);
      let expected = expectedUrls[currentIndex];
      is(uri.spec, expected, "Saved URL visit " + uri.spec);

      let placeId = await lazy.PlacesTestUtils.getDatabaseValue(
        "moz_places",
        "id",
        {
          url: uri.spec,
        }
      );
      let fromVisitId = await lazy.PlacesTestUtils.getDatabaseValue(
        "moz_historyvisits",
        "from_visit",
        {
          place_id: placeId,
        }
      );

      if (currentIndex == 0) {
        is(fromVisitId, 0, "First visit has no from visit");
      } else {
        let lastVisitId = await lazy.PlacesTestUtils.getDatabaseValue(
          "moz_historyvisits",
          "place_id",
          {
            id: fromVisitId,
          }
        );
        let fromVisitUrl = await lazy.PlacesTestUtils.getDatabaseValue(
          "moz_places",
          "url",
          {
            id: lastVisitId,
          }
        );
        is(
          fromVisitUrl,
          expectedUrls[currentIndex - 1],
          "From visit was " + expectedUrls[currentIndex - 1]
        );
      }

      currentIndex++;
      return currentIndex >= expectedUrls.length;
    }
  );

  const testUrl =
    "http://example.com/tests/toolkit/components/places/tests/browser/begin.html";
  await BrowserTestUtils.openNewForegroundTab(gBrowser, testUrl);

  // Load begin page, click link on page to record visits.
  await BrowserTestUtils.synthesizeMouseAtCenter(
    "#clickme",
    {},
    gBrowser.selectedBrowser
  );
  await visitUriPromise;

  // Clean up.
  await PlacesUtils.history.clear();
  gBrowser.removeCurrentTab();
});

add_task(async function test_userpass() {
  // Avoid showing the auth prompt.
  await SpecialPowers.pushPrefEnv({
    set: [["network.auth.confirmAuth.enabled", false]],
  });

  // Open a html having test links.
  await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "https://example.org/tests/toolkit/components/places/tests/browser/userpass.html"
  );

  const clickedUrl =
    "https://user:pass@example.org/tests/toolkit/components/places/tests/browser/favicon.html";
  const exposablePageUrl =
    "https://example.org/tests/toolkit/components/places/tests/browser/favicon.html";

  let visitUriPromise = lazy.TestUtils.topicObserved(
    "uri-visit-saved",
    async subject => {
      let uri = subject.QueryInterface(Ci.nsIURI);
      if (uri.spec !== exposablePageUrl) {
        return false;
      }

      let placeForExposable = await lazy.PlacesTestUtils.getDatabaseValue(
        "moz_places",
        "id",
        {
          url: exposablePageUrl,
        }
      );
      Assert.ok(placeForExposable, "Found the place for exposable URL");

      let placeForOriginal = await lazy.PlacesTestUtils.getDatabaseValue(
        "moz_places",
        "id",
        {
          url: clickedUrl,
        }
      );
      Assert.ok(!placeForOriginal, "Not found the place for the original URL");

      return true;
    }
  );

  // Open the target link as background.
  await ContentTask.spawn(gBrowser.selectedBrowser, null, async () => {
    let link = content.document.getElementById("target-userpass");
    EventUtils.synthesizeMouseAtCenter(
      link,
      {
        ctrlKey: true,
        metaKey: true,
      },
      content
    );
    return link.href;
  });

  // Wait for fireing visited event.
  await visitUriPromise;

  // Check the title.
  await BrowserTestUtils.waitForCondition(async () => {
    let titleForExposable = await lazy.PlacesTestUtils.getDatabaseValue(
      "moz_places",
      "title",
      {
        url: exposablePageUrl,
      }
    );
    return titleForExposable == "favicon page";
  }, "Wait for the proper title is updated");

  // Check the link status.
  const expectedResults = {
    "target-userpass": true,
    "no-userpass": true,
    "another-userpass": false,
    "another-url": false,
  };

  for (const [key, value] of Object.entries(expectedResults)) {
    await ContentTask.spawn(
      gBrowser.selectedBrowser,
      [key, value],
      async ([k, v]) => {
        // ElementState::VISITED
        const VISITED_STATE = 1 << 18;
        await ContentTaskUtils.waitForCondition(() => {
          const isVisited = !!(
            content.InspectorUtils.getContentState(
              content.document.getElementById(k)
            ) & VISITED_STATE
          );
          return isVisited == v;
        });
      }
    );
    Assert.ok(true, `The status of ${key} is correct`);
  }

  // Clean up.
  await PlacesUtils.history.clear();
  // Remove the tab for userpass.html
  gBrowser.removeCurrentTab();
  // Remove the tab for favicon.html
  gBrowser.removeCurrentTab();
});
