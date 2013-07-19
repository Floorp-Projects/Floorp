/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests caching in AddonRepository.jsm

Components.utils.import("resource://gre/modules/AddonRepository.jsm");

Components.utils.import("resource://testing-common/httpd.js");
let gServer;

const PORT      = 4444;
const BASE_URL  = "http://localhost:" + PORT;

const PREF_GETADDONS_CACHE_ENABLED = "extensions.getAddons.cache.enabled";
const PREF_GETADDONS_CACHE_TYPES   = "extensions.getAddons.cache.types";
const PREF_GETADDONS_BYIDS         = "extensions.getAddons.get.url";
const PREF_GETADDONS_BYIDS_PERF    = "extensions.getAddons.getWithPerformance.url";
const GETADDONS_RESULTS            = BASE_URL + "/data/test_AddonRepository_cache.xml";
const GETADDONS_EMPTY              = BASE_URL + "/data/test_AddonRepository_empty.xml";
const GETADDONS_FAILED             = BASE_URL + "/data/test_AddonRepository_failed.xml";

const FILE_DATABASE = "addons.sqlite";
const ADDON_NAMES = ["test_AddonRepository_1",
                     "test_AddonRepository_2",
                     "test_AddonRepository_3"];
const ADDON_IDS = ADDON_NAMES.map(function(aName) aName + "@tests.mozilla.org");
const ADDON_FILES = ADDON_NAMES.map(do_get_addon);

const PREF_ADDON0_CACHE_ENABLED = "extensions." + ADDON_IDS[0] + ".getAddons.cache.enabled";
const PREF_ADDON1_CACHE_ENABLED = "extensions." + ADDON_IDS[1] + ".getAddons.cache.enabled";

// Properties of an individual add-on that should be checked
// Note: size and updateDate are checked separately
const ADDON_PROPERTIES = ["id", "type", "name", "version", "creator",
                          "developers", "translators", "contributors",
                          "description", "fullDescription",
                          "developerComments", "eula", "iconURL", "icons",
                          "screenshots", "homepageURL", "supportURL",
                          "optionsURL", "aboutURL", "contributionURL",
                          "contributionAmount", "averageRating", "reviewCount",
                          "reviewURL", "totalDownloads", "weeklyDownloads",
                          "dailyUsers", "sourceURI", "repositoryStatus",
                          "compatibilityOverrides"];

// The size and updateDate properties are annoying to test for XPI add-ons.
// However, since we only care about whether the repository value vs. the
// XPI value is used, we can just test if the property value matches
// the repository value
const REPOSITORY_SIZE       = 9;
const REPOSITORY_UPDATEDATE = 9;

// Get the URI of a subfile locating directly in the folder of
// the add-on corresponding to the specified id
function get_subfile_uri(aId, aFilename) {
  let file = gProfD.clone();
  file.append("extensions");
  return do_get_addon_root_uri(file, aId) + aFilename;
}


