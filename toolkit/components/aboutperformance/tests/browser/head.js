/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

var { utils: Cu, interfaces: Ci, classes: Cc } = Components;

Cu.import("resource://gre/modules/Services.jsm", this);

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
