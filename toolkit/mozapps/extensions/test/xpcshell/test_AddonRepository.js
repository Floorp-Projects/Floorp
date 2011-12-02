/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests AddonRepository.jsm

Components.utils.import("resource://gre/modules/AddonRepository.jsm");

do_load_httpd_js();
var gServer;

const PREF_GETADDONS_BROWSEADDONS        = "extensions.getAddons.browseAddons";
const PREF_GETADDONS_BYIDS               = "extensions.getAddons.get.url";
const PREF_GETADDONS_BROWSERECOMMENDED   = "extensions.getAddons.recommended.browseURL";
const PREF_GETADDONS_GETRECOMMENDED      = "extensions.getAddons.recommended.url";
const PREF_GETADDONS_BROWSESEARCHRESULTS = "extensions.getAddons.search.browseURL";
const PREF_GETADDONS_GETSEARCHRESULTS    = "extensions.getAddons.search.url";

const PORT          = 4444;
const BASE_URL      = "http://localhost:" + PORT;
const DEFAULT_URL   = "about:blank";

// Path to source URI of installed add-on
const INSTALL_URL1  = "/addons/test_AddonRepository_1.xpi";
// Path to source URI of installing add-on
const INSTALL_URL2  = "/addons/test_AddonRepository_2.xpi";
// Path to source URI of non-active add-on (state = STATE_AVAILABLE)
const INSTALL_URL3  = "/addons/test_AddonRepository_3.xpi";

// Properties of an individual add-on that should be checked
// Note: name is checked separately
var ADDON_PROPERTIES = ["id", "type", "version", "creator", "developers",
                        "description", "fullDescription", "developerComments",
                        "eula", "iconURL", "screenshots", "homepageURL",
                        "supportURL", "contributionURL", "contributionAmount",
                        "averageRating", "reviewCount", "reviewURL",
                        "totalDownloads", "weeklyDownloads", "dailyUsers",
                        "sourceURI", "repositoryStatus", "size", "updateDate",
                        "purchaseURL", "purchaseAmount", "purchaseDisplayAmount",
                        "compatibilityOverrides"];

// Results of getAddonsByIDs
var GET_RESULTS = [{
  id:                     "test1@tests.mozilla.org",
  type:                   "extension",
  version:                "1.1",
  creator:                {
                            name: "Test Creator 1",
                            url:  BASE_URL + "/creator1.html"
                          },
  developers:             [{
                            name: "Test Developer 1",
                            url:  BASE_URL + "/developer1.html"
                          }],
  description:            "Test Summary 1",
  fullDescription:        "Test Description 1",
  developerComments:      "Test Developer Comments 1",
  eula:                   "Test EULA 1",
  iconURL:                BASE_URL + "/icon1.png",
  screenshots:            [{
                            url:             BASE_URL + "/full1-1.png",
                            width:           400,
                            height:          300,
                            thumbnailURL:    BASE_URL + "/thumbnail1-1.png",
                            thumbnailWidth:  200,
                            thumbnailHeight: 150,
                            caption:         "Caption 1 - 1"
                          }, {
                            url:          BASE_URL + "/full2-1.png",
                            thumbnailURL: BASE_URL + "/thumbnail2-1.png",
                            caption:      "Caption 2 - 1"
                          }],
  homepageURL:            BASE_URL + "/learnmore1.html",
  supportURL:             BASE_URL + "/support1.html",
  contributionURL:        BASE_URL + "/meetDevelopers1.html",
  contributionAmount:     "$11.11",
  averageRating:          4,
  reviewCount:            1111,
  reviewURL:              BASE_URL + "/review1.html",
  totalDownloads:         2222,
  weeklyDownloads:        3333,
  dailyUsers:             4444,
  sourceURI:              BASE_URL + INSTALL_URL2,
  repositoryStatus:       8,
  size:                   5555,
  updateDate:             new Date(1265033045000),
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
  id:                     "test_AddonRepository_1@tests.mozilla.org",
  version:                "1.4",
  repositoryStatus:       9999
}];