// Expected repository add-ons
const REPOSITORY_ADDONS = [{
  id:                     ADDON_IDS[0],
  type:                   "extension",
  name:                   "Repo Add-on 1",
  version:                "2.1",
  creator:                {
                            name: "Repo Add-on 1 - Creator",
                            url:  BASE_URL + "/repo/1/creator.html"
                          },
  developers:             [{
                            name: "Repo Add-on 1 - First Developer",
                            url:  BASE_URL + "/repo/1/firstDeveloper.html"
                          }, {
                            name: "Repo Add-on 1 - Second Developer",
                            url:  BASE_URL + "/repo/1/secondDeveloper.html"
                          }],
  description:            "Repo Add-on 1 - Description\nSecond line",
  fullDescription:        "Repo Add-on 1 - Full Description & some extra",
  developerComments:      "Repo Add-on 1\nDeveloper Comments",
  eula:                   "Repo Add-on 1 - EULA",
  iconURL:                BASE_URL + "/repo/1/icon.png",
  icons:                  { "32": BASE_URL + "/repo/1/icon.png" },
  homepageURL:            BASE_URL + "/repo/1/homepage.html",
  supportURL:             BASE_URL + "/repo/1/support.html",
  contributionURL:        BASE_URL + "/repo/1/meetDevelopers.html",
  contributionAmount:     "$11.11",
  averageRating:          1,
  reviewCount:            1111,
  reviewURL:              BASE_URL + "/repo/1/review.html",
  totalDownloads:         2221,
  weeklyDownloads:        3331,
  dailyUsers:             4441,
  sourceURI:              BASE_URL + "/repo/1/install.xpi",
  repositoryStatus:       4,
  compatibilityOverrides: [{
                            type: "incompatible",
                            minVersion: 0.1,
                            maxVersion: 0.2,
                            appID: "xpcshell@tests.mozilla.org",
                            appMinVersion: 3.0,
                            appMaxVersion: 4.0
                          }, {
                            type: "incompatible",
                            minVersion: 0.2,
                            maxVersion: 0.3,
                            appID: "xpcshell@tests.mozilla.org",
                            appMinVersion: 5.0,
                            appMaxVersion: 6.0
                          }]
}, {
  id:                     ADDON_IDS[1],
  type:                   "theme",
  name:                   "Repo Add-on 2",
  version:                "2.2",
  creator:                {
                            name: "Repo Add-on 2 - Creator",
                            url:  BASE_URL + "/repo/2/creator.html"
                          },
  developers:             [{
                            name: "Repo Add-on 2 - First Developer",
                            url:  BASE_URL + "/repo/2/firstDeveloper.html"
                          }, {
                            name: "Repo Add-on 2 - Second Developer",
                            url:  BASE_URL + "/repo/2/secondDeveloper.html"
                          }],
  description:            "Repo Add-on 2 - Description",
  fullDescription:        "Repo Add-on 2 - Full Description",
  developerComments:      "Repo Add-on 2 - Developer Comments",
  eula:                   "Repo Add-on 2 - EULA",
  iconURL:                BASE_URL + "/repo/2/icon.png",
  icons:                  { "32": BASE_URL + "/repo/2/icon.png" },
  screenshots:            [{
                            url:          BASE_URL + "/repo/2/firstFull.png",
                            thumbnailURL: BASE_URL + "/repo/2/firstThumbnail.png",
                            caption:      "Repo Add-on 2 - First Caption"
                          } , {
                            url:          BASE_URL + "/repo/2/secondFull.png",
                            thumbnailURL: BASE_URL + "/repo/2/secondThumbnail.png",
                            caption:      "Repo Add-on 2 - Second Caption"
                          }],
  homepageURL:            BASE_URL + "/repo/2/homepage.html",
  supportURL:             BASE_URL + "/repo/2/support.html",
  contributionURL:        BASE_URL + "/repo/2/meetDevelopers.html",
  contributionAmount:     null,
  averageRating:          2,
  reviewCount:            1112,
  reviewURL:              BASE_URL + "/repo/2/review.html",
  totalDownloads:         2222,
  weeklyDownloads:        3332,
  dailyUsers:             4442,
  sourceURI:              BASE_URL + "/repo/2/install.xpi",
  repositoryStatus:       9
}, {
  id:                     ADDON_IDS[2],
  name:                   "Repo Add-on 3",
  version:                "2.3",
  iconURL:                BASE_URL + "/repo/3/icon.png",
  icons:                  { "32": BASE_URL + "/repo/3/icon.png" },
  screenshots:            [{
                            url:          BASE_URL + "/repo/3/firstFull.png",
                            thumbnailURL: BASE_URL + "/repo/3/firstThumbnail.png",
                            caption:      "Repo Add-on 3 - First Caption"
                          } , {
                            url:          BASE_URL + "/repo/3/secondFull.png",
                            thumbnailURL: BASE_URL + "/repo/3/secondThumbnail.png",
                            caption:      "Repo Add-on 3 - Second Caption"
                          }]
}];


