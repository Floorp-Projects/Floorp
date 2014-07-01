Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "NetUtil",
                                  "resource://gre/modules/NetUtil.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Promise",
                                  "resource://gre/modules/Promise.jsm");
// Global test server for serving safebrowsing updates.
let gHttpServ = null;
// Global nsIUrlClassifierDBService
let gDbService = Cc["@mozilla.org/url-classifier/dbservice;1"]
  .getService(Ci.nsIUrlClassifierDBService);
// Security manager for creating nsIPrincipals from URIs
let gSecMan = Cc["@mozilla.org/scriptsecuritymanager;1"]
  .getService(Ci.nsIScriptSecurityManager);

// A map of tables to arrays of update redirect urls.
let gTables = {};

// Construct an update from a file.
function readFileToString(aFilename) {
  let f = do_get_file(aFilename);
  let stream = Cc["@mozilla.org/network/file-input-stream;1"]
    .createInstance(Ci.nsIFileInputStream);
  stream.init(f, -1, 0, 0);
  let buf = NetUtil.readInputStreamToString(stream, stream.available());
  return buf;
}

// Registers a table for which to serve update chunks. Returns a promise that
// resolves when that chunk has been downloaded.
function registerTableUpdate(aTable, aFilename) {
  let deferred = Promise.defer();
  // If we haven't been given an update for this table yet, add it to the map
  if (!(aTable in gTables)) {
    gTables[aTable] = [];
  }

  // The number of chunks associated with this table.
  let numChunks = gTables[aTable].length + 1;
  let redirectPath = "/" + aTable + "-" + numChunks;
  let redirectUrl = "localhost:4444" + redirectPath;

  // Store redirect url for that table so we can return it later when we
  // process an update request.
  gTables[aTable].push(redirectUrl);

  gHttpServ.registerPathHandler(redirectPath, function(request, response) {
    do_print("Mock safebrowsing server handling request for " + redirectPath);
    let contents = readFileToString(aFilename);
    response.setHeader("Content-Type",
                       "application/vnd.google.safebrowsing-update", false);
    response.setStatusLine(request.httpVersion, 200, "OK");
    response.bodyOutputStream.write(contents, contents.length);
    deferred.resolve(contents);
  });
  return deferred.promise;
}

// Construct a response with redirect urls.
function processUpdateRequest() {
  let response = "n:1000\n";
  for (let table in gTables) {
    response += "i:" + table + "\n";
    for (let i = 0; i < gTables[table].length; ++i) {
      response += "u:" + gTables[table][i] + "\n";
    }
  }
  do_print("Returning update response: " + response);
  return response;
}

// Set up our test server to handle update requests.
function run_test() {
  gHttpServ = new HttpServer();
  gHttpServ.registerDirectory("/", do_get_cwd());

  gHttpServ.registerPathHandler("/downloads", function(request, response) {
    let buf = NetUtil.readInputStreamToString(request.bodyInputStream,
      request.bodyInputStream.available());
    let blob = processUpdateRequest();
    response.setHeader("Content-Type",
                       "application/vnd.google.safebrowsing-update", false);
    response.setStatusLine(request.httpVersion, 200, "OK");
    response.bodyOutputStream.write(blob, blob.length);
  });

  gHttpServ.start(4444);
  run_next_test();
}

function createURI(s) {
  let service = Cc["@mozilla.org/network/io-service;1"]
    .getService(Ci.nsIIOService);
  return service.newURI(s, null, null);
}

// Just throw if we ever get an update or download error.
function handleError(aEvent) {
  do_throw("We didn't download or update correctly: " + aEvent);
}

add_test(function test_update() {
  let streamUpdater = Cc["@mozilla.org/url-classifier/streamupdater;1"]
    .getService(Ci.nsIUrlClassifierStreamUpdater);

  // Load up some update chunks for the safebrowsing server to serve.
  registerTableUpdate("goog-downloadwhite-digest256", "data/digest1.chunk");
  registerTableUpdate("goog-downloadwhite-digest256", "data/digest2.chunk");

  // Download some updates, and don't continue until the downloads are done.
  function updateSuccess(aEvent) {
    // Timeout of n:1000 is constructed in processUpdateRequest above and
    // passed back in the callback in nsIUrlClassifierStreamUpdater on success.
    do_check_eq("1000", aEvent);
    do_print("All data processed");
    run_next_test();
  }
  streamUpdater.downloadUpdates(
    "goog-downloadwhite-digest256",
    "goog-downloadwhite-digest256;\n",
    "http://localhost:4444/downloads",
    updateSuccess, handleError, handleError);
});

add_test(function test_url_not_whitelisted() {
  let uri = createURI("http://example.com");
  let principal = gSecMan.getNoAppCodebasePrincipal(uri);
  gDbService.lookup(principal, "goog-downloadwhite-digest256",
    function handleEvent(aEvent) {
      // This URI is not on any lists.
      do_check_eq("", aEvent);
      run_next_test();
    });
});

add_test(function test_url_whitelisted() {
  // Hash of "whitelisted.com/" (canonicalized URL) is:
  // 93CA5F48E15E9861CD37C2D95DB43D23CC6E6DE5C3F8FA6E8BE66F97CC518907
  let uri = createURI("http://whitelisted.com");
  let principal = gSecMan.getNoAppCodebasePrincipal(uri);
  gDbService.lookup(principal, "goog-downloadwhite-digest256",
    function handleEvent(aEvent) {
      do_check_eq("goog-downloadwhite-digest256", aEvent);
      run_next_test();
    });
});
