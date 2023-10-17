/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests AddonRepository.jsm

var gServer = createHttpServer({ hosts: ["example.com"] });

const PREF_GETADDONS_BROWSEADDONS = "extensions.getAddons.browseAddons";
const PREF_GETADDONS_BROWSESEARCHRESULTS =
  "extensions.getAddons.search.browseURL";
const PREF_GET_BROWSER_MAPPINGS = "extensions.getAddons.browserMappings.url";

const BASE_URL = "http://example.com";
const DEFAULT_URL = "about:blank";

const ADDONS = [
  {
    manifest: {
      name: "XPI Add-on 1",
      version: "1.1",
      browser_specific_settings: {
        gecko: { id: "test_AddonRepository_1@tests.mozilla.org" },
      },
    },
  },
  {
    manifest: {
      name: "XPI Add-on 2",
      version: "1.2",
      theme: {},
      browser_specific_settings: {
        gecko: { id: "test_AddonRepository_2@tests.mozilla.org" },
      },
    },
  },
  {
    manifest: {
      name: "XPI Add-on 3",
      version: "1.3",
      theme: {},
      browser_specific_settings: {
        gecko: { id: "test_AddonRepository_3@tests.mozilla.org" },
      },
    },
  },
];

// Path to source URI of installing add-on
const INSTALL_URL2 = "/addons/test_AddonRepository_2.xpi";
// Path to source URI of non-active add-on (state = STATE_AVAILABLE)
const INSTALL_URL3 = "/addons/test_AddonRepository_3.xpi";

// Properties of an individual add-on that should be checked
// Note: name is checked separately
var ADDON_PROPERTIES = [
  "id",
  "type",
  "version",
  "creator",
  "developers",
  "description",
  "fullDescription",
  "iconURL",
  "icons",
  "screenshots",
  "supportURL",
  "contributionURL",
  "averageRating",
  "reviewCount",
  "reviewURL",
  "weeklyDownloads",
  "dailyUsers",
  "sourceURI",
  "updateDate",
  "amoListingURL",
];

// Results of getAddonsByIDs
var GET_RESULTS = [
  {
    id: "test1@tests.mozilla.org",
    type: "extension",
    version: "1.1",
    creator: {
      name: "Test Creator 1",
      url: BASE_URL + "/creator1.html",
    },
    developers: [
      {
        name: "Test Developer 1",
        url: BASE_URL + "/developer1.html",
      },
    ],
    description: "Test Summary 1",
    fullDescription: "Test Description 1",
    iconURL: BASE_URL + "/icon1.png",
    icons: { 32: BASE_URL + "/icon1.png" },
    screenshots: [
      {
        url: BASE_URL + "/full1-1.png",
        width: 400,
        height: 300,
        thumbnailURL: BASE_URL + "/thumbnail1-1.png",
        thumbnailWidth: 200,
        thumbnailHeight: 150,
        caption: "Caption 1 - 1",
      },
      {
        url: BASE_URL + "/full2-1.png",
        thumbnailURL: BASE_URL + "/thumbnail2-1.png",
        caption: "Caption 2 - 1",
      },
    ],
    supportURL: BASE_URL + "/support1.html",
    contributionURL: BASE_URL + "/contribution1.html",
    averageRating: 4,
    reviewCount: 1111,
    reviewURL: BASE_URL + "/review1.html",
    weeklyDownloads: 3333,
    sourceURI: BASE_URL + INSTALL_URL2,
    updateDate: new Date(1265033045000),
    amoListingURL:
      "https://addons.mozilla.org/en-US/firefox/addon/test1@tests.mozilla.org/",
  },
  {
    id: "test2@tests.mozilla.org",
    type: "extension",
    version: "2.0",
    icons: {},
    sourceURI: "http://example.com/addons/bleah.xpi",
  },
  {
    id: "test_AddonRepository_1@tests.mozilla.org",
    type: "theme",
    version: "1.4",
    icons: {},
  },
];

