ChromeUtils.import("resource://gre/modules/Preferences.jsm", this);
ChromeUtils.import("resource://testing-common/AddonTestUtils.jsm", this);
ChromeUtils.import("resource://testing-common/TestUtils.jsm", this);
ChromeUtils.import("resource://normandy-content/AboutPages.jsm", this);
ChromeUtils.import("resource://normandy/lib/NormandyApi.jsm", this);
ChromeUtils.import("resource://normandy/lib/TelemetryEvents.jsm", this);
ChromeUtils.defineModuleGetter(this, "TelemetryTestUtils",
                               "resource://testing-common/TelemetryTestUtils.jsm");

const CryptoHash = Components.Constructor("@mozilla.org/security/hash;1",
                                          "nsICryptoHash", "initWithString");
const FileInputStream = Components.Constructor("@mozilla.org/network/file-input-stream;1",
                                               "nsIFileInputStream", "init");

const {sinon} = ChromeUtils.import("resource://testing-common/Sinon.jsm");

// Make sinon assertions fail in a way that mochitest understands
sinon.assert.fail = function(message) {
  ok(false, message);
};

// Prep Telemetry to receive events from tests
TelemetryEvents.init();

this.UUID_REGEX = /[a-f0-9]{8}-[a-f0-9]{4}-[a-f0-9]{4}-[a-f0-9]{4}-[a-f0-9]{12}/;

this.TEST_XPI_URL = (function() {
  const dir = getChromeDir(getResolvedURI(gTestPath));
  dir.append("addons");
  dir.append("normandydriver-1.0.xpi");
  return Services.io.newFileURI(dir).spec;
})();

this.withWebExtension = function(manifestOverrides = {}) {
  return function wrapper(testFunction) {
    return async function wrappedTestFunction(...args) {
      const random = Math.random().toString(36).replace(/0./, "").substr(-3);
      let id = `normandydriver_${random}@example.com`;
      if ("id" in manifestOverrides) {
        id = manifestOverrides.id;
        delete manifestOverrides.id;
      }

      const manifest = Object.assign({
        manifest_version: 2,
        name: "normandy_fixture",
        version: "1.0",
        description: "Dummy test fixture that's a webextension",
        applications: {
          gecko: { id },
        },
      }, manifestOverrides);

      const addonFile = AddonTestUtils.createTempWebExtensionFile({manifest});

      // Workaround: Add-on files are cached by URL, and
      // createTempWebExtensionFile re-uses filenames if the previous file has
      // been deleted. So we need to flush the cache to avoid it.
      Services.obs.notifyObservers(addonFile, "flush-cache-entry");

      try {
        await testFunction(...args, [id, addonFile]);
      } finally {
        AddonTestUtils.cleanupTempXPIs();
      }
    };
  };
};

this.withCorruptedWebExtension = function() {
  // This should be an invalid manifest version, so that installing this add-on fails.
  return this.withWebExtension({ manifest_version: -1 });
};

this.withInstalledWebExtension = function(manifestOverrides = {}, expectUninstall = false) {
  return function wrapper(testFunction) {
    return decorate(
      withWebExtension(manifestOverrides),
      async function wrappedTestFunction(...args) {
        const [id, file] = args[args.length - 1];
        const startupPromise = AddonTestUtils.promiseWebExtensionStartup(id);
        const addonInstall = await AddonManager.getInstallForFile(file, "application/x-xpinstall");
        await addonInstall.install();
        await startupPromise;

        try {
          await testFunction(...args);
        } finally {
          const addonToUninstall = await AddonManager.getAddonByID(id);
          if (addonToUninstall) {
            await addonToUninstall.uninstall();
          } else {
            ok(expectUninstall, "Add-on should not be unexpectedly uninstalled during test");
          }
        }
      }
    );
  };
};

