"use strict";

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

const { MockRegistrar } = ChromeUtils.import(
  "resource://testing-common/MockRegistrar.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  AddonManager: "resource://gre/modules/AddonManager.jsm",
  AddonTestUtils: "resource://testing-common/AddonTestUtils.jsm",
  ExtensionTestUtils: "resource://testing-common/ExtensionXPCShellUtils.jsm",
  RemoteSettings: "resource://services-settings/remote-settings.js",
  SearchUtils: "resource://gre/modules/SearchUtils.jsm",
  sinon: "resource://testing-common/Sinon.jsm",
});

var EXPORTED_SYMBOLS = ["SearchTestUtils"];

var gTestScope;

var SearchTestUtils = {
  init(testScope) {
    gTestScope = testScope;
    let env = Cc["@mozilla.org/process/environment;1"].getService(
      Ci.nsIEnvironment
    );
    this._isMochitest = !env.exists("XPCSHELL_TEST_PROFILE_DIR");
    if (this._isMochitest) {
      this._isMochitest = true;
      lazy.AddonTestUtils.initMochitest(testScope);
    } else {
      this._isMochitest = false;
      // This handles xpcshell-tests.
      gTestScope.ExtensionTestUtils = lazy.ExtensionTestUtils;
      this.initXPCShellAddonManager(testScope);
    }
  },

  /**
   * Adds an OpenSearch based engine to the search service. It will remove
   * the engine at the end of the test.
   *
   * @param {string}   url                     The URL of the engine to add.
   * @param {Function} registerCleanupFunction Pass the registerCleanupFunction
   *                                           from the test's scope.
   * @returns {Promise} Returns a promise that is resolved with the new engine
   *                    or rejected if it fails.
   */
  async promiseNewSearchEngine(url) {
    // OpenSearch engines can only be added via http protocols.
    url = url.replace("chrome://mochitests/content", "https://example.com");
    let engine = await Services.search.addOpenSearchEngine(url, "");
    gTestScope.registerCleanupFunction(async () => {
      try {
        await Services.search.removeEngine(engine);
      } catch (ex) {
        // Don't throw if the test has already removed it.
      }
    });
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
        // Let the stack unwind.
        Services.tm.dispatchToMainThread(() => resolve(aSubject));
      }, topic);
    });
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
   * @returns {object}
   *   An object that is a sinon stub for the configuration getter.
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

    const settings = await lazy.RemoteSettings(lazy.SearchUtils.SETTINGS_KEY);
    if (config) {
      return lazy.sinon.stub(settings, "get").returns(config);
    }

    let response = await fetch(`resource://search-extensions/engines.json`);
    let json = await response.json();
    return lazy.sinon.stub(settings, "get").returns(json.data);
  },

  async useMochitestEngines(testDir) {
    // Replace the path we load search engines from with
    // the path to our test data.
    let resProt = Services.io
      .getProtocolHandler("resource")
      .QueryInterface(Ci.nsIResProtocolHandler);
    let originalSubstitution = resProt.getSubstitution("search-extensions");
    resProt.setSubstitution(
      "search-extensions",
      Services.io.newURI("file://" + testDir.path)
    );
    gTestScope.registerCleanupFunction(() => {
      resProt.setSubstitution("search-extensions", originalSubstitution);
    });
  },

  /**
   * Convert a list of engine configurations into engine objects.
   *
   * @param {Array} engineConfigurations
   **/
  async searchConfigToEngines(engineConfigurations) {
    let engines = [];
    for (let config of engineConfigurations) {
      let engine = await Services.search.wrappedJSObject._makeEngineFromConfig(
        config
      );
      engines.push(engine);
    }
    return engines;
  },

  /**
   * Provides various setup for xpcshell-tests installing WebExtensions. Should
   * be called from the global scope of the test.
   *
   * @param {object} scope
   *  The global scope of the test being run.
   * @param {*} usePrivilegedSignatures
   *  How to sign created addons.
   */
  initXPCShellAddonManager(scope, usePrivilegedSignatures = false) {
    let scopes =
      lazy.AddonManager.SCOPE_PROFILE | lazy.AddonManager.SCOPE_APPLICATION;
    Services.prefs.setIntPref("extensions.enabledScopes", scopes);
    // Only do this once.
    try {
      gTestScope.ExtensionTestUtils.init(scope);
    } catch (ex) {
      // This can happen if init is called twice.
      if (ex.result != Cr.NS_ERROR_FILE_ALREADY_EXISTS) {
        throw ex;
      }
    }
    lazy.AddonTestUtils.usePrivilegedSignatures = usePrivilegedSignatures;
    lazy.AddonTestUtils.overrideCertDB();
  },

  /**
   * Add a search engine as a WebExtension.
   *
   * Note: for tests, the extension must generally be unloaded before
   * `registerCleanupFunction`s are triggered. See bug 1694409.
   *
   * This function automatically registers an unload for the extension, this
   * may be skipped with the skipUnload argument.
   *
   * @param {object} [options]
   *   @see createEngineManifest
   * @param {boolean} [skipUnload]
   *   If true, this will skip the automatic unloading of the extension.
   * @returns {object}
   *   The loaded extension. This will need unloading before ending the test.
   */
  async installSearchExtension(options = {}, skipUnload = false) {
    await Services.search.init();

    let extensionInfo = {
      useAddonManager: "permanent",
      manifest: this.createEngineManifest(options),
    };

    let extension;

    // Cleanup must be registered before loading the extension to avoid
    // failures for mochitests.
    if (!skipUnload && this._isMochitest) {
      gTestScope.registerCleanupFunction(async () => {
        await extension.unload();
      });
    }

    extension = gTestScope.ExtensionTestUtils.loadExtension(extensionInfo);
    await extension.startup();
    if (!options.skipWaitForSearchEngine) {
      await lazy.AddonTestUtils.waitForSearchProviderStartup(extension);
    }

    // For xpcshell-tests we must register the unload after adding the extension.
    // See bug 1694409 for why this is.
    if (!skipUnload && !this._isMochitest) {
      gTestScope.registerCleanupFunction(async () => {
        await extension.unload();
      });
    }

    return extension;
  },

  /**
   * Install a search engine as a system extension to simulate
   * Normandy updates. For xpcshell-tests only.
   *
   * @param {object} [options]
   *   @see createEngineManifest
   */
  async installSystemSearchExtension(options = {}) {
    options.id = (options.id ?? "example") + "@search.mozilla.org";
    let xpi = await lazy.AddonTestUtils.createTempWebExtensionFile({
      manifest: this.createEngineManifest(options),
      background() {
        // eslint-disable-next-line no-undef
        browser.test.sendMessage("started");
      },
    });
    let wrapper = gTestScope.ExtensionTestUtils.expectExtension(options.id);

    const install = await lazy.AddonManager.getInstallForURL(
      `file://${xpi.path}`,
      {
        useSystemLocation: true,
      }
    );

    install.install();

    await wrapper.awaitStartup();
    await wrapper.awaitMessage("started");

    return wrapper;
  },

  /**
   * Create a search engine extension manifest.
   *
   * @param {object} [options]
   * @param {string} [options.id]
   *   The id to use for the WebExtension.
   * @param {string} [options.name]
   *   The display name to use for the WebExtension.
   * @param {string} [options.version]
   *   The version to use for the WebExtension.
   * @param {string} [options.favicon_url]
   *   The favicon to use for the search engine in the WebExtension.
   * @param {string} [options.keyword]
   *   The keyword to use for the search engine.
   * @param {string} [options.encoding]
   *   The encoding to use for the search engine.
   * @param {string} [options.search_url]
   *   The search URL to use for the search engine.
   * @param {string} [options.search_url_get_params]
   *   The GET search URL parameters to use for the search engine
   * @param {string} [options.search_url_post_params]
   *   The POST search URL parameters to use for the search engine
   * @param {string} [options.suggest_url]
   *   The suggestion URL to use for the search engine.
   * @param {string} [options.suggest_url_get_params]
   *   The suggestion URL parameters to use for the search engine.
   * @param {string} [options.search_form]
   *   The search form to use for the search engine.
   * @param {string} [options.favicon_url]
   *   The favicon URL to use for the search engine.
   * @returns {object}
   *   The generated manifest.
   */
  createEngineManifest(options = {}) {
    options.name = options.name ?? "Example";
    options.id = options.id ?? options.name.toLowerCase().replaceAll(" ", "");
    if (!options.id.includes("@")) {
      options.id += "@tests.mozilla.org";
    }
    options.version = options.version ?? "1.0";
    let manifest = {
      version: options.version,
      applications: {
        gecko: {
          id: options.id,
        },
      },
      chrome_settings_overrides: {
        search_provider: {
          name: options.name,
          search_url: options.search_url ?? "https://example.com/",
        },
      },
    };

    if (options.search_url_post_params) {
      manifest.chrome_settings_overrides.search_provider.search_url_post_params =
        options.search_url_post_params;
    } else {
      manifest.chrome_settings_overrides.search_provider.search_url_get_params =
        options.search_url_get_params ?? "?q={searchTerms}";
    }

    if (options.favicon_url) {
      manifest.chrome_settings_overrides.search_provider.favicon_url =
        options.favicon_url;
    }
    if (options.encoding) {
      manifest.chrome_settings_overrides.search_provider.encoding =
        options.encoding;
    }
    if (options.keyword) {
      manifest.chrome_settings_overrides.search_provider.keyword =
        options.keyword;
    }
    if (options.suggest_url) {
      manifest.chrome_settings_overrides.search_provider.suggest_url =
        options.suggest_url;
    }
    if (options.suggest_url) {
      manifest.chrome_settings_overrides.search_provider.suggest_url_get_params =
        options.suggest_url_get_params;
    }
    if (options.search_form) {
      manifest.chrome_settings_overrides.search_provider.search_form =
        options.search_form;
    }
    if (options.favicon_url) {
      manifest.chrome_settings_overrides.search_provider.favicon_url =
        options.favicon_url;
    }
    return manifest;
  },

  /**
   * A mock idleService that allows us to simulate RemoteSettings
   * configuration updates.
   */
  idleService: {
    _observers: new Set(),

    _reset() {
      this._observers.clear();
    },

    _fireObservers(state) {
      for (let observer of this._observers.values()) {
        observer.observe(observer, state, null);
      }
    },

    QueryInterface: ChromeUtils.generateQI(["nsIUserIdleService"]),
    idleTime: 19999,

    addIdleObserver(observer, time) {
      this._observers.add(observer);
    },

    removeIdleObserver(observer, time) {
      this._observers.delete(observer);
    },
  },

  /**
   * Register the mock idleSerice.
   *
   * @param {Fun} registerCleanupFunction
   */
  useMockIdleService() {
    let fakeIdleService = MockRegistrar.register(
      "@mozilla.org/widget/useridleservice;1",
      SearchTestUtils.idleService
    );
    gTestScope.registerCleanupFunction(() => {
      MockRegistrar.unregister(fakeIdleService);
    });
  },

  /**
   * Simulates an update to the RemoteSettings configuration.
   *
   * @param {object} [config]
   *  The new configuration.
   */
  async updateRemoteSettingsConfig(config) {
    if (!config) {
      let settings = lazy.RemoteSettings(lazy.SearchUtils.SETTINGS_KEY);
      config = await settings.get();
    }
    const reloadObserved = SearchTestUtils.promiseSearchNotification(
      "engines-reloaded"
    );
    await lazy.RemoteSettings(lazy.SearchUtils.SETTINGS_KEY).emit("sync", {
      data: { current: config },
    });

    this.idleService._fireObservers("idle");
    await reloadObserved;
  },
};