// Values for testing AddonRepository.getAddonsByIDs()
var GET_TEST = {
  preference: PREF_GETADDONS_BYIDS,
  preferenceValue: BASE_URL + "/%OS%/%VERSION%/%IDS%",
  failedIDs: ["test1@tests.mozilla.org"],
  failedURL: "/XPCShell/1/test1%40tests.mozilla.org",
  successfulIDs: [
    "test1@tests.mozilla.org",
    "test2@tests.mozilla.org",
    "{00000000-1111-2222-3333-444444444444}",
    "test_AddonRepository_1@tests.mozilla.org",
  ],
  successfulURL:
    "/XPCShell/1/test1%40tests.mozilla.org%2C" +
    "test2%40tests.mozilla.org%2C" +
    "%7B00000000-1111-2222-3333-444444444444%7D%2C" +
    "test_AddonRepository_1%40tests.mozilla.org",
  successfulRTAURL:
    "/XPCShell/1/rta%3AdGVzdDFAdGVzdHMubW96aWxsYS5vcmc%2C" +
    "test2%40tests.mozilla.org%2C" +
    "%7B00000000-1111-2222-3333-444444444444%7D%2C" +
    "test_AddonRepository_1%40tests.mozilla.org",
};

const GET_BROWSER_MAPPINGS_URL = `${BASE_URL}/browser-mappings/%BROWSER%`;

// Test that actual results and expected results are equal
function check_results(aActualAddons, aExpectedAddons) {
  do_check_addons(aActualAddons, aExpectedAddons, ADDON_PROPERTIES);

  // Additional tests
  aActualAddons.forEach(function check_each_addon(aActualAddon) {
    // Separately check name so better messages are output when test fails
    if (aActualAddon.name == "FAIL") {
      do_throw(aActualAddon.id + " - " + aActualAddon.description);
    }
    if (aActualAddon.name != "PASS") {
      do_throw(aActualAddon.id + " - invalid add-on name " + aActualAddon.name);
    }
  });
}

add_task(async function setup() {
  // Setup for test
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9");

  let xpis = ADDONS.map(addon => createTempWebExtensionFile(addon));

  // Register other add-on XPI files
  gServer.registerFile(INSTALL_URL2, xpis[1]);
  gServer.registerFile(INSTALL_URL3, xpis[2]);

  // Register files used to test search failure
  gServer.registerFile(
    GET_TEST.failedURL,
    do_get_file("data/test_AddonRepository_fail.json")
  );

  // Register files used to test search success
  gServer.registerFile(
    GET_TEST.successfulURL,
    do_get_file("data/test_AddonRepository_getAddonsByIDs.json")
  );
  // Register file for RTA test
  gServer.registerFile(
    GET_TEST.successfulRTAURL,
    do_get_file("data/test_AddonRepository_getAddonsByIDs.json")
  );

  // Register some files/handlers for browser mapping tests.
  gServer.registerFile(
    // Keep in sync with `GET_BROWSER_MAPPINGS_URL`.
    "/browser-mappings/valid-browser-id",
    do_get_file("data/test_AddonRepository_getMappedAddons.json")
  );
  gServer.registerFile(
    // This is used in `test_getMappedAddons_empty_mapping()` and should be
    // updated if `GET_BROWSER_MAPPINGS_URL` is also updated.
    "/browser-mappings/browser-id-empty-results",
    do_get_file("data/test_AddonRepository_getMappedAddons_empty.json")
  );
  gServer.registerPrefixHandler(
    // Keep in sync with the pref set in `test_getMappedAddons_with_paging()`.
    "/browser-mappings/with-paging/valid-browser-id/",
    // This handler parses the query string of the request it receives in order
    // to force the `getMappedAddons()` method to call the same API endpoint a
    // few times (by incrementing the integer value in the query string every
    // time). After that, this handler returns a "next' URL that points to one
    // of the valid endpoints registered above, which won't have a "next" URL.
    // We do this to verify that `getMappedAddons()` supports paginated API
    // results.
    (request, response) => {
      const page = parseInt(request.queryString, 10);
      const nextPath =
        page < 3
          ? `with-paging/valid-browser-id/?${page + 1}`
          : `valid-browser-id`;

      response.setHeader("content-type", "application/json");
      response.write(
        JSON.stringify({
          count: 0,
          next: `${BASE_URL}/browser-mappings/${nextPath}`,
          page_count: page,
          page_size: 1,
          previous: null,
          results: [],
        })
      );
    }
  );

  await promiseStartupManager();

  // Install an add-on so can check that it isn't returned in the results
  await promiseInstallFile(xpis[0]);
  await promiseRestartManager();

  // Create an active AddonInstall so can check that it isn't returned in the results
  let install = await AddonManager.getInstallForURL(BASE_URL + INSTALL_URL2);
  let promise = promiseCompleteInstall(install);
  registerCleanupFunction(() => promise);

  // Create a non-active AddonInstall so can check that it is returned in the results
  await AddonManager.getInstallForURL(BASE_URL + INSTALL_URL3);
});