this.withMockNormandyApi = function(testFunction) {
  return async function inner(...args) {
    const mockApi = {actions: [], recipes: [], implementations: {}, extensionDetails: {}};

    // Use callsFake instead of resolves so that the current values in mockApi are used.
    mockApi.fetchActions = sinon.stub(NormandyApi, "fetchActions").callsFake(async () => mockApi.actions);
    mockApi.fetchRecipes = sinon.stub(NormandyApi, "fetchRecipes").callsFake(async () => mockApi.recipes);
    mockApi.fetchImplementation = sinon.stub(NormandyApi, "fetchImplementation").callsFake(
      async action => {
        const impl = mockApi.implementations[action.name];
        if (!impl) {
          throw new Error(`Missing implementation for ${action.name}`);
        }
        return impl;
      }
    );
    mockApi.fetchExtensionDetails = sinon.stub(NormandyApi, "fetchExtensionDetails").callsFake(
      async extensionId => {
        const details = mockApi.extensionDetails[extensionId];
        if (!details) {
          throw new Error(`Missing extension details for ${extensionId}`);
        }
        return details;
      }
    );

    try {
      await testFunction(mockApi, ...args);
    } finally {
      mockApi.fetchActions.restore();
      mockApi.fetchRecipes.restore();
      mockApi.fetchImplementation.restore();
      mockApi.fetchExtensionDetails.restore();
    }
  };
};

const preferenceBranches = {
  user: Preferences,
  default: new Preferences({defaultBranch: true}),
};

this.withMockPreferences = function(testFunction) {
  return async function inner(...args) {
    const prefManager = new MockPreferences();
    try {
      await testFunction(...args, prefManager);
    } finally {
      prefManager.cleanup();
    }
  };
};

class MockPreferences {
  constructor() {
    this.oldValues = {user: {}, default: {}};
  }

  set(name, value, branch = "user") {
    this.preserve(name, branch);
    preferenceBranches[branch].set(name, value);
  }

  preserve(name, branch) {
    if (!(name in this.oldValues[branch])) {
      const preferenceBranch = preferenceBranches[branch];
      let oldValue;
      let existed;
      try {
        oldValue = preferenceBranch.get(name);
        existed = preferenceBranch.has(name);
      } catch (e) {
        oldValue = null;
        existed = false;
      }
      this.oldValues[branch][name] = {oldValue, existed};
    }
  }

  cleanup() {
    for (const [branchName, values] of Object.entries(this.oldValues)) {
      const preferenceBranch = preferenceBranches[branchName];
      for (const [name, {oldValue, existed}] of Object.entries(values)) {
        const before = preferenceBranch.get(name);

        if (before === oldValue) {
          continue;
        }

        if (existed) {
          preferenceBranch.set(name, oldValue);
        } else if (branchName === "default") {
          Services.prefs.getDefaultBranch(name).deleteBranch("");
        } else {
          preferenceBranch.reset(name);
        }

        const after = preferenceBranch.get(name);
        if (before === after && before !== undefined) {
          throw new Error(
            `Couldn't reset pref "${name}" to "${oldValue}" on "${branchName}" branch ` +
            `(value stayed "${before}", did ${existed ? "" : "not "}exist)`
          );
        }
      }
    }
  }
}

this.withPrefEnv = function(inPrefs) {
  return function wrapper(testFunc) {
    return async function inner(...args) {
      await SpecialPowers.pushPrefEnv(inPrefs);
      try {
        await testFunc(...args);
      } finally {
        await SpecialPowers.popPrefEnv();
      }
    };
  };
};

/**
 * Combine a list of functions right to left. The rightmost function is passed
 * to the preceding function as the argument; the result of this is passed to
 * the next function until all are exhausted. For example, this:
 *
 * decorate(func1, func2, func3);
 *
 * is equivalent to this:
 *
 * func1(func2(func3));
 */
this.decorate = function(...args) {
  const funcs = Array.from(args);
  let decorated = funcs.pop();
  const origName = decorated.name;
  funcs.reverse();
  for (const func of funcs) {
    decorated = func(decorated);
  }
  Object.defineProperty(decorated, "name", {value: origName});
  return decorated;
};

/**
 * Wrapper around add_task for declaring tests that use several with-style
 * wrappers. The last argument should be your test function; all other arguments
 * should be functions that accept a single test function argument.
 *
 * The arguments are combined using decorate and passed to add_task as a single
 * test function.
 *
 * @param {[Function]} args
 * @example
 *   decorate_task(
 *     withMockPreferences,
 *     withMockNormandyApi,
 *     async function myTest(mockPreferences, mockApi) {
 *       // Do a test
 *     }
 *   );
 */
this.decorate_task = function(...args) {
  return add_task(decorate(...args));
};