// Results of retrieveRecommendedAddons and searchAddons
var SEARCH_RESULTS = [{
  id:                     "test1@tests.mozilla.org",
  type:                   "extension",
  version:                "1.1",
  creator:                {
                            name: "Test Creator 1",
                            url:  BASE_URL + "/creator1.html"
                          },
  repositoryStatus:       8,
  sourceURI:              BASE_URL + "/test1.xpi"
}, {
  id:                     "test2@tests.mozilla.org",
  type:                   "extension",
  version:                "1.2",
  creator:                {
                            name: "Test Creator 2",
                            url:  BASE_URL + "/creator2.html"
                          },
  developers:             [{
                            name: "Test Developer 2",
                            url:  BASE_URL + "/developer2.html"
                          }],
  description:            "Test Summary 2\n\nparagraph",
  fullDescription:        "Test Description 2\nnewline",
  developerComments:      "Test Developer\nComments 2",
  eula:                   "Test EULA 2",
  iconURL:                BASE_URL + "/icon2.png",
  screenshots:            [{
                            url:          BASE_URL + "/full1-2.png",
                            thumbnailURL: BASE_URL + "/thumbnail1-2.png"
                          }, {
                            url:          BASE_URL + "/full2-2.png",
                            thumbnailURL: BASE_URL + "/thumbnail2-2.png",
                            caption:      "Caption 2"
                          }],
  homepageURL:            BASE_URL + "/learnmore2.html",
  supportURL:             BASE_URL + "/support2.html",
  contributionURL:        BASE_URL + "/meetDevelopers2.html",
  contributionAmount:     null,
  repositoryStatus:       4,
  sourceURI:              BASE_URL + "/test2.xpi"
}, {
  id:                     "test3@tests.mozilla.org",
  type:                   "theme",
  version:                "1.3",
  creator:                {
                            name: "Test Creator 3",
                            url:  BASE_URL + "/creator3.html"
                          },
  developers:             [{
                            name: "First Test Developer 3",
                            url:  BASE_URL + "/developer1-3.html"
                          }, {
                            name: "Second Test Developer 3",
                            url:  BASE_URL + "/developer2-3.html"
                          }],
  description:            "Test Summary 3",
  fullDescription:        "Test Description 3\n\n    List item 1\n    List item 2",
  developerComments:      "Test Developer Comments 3",
  eula:                   "Test EULA 3",
  iconURL:                BASE_URL + "/icon3.png",
  screenshots:            [{
                            url:          BASE_URL + "/full1-3.png",
                            thumbnailURL: BASE_URL + "/thumbnail1-3.png",
                            caption:      "Caption 1 - 3"
                          }, {
                            url:          BASE_URL + "/full2-3.png",
                            caption:      "Caption 2 - 3"
                          }, {
                            url:          BASE_URL + "/full3-3.png",
                            thumbnailURL: BASE_URL + "/thumbnail3-3.png",
                            caption:      "Caption 3 - 3"
                          }],
  homepageURL:            BASE_URL + "/homepage3.html",
  supportURL:             BASE_URL + "/support3.html",
  contributionURL:        BASE_URL + "/meetDevelopers3.html",
  contributionAmount:     "$11.11",
  averageRating:          2,
  reviewCount:            1111,
  reviewURL:              BASE_URL + "/review3.html",
  totalDownloads:         2222,
  weeklyDownloads:        3333,
  dailyUsers:             4444,
  sourceURI:              BASE_URL + "/test3.xpi",
  repositoryStatus:       8,
  size:                   5555,
  updateDate:             new Date(1265033045000),
  
}, {
  id:                     "purchase1@tests.mozilla.org",
  type:                   "extension",
  version:                "2.0",
  creator:                {
                            name: "Test Creator - Last Passing",
                            url:  BASE_URL + "/creatorLastPassing.html"
                          },
  averageRating:          5,
  repositoryStatus:       4,
  purchaseURL:            "http://localhost:4444/purchaseURL1",
  purchaseAmount:         5,
  purchaseDisplayAmount:  "$5"
}, {
  id:                     "purchase2@tests.mozilla.org",
  type:                   "extension",
  version:                "2.0",
  creator:                {
                            name: "Test Creator - Last Passing",
                            url:  BASE_URL + "/creatorLastPassing.html"
                          },
  averageRating:          5,
  repositoryStatus:       4,
  purchaseURL:            "http://localhost:4444/purchaseURL2",
  purchaseAmount:         10,
  purchaseDisplayAmount:  "$10"
}, {
  id:                     "test-lastPassing@tests.mozilla.org",
  type:                   "extension",
  version:                "2.0",
  creator:                {
                            name: "Test Creator - Last Passing",
                            url:  BASE_URL + "/creatorLastPassing.html"
                          },
  averageRating:          5,
  repositoryStatus:       4,
  sourceURI:              BASE_URL + "/addons/test_AddonRepository_3.xpi"
}];

