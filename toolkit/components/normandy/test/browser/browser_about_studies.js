"use strict";

const { PreferenceExperiments } = ChromeUtils.import(
  "resource://normandy/lib/PreferenceExperiments.jsm"
);
const { RecipeRunner } = ChromeUtils.import(
  "resource://normandy/lib/RecipeRunner.jsm"
);
const { ExperimentFakes } = ChromeUtils.import(
  "resource://testing-common/NimbusTestUtils.jsm"
);
const { ExperimentManager } = ChromeUtils.import(
  "resource://nimbus/lib/ExperimentManager.jsm"
);
const { RemoteSettingsExperimentLoader } = ChromeUtils.import(
  "resource://nimbus/lib/RemoteSettingsExperimentLoader.jsm"
);
const { PromiseUtils } = ChromeUtils.import(
  "resource://gre/modules/PromiseUtils.jsm"
);
const { NormandyTestUtils } = ChromeUtils.import(
  "resource://testing-common/NormandyTestUtils.jsm"
);
const {
  addonStudyFactory,
  preferenceStudyFactory,
} = NormandyTestUtils.factories;

function withAboutStudies() {
  return function(testFunc) {
    return async args =>
      BrowserTestUtils.withNewTab("about:studies", async browser =>
        testFunc({ ...args, browser })
      );
  };
}

// Test that the code renders at all
decorate_task(withAboutStudies(), async function testAboutStudiesWorks({
  browser,
}) {
  const appFound = await SpecialPowers.spawn(
    browser,
    [],
    () => !!content.document.getElementById("app")
  );
  ok(appFound, "App element was found");
});

// Test that the learn more element is displayed correctly
decorate_task(
  withPrefEnv({
    set: [["app.normandy.shieldLearnMoreUrl", "http://test/%OS%/"]],
  }),
  withAboutStudies(),
  async function testLearnMore({ browser }) {
    SpecialPowers.spawn(browser, [], async () => {
      const doc = content.document;
      await ContentTaskUtils.waitForCondition(() =>
        doc.getElementById("shield-studies-learn-more")
      );
      doc.getElementById("shield-studies-learn-more").click();
    });
    await BrowserTestUtils.waitForLocationChange(gBrowser);

    const location = browser.currentURI.spec;
    is(
      location,
      AboutPages.aboutStudies.getShieldLearnMoreHref(),
      "Clicking Learn More opens the correct page on SUMO."
    );
    ok(!location.includes("%OS%"), "The Learn More URL is formatted.");
  }
);

// Test that jumping to preferences worked as expected
decorate_task(withAboutStudies(), async function testUpdatePreferences({
  browser,
}) {
  let loadPromise = BrowserTestUtils.firstBrowserLoaded(window);

  // We have to use gBrowser instead of browser in most spots since we're
  // dealing with a new tab outside of the about:studies tab.
  const tab = await BrowserTestUtils.switchTab(gBrowser, () => {
    SpecialPowers.spawn(browser, [], async () => {
      const doc = content.document;
      await ContentTaskUtils.waitForCondition(() =>
        doc.getElementById("shield-studies-update-preferences")
      );
      content.document
        .getElementById("shield-studies-update-preferences")
        .click();
    });
  });

  await loadPromise;

  const location = gBrowser.currentURI.spec;
  is(
    location,
    "about:preferences#privacy",
    "Clicking Update Preferences opens the privacy section of the new about:preferences."
  );

  BrowserTestUtils.removeTab(tab);
});