let _addonStudyFactoryId = 0;
this.addonStudyFactory = function(attrs) {
  return Object.assign({
    recipeId: _addonStudyFactoryId++,
    name: "Test study",
    description: "fake",
    active: true,
    addonId: "fake@example.com",
    addonUrl: "http://test/addon.xpi",
    addonVersion: "1.0.0",
    studyStartDate: new Date(),
    extensionApiId: 1,
    extensionHash: "ade1c14196ec4fe0aa0a6ba40ac433d7c8d1ec985581a8a94d43dc58991b5171",
    extensionHashAlgorithm: "sha256",
  }, attrs);
};

let _preferenceStudyFactoryId = 0;
this.preferenceStudyFactory = function(attrs) {
  const defaultPref = {
    "test.study": {},
  };
  const defaultPrefInfo = {
    preferenceValue: false,
    preferenceType: "boolean",
    previousPreferenceValue: undefined,
    preferenceBranchType: "default",
  };
  const preferences = {};
  for (const [prefName, prefInfo] of Object.entries(attrs.preferences || defaultPref)) {
    preferences[prefName] = { ...defaultPrefInfo, ...prefInfo };
  }

  return Object.assign({
    name: `Test study ${_preferenceStudyFactoryId++}`,
    branch: "control",
    expired: false,
    lastSeen: new Date().toJSON(),
    experimentType: "exp",
  }, attrs, {
    preferences,
  });
};

this.withStub = function(...stubArgs) {
  return function wrapper(testFunction) {
    return async function wrappedTestFunction(...args) {
      const stub = sinon.stub(...stubArgs);
      try {
        await testFunction(...args, stub);
      } finally {
        stub.restore();
      }
    };
  };
};

this.withSpy = function(...spyArgs) {
  return function wrapper(testFunction) {
    return async function wrappedTestFunction(...args) {
      const spy = sinon.spy(...spyArgs);
      try {
        await testFunction(...args, spy);
      } finally {
        spy.restore();
      }
    };
  };
};

this.studyEndObserved = function(recipeId) {
  return TestUtils.topicObserved(
    "shield-study-ended",
    (subject, endedRecipeId) => Number.parseInt(endedRecipeId) === recipeId,
  );
};

this.withSendEventStub = function(testFunction) {
  return async function wrappedTestFunction(...args) {
    const stub = sinon.spy(TelemetryEvents, "sendEvent");
    stub.assertEvents = (expected) => {
      expected = expected.map(event => ["normandy"].concat(event));
      TelemetryTestUtils.assertEvents(expected, {category: "normandy"}, {clear: false});
    };
    Services.telemetry.clearEvents();
    try {
      await testFunction(...args, stub);
    } finally {
      stub.restore();
      Assert.ok(!stub.threw(), "some telemetry call failed");
    }
  };
};

let _recipeId = 1;
this.recipeFactory = function(overrides = {}) {
  return Object.assign({
    id: _recipeId++,
    arguments: overrides.arguments || {},
  }, overrides);
};

function mockLogger() {
  const logStub = sinon.stub();
  logStub.fatal = sinon.stub();
  logStub.error = sinon.stub();
  logStub.warn = sinon.stub();
  logStub.info = sinon.stub();
  logStub.config = sinon.stub();
  logStub.debug = sinon.stub();
  logStub.trace = sinon.stub();
  return logStub;
}

this.CryptoUtils = {
  _getHashStringForCrypto(aCrypto) {
    // return the two-digit hexadecimal code for a byte
    let toHexString = charCode => ("0" + charCode.toString(16)).slice(-2);

    // convert the binary hash data to a hex string.
    let binary = aCrypto.finish(false);
    let hash = Array.from(binary, c => toHexString(c.charCodeAt(0)));
    return hash.join("").toLowerCase();
  },

  /**
   * Get the computed hash for a given file
   * @param {nsIFile} file The file to be hashed
   * @param {string} [algorithm] The hashing algorithm to use
   */
  getFileHash(file, algorithm = "sha256") {
    const crypto = CryptoHash(algorithm);
    const fis = new FileInputStream(file, -1, -1, false);
    crypto.updateFromStream(fis, file.fileSize);
    const hash = this._getHashStringForCrypto(crypto);
    fis.close();
    return hash;
  },
};
