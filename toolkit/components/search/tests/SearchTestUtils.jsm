"use strict";

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  NetUtil: "resource://gre/modules/NetUtil.jsm",
  RemoteSettings: "resource://services-settings/remote-settings.js",
  SearchUtils: "resource://gre/modules/SearchUtils.jsm",
  Services: "resource://gre/modules/Services.jsm",
  sinon: "resource://testing-common/Sinon.jsm",
});

var EXPORTED_SYMBOLS = ["SearchTestUtils"];

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
    gTestGlobals.registerCleanupFunction(async () =>
      Services.search.removeEngine(engine)
    );
    return engine;
  },

  /**
   * Returns a promise that is resolved when an observer notification from the
   * search service fires with the specified data.
   *
   * @param {*} expectedData
   *        The value the observer notification sends that causes us to resolve
   *        the promise.
   * @param {string} topic
   *        The notification topic to observe. Defaults to 'browser-search-service'.
   * @returns {Promise}
   *        Returns a promise that is resolved with the subject of the
   *        topic once the topic with the data has been observed.
   */
  promiseSearchNotification(expectedData, topic = "browser-search-service") {
    return new Promise(resolve => {
      Services.obs.addObserver(function observer(aSubject, aTopic, aData) {
        if (aData != expectedData) {
          return;
        }

        Services.obs.removeObserver(observer, topic);
        resolve(aSubject);
      }, topic);
    });
  },

  parseJsonFromStream(aInputStream) {
    let bytes = NetUtil.readInputStream(aInputStream, aInputStream.available());
    return JSON.parse(new TextDecoder().decode(bytes));
  },

  /**
   * Load engines from test data located in particular folders.
   *
   * @param {string} [folder]
   *   The folder name to use.
   * @param {string} [subFolder]
   *   The subfolder to use, if any.
   * @param {array} [config]
   *   An array which contains the configuration to set.
   */
  async useTestEngines(folder = "data", subFolder = null, config = null) {
    let url = `resource://test/${folder}/`;
    if (subFolder) {
      url += `${subFolder}/`;
    }
    let resProt = Services.io
      .getProtocolHandler("resource")
      .QueryInterface(Ci.nsIResProtocolHandler);
    resProt.setSubstitution("search-extensions", Services.io.newURI(url));
    if (
      Services.prefs.getBoolPref(
        SearchUtils.BROWSER_SEARCH_PREF + "modernConfig"
      )
    ) {
      const settings = await RemoteSettings(SearchUtils.SETTINGS_KEY);
      if (config) {
        sinon.stub(settings, "get").returns(config);
      } else {
        let chan = NetUtil.newChannel({
          uri: "resource://search-extensions/engines.json",
          loadUsingSystemPrincipal: true,
        });
        let json = this.parseJsonFromStream(chan.open());
        sinon.stub(settings, "get").returns(json.data);
      }
    }
  },

  /**
   * Convert a list of engine configurations into engine objects.
   *
   * @param {Array} engineConfigurations
   **/
  async searchConfigToEngines(engineConfigurations) {
    let engines = [];
    for (let config of engineConfigurations) {
      let engine = await Services.search.makeEngineFromConfig(config);
      engines.push(engine);
    }
    return engines;
  },
});
