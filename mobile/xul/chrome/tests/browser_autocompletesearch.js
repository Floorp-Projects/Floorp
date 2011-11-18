Components.utils.import("resource://gre/modules/Services.jsm");

let match= [
  ["http://example.com/a", "A", "favicon", "http://example.com/a/favicon.png"],
  ["http://example.com/b", "B", "favicon", "http://example.com/b/favicon.png"],
  ["http://example.com/c", "C", "favicon", "http://example.com/c/favicon.png"],
  ["http://example.com/d", "D", "bookmark", "http://example.com/d/favicon.png"],
  ["http://example.com/e", "E", "boolmark", "http://example.com/e/favicon.png"]
];

var gAutocomplete = null;
var gProfileDir = null;

function test() {
  waitForExplicitFinish();

  gProfileDir = Services.dirsvc.get("ProfD", Ci.nsIFile);

  // First we need to remove the existing cache file so we can control the state of the service
  let oldCacheFile = gProfileDir.clone();
  oldCacheFile.append("autocomplete.json");
  if (oldCacheFile.exists())
    oldCacheFile.remove(true);

  // Since we removed the cache file, we know the service will need to write out a new
  // file. We use that as a trigger to move forward.
  Services.obs.addObserver(function (aSubject, aTopic, aData) {
    Services.obs.removeObserver(arguments.callee, aTopic, false);
    saveMockCache();
  }, "browser:cache-session-history-write-complete", false);

  // This might trigger an init or it may have already happened. That's why we need
  // to do some work to control the state.
  gAutocomplete = Cc["@mozilla.org/autocomplete/search;1?name=history"].getService(Ci.nsIAutoCompleteSearch);

  // Trigger the new cache to be written out, since the existing was removed
  Services.obs.notifyObservers(null, "browser:cache-session-history-reload", "");
}

function saveMockCache() {
  // Now we write our own mock data cache into the profile
  let oldCacheFile = gProfileDir.clone();
  oldCacheFile.append("autocomplete.json");
  if (oldCacheFile.exists())
    oldCacheFile.remove(true);

  let mockCachePath = gTestPath;
  info("mock path: " + mockCachePath);
  let mockCacheURI = getResolvedURI(mockCachePath);
  info("mock URI: " + mockCacheURI.spec);
  if (mockCacheURI instanceof Ci.nsIJARURI) {
    // Android tests are stored in a JAR file, so we need to extract the mock_autocomplete.json file
    info("jar file: " + mockCacheURI.JARFile.spec);
    let zReader = Cc["@mozilla.org/libjar/zip-reader;1"].createInstance(Ci.nsIZipReader);
    let fileHandler = Cc["@mozilla.org/network/protocol;1?name=file"].getService(Ci.nsIFileProtocolHandler);
    let fileName = fileHandler.getFileFromURLSpec(mockCacheURI.JARFile.spec);
    zReader.open(fileName);

    let extract = mockCacheURI.spec.split("!")[1];
    extract = extract.substring(1, extract.lastIndexOf("/") + 1);
    extract += "mock_autocomplete.json";
    info("extract path: " + extract);
    let target = gProfileDir.clone();
    target.append("autocomplete.json");
    info("target path: " + target.path);
    zReader.extract(extract, target);
  } else {
    // Tests are run from a folder, so we can just copy the mock_autocomplete.json file
    let mockCacheFile = getChromeDir(mockCacheURI);
    info("mock file: " + mockCacheFile.path);
    mockCacheFile.append("mock_autocomplete.json");
    mockCacheFile.copyToFollowingLinks(gProfileDir, "autocomplete.json");
  }

  // Listen for when the mock cache has been loaded
  Services.obs.addObserver(function (aSubject, aTopic, aData) {
    Services.obs.removeObserver(arguments.callee, aTopic, false);
    runTest();
  }, "browser:cache-session-history-read-complete", false);

  // Trigger the new mock cache to be loaded
  Services.obs.notifyObservers(null, "browser:cache-session-history-reload", "");
}

function runTest() {
  let cacheFile = gProfileDir.clone();
  cacheFile.append("autocomplete.json");
  ok(cacheFile.exists(), "Mock autocomplete JSON cache file exists");

  // Compare the mock data, which should be used now that we loaded it into the service
  gAutocomplete.startSearch("", "", null, {
    onSearchResult: function(search, result) {
      is(result.matchCount, 5, "matchCount is correct");
      
      for (let i=0; i<5; i++) {
        is(result.getValueAt(i), match[i][0], "value matches");
        is(result.getCommentAt(i), match[i][1], "comment matches");
        is(result.getStyleAt(i), match[i][2], "style matches");
        is(result.getImageAt(i), match[i][3], "image matches");
      }

      if (cacheFile.exists())
        cacheFile.remove(true);

      finish();
    }
  });
}