// Tests homepageURL and getSearchURL()
add_task(async function test_1() {
  function check_urls(aPreference, aGetURL, aTests) {
    aTests.forEach(function (aTest) {
      Services.prefs.setCharPref(aPreference, aTest.preferenceValue);
      Assert.equal(aGetURL(aTest), aTest.expectedURL);
    });
  }

  var urlTests = [
    {
      preferenceValue: BASE_URL,
      expectedURL: BASE_URL,
    },
    {
      preferenceValue: BASE_URL + "/%OS%/%VERSION%",
      expectedURL: BASE_URL + "/XPCShell/1",
    },
  ];

  // Extra tests for AddonRepository.getSearchURL();
  var searchURLTests = [
    {
      searchTerms: "test",
      preferenceValue: BASE_URL + "/search?q=%TERMS%",
      expectedURL: BASE_URL + "/search?q=test",
    },
    {
      searchTerms: "test search",
      preferenceValue: BASE_URL + "/%TERMS%",
      expectedURL: BASE_URL + "/test%20search",
    },
    {
      searchTerms: 'odd=search:with&weird"characters',
      preferenceValue: BASE_URL + "/%TERMS%",
      expectedURL: BASE_URL + "/odd%3Dsearch%3Awith%26weird%22characters",
    },
  ];

  // Setup tests for homepageURL and getSearchURL()
  var tests = [
    {
      initiallyUndefined: true,
      preference: PREF_GETADDONS_BROWSEADDONS,
      urlTests,
      getURL: () => AddonRepository.homepageURL,
    },
    {
      initiallyUndefined: false,
      preference: PREF_GETADDONS_BROWSESEARCHRESULTS,
      urlTests: urlTests.concat(searchURLTests),
      getURL: function getSearchURL(aTest) {
        var searchTerms =
          aTest && aTest.searchTerms ? aTest.searchTerms : "unused terms";
        return AddonRepository.getSearchURL(searchTerms);
      },
    },
  ];

  tests.forEach(function url_test(aTest) {
    if (aTest.initiallyUndefined) {
      // Preference is not defined by default
      Assert.equal(
        Services.prefs.getPrefType(aTest.preference),
        Services.prefs.PREF_INVALID
      );
      Assert.equal(aTest.getURL(), DEFAULT_URL);
    }

    check_urls(aTest.preference, aTest.getURL, aTest.urlTests);
  });
});

// Tests failure of AddonRepository.getAddonsByIDs()
add_task(async function test_getAddonsByID_fails() {
  Services.prefs.setCharPref(GET_TEST.preference, GET_TEST.preferenceValue);

  await Assert.rejects(
    AddonRepository.getAddonsByIDs(GET_TEST.failedIDs),
    /Error: GET.*?failed/
  );
});

// Tests success of AddonRepository.getAddonsByIDs()
add_task(async function test_getAddonsByID_succeeds() {
  let result = await AddonRepository.getAddonsByIDs(GET_TEST.successfulIDs);

  check_results(result, GET_RESULTS);
});

// Tests success of AddonRepository.getAddonsByIDs() with rta ID.
add_task(async function test_getAddonsByID_rta() {
  let id = `rta:${btoa(GET_TEST.successfulIDs[0])}`.slice(0, -1);
  GET_TEST.successfulIDs[0] = id;
  let result = await AddonRepository.getAddonsByIDs(GET_TEST.successfulIDs);

  check_results(result, GET_RESULTS);
});

