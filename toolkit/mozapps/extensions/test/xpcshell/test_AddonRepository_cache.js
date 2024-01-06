/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests caching in AddonRepository.sys.mjs.

var gServer;

const HOST = "example.com";
const BASE_URL = "http://example.com";

const PREF_GETADDONS_CACHE_ENABLED = "extensions.getAddons.cache.enabled";
const PREF_GETADDONS_CACHE_TYPES = "extensions.getAddons.cache.types";
const GETADDONS_RESULTS = BASE_URL + "/data/test_AddonRepository_cache.json";
const EMPTY_RESULT = BASE_URL + "/data/test_AddonRepository_empty.json";
const FAILED_RESULT = BASE_URL + "/data/test_AddonRepository_fail.json";

const FILE_DATABASE = "addons.json";

const ADDONS = [
  {
    manifest: {
      name: "XPI Add-on 1",
      version: "1.1",

      description: "XPI Add-on 1 - Description",
      developer: {
        name: "XPI Add-on 1 - Author",
      },

      homepage_url: "http://example.com/xpi/1/homepage.html",
      icons: {
        32: "icon.png",
      },

      options_ui: {
        page: "options.html",
      },

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
      icons: {
        32: "icon.png",
      },
      theme: {},
      browser_specific_settings: {
        gecko: { id: "test_AddonRepository_3@tests.mozilla.org" },
      },
    },
    files: {
      "preview.png": "",
    },
  },
];

const ADDON_IDS = ADDONS.map(
  addon => addon.manifest.browser_specific_settings.gecko.id
);
const ADDON_FILES = ADDONS.map(addon =>
  AddonTestUtils.createTempWebExtensionFile(addon)
);

const PREF_ADDON0_CACHE_ENABLED =
  "extensions." + ADDON_IDS[0] + ".getAddons.cache.enabled";
const PREF_ADDON1_CACHE_ENABLED =
  "extensions." + ADDON_IDS[1] + ".getAddons.cache.enabled";

// Properties of an individual add-on that should be checked
// Note: updateDate is checked separately
const ADDON_PROPERTIES = [
  "id",
  "type",
  "name",
  "version",
  "developers",
  "description",
  "fullDescription",
  "icons",
  "screenshots",
  "homepageURL",
  "supportURL",
  "optionsURL",
  "averageRating",
  "reviewCount",
  "reviewURL",
  "weeklyDownloads",
  "sourceURI",
];

// The updateDate property is annoying to test for XPI add-ons.
// However, since we only care about whether the repository value vs. the
// XPI value is used, we can just test if the property value matches
// the repository value
const REPOSITORY_UPDATEDATE = 9;

// Get the URI of a subfile locating directly in the folder of
// the add-on corresponding to the specified id
function get_subfile_uri(aId, aFilename) {
  let file = gProfD.clone();
  file.append("extensions");
  return do_get_addon_root_uri(file, aId) + aFilename;
}

