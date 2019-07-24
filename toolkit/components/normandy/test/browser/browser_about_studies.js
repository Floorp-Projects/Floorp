"use strict";

ChromeUtils.import("resource://normandy/lib/AddonStudies.jsm", this);
ChromeUtils.import("resource://normandy/lib/PreferenceExperiments.jsm", this);
ChromeUtils.import("resource://normandy/lib/RecipeRunner.jsm", this);
ChromeUtils.import("resource://normandy-content/AboutPages.jsm", this);

const { NormandyTestUtils } = ChromeUtils.import(
  "resource://testing-common/NormandyTestUtils.jsm"
);
const {
  addonStudyFactory,
  preferenceStudyFactory,
} = NormandyTestUtils.factories;

function withAboutStudies(testFunc) {
  return async (...args) =>
    BrowserTestUtils.withNewTab("about:studies", async browser =>
      testFunc(...args, browser)
    );
}

// Test that the code renders at all
decorate_task(withAboutStudies, async function testAboutStudiesWorks(browser) {
  const appFound = await ContentTask.spawn(browser, null, () =>
    content.document.getElementById("app")
  );
  ok(appFound, "App element was found");
});

// Test that the learn more element is displayed correctly
decorate_task(
  withPrefEnv({
    set: [["app.normandy.shieldLearnMoreUrl", "http://test/%OS%/"]],
  }),
  withAboutStudies,
  async function testLearnMore(browser) {
    ContentTask.spawn(browser, null, async () => {
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
decorate_task(withAboutStudies, async function testUpdatePreferences(browser) {
  let loadPromise = BrowserTestUtils.firstBrowserLoaded(window);

  // We have to use gBrowser instead of browser in most spots since we're
  // dealing with a new tab outside of the about:studies tab.
  const tab = await BrowserTestUtils.switchTab(gBrowser, () => {
    ContentTask.spawn(browser, null, async () => {
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
      name: "D Fake Preference Study",
      lastSeen: new Date(2018, 0, 3),
      expired: false,
    }),
    preferenceStudyFactory({
      name: "E Fake Preference Study",
      lastSeen: new Date(2018, 0, 5),
      expired: true,
    }),
    preferenceStudyFactory({
      name: "F Fake Preference Study",
      lastSeen: new Date(2018, 0, 6),
      expired: false,
    }),
  ]),
  withAboutStudies,
  async function testStudyListing(addonStudies, prefStudies, browser) {
    await ContentTask.spawn(
      browser,
      { addonStudies, prefStudies },
      async ({ addonStudies, prefStudies }) => {
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
            prefStudies[2].name,
            addonStudies[0].slug,
            prefStudies[0].name,
            addonStudies[2].slug,
          ],
          "Active studies are grouped by enabled status, and sorted by date"
        );
        Assert.deepEqual(
          inactiveNames,
          [prefStudies[1].name, addonStudies[1].slug],
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

        const activePrefStudy = getStudyRow(doc, prefStudies[0].name);
        const preferenceName = Object.keys(prefStudies[0].preferences)[0];
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
        is(
          activePrefStudy
            .querySelector(".study-icon")
            .textContent.toLowerCase(),
          "d",
          "Study icons use the first letter of the study name."
        );

        const inactivePrefStudy = getStudyRow(doc, prefStudies[1].name);
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
          getStudyRow(doc, prefStudies[0].name).matches(".study.disabled")
        );
        ok(
          getStudyRow(doc, prefStudies[0].name).matches(".study.disabled"),
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
      prefStudies[0].name
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
  withAboutStudies,
  async function testStudyListingNoStudies(studies, browser) {
    await ContentTask.spawn(browser, null, async () => {
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
  withAboutStudies,
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
      name: "B Fake Preference Study",
      lastSeen: new Date(2018, 0, 5),
      expired: true,
    }),
  ]),
  async function testStudyListingDisabled(
    browser,
    addonStudies,
    preferenceStudies
  ) {
    try {
      RecipeRunner.disable();

      await ContentTask.spawn(browser, null, async () => {
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
  withAboutStudies,
  AddonStudies.withStudies([]),
  PreferenceExperiments.withMockExperiments([]),
  async function testStudyListingStudiesOptOut(browser) {
    RecipeRunner.checkPrefs();
    ok(
      RecipeRunner.enabled,
      "RecipeRunner should be enabled as a Precondition"
    );

    await ContentTask.spawn(browser, null, async () => {
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
      name: "Fake Preference Study",
      lastSeen: new Date(2018, 0, 3),
      expired: false,
    }),
  ]),
  withAboutStudies,
  async function testStudyListing([addonStudy], [prefStudy], browser) {
    // The content page has already loaded. Disabling the studies here shouldn't
    // affect it, since it doesn't live-update.
    await AddonStudies.markAsEnded(addonStudy, "disabled-automatically-test");
    await PreferenceExperiments.stop(prefStudy.name, {
      resetValue: false,
      reason: "disabled-automatically-test",
    });

    await ContentTask.spawn(
      browser,
      { addonStudy, prefStudy },
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
          [addonStudy.slug, prefStudy.name],
          "Both studies should be listed as active, even though they have been disabled outside of the page"
        );
        Assert.deepEqual(
          inactiveNames,
          [],
          "No studies should be listed as inactive"
        );

        const activeAddonStudy = getStudyRow(doc, addonStudy.slug);
        const activePrefStudy = getStudyRow(doc, prefStudy.name);

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
          getStudyRow(doc, prefStudy.name).matches(".study.disabled")
        );
        ok(
          getStudyRow(doc, prefStudy.name).matches(".study.disabled"),
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