// Test that the study listing shows studies in the proper order and grouping
decorate_task(
  AddonStudies.withStudies([
    addonStudyFactory({
      slug: "fake-study-a",
      userFacingName: "A Fake Add-on Study",
      active: true,
      userFacingDescription: "A fake description",
      studyStartDate: new Date(2018, 0, 4),
    }),
    addonStudyFactory({
      slug: "fake-study-b",
      userFacingName: "B Fake Add-on Study",
      active: false,
      userFacingDescription: "B fake description",
      studyStartDate: new Date(2018, 0, 2),
    }),
    addonStudyFactory({
      slug: "fake-study-c",
      userFacingName: "C Fake Add-on Study",
      active: true,
      userFacingDescription: "C fake description",
      studyStartDate: new Date(2018, 0, 1),
    }),
  ]),
  PreferenceExperiments.withMockExperiments([
    preferenceStudyFactory({
      slug: "fake-study-d",
      userFacingName: null,
      userFacingDescription: null,
      lastSeen: new Date(2018, 0, 3),
      expired: false,
    }),
    preferenceStudyFactory({
      slug: "fake-study-e",
      userFacingName: "E Fake Preference Study",
      lastSeen: new Date(2018, 0, 5),
      expired: true,
    }),
    preferenceStudyFactory({
      slug: "fake-study-f",
      userFacingName: "F Fake Preference Study",
      lastSeen: new Date(2018, 0, 6),
      expired: false,
    }),
  ]),
  withAboutStudies(),
  async function testStudyListing({ addonStudies, prefExperiments, browser }) {
    await SpecialPowers.spawn(
      browser,
      [{ addonStudies, prefExperiments }],
      async ({ addonStudies, prefExperiments }) => {
        const doc = content.document;

        function getStudyRow(docElem, slug) {
          return docElem.querySelector(`.study[data-study-slug="${slug}"]`);
        }

        await ContentTaskUtils.waitForCondition(
          () => doc.querySelectorAll(".active-study-list .study").length
        );
        const activeNames = Array.from(
          doc.querySelectorAll(".active-study-list .study")
        ).map(row => row.dataset.studySlug);
        const inactiveNames = Array.from(
          doc.querySelectorAll(".inactive-study-list .study")
        ).map(row => row.dataset.studySlug);

        Assert.deepEqual(
          activeNames,
          [
            prefExperiments[2].slug,
            addonStudies[0].slug,
            prefExperiments[0].slug,
            addonStudies[2].slug,
          ],
          "Active studies are grouped by enabled status, and sorted by date"
        );
        Assert.deepEqual(
          inactiveNames,
          [prefExperiments[1].slug, addonStudies[1].slug],
          "Inactive studies are grouped by enabled status, and sorted by date"
        );

        const activeAddonStudy = getStudyRow(doc, addonStudies[0].slug);
        ok(
          activeAddonStudy
            .querySelector(".study-description")
            .textContent.includes(addonStudies[0].userFacingDescription),
          "Study descriptions are shown in about:studies."
        );
        is(
          activeAddonStudy.querySelector(".study-status").textContent,
          "Active",
          "Active studies show an 'Active' indicator."
        );
        ok(
          activeAddonStudy.querySelector(".remove-button"),
          "Active studies show a remove button"
        );
        is(
          activeAddonStudy
            .querySelector(".study-icon")
            .textContent.toLowerCase(),
          "a",
          "Study icons use the first letter of the study name."
        );

        const inactiveAddonStudy = getStudyRow(doc, addonStudies[1].slug);
        is(
          inactiveAddonStudy.querySelector(".study-status").textContent,
          "Complete",
          "Inactive studies are marked as complete."
        );
        ok(
          !inactiveAddonStudy.querySelector(".remove-button"),
          "Inactive studies do not show a remove button"
        );

        const activePrefStudy = getStudyRow(doc, prefExperiments[0].slug);
        const preferenceName = Object.keys(prefExperiments[0].preferences)[0];
        ok(
          activePrefStudy
            .querySelector(".study-description")
            .textContent.includes(preferenceName),
          "Preference studies show the preference they are changing"
        );
        is(
          activePrefStudy.querySelector(".study-status").textContent,
          "Active",
          "Active studies show an 'Active' indicator."
        );
        ok(
          activePrefStudy.querySelector(".remove-button"),
          "Active studies show a remove button"
        );

        const inactivePrefStudy = getStudyRow(doc, prefExperiments[1].slug);
        is(
          inactivePrefStudy.querySelector(".study-status").textContent,
          "Complete",
          "Inactive studies are marked as complete."
        );
        ok(
          !inactivePrefStudy.querySelector(".remove-button"),
          "Inactive studies do not show a remove button"
        );

        activeAddonStudy.querySelector(".remove-button").click();
        await ContentTaskUtils.waitForCondition(() =>
          getStudyRow(doc, addonStudies[0].slug).matches(".study.disabled")
        );
        ok(
          getStudyRow(doc, addonStudies[0].slug).matches(".study.disabled"),
          "Clicking the remove button updates the UI to show that the study has been disabled."
        );

        activePrefStudy.querySelector(".remove-button").click();
        await ContentTaskUtils.waitForCondition(() =>
          getStudyRow(doc, prefExperiments[0].slug).matches(".study.disabled")
        );
        ok(
          getStudyRow(doc, prefExperiments[0].slug).matches(".study.disabled"),
          "Clicking the remove button updates the UI to show that the study has been disabled."
        );
      }
    );

    const updatedAddonStudy = await AddonStudies.get(addonStudies[0].recipeId);
    ok(
      !updatedAddonStudy.active,
      "Clicking the remove button marks addon studies as inactive in storage."
    );

    const updatedPrefStudy = await PreferenceExperiments.get(
      prefExperiments[0].slug
    );
    ok(
      updatedPrefStudy.expired,
      "Clicking the remove button marks preference studies as expired in storage."
    );
  }
);