add_task(
  {
    pref_set: [[PREF_GET_BROWSER_MAPPINGS, GET_BROWSER_MAPPINGS_URL]],
  },
  async function test_getMappedAddons() {
    const {
      addons: result,
      matchedIDs,
      unmatchedIDs,
    } = await AddonRepository.getMappedAddons("valid-browser-id", [
      "browser-extension-test-1",
      "browser-extension-test-2",
      // This one is mapped but the search API won't return any data.
      "browser-extension-test-3",
      "browser-extension-test-4",
      // These ones are not mapped to any Firefox add-ons.
      "browser-extension-test-5",
      "browser-extension-test-6",
    ]);
    Assert.equal(result.length, 3, "expected 3 mapped add-ons");
    check_results(result, GET_RESULTS);
    Assert.deepEqual(matchedIDs, [
      "browser-extension-test-1",
      "browser-extension-test-2",
      "browser-extension-test-3",
      "browser-extension-test-4",
    ]);
    Assert.deepEqual(unmatchedIDs, [
      "browser-extension-test-5",
      "browser-extension-test-6",
    ]);
  }
);

add_task(
  {
    pref_set: [[PREF_GET_BROWSER_MAPPINGS, GET_BROWSER_MAPPINGS_URL]],
  },
  async function test_getMappedAddons_empty_list_of_ids() {
    const {
      addons: result,
      matchedIDs,
      unmatchedIDs,
    } = await AddonRepository.getMappedAddons("valid-browser-id", []);
    Assert.equal(result.length, 0, "expected 0 mapped add-ons");
    Assert.equal(matchedIDs.length, 0, "expected 0 matched IDs");
    Assert.equal(unmatchedIDs.length, 0, "expected 0 unmatched IDs");
  }
);

add_task(
  {
    pref_set: [[PREF_GET_BROWSER_MAPPINGS, GET_BROWSER_MAPPINGS_URL]],
  },
  async function test_getMappedAddons_invalid_ids() {
    const {
      addons: result,
      matchedIDs,
      unmatchedIDs,
    } = await AddonRepository.getMappedAddons("valid-browser-id", [
      "",
      null,
      undefined,
    ]);
    Assert.equal(result.length, 0, "expected 0 mapped add-ons");
    Assert.equal(matchedIDs.length, 0, "expected 0 matched IDs");
    Assert.deepEqual(unmatchedIDs, ["", null, undefined]);
  }
);

add_task(
  {
    pref_set: [[PREF_GET_BROWSER_MAPPINGS, GET_BROWSER_MAPPINGS_URL]],
  },
  async function test_getMappedAddons_empty_mapping() {
    const {
      addons: result,
      matchedIDs,
      unmatchedIDs,
    } = await AddonRepository.getMappedAddons("browser-id-empty-results", [
      "browser-extension-test-1",
      "browser-extension-test-2",
      "browser-extension-test-3",
    ]);
    Assert.equal(result.length, 0, "expected no mapped add-ons");
    Assert.equal(matchedIDs.length, 0, "expected 0 matched IDs");
    Assert.equal(unmatchedIDs.length, 3, "expected 3 unmatched IDs");
  }
);

add_task(
  {
    pref_set: [
      [
        PREF_GET_BROWSER_MAPPINGS,
        `${BASE_URL}/browser-mappings/with-paging/%BROWSER%/?1`,
      ],
    ],
  },
  async function test_getMappedAddons_with_paging() {
    const {
      addons: result,
      matchedIDs,
      unmatchedIDs,
    } = await AddonRepository.getMappedAddons("valid-browser-id", [
      "browser-extension-test-1",
      "browser-extension-test-2",
      // This one is mapped but the search API won't return any data.
      "browser-extension-test-3",
      "browser-extension-test-4",
    ]);
    Assert.equal(result.length, 3, "expected 3 mapped add-ons");
    check_results(result, GET_RESULTS);
    Assert.deepEqual(matchedIDs, [
      "browser-extension-test-1",
      "browser-extension-test-2",
      "browser-extension-test-3",
      "browser-extension-test-4",
    ]);
    Assert.deepEqual(unmatchedIDs, []);
  }
);
