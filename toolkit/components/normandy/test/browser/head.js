ChromeUtils.import("resource://gre/modules/Preferences.jsm", this);
ChromeUtils.import("resource://testing-common/AddonTestUtils.jsm", this);
ChromeUtils.import("resource://testing-common/TestUtils.jsm", this);
ChromeUtils.import("resource://normandy-content/AboutPages.jsm", this);
ChromeUtils.import("resource://normandy/lib/Addons.jsm", this);
ChromeUtils.import("resource://normandy/lib/SandboxManager.jsm", this);
ChromeUtils.import("resource://normandy/lib/NormandyDriver.jsm", this);
ChromeUtils.import("resource://normandy/lib/NormandyApi.jsm", this);
ChromeUtils.import("resource://normandy/lib/TelemetryEvents.jsm", this);

// Load mocking/stubbing library, sinon
// docs: http://sinonjs.org/docs/
/* global sinon */
Services.scriptloader.loadSubScript("resource://testing-common/sinon-2.3.2.js");

// Make sinon assertions fail in a way that mochitest understands
sinon.assert.fail = function(message) {
  ok(false, message);
};

registerCleanupFunction(async function() {
  // Cleanup window or the test runner will throw an error
  delete window.sinon;
});

// Prep Telemetry to receive events from tests
TelemetryEvents.init();

this.UUID_REGEX = /[a-f0-9]{8}-[a-f0-9]{4}-[a-f0-9]{4}-[a-f0-9]{4}-[a-f0-9]{12}/;

this.TEST_XPI_URL = (function() {
  const dir = getChromeDir(getResolvedURI(gTestPath));
  dir.append("fixtures");
  dir.append("normandy.xpi");
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

this.withInstalledWebExtension = function(manifestOverrides = {}) {
  return function wrapper(testFunction) {
    return decorate(
      withWebExtension(manifestOverrides),
      async function wrappedTestFunction(...args) {
        const [id, file] = args[args.length - 1];
        const startupPromise = AddonTestUtils.promiseWebExtensionStartup(id);
        const url = Services.io.newFileURI(file).spec;
        await Addons.install(url);
        await startupPromise;
        try {
          await testFunction(...args);
        } finally {
          if (await Addons.get(id)) {
            await Addons.uninstall(id);
          }
        }
      }
    );
  };
};

this.withSandboxManager = function(Assert) {
  return function wrapper(testFunction) {
    return async function wrappedTestFunction(...args) {
      const sandboxManager = new SandboxManager();
      sandboxManager.addHold("test running");

      await testFunction(...args, sandboxManager);

      sandboxManager.removeHold("test running");
      await sandboxManager.isNuked()
        .then(() => Assert.ok(true, "sandbox is nuked"))
        .catch(e => Assert.ok(false, "sandbox is nuked", e));
    };
  };
};

this.withDriver = function(Assert, testFunction) {
  return withSandboxManager(Assert)(async function inner(...args) {
    const sandboxManager = args[args.length - 1];
    const driver = new NormandyDriver(sandboxManager);
    await testFunction(driver, ...args);
  });
};

this.withMockNormandyApi = function(testFunction) {
  return async function inner(...args) {
    const mockApi = {actions: [], recipes: [], implementations: {}};

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

    try {
      await testFunction(mockApi, ...args);
    } finally {
      mockApi.fetchActions.restore();
      mockApi.fetchRecipes.restore();
      mockApi.fetchImplementation.restore();
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
 * to the preceeding function as the argument; the result of this is passed to
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
  funcs.reverse();
  for (const func of funcs) {
    decorated = func(decorated);
  }
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

let _studyFactoryId = 0;
this.studyFactory = function(attrs) {
  return Object.assign({
    recipeId: _studyFactoryId++,
    name: "Test study",
    description: "fake",
    active: true,
    addonId: "fake@example.com",
    addonUrl: "http://test/addon.xpi",
    addonVersion: "1.0.0",
    studyStartDate: new Date(),
  }, attrs);
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

    /* Checks that calls match the event schema. */
    function checkEventMatchesSchema(method, object, value, extra) {
      let match = true;
      const spec = Array.from(Object.values(TelemetryEvents.eventSchema))
        .filter(spec => spec.methods.includes(method))[0];

      if (spec) {
        if (!spec.objects.includes(object)) {
          match = false;
        }

        for (const key of Object.keys(extra)) {
          if (!spec.extra_keys.includes(key)) {
            match = false;
          }
        }
      } else {
        match = false;
      }

      ok(match, `sendEvent(${method}, ${object}, ${value}, ${JSON.stringify(extra)}) should match spec`);
    }

    const stub = sinon.stub(TelemetryEvents, "sendEvent");
    stub.callsFake(checkEventMatchesSchema);
    try {
      await testFunction(...args, stub);
    } finally {
      stub.restore();
    }
  };
};
