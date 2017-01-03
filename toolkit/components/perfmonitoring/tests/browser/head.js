/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

var { utils: Cu, interfaces: Ci, classes: Cc } = Components;

Cu.import("resource://gre/modules/Promise.jsm", this);
Cu.import("resource://gre/modules/AddonManager.jsm", this);
Cu.import("resource://gre/modules/AddonWatcher.jsm", this);
Cu.import("resource://gre/modules/PerformanceWatcher.jsm", this);
Cu.import("resource://gre/modules/Services.jsm", this);
Cu.import("resource://testing-common/ContentTaskUtils.jsm", this);

/**
 * Base class for simulating slow addons/webpages.
 */
function CPUBurner(url, jankThreshold) {
  info(`CPUBurner: Opening tab for ${url}\n`);
  this.url = url;
  this.tab = gBrowser.addTab(url);
  this.jankThreshold = jankThreshold;
  let browser = this.tab.linkedBrowser;
  this._browser = browser;
  ContentTask.spawn(this._browser, null, CPUBurner.frameScript);
  this.promiseInitialized = BrowserTestUtils.browserLoaded(browser);
}
CPUBurner.prototype = {
  get windowId() {
    return this._browser.outerWindowID;
  },
  /**
   * Burn CPU until it triggers a listener with the specified jank threshold.
   */
  run: Task.async(function*(burner, max, listener) {
    listener.reset();
    for (let i = 0; i < max; ++i) {
      yield new Promise(resolve => setTimeout(resolve, 50));
      try {
        yield this[burner]();
      } catch (ex) {
        return false;
      }
      if (listener.triggered && listener.result >= this.jankThreshold) {
        return true;
      }
    }
    return false;
  }),
  dispose() {
    info(`CPUBurner: Closing tab for ${this.url}\n`);
    gBrowser.removeTab(this.tab);
  }
};
// This function is injected in all frames
CPUBurner.frameScript = function() {
  try {
    "use strict";

    const { utils: Cu, classes: Cc, interfaces: Ci } = Components;
    let sandboxes = new Map();
    let getSandbox = function(addonId) {
      let sandbox = sandboxes.get(addonId);
      if (!sandbox) {
        sandbox = Components.utils.Sandbox(Services.scriptSecurityManager.getSystemPrincipal(), { addonId  });
        sandboxes.set(addonId, sandbox);
      }
      return sandbox;
    };

    let burnCPU = function() {
      var start = Date.now();
      var ignored = [];
      while (Date.now() - start < 500) {
        ignored[ignored.length % 2] = ignored.length;
      }
    };
    let burnCPUInSandbox = function(addonId) {
      let sandbox = getSandbox(addonId);
      Cu.evalInSandbox(burnCPU.toSource() + "()", sandbox);
    };

    {
      let topic = "test-performance-watcher:burn-content-cpu";
        addMessageListener(topic, function(msg) {
        try {
          if (msg.data && msg.data.addonId) {
            burnCPUInSandbox(msg.data.addonId);
          } else {
            burnCPU();
          }
          sendAsyncMessage(topic, {});
        } catch (ex) {
          dump(`This is the content attempting to burn CPU: error ${ex}\n`);
          dump(`${ex.stack}\n`);
        }
      });
    }

    // Bind the function to the global context or it might be GC'd during test
    // causing failures (bug 1230027)
    this.burnCPOWInSandbox = function(addonId) {
      try {
        burnCPUInSandbox(addonId);
      } catch (ex) {
        dump(`This is the addon attempting to burn CPOW: error ${ex}\n`);
        dump(`${ex.stack}\n`);
      }
    }

    sendAsyncMessage("test-performance-watcher:cpow-init", {}, {
      burnCPOWInSandbox: this.burnCPOWInSandbox
    });

  } catch (ex) {
    Cu.reportError("This is the addon: error " + ex);
    Cu.reportError(ex.stack);
  }
};

/**
 * Base class for listening to slow group alerts
 */
function AlertListener(accept, {register, unregister}) {
  this.listener = (...args) => {
    if (this._unregistered) {
      throw new Error("Listener was unregistered");
    }
    let result = accept(...args);
    if (!result) {
      return;
    }
    this.result = result;
    this.triggered = true;
    return;
  };
  this.triggered = false;
  this.result = null;
  this._unregistered = false;
  this._unregister = unregister;
  registerCleanupFunction(() => {
    this.unregister();
  });
  register();
}
AlertListener.prototype = {
  unregister() {
    this.reset();
    if (this._unregistered) {
      info(`head.js: No need to unregister, we're already unregistered.\n`);
      return;
    }
    info(`head.js: Unregistering listener.\n`);
    this._unregistered = true;
    this._unregister();
    info(`head.js: Unregistration complete.\n`);
  },
  reset() {
    this.triggered = false;
    this.result = null;
  },
};