// Expected repository add-ons
const REPOSITORY_ADDONS = [
  {
    id: ADDON_IDS[0],
    type: "extension",
    name: "Repo Add-on 1",
    version: "2.1",
    developers: [
      {
        name: "Repo Add-on 1 - First Developer",
        url: BASE_URL + "/repo/1/firstDeveloper.html",
      },
      {
        name: "Repo Add-on 1 - Second Developer",
        url: BASE_URL + "/repo/1/secondDeveloper.html",
      },
    ],
    description: "Repo Add-on 1 - Description\nSecond line",
    fullDescription: "Repo Add-on 1 - Full Description & some extra",
    icons: { 32: BASE_URL + "/repo/1/icon.png" },
    homepageURL: BASE_URL + "/repo/1/homepage.html",
    supportURL: BASE_URL + "/repo/1/support.html",
    averageRating: 1,
    reviewCount: 1111,
    reviewURL: BASE_URL + "/repo/1/review.html",
    weeklyDownloads: 3331,
    sourceURI: BASE_URL + "/repo/1/install.xpi",
  },
  {
    id: ADDON_IDS[1],
    type: "theme",
    name: "Repo Add-on 2",
    version: "2.2",
    developers: [
      {
        name: "Repo Add-on 2 - First Developer",
        url: BASE_URL + "/repo/2/firstDeveloper.html",
      },
      {
        name: "Repo Add-on 2 - Second Developer",
        url: BASE_URL + "/repo/2/secondDeveloper.html",
      },
    ],
    description: "Repo Add-on 2 - Description",
    fullDescription: "Repo Add-on 2 - Full Description",
    icons: { 32: BASE_URL + "/repo/2/icon.png" },
    screenshots: [
      {
        url: BASE_URL + "/repo/2/firstFull.png",
        thumbnailURL: BASE_URL + "/repo/2/firstThumbnail.png",
        caption: "Repo Add-on 2 - First Caption",
      },
      {
        url: BASE_URL + "/repo/2/secondFull.png",
        thumbnailURL: BASE_URL + "/repo/2/secondThumbnail.png",
        caption: "Repo Add-on 2 - Second Caption",
      },
    ],
    homepageURL: BASE_URL + "/repo/2/homepage.html",
    supportURL: BASE_URL + "/repo/2/support.html",
    averageRating: 2,
    reviewCount: 1112,
    reviewURL: BASE_URL + "/repo/2/review.html",
    weeklyDownloads: 3332,
    sourceURI: BASE_URL + "/repo/2/install.xpi",
  },
  {
    id: ADDON_IDS[2],
    type: "theme",
    name: "Repo Add-on 3",
    version: "2.3",
    icons: { 32: BASE_URL + "/repo/3/icon.png" },
    screenshots: [
      {
        url: BASE_URL + "/repo/3/firstFull.png",
        thumbnailURL: BASE_URL + "/repo/3/firstThumbnail.png",
        caption: "Repo Add-on 3 - First Caption",
      },
      {
        url: BASE_URL + "/repo/3/secondFull.png",
        thumbnailURL: BASE_URL + "/repo/3/secondThumbnail.png",
        caption: "Repo Add-on 3 - Second Caption",
      },
    ],
  },
];

function extensionURL(id, path) {
  return WebExtensionPolicy.getByID(id).getURL(path);
}

// Expected add-ons when not using cache
const WITHOUT_CACHE = [
  {
    id: ADDON_IDS[0],
    type: "extension",
    name: "XPI Add-on 1",
    version: "1.1",
    authors: [{ name: "XPI Add-on 1 - Author" }],
    description: "XPI Add-on 1 - Description",
    get icons() {
      return { 32: get_subfile_uri(ADDON_IDS[0], "icon.png") };
    },
    homepageURL: `${BASE_URL}/xpi/1/homepage.html`,
    get optionsURL() {
      return extensionURL(ADDON_IDS[0], "options.html");
    },
    sourceURI: NetUtil.newURI(ADDON_FILES[0]).spec,
  },
  {
    id: ADDON_IDS[1],
    type: "theme",
    name: "XPI Add-on 2",
    version: "1.2",
    sourceURI: NetUtil.newURI(ADDON_FILES[1]).spec,
    icons: {},
  },
  {
    id: ADDON_IDS[2],
    type: "theme",
    name: "XPI Add-on 3",
    version: "1.3",
    get icons() {
      return { 32: get_subfile_uri(ADDON_IDS[2], "icon.png") };
    },
    screenshots: [
      {
        get url() {
          return get_subfile_uri(ADDON_IDS[2], "preview.png");
        },
      },
    ],
    sourceURI: NetUtil.newURI(ADDON_FILES[2]).spec,
  },
];

