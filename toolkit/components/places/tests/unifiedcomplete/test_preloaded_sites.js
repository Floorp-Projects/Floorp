/**
 * Test for bug 1211726 - preload list of top web sites for better
 * autocompletion on empty profiles.
 */

const PREF_FEATURE_ENABLED = "browser.urlbar.usepreloadedtopurls.enabled";
const PREF_FEATURE_EXPIRE_DAYS = "browser.urlbar.usepreloadedtopurls.expire_days";

const autocompleteObject = Cc["@mozilla.org/autocomplete/search;1?name=unifiedcomplete"]
                             .getService(Ci.mozIPlacesAutoComplete);

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
  do_print("List Results do appear " + condition);
  await check_autocomplete({
    search: "ooo",
    matches: [
      { uri: yahoooURI, title: "Yahooo",  style: ["preloaded-top-site"] },
      { uri: gooogleURI, title: "Gooogle", style: ["preloaded-top-site"] },
    ],
  });

  do_print("Autofill does appear " + condition);
  await check_autocomplete({
    search: "gooo",
    autofilled: "gooogle.com/", // Will fail without trailing slash
    completed: "https://gooogle.com/",
  });
}

async function assert_feature_does_not_appear(condition) {
  do_print("List Results don't appear " + condition);
  await check_autocomplete({
    search: "ooo",
    matches: [],
  });

  do_print("Autofill doesn't appear " + condition);
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
  await addBookmark( { uri: boookmarkURI, title: "Boookmark" } );

  Services.prefs.setBoolPref(PREF_FEATURE_ENABLED, true);
  Services.prefs.setIntPref(PREF_FEATURE_EXPIRE_DAYS, 14);

  do_print("Preloaded Top Sites are placed lower than Bookmarks");
  await check_autocomplete({
    checkSorting: true,
    search: "ooo",
    matches: [
      { uri: boookmarkURI, title: "Boookmark",  style: ["bookmark"] },
      { uri: yahoooURI, title: "Yahooo",  style: ["preloaded-top-site"] },
      { uri: gooogleURI, title: "Gooogle", style: ["preloaded-top-site"] },
    ],
  });

  await cleanup();
});

add_task(async function test_sorting_against_history() {
  let histoooryURI = NetUtil.newURI("https://histooory.com");
  await PlacesTestUtils.addVisits( { uri: histoooryURI, title: "Histooory" } );

  Services.prefs.setBoolPref(PREF_FEATURE_ENABLED, true);
  Services.prefs.setIntPref(PREF_FEATURE_EXPIRE_DAYS, 14);

  do_print("Preloaded Top Sites are placed lower than History entries");
  await check_autocomplete({
    checkSorting: true,
    search: "ooo",
    matches: [
      { uri: histoooryURI, title: "Histooory" },
      { uri: yahoooURI, title: "Yahooo",  style: ["preloaded-top-site"] },
      { uri: gooogleURI, title: "Gooogle", style: ["preloaded-top-site"] },
    ],
  });

  await cleanup();
});

