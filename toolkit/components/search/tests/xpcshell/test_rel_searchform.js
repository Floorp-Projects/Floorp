/* Any copyright is dedicated to the Public Domain.
 *    http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Tests that <Url rel="searchform"/> is properly recognized as a searchForm.
 */

"use strict";

const Ci = Components.interfaces;

Components.utils.import("resource://testing-common/httpd.js");

let engines = [
  "engine-rel-searchform.xml",
  "engine-rel-searchform-post.xml",
];

function search_observer(aSubject, aTopic, aData) {
  let engine = aSubject.QueryInterface(Ci.nsISearchEngine);
  do_print("Observer: " + aData + " for " + engine.name);

  if (aData != "engine-added")
    return;

  let idx = engines.indexOf(engine.name);
  if (idx < 0)
    // The engine is some other engine unrelated to the test, so ignore it.
    return;

  // The final searchForm of the engine should be a URL whose domain is the
  // <ShortName> in the engine's XML and that has a ?search parameter.  The
  // point of the ?search parameter is to avoid accidentally matching the value
  // returned as a last resort by Engine's searchForm getter, which is simply
  // the prePath of the engine's first HTML <Url>.
  do_check_eq(engine.searchForm, "http://" + engine.name + "/?search");

  engines.splice(idx, 1);
  if (!engines.length)
    do_test_finished();
}

function run_test() {
  removeMetadata();
  updateAppInfo();

  let httpServer = new HttpServer();
  httpServer.start(-1);
  httpServer.registerDirectory("/", do_get_cwd());

  do_register_cleanup(function cleanup() {
    httpServer.stop(function() {});
    Services.obs.removeObserver(search_observer, "browser-search-engine-modified");
  });

  do_test_pending();
  Services.obs.addObserver(search_observer, "browser-search-engine-modified", false);

  for (let basename of engines) {
    Services.search.addEngine("http://localhost:" +
                              httpServer.identity.primaryPort +
                              "/data/" + basename,
                              Ci.nsISearchEngine.DATA_XML,
                              null, false);
  }
}