/**
 * Simulate a slow add-on.
 */
function AddonBurner(addonId = "fake add-on id: " + Math.random()) {
  this.jankThreshold = 200000;
  CPUBurner.call(this, `http://example.com/?uri=${addonId}`, this.jankThreshold);
  this._addonId = addonId;
  this._sandbox = Components.utils.Sandbox(Services.scriptSecurityManager.getSystemPrincipal(), { addonId: this._addonId });
  this._CPOWBurner = null;

  this._promiseCPOWBurner = new Promise(resolve => {
    this._browser.messageManager.addMessageListener("test-performance-watcher:cpow-init", msg => {
      // Note that we cannot resolve Promises with CPOWs now that they
      // have been outlawed in bug 1233497, so we stash it in the
      // AddonBurner instance instead.
      this._CPOWBurner = msg.objects.burnCPOWInSandbox;
      resolve();
    });
  });
}
AddonBurner.prototype = Object.create(CPUBurner.prototype);
Object.defineProperty(AddonBurner.prototype, "addonId", {
  get() {
    return this._addonId;
  }
});

/**
 * Simulate slow code being executed by the add-on in the chrome.
 */
AddonBurner.prototype.burnCPU = function() {
  Cu.evalInSandbox(AddonBurner.burnCPU.toSource() + "()", this._sandbox);
};

/**
 * Simulate slow code being executed by the add-on in a CPOW.
 */
AddonBurner.prototype.promiseBurnCPOW = Task.async(function*() {
  yield this._promiseCPOWBurner;
  ok(this._CPOWBurner, "Got the CPOW burner");
  let burner = this._CPOWBurner;
  info("Parent: Preparing to burn CPOW");
  try {
    yield burner(this._addonId);
    info("Parent: Done burning CPOW");
  } catch (ex) {
    info(`Parent: Error burning CPOW: ${ex}\n`);
    info(ex.stack + "\n");
  }
});

/**
 * Simulate slow code being executed by the add-on in the content.
 */
AddonBurner.prototype.promiseBurnContentCPU = function() {
  return promiseContentResponse(this._browser, "test-performance-watcher:burn-content-cpu", {addonId: this._addonId});
};
AddonBurner.burnCPU = function() {
  var start = Date.now();
  var ignored = [];
  while (Date.now() - start < 500) {
    ignored[ignored.length % 2] = ignored.length;
  }
};


function AddonListener(addonId, accept) {
  let target = {addonId};
  AlertListener.call(this, accept, {
    register: () => {
      info(`AddonListener: registering ${JSON.stringify(target, null, "\t")}`);
      PerformanceWatcher.addPerformanceListener({addonId}, this.listener);
    },
    unregister: () => {
      info(`AddonListener: unregistering ${JSON.stringify(target, null, "\t")}`);
      PerformanceWatcher.removePerformanceListener({addonId}, this.listener);
    }
  });
}
AddonListener.prototype = Object.create(AlertListener.prototype);

function promiseContentResponse(browser, name, message) {
  let mm = browser.messageManager;
  let promise = new Promise(resolve => {
    function removeListener() {
      mm.removeMessageListener(name, listener);
    }

    function listener(msg) {
      removeListener();
      resolve(msg.data);
    }

    mm.addMessageListener(name, listener);
    registerCleanupFunction(removeListener);
  });
  mm.sendAsyncMessage(name, message);
  return promise;
}
function promiseContentResponseOrNull(browser, name, message) {
  if (!browser.messageManager) {
    return null;
  }
  return promiseContentResponse(browser, name, message);
}

/**
 * `true` if we are running an OS in which the OS performance
 * clock has a low precision and might unpredictably
 * never be updated during the execution of the test.
 */
function hasLowPrecision() {
  let [sysName, sysVersion] = [Services.sysinfo.getPropertyAsAString("name"), Services.sysinfo.getPropertyAsDouble("version")];
  info(`Running ${sysName} version ${sysVersion}`);

  if (sysName == "Windows_NT" && sysVersion < 6) {
    info("Running old Windows, need to deactivate tests due to bad precision.");
    return true;
  }
  if (sysName == "Linux" && sysVersion <= 2.6) {
    info("Running old Linux, need to deactivate tests due to bad precision.");
    return true;
  }
  info("This platform has good precision.")
  return false;
}