// Expected add-ons when using cache
const WITH_CACHE = [
  {
    id: ADDON_IDS[0],
    type: "extension",
    name: "XPI Add-on 1",
    version: "1.1",
    developers: [
      {
        name: "Repo Add-on 1 - First Developer",
        url: BASE_URL + "/repo/1/firstDeveloper.html",
      },
      {
        name: "Repo Add-on 1 - Second Developer",
        url: BASE_URL + "/repo/1/secondDeveloper.html",
      },
    ],
    description: "XPI Add-on 1 - Description",
    fullDescription: "Repo Add-on 1 - Full Description & some extra",
    get icons() {
      return { 32: get_subfile_uri(ADDON_IDS[0], "icon.png") };
    },
    homepageURL: BASE_URL + "/xpi/1/homepage.html",
    supportURL: BASE_URL + "/repo/1/support.html",
    get optionsURL() {
      return extensionURL(ADDON_IDS[0], "options.html");
    },
    averageRating: 1,
    reviewCount: 1111,
    reviewURL: BASE_URL + "/repo/1/review.html",
    weeklyDownloads: 3331,
    sourceURI: NetUtil.newURI(ADDON_FILES[0]).spec,
  },
  {
    id: ADDON_IDS[1],
    type: "theme",
    name: "XPI Add-on 2",
    version: "1.2",
    developers: [
      {
        name: "Repo Add-on 2 - First Developer",
        url: BASE_URL + "/repo/2/firstDeveloper.html",
      },
      {
        name: "Repo Add-on 2 - Second Developer",
        url: BASE_URL + "/repo/2/secondDeveloper.html",
      },
    ],
    description: "Repo Add-on 2 - Description",
    fullDescription: "Repo Add-on 2 - Full Description",
    icons: { 32: BASE_URL + "/repo/2/icon.png" },
    screenshots: [
      {
        url: BASE_URL + "/repo/2/firstFull.png",
        thumbnailURL: BASE_URL + "/repo/2/firstThumbnail.png",
        caption: "Repo Add-on 2 - First Caption",
      },
      {
        url: BASE_URL + "/repo/2/secondFull.png",
        thumbnailURL: BASE_URL + "/repo/2/secondThumbnail.png",
        caption: "Repo Add-on 2 - Second Caption",
      },
    ],
    homepageURL: BASE_URL + "/repo/2/homepage.html",
    supportURL: BASE_URL + "/repo/2/support.html",
    averageRating: 2,
    reviewCount: 1112,
    reviewURL: BASE_URL + "/repo/2/review.html",
    weeklyDownloads: 3332,
    sourceURI: NetUtil.newURI(ADDON_FILES[1]).spec,
  },
  {
    id: ADDON_IDS[2],
    type: "theme",
    name: "XPI Add-on 3",
    version: "1.3",
    get iconURL() {
      return get_subfile_uri(ADDON_IDS[2], "icon.png");
    },
    get icons() {
      return { 32: get_subfile_uri(ADDON_IDS[2], "icon.png") };
    },
    screenshots: [
      {
        url: BASE_URL + "/repo/3/firstFull.png",
        thumbnailURL: BASE_URL + "/repo/3/firstThumbnail.png",
        caption: "Repo Add-on 3 - First Caption",
      },
      {
        url: BASE_URL + "/repo/3/secondFull.png",
        thumbnailURL: BASE_URL + "/repo/3/secondThumbnail.png",
        caption: "Repo Add-on 3 - Second Caption",
      },
    ],
    sourceURI: NetUtil.newURI(ADDON_FILES[2]).spec,
  },
];

// Expected add-ons when using cache for extension, but no cache for themes.
const WITH_EXTENSION_CACHE = [
  {
    id: ADDON_IDS[0],
    type: "extension",
    name: "XPI Add-on 1",
    version: "1.1",
    developers: [
      {
        name: "Repo Add-on 1 - First Developer",
        url: BASE_URL + "/repo/1/firstDeveloper.html",
      },
      {
        name: "Repo Add-on 1 - Second Developer",
        url: BASE_URL + "/repo/1/secondDeveloper.html",
      },
    ],
    description: "XPI Add-on 1 - Description",
    fullDescription: "Repo Add-on 1 - Full Description & some extra",
    get icons() {
      return { 32: get_subfile_uri(ADDON_IDS[0], "icon.png") };
    },
    homepageURL: BASE_URL + "/xpi/1/homepage.html",
    supportURL: BASE_URL + "/repo/1/support.html",
    get optionsURL() {
      return extensionURL(ADDON_IDS[0], "options.html");
    },
    averageRating: 1,
    reviewCount: 1111,
    reviewURL: BASE_URL + "/repo/1/review.html",
    weeklyDownloads: 3331,
    sourceURI: NetUtil.newURI(ADDON_FILES[0]).spec,
  },
  {
    id: ADDON_IDS[1],
    type: "theme",
    name: "XPI Add-on 2",
    version: "1.2",
    sourceURI: NetUtil.newURI(ADDON_FILES[1]).spec,
    icons: {},
  },
  {
    id: ADDON_IDS[2],
    type: "theme",
    name: "XPI Add-on 3",
    version: "1.3",
    get iconURL() {
      return get_subfile_uri(ADDON_IDS[2], "icon.png");
    },
    get icons() {
      return { 32: get_subfile_uri(ADDON_IDS[2], "icon.png") };
    },
    screenshots: [
      {
        get url() {
          return get_subfile_uri(ADDON_IDS[2], "preview.png");
        },
      },
    ],
    sourceURI: NetUtil.newURI(ADDON_FILES[2]).spec,
  },
];

