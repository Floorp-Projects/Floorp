/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Cu.import("resource://gre/modules/Log.jsm");
Cu.import("resource://gre/modules/Preferences.jsm");
Cu.import("resource://services-sync/addonutils.js");
Cu.import("resource://services-sync/util.js");

const HTTP_PORT = 8888;
const SERVER_ADDRESS = "http://127.0.0.1:8888";

var prefs = new Preferences();

prefs.set("extensions.getAddons.get.url",
          SERVER_ADDRESS + "/search/guid:%IDS%");

loadAddonTestFunctions();
startupManager();

function createAndStartHTTPServer(port = HTTP_PORT) {
  try {
    let server = new HttpServer();

    server.registerFile("/search/guid:missing-sourceuri%40tests.mozilla.org",
                        do_get_file("missing-sourceuri.xml"));

    server.registerFile("/search/guid:rewrite%40tests.mozilla.org",
                        do_get_file("rewrite-search.xml"));

    server.start(port);

    return server;
  } catch (ex) {
    _("Got exception starting HTTP server on port " + port);
    _("Error: " + Log.exceptionStr(ex));
    do_throw(ex);
  }
  return null; /* not hit, but keeps eslint happy! */
}

function run_test() {
  initTestLogging("Trace");

  run_next_test();
}

add_test(function test_handle_empty_source_uri() {
  _("Ensure that search results without a sourceURI are properly ignored.");

  let server = createAndStartHTTPServer();

  const ID = "missing-sourceuri@tests.mozilla.org";

  let cb = Async.makeSpinningCallback();
  AddonUtils.installAddons([{id: ID, requireSecureURI: false}], cb);
  let result = cb.wait();

  Assert.ok("installedIDs" in result);
  Assert.equal(0, result.installedIDs.length);

  Assert.ok("skipped" in result);
  Assert.ok(result.skipped.includes(ID));

  server.stop(run_next_test);
});

add_test(function test_ignore_untrusted_source_uris() {
  _("Ensures that source URIs from insecure schemes are rejected.");

  const bad = ["http://example.com/foo.xpi",
               "ftp://example.com/foo.xpi",
               "silly://example.com/foo.xpi"];

  const good = ["https://example.com/foo.xpi"];

  for (let s of bad) {
    let sourceURI = Services.io.newURI(s);
    let addon = {sourceURI, name: "bad", id: "bad"};

    let canInstall = AddonUtils.canInstallAddon(addon);
    Assert.ok(!canInstall, "Correctly rejected a bad URL");
  }

  for (let s of good) {
    let sourceURI = Services.io.newURI(s);
    let addon = {sourceURI, name: "good", id: "good"};

    let canInstall = AddonUtils.canInstallAddon(addon);
    Assert.ok(canInstall, "Correctly accepted a good URL");
  }
  run_next_test();
});

add_test(function test_source_uri_rewrite() {
  _("Ensure that a 'src=api' query string is rewritten to 'src=sync'");

  // This tests for conformance with bug 708134 so server-side metrics aren't
  // skewed.

  // We resort to monkeypatching because of the API design.
  let oldFunction = AddonUtils.__proto__.installAddonFromSearchResult;

  let installCalled = false;
  AddonUtils.__proto__.installAddonFromSearchResult =
    function testInstallAddon(addon, metadata, cb) {

    Assert.equal(SERVER_ADDRESS + "/require.xpi?src=sync",
                 addon.sourceURI.spec);

    installCalled = true;

    AddonUtils.getInstallFromSearchResult(addon, function(error, install) {
      Assert.equal(null, error);
      Assert.equal(SERVER_ADDRESS + "/require.xpi?src=sync",
                   install.sourceURI.spec);

      cb(null, {id: addon.id, addon, install});
    }, false);
  };

  let server = createAndStartHTTPServer();

  let installCallback = Async.makeSpinningCallback();
  let installOptions = {
    id: "rewrite@tests.mozilla.org",
    requireSecureURI: false,
  };
  AddonUtils.installAddons([installOptions], installCallback);

  installCallback.wait();
  Assert.ok(installCalled);
  AddonUtils.__proto__.installAddonFromSearchResult = oldFunction;

  server.stop(run_next_test);
});
