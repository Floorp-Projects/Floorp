"use strict";

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Promise",
  "resource://gre/modules/Promise.jsm");

/**
 * A wrapper for the findbar's method "close", which is not synchronous
 * because of animation.
 */
function closeFindbarAndWait(findbar) {
  return new Promise((resolve) => {
    if (findbar.hidden) {
      resolve();
      return;
    }
    findbar.addEventListener("transitionend", function cont(aEvent) {
      if (aEvent.propertyName != "visibility") {
        return;
      }
      findbar.removeEventListener("transitionend", cont);
      resolve();
    });
    findbar.close();
  });
}

function pushPrefs(...aPrefs) {
  let deferred = Promise.defer();
  SpecialPowers.pushPrefEnv({"set": aPrefs}, deferred.resolve);
  return deferred.promise;
}

/**
 * Used to check whether the audio unblocking icon is in the tab.
 */
function* waitForTabBlockEvent(tab, expectBlocked) {
  if (tab.soundBlocked == expectBlocked) {
    ok(true, "The tab should " + (expectBlocked ? "" : "not ") + "be blocked");
  } else {
    info("Block state doens't match, wait for attributes changes.");
    yield BrowserTestUtils.waitForEvent(tab, "TabAttrModified", false, (event) => {
      if (event.detail.changed.indexOf("blocked") >= 0) {
        is(tab.soundBlocked, expectBlocked, "The tab should " + (expectBlocked ? "" : "not ") + "be blocked");
        return true;
      }
      return false;
    });
  }
}