var gDBFile = gProfD.clone();
gDBFile.append(FILE_DATABASE);

/*
 * Check the actual add-on results against the expected add-on results
 *
 * @param  aActualAddons
 *         The array of actual add-ons to check
 * @param  aExpectedAddons
 *         The array of expected add-ons to check against
 * @param  aFromRepository
 *         An optional boolean representing if the add-ons are from
 *         the repository
 */
function check_results(aActualAddons, aExpectedAddons, aFromRepository) {
  aFromRepository = !!aFromRepository;

  do_check_addons(aActualAddons, aExpectedAddons, ADDON_PROPERTIES);

  // Separately test updateDate (it should only be equal to the
  // REPOSITORY values if it is from the repository)
  aActualAddons.forEach(function (aActualAddon) {
    if (aActualAddon.updateDate) {
      let time = aActualAddon.updateDate.getTime();
      Assert.equal(time === 1000 * REPOSITORY_UPDATEDATE, aFromRepository);
    }
  });
}

/*
 * Check the add-ons in the cache. This function also tests
 * AddonRepository.getCachedAddonByID()
 *
 * @param  aExpectedToFind
 *         An array of booleans representing which REPOSITORY_ADDONS are
 *         expected to be found in the cache
 * @param  aExpectedImmediately
 *         A boolean representing if results from the cache are expected
 *         immediately. Results are not immediate if the cache has not been
 *         initialized yet.
 * @return Promise{null}
 *         Resolves once the checks are complete
 */
function check_cache(aExpectedToFind, aExpectedImmediately) {
  Assert.equal(aExpectedToFind.length, REPOSITORY_ADDONS.length);

  let lookups = [];

  for (let i = 0; i < REPOSITORY_ADDONS.length; i++) {
    lookups.push(
      new Promise((resolve, reject) => {
        let immediatelyFound = true;
        let expected = aExpectedToFind[i] ? REPOSITORY_ADDONS[i] : null;
        // can't Promise-wrap this because we're also testing whether the callback is
        // sync or async
        AddonRepository.getCachedAddonByID(
          REPOSITORY_ADDONS[i].id,
          function (aAddon) {
            Assert.equal(immediatelyFound, aExpectedImmediately);
            if (expected == null) {
              Assert.equal(aAddon, null);
            } else {
              check_results([aAddon], [expected], true);
            }
            resolve();
          }
        );
        immediatelyFound = false;
      })
    );
  }
  return Promise.all(lookups);
}

/*
 * Task to check an initialized cache by checking the cache, then restarting the
 * manager, and checking the cache. This checks that the cache is consistent
 * across manager restarts.
 *
 * @param  aExpectedToFind
 *         An array of booleans representing which REPOSITORY_ADDONS are
 *         expected to be found in the cache
 */
async function check_initialized_cache(aExpectedToFind) {
  await check_cache(aExpectedToFind, true);
  await promiseRestartManager();

  // If cache is disabled, then expect results immediately
  let cacheEnabled = Services.prefs.getBoolPref(PREF_GETADDONS_CACHE_ENABLED);
  await check_cache(aExpectedToFind, !cacheEnabled);
}

add_task(async function setup() {
  // Setup for test
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9");

  await promiseStartupManager();

  // Install XPI add-ons
  await promiseInstallAllFiles(ADDON_FILES);
  await promiseRestartManager();

  gServer = AddonTestUtils.createHttpServer({ hosts: [HOST] });
  gServer.registerDirectory("/data/", do_get_file("data"));
});