// Test that a message is shown when no studies have been run
decorate_task(
  AddonStudies.withStudies([]),
  withAboutStudies(),
  async function testStudyListingNoStudies({ browser }) {
    await SpecialPowers.spawn(browser, [], async () => {
      const doc = content.document;
      await ContentTaskUtils.waitForCondition(
        () => doc.querySelectorAll(".study-list-info").length
      );
      const studyRows = doc.querySelectorAll(".study-list .study");
      is(studyRows.length, 0, "There should be no studies");
      is(
        doc.querySelector(".study-list-info").textContent,
        "You have not participated in any studies.",
        "A message is shown when no studies exist"
      );
    });
  }
);

// Test that the message shown when studies are disabled and studies exist
decorate_task(
  withAboutStudies(),
  AddonStudies.withStudies([
    addonStudyFactory({
      userFacingName: "A Fake Add-on Study",
      slug: "fake-addon-study",
      active: false,
      userFacingDescription: "A fake description",
      studyStartDate: new Date(2018, 0, 4),
    }),
  ]),
  PreferenceExperiments.withMockExperiments([
    preferenceStudyFactory({
      slug: "fake-pref-study",
      userFacingName: "B Fake Preference Study",
      lastSeen: new Date(2018, 0, 5),
      expired: true,
    }),
  ]),
  async function testStudyListingDisabled({ browser }) {
    try {
      RecipeRunner.disable();

      await SpecialPowers.spawn(browser, [], async () => {
        const doc = content.document;
        await ContentTaskUtils.waitForCondition(() =>
          doc.querySelector(".info-box-content > span")
        );

        is(
          doc.querySelector(".info-box-content > span").textContent,
          "This is a list of studies that you have participated in. No new studies will run.",
          "A message is shown when studies are disabled"
        );
      });
    } finally {
      // reset RecipeRunner.enabled
      RecipeRunner.checkPrefs();
    }
  }
);

// Test for bug 1498940 - detects studies disabled when only study opt-out is set
decorate_task(
  withPrefEnv({
    set: [
      ["datareporting.healthreport.uploadEnabled", true],
      ["app.normandy.api_url", "https://example.com"],
      ["app.shield.optoutstudies.enabled", false],
    ],
  }),
  withAboutStudies(),
  AddonStudies.withStudies([]),
  PreferenceExperiments.withMockExperiments([]),
  async function testStudyListingStudiesOptOut({ browser }) {
    RecipeRunner.checkPrefs();
    ok(
      RecipeRunner.enabled,
      "RecipeRunner should be enabled as a Precondition"
    );

    await SpecialPowers.spawn(browser, [], async () => {
      const doc = content.document;
      await ContentTaskUtils.waitForCondition(() => {
        const span = doc.querySelector(".info-box-content > span");
        return span && span.textContent;
      });

      is(
        doc.querySelector(".info-box-content > span").textContent,
        "This is a list of studies that you have participated in. No new studies will run.",
        "A message is shown when studies are disabled"
      );
    });
  }
);

