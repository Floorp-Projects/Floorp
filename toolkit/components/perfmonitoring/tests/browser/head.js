/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* eslint-env mozilla/frame-script */

ChromeUtils.import("resource://gre/modules/AddonManager.jsm", this);
ChromeUtils.import("resource://gre/modules/Services.jsm", this);
ChromeUtils.import("resource://testing-common/ContentTaskUtils.jsm", this);

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
  info("This platform has good precision.");
  return false;
}