// Expected add-ons when not using cache
const WITHOUT_CACHE = [{
  id:                     ADDON_IDS[0],
  type:                   "extension",
  name:                   "XPI Add-on 1",
  version:                "1.1",
  creator:                { name: "XPI Add-on 1 - Creator" },
  developers:             [{ name: "XPI Add-on 1 - First Developer" },
                           { name: "XPI Add-on 1 - Second Developer" }],
  translators:            [{ name: "XPI Add-on 1 - First Translator" },
                           { name: "XPI Add-on 1 - Second Translator" }],
  contributors:           [{ name: "XPI Add-on 1 - First Contributor" },
                           { name: "XPI Add-on 1 - Second Contributor" }],
  description:            "XPI Add-on 1 - Description",
  iconURL:                BASE_URL + "/xpi/1/icon.png",
  icons:                  { "32": BASE_URL + "/xpi/1/icon.png" },
  homepageURL:            BASE_URL + "/xpi/1/homepage.html",
  optionsURL:             BASE_URL + "/xpi/1/options.html",
  aboutURL:               BASE_URL + "/xpi/1/about.html",
  sourceURI:              NetUtil.newURI(ADDON_FILES[0]).spec
}, {
  id:                     ADDON_IDS[1],
  type:                   "theme",
  name:                   "XPI Add-on 2",
  version:                "1.2",
  sourceURI:              NetUtil.newURI(ADDON_FILES[1]).spec,
  icons:                  {}
}, {
  id:                     ADDON_IDS[2],
  type:                   "theme",
  name:                   "XPI Add-on 3",
  version:                "1.3",
  get iconURL () {
    return get_subfile_uri(ADDON_IDS[2], "icon.png");
  },
  get icons () {
    return { "32": get_subfile_uri(ADDON_IDS[2], "icon.png") };
  },
  screenshots:            [{ get url () { return get_subfile_uri(ADDON_IDS[2], "preview.png"); } }],
  sourceURI:              NetUtil.newURI(ADDON_FILES[2]).spec
}];


// Expected add-ons when using cache
const WITH_CACHE = [{
  id:                     ADDON_IDS[0],
  type:                   "extension",
  name:                   "XPI Add-on 1",
  version:                "1.1",
  creator:                {
                            name: "Repo Add-on 1 - Creator",
                            url:  BASE_URL + "/repo/1/creator.html"
                          },
  developers:             [{ name: "XPI Add-on 1 - First Developer" },
                           { name: "XPI Add-on 1 - Second Developer" }],
  translators:            [{ name: "XPI Add-on 1 - First Translator" },
                           { name: "XPI Add-on 1 - Second Translator" }],
  contributors:           [{ name: "XPI Add-on 1 - First Contributor" },
                           { name: "XPI Add-on 1 - Second Contributor" }],
  description:            "XPI Add-on 1 - Description",
  fullDescription:        "Repo Add-on 1 - Full Description & some extra",
  developerComments:      "Repo Add-on 1\nDeveloper Comments",
  eula:                   "Repo Add-on 1 - EULA",
  iconURL:                BASE_URL + "/xpi/1/icon.png",
  icons:                  { "32": BASE_URL + "/xpi/1/icon.png" },
  homepageURL:            BASE_URL + "/xpi/1/homepage.html",
  supportURL:             BASE_URL + "/repo/1/support.html",
  optionsURL:             BASE_URL + "/xpi/1/options.html",
  aboutURL:               BASE_URL + "/xpi/1/about.html",
  contributionURL:        BASE_URL + "/repo/1/meetDevelopers.html",
  contributionAmount:     "$11.11",
  averageRating:          1,
  reviewCount:            1111,
  reviewURL:              BASE_URL + "/repo/1/review.html",
  totalDownloads:         2221,
  weeklyDownloads:        3331,
  dailyUsers:             4441,
  sourceURI:              NetUtil.newURI(ADDON_FILES[0]).spec,
  repositoryStatus:       4,
  compatibilityOverrides: [{
                            type: "incompatible",
                            minVersion: 0.1,
                            maxVersion: 0.2,
                            appID: "xpcshell@tests.mozilla.org",
                            appMinVersion: 3.0,
                            appMaxVersion: 4.0
                          }, {
                            type: "incompatible",
                            minVersion: 0.2,
                            maxVersion: 0.3,
                            appID: "xpcshell@tests.mozilla.org",
                            appMinVersion: 5.0,
                            appMaxVersion: 6.0
                          }]
}, {
  id:                     ADDON_IDS[1],
  type:                   "theme",
  name:                   "XPI Add-on 2",
  version:                "1.2",
  creator:                {
                            name: "Repo Add-on 2 - Creator",
                            url:  BASE_URL + "/repo/2/creator.html"
                          },
  developers:             [{
                            name: "Repo Add-on 2 - First Developer",
                            url:  BASE_URL + "/repo/2/firstDeveloper.html"
                          }, {
                            name: "Repo Add-on 2 - Second Developer",
                            url:  BASE_URL + "/repo/2/secondDeveloper.html"
                          }],
  description:            "Repo Add-on 2 - Description",
  fullDescription:        "Repo Add-on 2 - Full Description",
  developerComments:      "Repo Add-on 2 - Developer Comments",
  eula:                   "Repo Add-on 2 - EULA",
  iconURL:                BASE_URL + "/repo/2/icon.png",
  icons:                  { "32": BASE_URL + "/repo/2/icon.png" },
  screenshots:            [{
                            url:          BASE_URL + "/repo/2/firstFull.png",
                            thumbnailURL: BASE_URL + "/repo/2/firstThumbnail.png",
                            caption:      "Repo Add-on 2 - First Caption"
                          } , {
                            url:          BASE_URL + "/repo/2/secondFull.png",
                            thumbnailURL: BASE_URL + "/repo/2/secondThumbnail.png",
                            caption:      "Repo Add-on 2 - Second Caption"
                          }],
  homepageURL:            BASE_URL + "/repo/2/homepage.html",
  supportURL:             BASE_URL + "/repo/2/support.html",
  contributionURL:        BASE_URL + "/repo/2/meetDevelopers.html",
  contributionAmount:     null,
  averageRating:          2,
  reviewCount:            1112,
  reviewURL:              BASE_URL + "/repo/2/review.html",
  totalDownloads:         2222,
  weeklyDownloads:        3332,
  dailyUsers:             4442,
  sourceURI:              NetUtil.newURI(ADDON_FILES[1]).spec,
  repositoryStatus:       9
}, {
  id:                     ADDON_IDS[2],
  type:                   "theme",
  name:                   "XPI Add-on 3",
  version:                "1.3",
  get iconURL () {
    return get_subfile_uri(ADDON_IDS[2], "icon.png");
  },
  get icons () {
    return { "32": get_subfile_uri(ADDON_IDS[2], "icon.png") };
  },
  screenshots:            [{
                            url:          BASE_URL + "/repo/3/firstFull.png",
                            thumbnailURL: BASE_URL + "/repo/3/firstThumbnail.png",
                            caption:      "Repo Add-on 3 - First Caption"
                          } , {
                            url:          BASE_URL + "/repo/3/secondFull.png",
                            thumbnailURL: BASE_URL + "/repo/3/secondThumbnail.png",
                            caption:      "Repo Add-on 3 - Second Caption"
                          }],
  sourceURI:              NetUtil.newURI(ADDON_FILES[2]).spec
}];

