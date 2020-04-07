/**
 * Test for bug 1211726 - preload list of top web sites for better
 * autocompletion on empty profiles.
 */

const PREF_FEATURE_ENABLED = "browser.urlbar.usepreloadedtopurls.enabled";
const PREF_FEATURE_EXPIRE_DAYS =
  "browser.urlbar.usepreloadedtopurls.expire_days";

const autocompleteObject = Cc[
  "@mozilla.org/autocomplete/search;1?name=unifiedcomplete"
].getService(Ci.mozIPlacesAutoComplete);

Cu.importGlobalProperties(["fetch"]);

// With or without trailing slash - no matter. URI.spec does have it always.
// Then, check_autocomplete() doesn't cut it off (uses stripPrefix()).
let yahoooURI = NetUtil.newURI("https://yahooo.com/");
let gooogleURI = NetUtil.newURI("https://gooogle.com/");

autocompleteObject.populatePreloadedSiteStorage([
  [yahoooURI.spec, "Yahooo"],
  [gooogleURI.spec, "Gooogle"],
]);

async function assert_feature_works(condition) {
  info("List Results do appear " + condition);
  await check_autocomplete({
    search: "ooo",
    matches: [
      { uri: yahoooURI, title: "Yahooo", style: ["preloaded-top-site"] },
      { uri: gooogleURI, title: "Gooogle", style: ["preloaded-top-site"] },
    ],
  });

  info("Autofill does appear " + condition);
  await check_autocomplete({
    search: "gooo",
    autofilled: "gooogle.com/", // Will fail without trailing slash
    completed: "https://gooogle.com/",
  });
}

async function assert_feature_does_not_appear(condition) {
  info("List Results don't appear " + condition);
  await check_autocomplete({
    search: "ooo",
    matches: [],
  });

  info("Autofill doesn't appear " + condition);
  // "search" is what you type,
  // "autofilled" is what you get in response in the url bar,
  // "completed" is what you get there when you hit enter.
  // So if they are all equal - it's the proof there was no autofill
  // (knowing we have a suitable preloaded top site).
  await check_autocomplete({
    search: "gooo",
    autofilled: "gooo",
    completed: "gooo",
  });
}

add_task(async function test_it_works() {
  // Not expired but OFF
  Services.prefs.setIntPref(PREF_FEATURE_EXPIRE_DAYS, 14);
  Services.prefs.setBoolPref(PREF_FEATURE_ENABLED, false);
  await assert_feature_does_not_appear("when OFF by prefs");

  // Now turn it ON
  Services.prefs.setBoolPref(PREF_FEATURE_ENABLED, true);
  await assert_feature_works("when ON by prefs");

  // And expire
  Services.prefs.setIntPref(PREF_FEATURE_EXPIRE_DAYS, 0);
  await assert_feature_does_not_appear("when expired");

  await cleanup();
});

add_task(async function test_sorting_against_bookmark() {
  let boookmarkURI = NetUtil.newURI("https://boookmark.com");
  await PlacesTestUtils.addBookmarkWithDetails({
    uri: boookmarkURI,
    title: "Boookmark",
  });

  Services.prefs.setBoolPref(PREF_FEATURE_ENABLED, true);
  Services.prefs.setIntPref(PREF_FEATURE_EXPIRE_DAYS, 14);

  info("Preloaded Top Sites are placed lower than Bookmarks");
  await check_autocomplete({
    checkSorting: true,
    search: "ooo",
    matches: [
      { uri: boookmarkURI, title: "Boookmark", style: ["bookmark"] },
      { uri: yahoooURI, title: "Yahooo", style: ["preloaded-top-site"] },
      { uri: gooogleURI, title: "Gooogle", style: ["preloaded-top-site"] },
    ],
  });

  await cleanup();
});

add_task(async function test_sorting_against_history() {
  let histoooryURI = NetUtil.newURI("https://histooory.com");
  await PlacesTestUtils.addVisits({ uri: histoooryURI, title: "Histooory" });

  Services.prefs.setBoolPref(PREF_FEATURE_ENABLED, true);
  Services.prefs.setIntPref(PREF_FEATURE_EXPIRE_DAYS, 14);

  info("Preloaded Top Sites are placed lower than History entries");
  await check_autocomplete({
    checkSorting: true,
    search: "ooo",
    matches: [
      { uri: histoooryURI, title: "Histooory" },
      { uri: yahoooURI, title: "Yahooo", style: ["preloaded-top-site"] },
      { uri: gooogleURI, title: "Gooogle", style: ["preloaded-top-site"] },
    ],
  });

  await cleanup();
});

