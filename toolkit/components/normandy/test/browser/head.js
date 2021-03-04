const { Preferences } = ChromeUtils.import(
  "resource://gre/modules/Preferences.jsm"
);
const { AddonTestUtils } = ChromeUtils.import(
  "resource://testing-common/AddonTestUtils.jsm"
);
const { TestUtils } = ChromeUtils.import(
  "resource://testing-common/TestUtils.jsm"
);
const { AboutPages } = ChromeUtils.import(
  "resource://normandy-content/AboutPages.jsm"
);
const { AddonStudies } = ChromeUtils.import(
  "resource://normandy/lib/AddonStudies.jsm"
);
const { NormandyApi } = ChromeUtils.import(
  "resource://normandy/lib/NormandyApi.jsm"
);
const { TelemetryEvents } = ChromeUtils.import(
  "resource://normandy/lib/TelemetryEvents.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "TelemetryTestUtils",
  "resource://testing-common/TelemetryTestUtils.jsm"
);

const CryptoHash = Components.Constructor(
  "@mozilla.org/security/hash;1",
  "nsICryptoHash",
  "initWithString"
);
const FileInputStream = Components.Constructor(
  "@mozilla.org/network/file-input-stream;1",
  "nsIFileInputStream",
  "init"
);

const { sinon } = ChromeUtils.import("resource://testing-common/Sinon.jsm");

// Make sinon assertions fail in a way that mochitest understands
sinon.assert.fail = function(message) {
  ok(false, message);
};

// Prep Telemetry to receive events from tests
TelemetryEvents.init();

this.TEST_XPI_URL = (function() {
  const dir = getChromeDir(getResolvedURI(gTestPath));
  dir.append("addons");
  dir.append("normandydriver-a-1.0.xpi");
  return Services.io.newFileURI(dir).spec;
})();