add_task(async function test_scheme_and_www() {
  // Order is important to check sorting
  let sites = [
    ["https://www.ooops-https-www.com/", "Ooops"],
    ["https://ooops-https.com/",         "Ooops"],
    ["HTTP://ooops-HTTP.com/",           "Ooops"],
    ["HTTP://www.ooops-HTTP-www.com/",   "Ooops"],
    ["https://foo.com/",     "Title with www"],
    ["https://www.bar.com/", "Tile"],
  ];

  let titlesMap = new Map(sites);

  autocompleteObject.populatePreloadedSiteStorage(sites);

  let tests =
  [
    // User typed,
    // Inline autofill,
    // Substitute after enter is pressed,
    //   [List matches, with sorting]
    //   not tested if omitted
    //   !!! first one is always an autofill entry !!!

    [// Protocol by itself doesn't match anything
    "https://",
    "https://",
    "https://",
      []
    ],

    [// "www." by itself doesn't match anything
    "www.",
    "www.",
    "www.",
      []
    ],

    [// Protocol with "www." by itself doesn't match anything
    "http://www.",
    "http://www.",
    "http://www.",
      []
    ],

    [// ftp: - ignore
    "ftp://ooops",
    "ftp://ooops",
    "ftp://ooops",
      []
    ],

    [// Edge case: no "www." in search string, autofill and list entries with "www."
    "ww",
    "www.ooops-https-www.com/",
    "https://www.ooops-https-www.com/", // 2nd in list, but has priority as strict
      [
      ["https://www.ooops-https-www.com/", "https://www.ooops-https-www.com"],
      "HTTP://www.ooops-HTTP-www.com/",
      ["https://foo.com/", "Title with www", ["preloaded-top-site"]],
      "https://www.bar.com/",
      ]
    ],

    [// Strict match, no "www."
    "ooops",
    "ooops-https.com/",
    "https://ooops-https.com/", // 2nd in list, but has priority as strict
      [// List entries are not sorted (initial sorting preserved)
       // except autofill entry is on top as always
      ["https://ooops-https.com/", "https://ooops-https.com"],
      "https://www.ooops-https-www.com/",
      "HTTP://ooops-HTTP.com/",
      "HTTP://www.ooops-HTTP-www.com/",
      ]
    ],

    [// Strict match with "www."
    "www.ooops",
    "www.ooops-https-www.com/",
    "https://www.ooops-https-www.com/",
      [// Matches with "www." sorted on top
      ["https://www.ooops-https-www.com/", "https://www.ooops-https-www.com"],
      "HTTP://www.ooops-HTTP-www.com/",
      "https://ooops-https.com/",
      "HTTP://ooops-HTTP.com/",
      ]
    ],

    [// Loose match: search no "www.", result with "www."
    "ooops-https-www",
    "ooops-https-www.com/",
    "https://www.ooops-https-www.com/",
      [
      ["https://www.ooops-https-www.com/", "https://www.ooops-https-www.com"],
      ]
    ],

    [// Loose match: search "www.", no-www site gets "www."
    "www.ooops-https.",
    "www.ooops-https.com/",
    "https://www.ooops-https.com/",
      [// Only autofill entry gets "www."
      ["https://www.ooops-https.com/", "https://www.ooops-https.com"],
      "https://ooops-https.com/", // List entry with preloaded top URL for match site
      ]
    ],

    [// Explicit protocol, no "www."
    "https://ooops",
    "https://ooops-https.com/",
    "https://ooops-https.com/",
      [
      ["https://ooops-https.com/", "https://ooops-https.com"],
      "https://www.ooops-https-www.com/",
      ]
    ],

    [// Explicit protocol, with "www."
    "https://www.ooops",
    "https://www.ooops-https-www.com/",
    "https://www.ooops-https-www.com/",
      [
      ["https://www.ooops-https-www.com/", "https://www.ooops-https-www.com"],
      "https://ooops-https.com/",
      ]
    ],

    [// Explicit HTTP protocol, no-www site gets "www."
    "http://www.ooops-http.",
    "http://www.ooops-http.com/",
    "http://www.ooops-http.com/",
      [
      ["HTTP://www.ooops-HTTP.com/", "www.ooops-http.com"],
      "HTTP://ooops-HTTP.com/",
      ]
    ],

    [// Wrong protocol
    "http://ooops-https",
    "http://ooops-https",
    "http://ooops-https",
      []
    ],
  ];

  function toMatch(entry, index) {
    if (Array.isArray(entry)) {
      return {
        uri: NetUtil.newURI(entry[0]),
        title: entry[1],
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
    do_print("User types: " + test[0]);
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
  let response = await fetch("chrome://global/content/unifiedcomplete-top-urls.json");

  do_print("Source file is supplied and fetched OK");
  Assert.ok(response.ok);

  do_print("The JSON is parsed");
  let sites = await response.json();

  do_print("Storage is populated");
  autocompleteObject.populatePreloadedSiteStorage(sites);

  let lastSite = sites.pop();
  let uri = NetUtil.newURI(lastSite[0]);

  do_print("Storage is populated from JSON correctly");
  await check_autocomplete({
    search: uri.host,
    autofilled: uri.host + "/",
    completed: uri.spec,
  });

  await cleanup();
});