const TOTAL_RESULTS = 1111;
const MAX_RESULTS = SEARCH_RESULTS.length;

// Used to differentiate between testing that a search success
// or a search failure for retrieveRecommendedAddons and searchAddons
const FAILED_MAX_RESULTS  = 9999;

// Values for testing AddonRepository.getAddonsByIDs()
var GET_TEST = {
  preference:       PREF_GETADDONS_BYIDS,
  preferenceValue:  BASE_URL + "/%OS%/%VERSION%/%API_VERSION%/" +
                    "%API_VERSION%/%IDS%",
  failedIDs:      ["test1@tests.mozilla.org"],
  failedURL:        "/XPCShell/1/1.5/1.5/test1%40tests.mozilla.org",
  successfulIDs:  ["test1@tests.mozilla.org",
                     "{00000000-1111-2222-3333-444444444444}",
                     "test_AddonRepository_1@tests.mozilla.org"],
  successfulURL:    "/XPCShell/1/1.5/1.5/test1%40tests.mozilla.org," +
                    "%7B00000000-1111-2222-3333-444444444444%7D," +
                    "test_AddonRepository_1%40tests.mozilla.org"
};

// Values for testing AddonRepository.retrieveRecommendedAddons()
var RECOMMENDED_TEST = {
  preference:       PREF_GETADDONS_GETRECOMMENDED,
  preferenceValue:  BASE_URL + "/%OS%/%VERSION%/%API_VERSION%/" +
                    "%API_VERSION%/%MAX_RESULTS%",
  failedURL:        "/XPCShell/1/1.5/1.5/" + (2 * FAILED_MAX_RESULTS),
  successfulURL:    "/XPCShell/1/1.5/1.5/" + (2 * MAX_RESULTS)
};

// Values for testing AddonRepository.searchAddons()
var SEARCH_TEST = {
  searchTerms:      "odd=search:with&weird\"characters",
  preference:       PREF_GETADDONS_GETSEARCHRESULTS,
  preferenceValue:  BASE_URL + "/%OS%/%VERSION%/%API_VERSION%/" +
                    "%API_VERSION%/%MAX_RESULTS%/%TERMS%",
  failedURL:        "/XPCShell/1/1.5/1.5/" + (2 * FAILED_MAX_RESULTS) +
                    "/odd%3Dsearch%3Awith%26weird%22characters",
  successfulURL:    "/XPCShell/1/1.5/1.5/" + (2 * MAX_RESULTS) +
                    "/odd%3Dsearch%3Awith%26weird%22characters"
};

// Test that actual results and expected results are equal
function check_results(aActualAddons, aExpectedAddons, aAddonCount, aInstallNull) {
  do_check_false(AddonRepository.isSearching);

  do_check_eq(aActualAddons.length, aAddonCount);
  do_check_addons(aActualAddons, aExpectedAddons, ADDON_PROPERTIES);

  // Additional tests
  aActualAddons.forEach(function(aActualAddon) {
    // Separately check name so better messages are outputted when failure
    if (aActualAddon.name == "FAIL")
      do_throw(aActualAddon.id + " - " + aActualAddon.description);
    if (aActualAddon.name != "PASS")
      do_throw(aActualAddon.id + " - " + "invalid add-on name " + aActualAddon.name);

    do_check_eq(aActualAddon.install == null, !!aInstallNull || !aActualAddon.sourceURI);

    // Check that sourceURI property consistent within actual addon
    if (aActualAddon.install)
      do_check_eq(aActualAddon.install.sourceURI.spec, aActualAddon.sourceURI.spec);
  });
}

// Complete a search, also testing cancelSearch() and isSearching
function complete_search(aSearch, aSearchCallback) {
  var failCallback = {
    searchSucceeded: function(addons, length, total) {
      do_throw("failCallback.searchSucceeded should not be called");
      end_test();
    },

    searchFailed: function() {
      do_throw("failCallback.searchFailed should not be called");
      end_test();
    }
  };

  var callbackCalled = false;
  var testCallback = {
    searchSucceeded: function(addons, length, total) {
      do_throw("testCallback.searchSucceeded should not be called");
      end_test();
    },

    searchFailed: function() {
      callbackCalled = true;
    }
  };

  // Should fail because cancelled it immediately
  aSearch(failCallback);
  do_check_true(AddonRepository.isSearching);
  AddonRepository.cancelSearch();
  do_check_false(AddonRepository.isSearching);

  aSearch(aSearchCallback);
  do_check_true(AddonRepository.isSearching);

  // searchFailed should be called immediately because already searching
  aSearch(testCallback);
  do_check_true(callbackCalled);
  do_check_true(AddonRepository.isSearching);
}