// Tests AddonRepository.cacheEnabled
add_task(async function run_test_1() {
  Services.prefs.setBoolPref(PREF_GETADDONS_CACHE_ENABLED, false);
  Assert.ok(!AddonRepository.cacheEnabled);
  Services.prefs.setBoolPref(PREF_GETADDONS_CACHE_ENABLED, true);
  Assert.ok(AddonRepository.cacheEnabled);
});

// Tests that the cache and database begin as empty
add_task(async function run_test_2() {
  Assert.ok(!gDBFile.exists());
  await check_cache([false, false, false], false);
  await AddonRepository.flush();
});

// Tests repopulateCache when the search fails
add_task(async function run_test_3() {
  Assert.ok(gDBFile.exists());
  Services.prefs.setBoolPref(PREF_GETADDONS_CACHE_ENABLED, true);
  Services.prefs.setCharPref(PREF_GETADDONS_BYIDS, FAILED_RESULT);

  await AddonRepository.backgroundUpdateCheck();
  await check_initialized_cache([false, false, false]);
});

// Tests repopulateCache when search returns no results
add_task(async function run_test_4() {
  Services.prefs.setCharPref(PREF_GETADDONS_BYIDS, EMPTY_RESULT);

  await AddonRepository.backgroundUpdateCheck();
  await check_initialized_cache([false, false, false]);
});

// Tests repopulateCache when search returns results
add_task(async function run_test_5() {
  Services.prefs.setCharPref(PREF_GETADDONS_BYIDS, GETADDONS_RESULTS);

  await AddonRepository.backgroundUpdateCheck();
  await check_initialized_cache([true, true, true]);
});

// Tests repopulateCache when caching is disabled for a single add-on
add_task(async function run_test_5_1() {
  Services.prefs.setBoolPref(PREF_ADDON0_CACHE_ENABLED, false);

  await AddonRepository.backgroundUpdateCheck();

  // Reset pref for next test
  Services.prefs.setBoolPref(PREF_ADDON0_CACHE_ENABLED, true);

  await check_initialized_cache([false, true, true]);
});

// Tests repopulateCache when caching is disabled
add_task(async function run_test_6() {
  Assert.ok(gDBFile.exists());
  Services.prefs.setBoolPref(PREF_GETADDONS_CACHE_ENABLED, false);

  await AddonRepository.backgroundUpdateCheck();
  // Database should have been deleted
  Assert.ok(!gDBFile.exists());

  Services.prefs.setBoolPref(PREF_GETADDONS_CACHE_ENABLED, true);
  await check_cache([false, false, false], false);
  await AddonRepository.flush();
});

// Tests cacheAddons when the search fails
add_task(async function run_test_7() {
  Assert.ok(gDBFile.exists());
  Services.prefs.setCharPref(PREF_GETADDONS_BYIDS, FAILED_RESULT);

  await AddonRepository.cacheAddons(ADDON_IDS);
  await check_initialized_cache([false, false, false]);
});

// Tests cacheAddons when the search returns no results
add_task(async function run_test_8() {
  Services.prefs.setCharPref(PREF_GETADDONS_BYIDS, EMPTY_RESULT);

  await AddonRepository.cacheAddons(ADDON_IDS);
  await check_initialized_cache([false, false, false]);
});

// Tests cacheAddons for a single add-on when search returns results
add_task(async function run_test_9() {
  Services.prefs.setCharPref(PREF_GETADDONS_BYIDS, GETADDONS_RESULTS);

  await AddonRepository.cacheAddons([ADDON_IDS[0]]);
  await check_initialized_cache([true, false, false]);
});

// Tests cacheAddons when caching is disabled for a single add-on
add_task(async function run_test_9_1() {
  Services.prefs.setBoolPref(PREF_ADDON1_CACHE_ENABLED, false);

  await AddonRepository.cacheAddons(ADDON_IDS);

  // Reset pref for next test
  Services.prefs.setBoolPref(PREF_ADDON1_CACHE_ENABLED, true);

  await check_initialized_cache([true, false, true]);
});

// Tests cacheAddons for multiple add-ons, some already in the cache,
add_task(async function run_test_10() {
  await AddonRepository.cacheAddons(ADDON_IDS);
  await check_initialized_cache([true, true, true]);
});

