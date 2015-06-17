Cu.import("resource://gre/modules/FormHistory.jsm");

const ENGINE_NAME = "engine-suggestions.xml";
const SERVER_PORT = 9000;
const SUGGEST_PREF = "browser.urlbar.suggest.searches";

// Set this to some other function to change how the server converts search
// strings into suggestions.
let suggestionsFromSearchString = searchStr => {
  let suffixes = ["foo", "bar"];
  return suffixes.map(s => searchStr + " " + s);
};

add_task(function* setUp() {
  // Set up a server that provides some suggestions by appending strings onto
  // the search query.
  let server = makeTestServer(SERVER_PORT);
  server.registerPathHandler("/suggest", (req, resp) => {
    // URL query params are x-www-form-urlencoded, which converts spaces into
    // plus signs, so un-convert any plus signs back to spaces.
    let searchStr = decodeURIComponent(req.queryString.replace(/\+/g, " "));
    let suggestions = suggestionsFromSearchString(searchStr);
    let data = [searchStr, suggestions];
    resp.setHeader("Content-Type", "application/json", false);
    resp.write(JSON.stringify(data));
  });

  // Install the test engine.
  let oldCurrentEngine = Services.search.currentEngine;
  do_register_cleanup(() => Services.search.currentEngine = oldCurrentEngine);
  let engine = yield addTestEngine(ENGINE_NAME, server);
  Services.search.currentEngine = engine;

  yield cleanup();
});

add_task(function* disabled() {
  Services.prefs.setBoolPref(SUGGEST_PREF, false);
  yield check_autocomplete({
    search: "hello",
    matches: [],
  });
  yield cleanup();
});

add_task(function* singleWordQuery() {
  Services.prefs.setBoolPref(SUGGEST_PREF, true);

  let searchStr = "hello";
  yield check_autocomplete({
    search: searchStr,
    matches: [{
      uri: makeActionURI(("searchengine"), {
        engineName: ENGINE_NAME,
        input: searchStr,
        searchQuery: searchStr,
        searchSuggestion: "hello foo",
      }),
      title: ENGINE_NAME,
      style: ["action", "searchengine"],
      icon: "",
    }, {
      uri: makeActionURI(("searchengine"), {
        engineName: ENGINE_NAME,
        input: searchStr,
        searchQuery: searchStr,
        searchSuggestion: "hello bar",
      }),
      title: ENGINE_NAME,
      style: ["action", "searchengine"],
      icon: "",
    }],
  });

  yield cleanup();
});

add_task(function* multiWordQuery() {
  Services.prefs.setBoolPref(SUGGEST_PREF, true);

  let searchStr = "hello world";
  yield check_autocomplete({
    search: searchStr,
    matches: [{
      uri: makeActionURI(("searchengine"), {
        engineName: ENGINE_NAME,
        input: searchStr,
        searchQuery: searchStr,
        searchSuggestion: "hello world foo",
      }),
      title: ENGINE_NAME,
      style: ["action", "searchengine"],
      icon: "",
    }, {
      uri: makeActionURI(("searchengine"), {
        engineName: ENGINE_NAME,
        input: searchStr,
        searchQuery: searchStr,
        searchSuggestion: "hello world bar",
      }),
      title: ENGINE_NAME,
      style: ["action", "searchengine"],
      icon: "",
    }],
  });

  yield cleanup();
});

add_task(function* suffixMatch() {
  Services.prefs.setBoolPref(SUGGEST_PREF, true);

  let oldFn = suggestionsFromSearchString;
  suggestionsFromSearchString = searchStr => {
    let prefixes = ["baz", "quux"];
    return prefixes.map(p => p + " " + searchStr);
  };

  let searchStr = "hello";
  yield check_autocomplete({
    search: searchStr,
    matches: [{
      uri: makeActionURI(("searchengine"), {
        engineName: ENGINE_NAME,
        input: searchStr,
        searchQuery: searchStr,
        searchSuggestion: "baz hello",
      }),
      title: ENGINE_NAME,
      style: ["action", "searchengine"],
      icon: "",
    }, {
      uri: makeActionURI(("searchengine"), {
        engineName: ENGINE_NAME,
        input: searchStr,
        searchQuery: searchStr,
        searchSuggestion: "quux hello",
      }),
      title: ENGINE_NAME,
      style: ["action", "searchengine"],
      icon: "",
    }],
  });

  suggestionsFromSearchString = oldFn;
  yield cleanup();
});