add_task(async function test_scheme_and_www() {
  // Order is important to check sorting
  let sites = [
    ["https://www.ooops-https-www.com/", "Ooops"],
    ["https://ooops-https.com/", "Ooops"],
    ["HTTP://ooops-HTTP.com/", "Ooops"],
    ["HTTP://www.ooops-HTTP-www.com/", "Ooops"],
    ["https://foo.com/", "Title with www"],
    ["https://www.bar.com/", "Tile"],
  ];

  let titlesMap = new Map(sites);

  autocompleteObject.populatePreloadedSiteStorage(sites);

  let tests = [
    // User typed,
    // Inline autofill (`autofilled`),
    // Substitute after enter is pressed (`completed`),
    //   [List matches, with sorting]
    //   not tested if omitted
    //   !!! first one is always an autofill entry !!!

    [
      // Protocol by itself doesn't match anything
      "https://",
      "https://",
      "https://",
      [],
    ],

    [
      "www.",
      "www.ooops-https-www.com/",
      "https://www.ooops-https-www.com/",
      [
        ["www.ooops-https-www.com/", "https://www.ooops-https-www.com"],
        "HTTP://www.ooops-HTTP-www.com/",
        "https://www.bar.com/",
      ],
    ],

    [
      "http://www.",
      "http://www.ooops-http-www.com/",
      "http://www.ooops-http-www.com/",
      [["http://www.ooops-http-www.com/", "www.ooops-http-www.com"]],
    ],

    ["ftp://ooops", "ftp://ooops", "ftp://ooops", []],

    [
      "ww",
      "www.ooops-https-www.com/",
      "https://www.ooops-https-www.com/",
      [
        ["www.ooops-https-www.com/", "https://www.ooops-https-www.com"],
        "HTTP://www.ooops-HTTP-www.com/",
        ["https://foo.com/", "Title with www", ["preloaded-top-site"]],
        "https://www.bar.com/",
      ],
    ],

    [
      "ooops",
      "ooops-https-www.com/",
      "https://www.ooops-https-www.com/",
      [
        ["ooops-https-www.com/", "https://www.ooops-https-www.com"],
        "https://ooops-https.com/",
        "HTTP://ooops-HTTP.com/",
        "HTTP://www.ooops-HTTP-www.com/",
      ],
    ],

    [
      "www.ooops",
      "www.ooops-https-www.com/",
      "https://www.ooops-https-www.com/",
      [
        ["www.ooops-https-www.com/", "https://www.ooops-https-www.com"],
        "HTTP://www.ooops-HTTP-www.com/",
      ],
    ],

    [
      "ooops-https-www",
      "ooops-https-www.com/",
      "https://www.ooops-https-www.com/",
      [["ooops-https-www.com/", "https://www.ooops-https-www.com"]],
    ],

    ["www.ooops-https.", "www.ooops-https.", "www.ooops-https.", []],

    [
      "https://ooops",
      "https://ooops-https-www.com/",
      "https://www.ooops-https-www.com/",
      [
        ["https://ooops-https-www.com/", "https://www.ooops-https-www.com"],
        "https://ooops-https.com/",
      ],
    ],

    [
      "https://www.ooops",
      "https://www.ooops-https-www.com/",
      "https://www.ooops-https-www.com/",
      [["https://www.ooops-https-www.com/", "https://www.ooops-https-www.com"]],
    ],

    [
      "http://www.ooops-http.",
      "http://www.ooops-http.",
      "http://www.ooops-http.",
      [],
    ],

    ["http://ooops-https", "http://ooops-https", "http://ooops-https", []],
  ];

  function toMatch(entry, index) {
    if (Array.isArray(entry)) {
      return {
        value: entry[0],
        comment: entry[1],
        style: entry[2] || ["autofill", "heuristic", "preloaded-top-site"],
      };
    }
    return {
      uri: NetUtil.newURI(entry),
      title: titlesMap.get(entry),
      style: ["preloaded-top-site"],
    };
  }

  for (let test of tests) {
    let matches = test[3] ? test[3].map(toMatch) : null;
    info("User types: " + test[0]);
    await check_autocomplete({
      checkSorting: true,
      search: test[0],
      autofilled: test[1].toLowerCase(),
      completed: test[2].toLowerCase(),
      matches,
    });
  }

  await cleanup();
});

add_task(async function test_data_file() {
  let response = await fetch(
    "chrome://global/content/unifiedcomplete-top-urls.json"
  );

  info("Source file is supplied and fetched OK");
  Assert.ok(response.ok);

  info("The JSON is parsed");
  let sites = await response.json();

  info("Storage is populated");
  autocompleteObject.populatePreloadedSiteStorage(sites);

  let lastSite = sites.pop();
  let uri = NetUtil.newURI(lastSite[0]);

  info("Storage is populated from JSON correctly");
  await check_autocomplete({
    search: uri.host,
    autofilled: uri.host + "/",
    completed: uri.spec,
  });

  await cleanup();
});

add_task(async function test_partial_scheme() {
  // "tt" should not result in a match of "ttps://whatever.com/".
  autocompleteObject.populatePreloadedSiteStorage([
    ["http://www.ttt.com/", "Test"],
  ]);
  await check_autocomplete({
    search: "tt",
    autofilled: "ttt.com/",
    completed: "http://www.ttt.com/",
    matches: [
      {
        value: "ttt.com/",
        comment: "www.ttt.com",
        style: ["autofill", "heuristic", "preloaded-top-site"],
      },
    ],
  });
  await cleanup();
});