// Tests cacheAddons when caching is disabled
add_task(async function run_test_11() {
  Assert.ok(gDBFile.exists());
  Services.prefs.setBoolPref(PREF_GETADDONS_CACHE_ENABLED, false);

  await AddonRepository.cacheAddons(ADDON_IDS);
  Assert.ok(gDBFile.exists());

  Services.prefs.setBoolPref(PREF_GETADDONS_CACHE_ENABLED, true);
  await check_initialized_cache([true, true, true]);
});

// Tests that XPI add-ons do not use any of the repository properties if
// caching is disabled, even if there are repository properties available
add_task(async function run_test_12() {
  Assert.ok(gDBFile.exists());
  Services.prefs.setBoolPref(PREF_GETADDONS_CACHE_ENABLED, false);
  Services.prefs.setCharPref(PREF_GETADDONS_BYIDS, GETADDONS_RESULTS);

  let addons = await promiseAddonsByIDs(ADDON_IDS);
  check_results(addons, WITHOUT_CACHE);
});

// Tests that a background update with caching disabled deletes the add-ons
// database, and that XPI add-ons still do not use any of repository properties
add_task(async function run_test_13() {
  Assert.ok(gDBFile.exists());
  Services.prefs.setCharPref(PREF_GETADDONS_BYIDS, EMPTY_RESULT);

  await AddonManagerPrivate.backgroundUpdateCheck();
  // Database should have been deleted
  Assert.ok(!gDBFile.exists());

  let aAddons = await promiseAddonsByIDs(ADDON_IDS);
  check_results(aAddons, WITHOUT_CACHE);
});

// Tests that the XPI add-ons have the correct properties if caching is
// enabled but has no information
add_task(async function run_test_14() {
  Services.prefs.setBoolPref(PREF_GETADDONS_CACHE_ENABLED, true);

  await AddonManagerPrivate.backgroundUpdateCheck();
  await AddonRepository.flush();
  Assert.ok(gDBFile.exists());

  let aAddons = await promiseAddonsByIDs(ADDON_IDS);
  check_results(aAddons, WITHOUT_CACHE);
});

// Tests that the XPI add-ons correctly use the repository properties when
// caching is enabled and the repository information is available
add_task(async function run_test_15() {
  Services.prefs.setCharPref(PREF_GETADDONS_BYIDS, GETADDONS_RESULTS);

  await AddonManagerPrivate.backgroundUpdateCheck();
  let aAddons = await promiseAddonsByIDs(ADDON_IDS);
  check_results(aAddons, WITH_CACHE);
});

// Tests that restarting the manager does not change the checked properties
// on the XPI add-ons (repository properties still exist and are still properly
// used)
add_task(async function run_test_16() {
  await promiseRestartManager();

  let aAddons = await promiseAddonsByIDs(ADDON_IDS);
  check_results(aAddons, WITH_CACHE);
});

// Tests that setting a list of types to cache works
add_task(async function run_test_17() {
  Services.prefs.setCharPref(
    PREF_GETADDONS_CACHE_TYPES,
    "foo,bar,extension,baz"
  );

  await AddonManagerPrivate.backgroundUpdateCheck();
  let aAddons = await promiseAddonsByIDs(ADDON_IDS);
  check_results(aAddons, WITH_EXTENSION_CACHE);
  Services.prefs.clearUserPref(PREF_GETADDONS_CACHE_TYPES);
});

// Tests that the cache is retained when the server/API is unreachable.
// Regression test for https://bugzilla.mozilla.org/show_bug.cgi?id=1870905
add_task(async function run_test_18() {
  // The response is expected to be JSON, so setting it to non-JSON is
  // equivalent to the server being unreachable in the implementation.
  Services.prefs.setCharPref(PREF_GETADDONS_BYIDS, "data:text/not-json,");

  await AddonManagerPrivate.backgroundUpdateCheck();
  let aAddons = await promiseAddonsByIDs(ADDON_IDS);
  check_results(aAddons, WITH_EXTENSION_CACHE);
  Services.prefs.setCharPref(PREF_GETADDONS_BYIDS, GETADDONS_RESULTS);
});