// Test that clicking remove on a study that was disabled by an outside source
// since the page loaded correctly updates.
decorate_task(
  AddonStudies.withStudies([
    addonStudyFactory({
      slug: "fake-addon-study",
      userFacingName: "Fake Add-on Study",
      active: true,
      userFacingDescription: "A fake description",
      studyStartDate: new Date(2018, 0, 4),
    }),
  ]),
  PreferenceExperiments.withMockExperiments([
    preferenceStudyFactory({
      slug: "fake-pref-study",
      userFacingName: "Fake Preference Study",
      lastSeen: new Date(2018, 0, 3),
      expired: false,
    }),
  ]),
  withAboutStudies(),
  async function testStudyListing({
    addonStudies: [addonStudy],
    prefExperiments: [prefStudy],
    browser,
  }) {
    // The content page has already loaded. Disabling the studies here shouldn't
    // affect it, since it doesn't live-update.
    await AddonStudies.markAsEnded(addonStudy, "disabled-automatically-test");
    await PreferenceExperiments.stop(prefStudy.slug, {
      resetValue: false,
      reason: "disabled-automatically-test",
    });

    await SpecialPowers.spawn(
      browser,
      [{ addonStudy, prefStudy }],
      async ({ addonStudy, prefStudy }) => {
        const doc = content.document;

        function getStudyRow(docElem, slug) {
          return docElem.querySelector(`.study[data-study-slug="${slug}"]`);
        }

        await ContentTaskUtils.waitForCondition(
          () => doc.querySelectorAll(".remove-button").length == 2
        );
        let activeNames = Array.from(
          doc.querySelectorAll(".active-study-list .study")
        ).map(row => row.dataset.studySlug);
        let inactiveNames = Array.from(
          doc.querySelectorAll(".inactive-study-list .study")
        ).map(row => row.dataset.studySlug);

        Assert.deepEqual(
          activeNames,
          [addonStudy.slug, prefStudy.slug],
          "Both studies should be listed as active, even though they have been disabled outside of the page"
        );
        Assert.deepEqual(
          inactiveNames,
          [],
          "No studies should be listed as inactive"
        );

        const activeAddonStudy = getStudyRow(doc, addonStudy.slug);
        const activePrefStudy = getStudyRow(doc, prefStudy.slug);

        activeAddonStudy.querySelector(".remove-button").click();
        await ContentTaskUtils.waitForCondition(() =>
          getStudyRow(doc, addonStudy.slug).matches(".study.disabled")
        );
        ok(
          getStudyRow(doc, addonStudy.slug).matches(".study.disabled"),
          "Clicking the remove button updates the UI to show that the study has been disabled."
        );

        activePrefStudy.querySelector(".remove-button").click();
        await ContentTaskUtils.waitForCondition(() =>
          getStudyRow(doc, prefStudy.slug).matches(".study.disabled")
        );
        ok(
          getStudyRow(doc, prefStudy.slug).matches(".study.disabled"),
          "Clicking the remove button updates the UI to show that the study has been disabled."
        );

        activeNames = Array.from(
          doc.querySelectorAll(".active-study-list .study")
        ).map(row => row.dataset.studySlug);

        Assert.deepEqual(
          activeNames,
          [],
          "No studies should be listed as active"
        );
      }
    );
  }
);