// Expected add-ons when using cache
const WITH_EXTENSION_CACHE = [{
  id:                     ADDON_IDS[0],
  type:                   "extension",
  name:                   "XPI Add-on 1",
  version:                "1.1",
  creator:                {
                            name: "Repo Add-on 1 - Creator",
                            url:  BASE_URL + "/repo/1/creator.html"
                          },
  developers:             [{ name: "XPI Add-on 1 - First Developer" },
                           { name: "XPI Add-on 1 - Second Developer" }],
  translators:            [{ name: "XPI Add-on 1 - First Translator" },
                           { name: "XPI Add-on 1 - Second Translator" }],
  contributors:           [{ name: "XPI Add-on 1 - First Contributor" },
                           { name: "XPI Add-on 1 - Second Contributor" }],
  description:            "XPI Add-on 1 - Description",
  fullDescription:        "Repo Add-on 1 - Full Description & some extra",
  developerComments:      "Repo Add-on 1\nDeveloper Comments",
  eula:                   "Repo Add-on 1 - EULA",
  iconURL:                BASE_URL + "/xpi/1/icon.png",
  icons:                  { "32": BASE_URL + "/xpi/1/icon.png" },
  homepageURL:            BASE_URL + "/xpi/1/homepage.html",
  supportURL:             BASE_URL + "/repo/1/support.html",
  optionsURL:             BASE_URL + "/xpi/1/options.html",
  aboutURL:               BASE_URL + "/xpi/1/about.html",
  contributionURL:        BASE_URL + "/repo/1/meetDevelopers.html",
  contributionAmount:     "$11.11",
  averageRating:          1,
  reviewCount:            1111,
  reviewURL:              BASE_URL + "/repo/1/review.html",
  totalDownloads:         2221,
  weeklyDownloads:        3331,
  dailyUsers:             4441,
  sourceURI:              NetUtil.newURI(ADDON_FILES[0]).spec,
  repositoryStatus:       4,
  compatibilityOverrides: [{
                            type: "incompatible",
                            minVersion: 0.1,
                            maxVersion: 0.2,
                            appID: "xpcshell@tests.mozilla.org",
                            appMinVersion: 3.0,
                            appMaxVersion: 4.0
                          }, {
                            type: "incompatible",
                            minVersion: 0.2,
                            maxVersion: 0.3,
                            appID: "xpcshell@tests.mozilla.org",
                            appMinVersion: 5.0,
                            appMaxVersion: 6.0
                          }]
}, {
  id:                     ADDON_IDS[1],
  type:                   "theme",
  name:                   "XPI Add-on 2",
  version:                "1.2",
  sourceURI:              NetUtil.newURI(ADDON_FILES[1]).spec,
  icons:                  {}
}, {
  id:                     ADDON_IDS[2],
  type:                   "theme",
  name:                   "XPI Add-on 3",
  version:                "1.3",
  get iconURL () {
    return get_subfile_uri(ADDON_IDS[2], "icon.png");
  },
  get icons () {
    return { "32": get_subfile_uri(ADDON_IDS[2], "icon.png") };
  },
  screenshots:            [{ get url () { return get_subfile_uri(ADDON_IDS[2], "preview.png"); } }],
  sourceURI:              NetUtil.newURI(ADDON_FILES[2]).spec
}];