function run_test() {
  // Setup for test
  do_test_pending();
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9");

  startupManager();

  // Install an add-on so can check that it isn't returned in the results
  installAllFiles([do_get_addon("test_AddonRepository_1")], function() {
    restartManager();

    gServer = new nsHttpServer();

    // Register other add-on XPI files
    gServer.registerFile(INSTALL_URL2,
                        do_get_addon("test_AddonRepository_2"));
    gServer.registerFile(INSTALL_URL3,
                        do_get_addon("test_AddonRepository_3"));

    // Register files used to test search failure
    gServer.registerFile(GET_TEST.failedURL,
                        do_get_file("data/test_AddonRepository_failed.xml"));
    gServer.registerFile(RECOMMENDED_TEST.failedURL,
                        do_get_file("data/test_AddonRepository_failed.xml"));
    gServer.registerFile(SEARCH_TEST.failedURL,
                        do_get_file("data/test_AddonRepository_failed.xml"));

    // Register files used to test search success
    gServer.registerFile(GET_TEST.successfulURL,
                        do_get_file("data/test_AddonRepository_getAddonsByIDs.xml"));
    gServer.registerFile(RECOMMENDED_TEST.successfulURL,
                        do_get_file("data/test_AddonRepository.xml"));
    gServer.registerFile(SEARCH_TEST.successfulURL,
                        do_get_file("data/test_AddonRepository.xml"));

    gServer.start(PORT);

    // Create an active AddonInstall so can check that it isn't returned in the results
    AddonManager.getInstallForURL(BASE_URL + INSTALL_URL2, function(aInstall) {
      aInstall.install();

      // Create a non-active AddonInstall so can check that it is returned in the results
      AddonManager.getInstallForURL(BASE_URL + INSTALL_URL3,
                                    run_test_1, "application/x-xpinstall");
    }, "application/x-xpinstall");
  });
}

function end_test() {
  gServer.stop(do_test_finished);
}

// Tests homepageURL, getRecommendedURL() and getSearchURL()
function run_test_1() {
  function check_urls(aPreference, aGetURL, aTests) {
    aTests.forEach(function(aTest) {
      Services.prefs.setCharPref(aPreference, aTest.preferenceValue);
      do_check_eq(aGetURL(aTest), aTest.expectedURL);
    });
  }

  var urlTests = [{
    preferenceValue:  BASE_URL,
    expectedURL:      BASE_URL
  }, {
    preferenceValue:  BASE_URL + "/%OS%/%VERSION%",
    expectedURL:      BASE_URL + "/XPCShell/1"
  }];

  // Extra tests for AddonRepository.getSearchURL();
  var searchURLTests = [{
    searchTerms:      "test",
    preferenceValue:  BASE_URL + "/search?q=%TERMS%",
    expectedURL:      BASE_URL + "/search?q=test"
  }, {
    searchTerms:      "test search",
    preferenceValue:  BASE_URL + "/%TERMS%",
    expectedURL:      BASE_URL + "/test%20search"
  }, {
    searchTerms:      "odd=search:with&weird\"characters",
    preferenceValue:  BASE_URL + "/%TERMS%",
    expectedURL:      BASE_URL + "/odd%3Dsearch%3Awith%26weird%22characters"
  }];

  // Setup tests for homepageURL, getRecommendedURL() and getSearchURL()
  var tests = [{
    initiallyUndefined: true,
    preference:         PREF_GETADDONS_BROWSEADDONS,
    urlTests:           urlTests,
    getURL:             function() AddonRepository.homepageURL
  }, {
    initiallyUndefined: true,
    preference:         PREF_GETADDONS_BROWSERECOMMENDED,
    urlTests:           urlTests,
    getURL:             function() AddonRepository.getRecommendedURL()
  }, {
    initiallyUndefined: false,
    preference:         PREF_GETADDONS_BROWSESEARCHRESULTS,
    urlTests:           urlTests.concat(searchURLTests),
    getURL:             function getSearchURL(aTest) {
                          var searchTerms = aTest && aTest.searchTerms ? aTest.searchTerms
                                                                       : "unused terms";
                          return AddonRepository.getSearchURL(searchTerms);
                        }
  }];

  tests.forEach(function(aTest) {
    if (aTest.initiallyUndefined) {
      // Preference is not defined by default
      do_check_eq(Services.prefs.getPrefType(aTest.preference),
                  Services.prefs.PREF_INVALID);
      do_check_eq(aTest.getURL(), DEFAULT_URL);
    }

    check_urls(aTest.preference, aTest.getURL, aTest.urlTests);
  });

  run_test_2();
}

