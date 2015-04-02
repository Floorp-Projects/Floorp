/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const { utils: Cu, interfaces: Ci, classes: Cc } = Components;

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
