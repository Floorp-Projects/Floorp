/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that by setting a Picture-in-Picture toggle position policy
 * in the sharedData structure, that the toggle position can be
 * change for a particular URI.
 */
add_task(async () => {
  let positionPolicies = [
    TOGGLE_POLICIES.TOP,
    TOGGLE_POLICIES.ONE_QUARTER,
    TOGGLE_POLICIES.MIDDLE,
    TOGGLE_POLICIES.THREE_QUARTERS,
    TOGGLE_POLICIES.BOTTOM,
  ];

  for (let policy of positionPolicies) {
    Services.ppmm.sharedData.set(SHARED_DATA_KEY, {
      "*://example.com/*": { policy },
    });
    Services.ppmm.sharedData.flush();

    let expectations = {
      "with-controls": { canToggle: true, policy },
      "no-controls": { canToggle: true, policy },
    };

    // For <video> elements with controls, the video controls overlap the
    // toggle when its on the bottom and can't be clicked, so we'll ignore
    // that case.
    if (policy == TOGGLE_POLICIES.BOTTOM) {
      expectations["with-controls"] = { canToggle: true };
    }

    await testToggle(TEST_PAGE, expectations);

    // And ensure that other pages aren't affected by this override.
    await testToggle(TEST_PAGE_2, {
      "with-controls": { canToggle: true },
      "no-controls": { canToggle: true },
    });
  }

  Services.ppmm.sharedData.set(SHARED_DATA_KEY, {});
  Services.ppmm.sharedData.flush();
});

/**
 * Tests that by setting a Picture-in-Picture toggle hidden policy
 * in the sharedData structure, that the toggle can be suppressed.
 */
add_task(async () => {
  Services.ppmm.sharedData.set(SHARED_DATA_KEY, {
    "*://example.com/*": { policy: TOGGLE_POLICIES.HIDDEN },
  });
  Services.ppmm.sharedData.flush();

  await testToggle(TEST_PAGE, {
    "with-controls": { canToggle: false, policy: TOGGLE_POLICIES.HIDDEN },
    "no-controls": { canToggle: false, policy: TOGGLE_POLICIES.HIDDEN },
  });

  // And ensure that other pages aren't affected by this override.
  await testToggle(TEST_PAGE_2, {
    "with-controls": { canToggle: true },
    "no-controls": { canToggle: true },
  });

  Services.ppmm.sharedData.set(SHARED_DATA_KEY, {});
  Services.ppmm.sharedData.flush();
});

/**
 * Tests that policies are re-evaluated if the page URI is transitioned
 * via the history API.
 */
add_task(async () => {
  Services.ppmm.sharedData.set(SHARED_DATA_KEY, {
    "*://example.com/*/test-page.html": { policy: TOGGLE_POLICIES.HIDDEN },
  });
  Services.ppmm.sharedData.flush();

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: TEST_PAGE,
    },
    async browser => {
      await ensureVideosReady(browser);
      await SimpleTest.promiseFocus(browser);

      await testToggleHelper(
        browser,
        "no-controls",
        false,
        TOGGLE_POLICIES.HIDDEN
      );

      await SpecialPowers.spawn(browser, [], async function () {
        content.history.pushState({}, "2", "otherpage.html");
      });

      // Since we no longer match the policy URI, we should be able
      // to use the Picture-in-Picture toggle.
      await testToggleHelper(browser, "no-controls", true);

      // Now use the history API to put us back at the original location,
      // which should have the HIDDEN policy re-applied.
      await SpecialPowers.spawn(browser, [], async function () {
        content.history.pushState({}, "Return", "test-page.html");
      });

      await testToggleHelper(
        browser,
        "no-controls",
        false,
        TOGGLE_POLICIES.HIDDEN
      );
    }
  );

  Services.ppmm.sharedData.set(SHARED_DATA_KEY, {});
  Services.ppmm.sharedData.flush();
});