/*
 * Trigger an AddonManager background update check
 *
 * @param  aCallback
 *         Callback to call once the background update is complete
 */
function trigger_background_update(aCallback) {
  Services.obs.addObserver({
    observe: function(aSubject, aTopic, aData) {
      Services.obs.removeObserver(this, "addons-background-update-complete");
      do_execute_soon(aCallback);
    }
  }, "addons-background-update-complete", false);

  gInternalManager.notify(null);
}

/*
 * Check whether or not the add-ons database exists
 *
 * @param  aExpectedExists
 *         Whether or not the database is expected to exist
 */
function check_database_exists(aExpectedExists) {
  let file = gProfD.clone();
  file.append(FILE_DATABASE);
  do_check_eq(file.exists(), aExpectedExists);
}

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

  // Separately test size and updateDate (they should only be equal to the
  // REPOSITORY values if they are from the repository)
  aActualAddons.forEach(function(aActualAddon) {
    if (aActualAddon.size)
      do_check_eq(aActualAddon.size === REPOSITORY_SIZE, aFromRepository);

    if (aActualAddon.updateDate) {
      let time = aActualAddon.updateDate.getTime();
      do_check_eq(time === 1000 * REPOSITORY_UPDATEDATE, aFromRepository);
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
 * @param  aCallback
 *         A callback to call once the checks are complete
 */
function check_cache(aExpectedToFind, aExpectedImmediately, aCallback) {
  do_check_eq(aExpectedToFind.length, REPOSITORY_ADDONS.length);

  let pendingAddons = REPOSITORY_ADDONS.length;
  let immediatelyFound = true;

  for (let i = 0; i < REPOSITORY_ADDONS.length; i++) {
    let expected = aExpectedToFind[i] ? REPOSITORY_ADDONS[i] : null;
    AddonRepository.getCachedAddonByID(REPOSITORY_ADDONS[i].id, function(aAddon) {
      do_check_eq(immediatelyFound, aExpectedImmediately);

      if (expected == null)
        do_check_eq(aAddon, null);
      else
        check_results([aAddon], [expected], true);

      if (--pendingAddons == 0)
        do_execute_soon(aCallback);
    });
  }

  immediatelyFound = false;
}

/*
 * Check an initialized cache by checking the cache, then restarting the
 * manager, and checking the cache. This checks that the cache is consistent
 * across manager restarts.
 *
 * @param  aExpectedToFind
 *         An array of booleans representing which REPOSITORY_ADDONS are
 *         expected to be found in the cache
 * @param  aCallback
 *         A callback to call once the checks are complete
 */
function check_initialized_cache(aExpectedToFind, aCallback) {
  check_cache(aExpectedToFind, true, function() {
    restartManager();

    // If cache is disabled, then expect results immediately
    let cacheEnabled = Services.prefs.getBoolPref(PREF_GETADDONS_CACHE_ENABLED);
    check_cache(aExpectedToFind, !cacheEnabled, aCallback);
  });
}


function run_test() {
  // Setup for test
  do_test_pending();
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9");

  startupManager();

  // Install XPI add-ons
  installAllFiles(ADDON_FILES, function() {
    restartManager();

    gServer = new HttpServer();
    gServer.registerDirectory("/data/", do_get_file("data"));
    gServer.start(PORT);

    do_execute_soon(run_test_1);
  });
}

function end_test() {
  gServer.stop(do_test_finished);
}

// Tests AddonRepository.cacheEnabled
function run_test_1() {
  Services.prefs.setBoolPref(PREF_GETADDONS_CACHE_ENABLED, false);
  do_check_false(AddonRepository.cacheEnabled);
  Services.prefs.setBoolPref(PREF_GETADDONS_CACHE_ENABLED, true);
  do_check_true(AddonRepository.cacheEnabled);

  do_execute_soon(run_test_2);
}

// Tests that the cache and database begin as empty
function run_test_2() {
  check_database_exists(false);
  check_cache([false, false, false], false, run_test_3);
}

// Tests repopulateCache when the search fails
function run_test_3() {
  check_database_exists(true);
  Services.prefs.setBoolPref(PREF_GETADDONS_CACHE_ENABLED, true);
  Services.prefs.setCharPref(PREF_GETADDONS_BYIDS, GETADDONS_FAILED);

  AddonRepository.repopulateCache(ADDON_IDS, function() {
    check_initialized_cache([false, false, false], run_test_4);
  });
}

// Tests repopulateCache when search returns no results
function run_test_4() {
  Services.prefs.setCharPref(PREF_GETADDONS_BYIDS, GETADDONS_EMPTY);

  AddonRepository.repopulateCache(ADDON_IDS, function() {
    check_initialized_cache([false, false, false], run_test_5);
  });
}

// Tests repopulateCache when search returns results
function run_test_5() {
  Services.prefs.setCharPref(PREF_GETADDONS_BYIDS, GETADDONS_RESULTS);

  AddonRepository.repopulateCache(ADDON_IDS, function() {
    check_initialized_cache([true, true, true], run_test_5_1);
  });
}

// Tests repopulateCache when caching is disabled for a single add-on
function run_test_5_1() {
  Services.prefs.setBoolPref(PREF_ADDON0_CACHE_ENABLED, false);

  AddonRepository.repopulateCache(ADDON_IDS, function() {
    // Reset pref for next test
    Services.prefs.setBoolPref(PREF_ADDON0_CACHE_ENABLED, true);
    check_initialized_cache([false, true, true], run_test_6);
  });
}

// Tests repopulateCache when caching is disabled
function run_test_6() {
  check_database_exists(true);
  Services.prefs.setBoolPref(PREF_GETADDONS_CACHE_ENABLED, false);

  AddonRepository.repopulateCache(ADDON_IDS, function() {
    // Database should have been deleted
    check_database_exists(false);

    Services.prefs.setBoolPref(PREF_GETADDONS_CACHE_ENABLED, true);
    check_cache([false, false, false], false, run_test_7);
  });
}

// Tests cacheAddons when the search fails
function run_test_7() {
  check_database_exists(true);
  Services.prefs.setCharPref(PREF_GETADDONS_BYIDS, GETADDONS_FAILED);

  AddonRepository.cacheAddons(ADDON_IDS, function() {
    check_initialized_cache([false, false, false], run_test_8);
  });
}

// Tests cacheAddons when the search returns no results
function run_test_8() {
  Services.prefs.setCharPref(PREF_GETADDONS_BYIDS, GETADDONS_EMPTY);

  AddonRepository.cacheAddons(ADDON_IDS, function() {
    check_initialized_cache([false, false, false], run_test_9);
  });
}

// Tests cacheAddons for a single add-on when search returns results
function run_test_9() {
  Services.prefs.setCharPref(PREF_GETADDONS_BYIDS, GETADDONS_RESULTS);

  AddonRepository.cacheAddons([ADDON_IDS[0]], function() {
    check_initialized_cache([true, false, false], run_test_9_1);
  });
}

// Tests cacheAddons when caching is disabled for a single add-on
function run_test_9_1() {
  Services.prefs.setBoolPref(PREF_ADDON1_CACHE_ENABLED, false);

  AddonRepository.cacheAddons(ADDON_IDS, function() {
    // Reset pref for next test
    Services.prefs.setBoolPref(PREF_ADDON1_CACHE_ENABLED, true);
    check_initialized_cache([true, false, true], run_test_10);
  });
}

// Tests cacheAddons for multiple add-ons, some already in the cache,
function run_test_10() {
  AddonRepository.cacheAddons(ADDON_IDS, function() {
    check_initialized_cache([true, true, true], run_test_11);
  });
}

// Tests cacheAddons when caching is disabled
function run_test_11() {
  check_database_exists(true);
  Services.prefs.setBoolPref(PREF_GETADDONS_CACHE_ENABLED, false);

  AddonRepository.cacheAddons(ADDON_IDS, function() {
    // Database deleted for repopulateCache, not cacheAddons
    check_database_exists(true);

    Services.prefs.setBoolPref(PREF_GETADDONS_CACHE_ENABLED, true);
    check_initialized_cache([true, true, true], run_test_12);
  });
}

// Tests that XPI add-ons do not use any of the repository properties if
// caching is disabled, even if there are repository properties available
function run_test_12() {
  check_database_exists(true);
  Services.prefs.setBoolPref(PREF_GETADDONS_CACHE_ENABLED, false);
  Services.prefs.setCharPref(PREF_GETADDONS_BYIDS, GETADDONS_RESULTS);

  AddonManager.getAddonsByIDs(ADDON_IDS, function(aAddons) {
    check_results(aAddons, WITHOUT_CACHE);
    do_execute_soon(run_test_13);
  });
}

// Tests that a background update with caching disabled deletes the add-ons
// database, and that XPI add-ons still do not use any of repository properties
function run_test_13() {
  check_database_exists(true);
  Services.prefs.setCharPref(PREF_GETADDONS_BYIDS_PERF, GETADDONS_EMPTY);

  trigger_background_update(function() {
    // Database should have been deleted
    check_database_exists(false);

    AddonManager.getAddonsByIDs(ADDON_IDS, function(aAddons) {
      check_results(aAddons, WITHOUT_CACHE);
      do_execute_soon(run_test_14);
    });
  });
}

// Tests that the XPI add-ons have the correct properties if caching is
// enabled but has no information
function run_test_14() {
  Services.prefs.setBoolPref(PREF_GETADDONS_CACHE_ENABLED, true);

  trigger_background_update(function() {
    check_database_exists(true);

    AddonManager.getAddonsByIDs(ADDON_IDS, function(aAddons) {
      check_results(aAddons, WITHOUT_CACHE);
      do_execute_soon(run_test_15);
    });
  });
}

// Tests that the XPI add-ons correctly use the repository properties when
// caching is enabled and the repository information is available
function run_test_15() {
  Services.prefs.setCharPref(PREF_GETADDONS_BYIDS_PERF, GETADDONS_RESULTS);

  trigger_background_update(function() {
    AddonManager.getAddonsByIDs(ADDON_IDS, function(aAddons) {
      check_results(aAddons, WITH_CACHE);
      do_execute_soon(run_test_16);
    });
  });
}

// Tests that restarting the manager does not change the checked properties
// on the XPI add-ons (repository properties still exist and are still properly
// used)
function run_test_16() {
  restartManager();

  AddonManager.getAddonsByIDs(ADDON_IDS, function(aAddons) {
    check_results(aAddons, WITH_CACHE);
    do_execute_soon(run_test_17);
  });
}

// Tests that setting a list of types to cache works
function run_test_17() {
  Services.prefs.setCharPref(PREF_GETADDONS_CACHE_TYPES, "foo,bar,extension,baz");

  trigger_background_update(function() {
    AddonManager.getAddonsByIDs(ADDON_IDS, function(aAddons) {
      check_results(aAddons, WITH_EXTENSION_CACHE);
      end_test();
    });
  });
}