// Tests failure of AddonRepository.getAddonsByIDs()
function run_test_2() {
  Services.prefs.setCharPref(GET_TEST.preference, GET_TEST.preferenceValue);
  var callback = {
    searchSucceeded: function(aAddonsList, aAddonCount, aTotalResults) {
      do_throw("searchAddons should not have succeeded");
      end_test();
    },

    searchFailed: function() {
      do_check_false(AddonRepository.isSearching);
      run_test_3();
    }
  };

  complete_search(function(aCallback) {
    AddonRepository.getAddonsByIDs(GET_TEST.failedIDs, aCallback);
  }, callback);
}

// Tests success of AddonRepository.getAddonsByIDs()
function run_test_3() {
  var callback = {
    searchSucceeded: function(aAddonsList, aAddonCount, aTotalResults) {
      do_check_eq(aTotalResults, -1);
      check_results(aAddonsList, GET_RESULTS, aAddonCount, true);
      run_test_4();
    },

    searchFailed: function() {
      do_throw("searchAddons should not have failed");
      end_test();
    }
  };

  complete_search(function(aCallback) {
    AddonRepository.getAddonsByIDs(GET_TEST.successfulIDs, aCallback);
  }, callback);
}

// Tests failure of AddonRepository.retrieveRecommendedAddons()
function run_test_4() {
  Services.prefs.setCharPref(RECOMMENDED_TEST.preference,
                             RECOMMENDED_TEST.preferenceValue);
  var callback = {
    searchSucceeded: function(aAddonsList, aAddonCount, aTotalResults) {
      do_throw("retrieveRecommendedAddons should not have succeeded");
      end_test();
    },

    searchFailed: function() {
      do_check_false(AddonRepository.isSearching);
      run_test_5();
    }
  };

  complete_search(function(aCallback) {
    AddonRepository.retrieveRecommendedAddons(FAILED_MAX_RESULTS, aCallback);
  }, callback);
}

// Tests success of AddonRepository.retrieveRecommendedAddons()
function run_test_5() {
  var callback = {
    searchSucceeded: function(aAddonsList, aAddonCount, aTotalResults) {
      do_check_eq(aTotalResults, -1);
      check_results(aAddonsList, SEARCH_RESULTS, aAddonCount);
      run_test_6();
    },

    searchFailed: function() {
      do_throw("retrieveRecommendedAddons should not have failed");
      end_test();
    }
  };

  complete_search(function(aCallback) {
    AddonRepository.retrieveRecommendedAddons(MAX_RESULTS, aCallback);
  }, callback);
}

// Tests failure of AddonRepository.searchAddons()
function run_test_6() {
  Services.prefs.setCharPref(SEARCH_TEST.preference, SEARCH_TEST.preferenceValue);
  var callback = {
    searchSucceeded: function(aAddonsList, aAddonCount, aTotalResults) {
      do_throw("searchAddons should not have succeeded");
      end_test();
    },

    searchFailed: function() {
      do_check_false(AddonRepository.isSearching);
      run_test_7();
    }
  };

  complete_search(function(aCallback) {
    var searchTerms = SEARCH_TEST.searchTerms;
    AddonRepository.searchAddons(searchTerms, FAILED_MAX_RESULTS, aCallback);
  }, callback);
}

// Tests success of AddonRepository.searchAddons()
function run_test_7() {
  var callback = {
    searchSucceeded: function(aAddonsList, aAddonCount, aTotalResults) {
      do_check_eq(aTotalResults, TOTAL_RESULTS);
      check_results(aAddonsList, SEARCH_RESULTS, aAddonCount);
      end_test();
    },

    searchFailed: function() {
      do_throw("searchAddons should not have failed");
      end_test();
    }
  };

  complete_search(function(aCallback) {
    var searchTerms = SEARCH_TEST.searchTerms;
    AddonRepository.searchAddons(searchTerms, MAX_RESULTS, aCallback);
  }, callback);
}