// Test that clicking remove on a study updates even about:studies pages
// that are not currently in focus.
decorate_task(
  AddonStudies.withStudies([
    addonStudyFactory({
      slug: "fake-addon-study",
      userFacingName: "Fake Add-on Study",
      active: true,
      userFacingDescription: "A fake description",
      studyStartDate: new Date(2018, 0, 4),
    }),
  ]),
  PreferenceExperiments.withMockExperiments([
    preferenceStudyFactory({
      slug: "fake-pref-study",
      userFacingName: "Fake Preference Study",
      lastSeen: new Date(2018, 0, 3),
      expired: false,
    }),
  ]),
  withAboutStudies(),
  async function testOtherTabsUpdated({
    addonStudies: [addonStudy],
    prefExperiments: [prefStudy],
    browser,
  }) {
    // Ensure that both our studies are active in the current tab.
    await SpecialPowers.spawn(
      browser,
      [{ addonStudy, prefStudy }],
      async ({ addonStudy, prefStudy }) => {
        const doc = content.document;
        await ContentTaskUtils.waitForCondition(
          () => doc.querySelectorAll(".remove-button").length == 2,
          "waiting for page to load"
        );
        let activeNames = Array.from(
          doc.querySelectorAll(".active-study-list .study")
        ).map(row => row.dataset.studySlug);
        let inactiveNames = Array.from(
          doc.querySelectorAll(".inactive-study-list .study")
        ).map(row => row.dataset.studySlug);

        Assert.deepEqual(
          activeNames,
          [addonStudy.slug, prefStudy.slug],
          "Both studies should be listed as active"
        );
        Assert.deepEqual(
          inactiveNames,
          [],
          "No studies should be listed as inactive"
        );
      }
    );

    // Open a new about:studies tab.
    await BrowserTestUtils.withNewTab("about:studies", async browser => {
      // Delete both studies in this tab; this should pass if previous tests have passed.
      await SpecialPowers.spawn(
        browser,
        [{ addonStudy, prefStudy }],
        async ({ addonStudy, prefStudy }) => {
          const doc = content.document;

          function getStudyRow(docElem, slug) {
            return docElem.querySelector(`.study[data-study-slug="${slug}"]`);
          }

          await ContentTaskUtils.waitForCondition(
            () => doc.querySelectorAll(".remove-button").length == 2,
            "waiting for page to load"
          );
          let activeNames = Array.from(
            doc.querySelectorAll(".active-study-list .study")
          ).map(row => row.dataset.studySlug);
          let inactiveNames = Array.from(
            doc.querySelectorAll(".inactive-study-list .study")
          ).map(row => row.dataset.studySlug);

          Assert.deepEqual(
            activeNames,
            [addonStudy.slug, prefStudy.slug],
            "Both studies should be listed as active in the new tab"
          );
          Assert.deepEqual(
            inactiveNames,
            [],
            "No studies should be listed as inactive in the new tab"
          );

          const activeAddonStudy = getStudyRow(doc, addonStudy.slug);
          const activePrefStudy = getStudyRow(doc, prefStudy.slug);

          activeAddonStudy.querySelector(".remove-button").click();
          await ContentTaskUtils.waitForCondition(() =>
            getStudyRow(doc, addonStudy.slug).matches(".study.disabled")
          );
          ok(
            getStudyRow(doc, addonStudy.slug).matches(".study.disabled"),
            "Clicking the remove button updates the UI in the new tab"
          );

          activePrefStudy.querySelector(".remove-button").click();
          await ContentTaskUtils.waitForCondition(() =>
            getStudyRow(doc, prefStudy.slug).matches(".study.disabled")
          );
          ok(
            getStudyRow(doc, prefStudy.slug).matches(".study.disabled"),
            "Clicking the remove button updates the UI in the new tab"
          );

          activeNames = Array.from(
            doc.querySelectorAll(".active-study-list .study")
          ).map(row => row.dataset.studySlug);

          Assert.deepEqual(
            activeNames,
            [],
            "No studies should be listed as active"
          );
        }
      );
    });

    // Ensure that the original tab has updated correctly.
    await SpecialPowers.spawn(
      browser,
      [{ addonStudy, prefStudy }],
      async ({ addonStudy, prefStudy }) => {
        const doc = content.document;
        await ContentTaskUtils.waitForCondition(
          () => doc.querySelectorAll(".inactive-study-list .study").length == 2,
          "Two studies should load into the inactive list, since they were disabled in a different tab"
        );
        let activeNames = Array.from(
          doc.querySelectorAll(".active-study-list .study")
        ).map(row => row.dataset.studySlug);
        let inactiveNames = Array.from(
          doc.querySelectorAll(".inactive-study-list .study")
        ).map(row => row.dataset.studySlug);
        Assert.deepEqual(
          activeNames,
          [],
          "No studies should be listed as active, since they were disabled in a different tab"
        );
        Assert.deepEqual(
          inactiveNames,
          [addonStudy.slug, prefStudy.slug],
          "Both studies should be listed as inactive, since they were disabled in a different tab"
        );
      }
    );
  }
);

