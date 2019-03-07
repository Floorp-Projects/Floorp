"use strict";

const {Services} = ChromeUtils.import("resource://gre/modules/Services.jsm");

var EXPORTED_SYMBOLS = [
  "SearchTestUtils",
];

var gTestGlobals;

var SearchTestUtils = Object.freeze({
  init(Assert, registerCleanupFunction) {
    gTestGlobals = {
      Assert,
      registerCleanupFunction,
    };
  },

  /**
   * Adds a search engine to the search service. It will remove the engine
   * at the end of the test.
   *
   * @param {string}   url                     The URL of the engine to add.
   * @param {Function} registerCleanupFunction Pass the registerCleanupFunction
   *                                           from the test's scope.
   * @returns {Promise} Returns a promise that is resolved with the new engine
   *                    or rejected if it fails.
   */
  async promiseNewSearchEngine(url) {
    let engine = await Services.search.addEngine(url, "", false);
    gTestGlobals.registerCleanupFunction(async () => Services.search.removeEngine(engine));
    return engine;
  },
});