this.withWebExtension = function(manifestOverrides = {}) {
  return function wrapper(testFunction) {
    return async function wrappedTestFunction(...args) {
      const random = Math.random()
        .toString(36)
        .replace(/0./, "")
        .substr(-3);
      let id = `normandydriver_${random}@example.com`;
      if ("id" in manifestOverrides) {
        id = manifestOverrides.id;
        delete manifestOverrides.id;
      }

      const manifest = Object.assign(
        {
          manifest_version: 2,
          name: "normandy_fixture",
          version: "1.0",
          description: "Dummy test fixture that's a webextension",
          applications: {
            gecko: { id },
          },
        },
        manifestOverrides
      );

      const addonFile = AddonTestUtils.createTempWebExtensionFile({ manifest });

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

this.withInstalledWebExtension = function(
  manifestOverrides = {},
  expectUninstall = false
) {
  return function wrapper(testFunction) {
    return decorate(
      withWebExtension(manifestOverrides),
      async function wrappedTestFunction(...args) {
        const [id, file] = args[args.length - 1];
        const startupPromise = AddonTestUtils.promiseWebExtensionStartup(id);
        const addonInstall = await AddonManager.getInstallForFile(
          file,
          "application/x-xpinstall"
        );
        await addonInstall.install();
        await startupPromise;

        try {
          await testFunction(...args);
        } finally {
          const addonToUninstall = await AddonManager.getAddonByID(id);
          if (addonToUninstall) {
            await addonToUninstall.uninstall();
          } else {
            ok(
              expectUninstall,
              "Add-on should not be unexpectedly uninstalled during test"
            );
          }
        }
      }
    );
  };
};

this.withMockNormandyApi = function() {
  return function(testFunction) {
    return async function inner(...args) {
      const mockApi = {
        actions: [],
        recipes: [],
        implementations: {},
        extensionDetails: {},
      };

      // Use callsFake instead of resolves so that the current values in mockApi are used.
      mockApi.fetchExtensionDetails = sinon
        .stub(NormandyApi, "fetchExtensionDetails")
        .callsFake(async extensionId => {
          const details = mockApi.extensionDetails[extensionId];
          if (!details) {
            throw new Error(`Missing extension details for ${extensionId}`);
          }
          return details;
        });

      try {
        await testFunction(...args, mockApi);
      } finally {
        mockApi.fetchExtensionDetails.restore();
      }
    };
  };
};

const preferenceBranches = {
  user: Preferences,
  default: new Preferences({ defaultBranch: true }),
};

this.withMockPreferences = function() {
  return function(testFunction) {
    return async function inner(...args) {
      const prefManager = new MockPreferences();
      try {
        await testFunction(...args, prefManager);
      } finally {
        prefManager.cleanup();
      }
    };
  };
};

class MockPreferences {
  constructor() {
    this.oldValues = { user: {}, default: {} };
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
      this.oldValues[branch][name] = { oldValue, existed };
    }
  }

  cleanup() {
    for (const [branchName, values] of Object.entries(this.oldValues)) {
      const preferenceBranch = preferenceBranches[branchName];
      for (const [name, { oldValue, existed }] of Object.entries(values)) {
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

this.withStudiesEnabled = function() {
  return function(testFunc) {
    return async function inner(...args) {
      await SpecialPowers.pushPrefEnv({
        set: [["app.shield.optoutstudies.enabled", true]],
      });
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
  Object.defineProperty(decorated, "name", { value: origName });
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
 *     withMockPreferences(),
 *     withMockNormandyApi(),
 *     async function myTest(mockPreferences, mockApi) {
 *       // Do a test
 *     }
 *   );
 */
this.decorate_task = function(...args) {
  return add_task(decorate(...args));
};

this.withStub = function(object, method, { returnValue } = {}) {
  return function wrapper(testFunction) {
    return async function wrappedTestFunction(...args) {
      const stub = sinon.stub(object, method);
      stub.returnValue = returnValue;
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
    (subject, endedRecipeId) => Number.parseInt(endedRecipeId) === recipeId
  );
};

this.withSendEventSpy = function() {
  return function(testFunction) {
    return async function wrappedTestFunction(...args) {
      const spy = sinon.spy(TelemetryEvents, "sendEvent");
      spy.assertEvents = expected => {
        expected = expected.map(event => ["normandy"].concat(event));
        TelemetryTestUtils.assertEvents(
          expected,
          { category: "normandy" },
          { clear: false }
        );
      };
      Services.telemetry.clearEvents();
      try {
        await testFunction(...args, spy);
      } finally {
        spy.restore();
        Assert.ok(!spy.threw(), "Telemetry events should not fail");
      }
    };
  };
};

let _recipeId = 1;
this.recipeFactory = function(overrides = {}) {
  return Object.assign(
    {
      id: _recipeId++,
      arguments: overrides.arguments || {},
    },
    overrides
  );
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

const FIXTURE_ADDON_ID = "normandydriver-a@example.com";
const FIXTURE_ADDON_BASE_URL =
  getRootDirectory(gTestPath).replace(
    "chrome://mochitests/content",
    "http://example.com"
  ) + "/addons/";

const FIXTURE_ADDONS = [
  "normandydriver-a-1.0",
  "normandydriver-b-1.0",
  "normandydriver-a-2.0",
];

// Generate fixture add-on details
this.FIXTURE_ADDON_DETAILS = {};
FIXTURE_ADDONS.forEach(addon => {
  const filename = `${addon}.xpi`;
  const dir = getChromeDir(getResolvedURI(gTestPath));
  dir.append("addons");
  dir.append(filename);
  const xpiFile = Services.io.newFileURI(dir).QueryInterface(Ci.nsIFileURL)
    .file;

  FIXTURE_ADDON_DETAILS[addon] = {
    url: `${FIXTURE_ADDON_BASE_URL}${filename}`,
    hash: CryptoUtils.getFileHash(xpiFile, "sha256"),
  };
});

this.extensionDetailsFactory = function(overrides = {}) {
  return Object.assign(
    {
      id: 1,
      name: "Normandy Fixture",
      xpi: FIXTURE_ADDON_DETAILS["normandydriver-a-1.0"].url,
      extension_id: FIXTURE_ADDON_ID,
      version: "1.0",
      hash: FIXTURE_ADDON_DETAILS["normandydriver-a-1.0"].hash,
      hash_algorithm: "sha256",
    },
    overrides
  );
};

/**
 * Utility function to uninstall addons safely. Preventing the issue mentioned
 * in bug 1485569.
 *
 * addon.uninstall is async, but it also triggers the AddonStudies onUninstall
 * listener, which is not awaited. Wrap it here and trigger a promise once it's
 * done so we can wait until AddonStudies cleanup is finished.
 */
this.safeUninstallAddon = async function(addon) {
  const activeStudies = (await AddonStudies.getAll()).filter(
    study => study.active
  );
  const matchingStudy = activeStudies.find(study => study.addonId === addon.id);

  let studyEndedPromise;
  if (matchingStudy) {
    studyEndedPromise = TestUtils.topicObserved(
      "shield-study-ended",
      (subject, message) => {
        return message === `${matchingStudy.recipeId}`;
      }
    );
  }

  const addonUninstallPromise = addon.uninstall();

  return Promise.all([studyEndedPromise, addonUninstallPromise]);
};

/**
 * Test decorator that is a modified version of the withInstalledWebExtension
 * decorator that safely uninstalls the created addon.
 */
this.withInstalledWebExtensionSafe = function(manifestOverrides = {}) {
  return testFunction => {
    return async function wrappedTestFunction(...args) {
      const decorated = withInstalledWebExtension(
        manifestOverrides,
        true
      )(async ([id, file]) => {
        try {
          await testFunction(...args, [id, file]);
        } finally {
          let addon = await AddonManager.getAddonByID(id);
          if (addon) {
            await safeUninstallAddon(addon);
            addon = await AddonManager.getAddonByID(id);
            ok(!addon, "add-on should be uninstalled");
          }
        }
      });
      await decorated();
    };
  };
};

/**
 * Test decorator to provide a web extension installed from a URL.
 */
this.withInstalledWebExtensionFromURL = function(url) {
  return function wrapper(testFunction) {
    return async function wrappedTestFunction(...args) {
      let startupPromise;
      let addonId;

      const install = await AddonManager.getInstallForURL(url);
      const listener = {
        onInstallStarted(cbInstall) {
          addonId = cbInstall.addon.id;
          startupPromise = AddonTestUtils.promiseWebExtensionStartup(addonId);
        },
      };
      install.addListener(listener);

      await install.install();
      await startupPromise;

      try {
        await testFunction(...args, [addonId, url]);
      } finally {
        const addonToUninstall = await AddonManager.getAddonByID(addonId);
        await safeUninstallAddon(addonToUninstall);
      }
    };
  };
};

/**
 * Test decorator that checks that the test cleans up all add-ons installed
 * during the test. Likely needs to be the first decorator used.
 */
this.ensureAddonCleanup = function(testFunction) {
  return async function wrappedTestFunction(...args) {
    const beforeAddons = new Set(await AddonManager.getAllAddons());

    try {
      await testFunction(...args);
    } finally {
      const afterAddons = new Set(await AddonManager.getAllAddons());
      Assert.deepEqual(
        beforeAddons,
        afterAddons,
        "The add-ons should be same before and after the test"
      );
    }
  };
};