add_task(async function test_nimbus_about_studies() {
  const recipe = ExperimentFakes.recipe("about-studies-foo");
  await ExperimentManager.enroll(recipe);
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:studies" },
    async browser => {
      const name = await SpecialPowers.spawn(browser, [], async () => {
        await ContentTaskUtils.waitForCondition(
          () => content.document.querySelector(".nimbus .remove-button"),
          "waiting for page/experiment to load"
        );
        return content.document.querySelector(".study-name").innerText;
      });
      // Make sure strings are properly shown
      Assert.equal(
        name,
        recipe.userFacingName,
        "Correct active experiment name"
      );
    }
  );
  ExperimentManager.unenroll(recipe.slug);
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:studies" },
    async browser => {
      const name = await SpecialPowers.spawn(browser, [], async () => {
        await ContentTaskUtils.waitForCondition(
          () => content.document.querySelector(".nimbus.disabled"),
          "waiting for experiment to become disabled"
        );
        return content.document.querySelector(".study-name").innerText;
      });
      // Make sure strings are properly shown
      Assert.equal(
        name,
        recipe.userFacingName,
        "Correct disabled experiment name"
      );
    }
  );
  // Cleanup for multiple test runs
  ExperimentManager.store._deleteForTests(recipe.slug);
  Assert.equal(ExperimentManager.store.getAll().length, 0, "Cleanup done");
});

add_task(async function test_nimbus_backwards_compatibility() {
  const recipe = ExperimentFakes.recipe("about-studies-foo");
  await ExperimentManager.enroll({
    experimentType: "messaging_experiment",
    ...recipe,
  });
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:studies" },
    async browser => {
      const name = await SpecialPowers.spawn(browser, [], async () => {
        await ContentTaskUtils.waitForCondition(
          () => content.document.querySelector(".nimbus .remove-button"),
          "waiting for page/experiment to load"
        );
        return content.document.querySelector(".study-name").innerText;
      });
      // Make sure strings are properly shown
      Assert.equal(
        name,
        recipe.userFacingName,
        "Correct active experiment name"
      );
    }
  );
  ExperimentManager.unenroll(recipe.slug);
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:studies" },
    async browser => {
      const name = await SpecialPowers.spawn(browser, [], async () => {
        await ContentTaskUtils.waitForCondition(
          () => content.document.querySelector(".nimbus.disabled"),
          "waiting for experiment to become disabled"
        );
        return content.document.querySelector(".study-name").innerText;
      });
      // Make sure strings are properly shown
      Assert.equal(
        name,
        recipe.userFacingName,
        "Correct disabled experiment name"
      );
    }
  );
  // Cleanup for multiple test runs
  ExperimentManager.store._deleteForTests(recipe.slug);
  Assert.equal(ExperimentManager.store.getAll().length, 0, "Cleanup done");
});

add_task(async function test_getStudiesEnabled() {
  RecipeRunner.initializedPromise = PromiseUtils.defer();
  let promise = AboutPages.aboutStudies.getStudiesEnabled();

  RecipeRunner.initializedPromise.resolve();
  let result = await promise;

  Assert.equal(
    result,
    Services.prefs.getBoolPref("app.shield.optoutstudies.enabled"),
    "about:studies is enabled if the pref is enabled"
  );
});

add_task(async function test_forceEnroll() {
  let sandbox = sinon.createSandbox();
  let stub = sandbox.stub(RemoteSettingsExperimentLoader, "optInToExperiment");

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url:
        "about:studies?optin_collection=collection123&optin_branch=branch123&optin_slug=slug123",
    },
    async browser => {
      await SpecialPowers.spawn(browser, [], async () => {
        await ContentTaskUtils.waitForCondition(
          () => content.document.querySelector(".info-box-content"),
          "Wait for content to load"
        );

        return true;
      });
    }
  );

  Assert.ok(stub.called, "Called optInToExperiment");
  Assert.deepEqual(
    stub.firstCall.args[0],
    { slug: "slug123", branch: "branch123", collection: "collection123" },
    "Called with correct arguments"
  );
  sandbox.restore();
});
