"use strict";

/**
 * Set the findbar value to the given text, start a search for that text, and
 * return a promise that resolves when the find has completed.
 *
 * @param gBrowser tabbrowser to search in the current tab.
 * @param searchText text to search for.
 * @param highlightOn true if highlight mode should be enabled before searching.
 * @returns Promise resolves when find is complete.
 */
async function promiseFindFinished(gBrowser, searchText, highlightOn = false) {
  let findbar = await gBrowser.getFindBar();
  findbar.startFind(findbar.FIND_NORMAL);
  let highlightElement = findbar.getElement("highlight");
  if (highlightElement.checked != highlightOn) {
    highlightElement.click();
  }
  return new Promise(resolve => {
    executeSoon(() => {
      findbar._findField.value = searchText;

      let resultListener;
      // When highlighting is on the finder sends a second "FOUND" message after
      // the search wraps. This causes timing problems with e10s. waitMore
      // forces foundOrTimeout wait for the second "FOUND" message before
      // resolving the promise.
      let waitMore = highlightOn;
      let findTimeout = setTimeout(() => foundOrTimedout(null), 5000);
      let foundOrTimedout = function (aData) {
        if (aData !== null && waitMore) {
          waitMore = false;
          return;
        }
        if (aData === null) {
          info("Result listener not called, timeout reached.");
        }
        clearTimeout(findTimeout);
        findbar.browser?.finder.removeResultListener(resultListener);
        resolve();
      };

      resultListener = {
        onFindResult: foundOrTimedout,
        onCurrentSelection() {},
        onMatchesCountResult() {},
        onHighlightFinished() {},
      };
      findbar.browser.finder.addResultListener(resultListener);
      findbar._find();
    });
  });
}

/**
 * A wrapper for the findbar's method "close", which is not synchronous
 * because of animation.
 */
function closeFindbarAndWait(findbar) {
  return new Promise(resolve => {
    if (findbar.hidden) {
      resolve();
      return;
    }
    if (window.matchMedia("(prefers-reduced-motion: reduce)").matches) {
      BrowserTestUtils.waitForMutationCondition(
        findbar,
        { attributes: true, attributeFilter: ["hidden"] },
        () => findbar.hidden
      ).then(resolve);
    } else {
      findbar.addEventListener("transitionend", function cont(aEvent) {
        if (aEvent.propertyName != "visibility") {
          return;
        }
        findbar.removeEventListener("transitionend", cont);
        resolve();
      });
    }
    let close = findbar.getElement("find-closebutton");
    close.doCommand();
  });
}

function pushPrefs(...aPrefs) {
  return new Promise(resolve => {
    SpecialPowers.pushPrefEnv({ set: aPrefs }, resolve);
  });
}

/**
 * Used to check whether the audio unblocking icon is in the tab.
 */
async function waitForTabBlockEvent(tab, expectBlocked) {
  if (tab.activeMediaBlocked == expectBlocked) {
    ok(true, "The tab should " + (expectBlocked ? "" : "not ") + "be blocked");
  } else {
    info("Block state doens't match, wait for attributes changes.");
    await BrowserTestUtils.waitForEvent(
      tab,
      "TabAttrModified",
      false,
      event => {
        if (event.detail.changed.includes("activemedia-blocked")) {
          is(
            tab.activeMediaBlocked,
            expectBlocked,
            "The tab should " + (expectBlocked ? "" : "not ") + "be blocked"
          );
          return true;
        }
        return false;
      }
    );
  }
}

/**
 * Used to check whether the tab has soundplaying attribute.
 */
async function waitForTabPlayingEvent(tab, expectPlaying) {
  if (tab.soundPlaying == expectPlaying) {
    ok(true, "The tab should " + (expectPlaying ? "" : "not ") + "be playing");
  } else {
    info("Playing state doesn't match, wait for attributes changes.");
    await BrowserTestUtils.waitForEvent(
      tab,
      "TabAttrModified",
      false,
      event => {
        if (event.detail.changed.includes("soundplaying")) {
          is(
            tab.soundPlaying,
            expectPlaying,
            "The tab should " + (expectPlaying ? "" : "not ") + "be playing"
          );
          return true;
        }
        return false;
      }
    );
  }
}

function disable_non_test_mouse(disable) {
  let utils = window.windowUtils;
  utils.disableNonTestMouseEvents(disable);
}

function hover_icon(icon, tooltip) {
  disable_non_test_mouse(true);

  let popupShownPromise = BrowserTestUtils.waitForEvent(tooltip, "popupshown");
  EventUtils.synthesizeMouse(icon, 1, 1, { type: "mouseover" });
  EventUtils.synthesizeMouse(icon, 2, 2, { type: "mousemove" });
  EventUtils.synthesizeMouse(icon, 3, 3, { type: "mousemove" });
  EventUtils.synthesizeMouse(icon, 4, 4, { type: "mousemove" });
  return popupShownPromise;
}

function leave_icon(icon) {
  EventUtils.synthesizeMouse(icon, 0, 0, { type: "mouseout" });
  EventUtils.synthesizeMouseAtCenter(document.documentElement, {
    type: "mousemove",
  });
  EventUtils.synthesizeMouseAtCenter(document.documentElement, {
    type: "mousemove",
  });
  EventUtils.synthesizeMouseAtCenter(document.documentElement, {
    type: "mousemove",
  });

  disable_non_test_mouse(false);
}

/**
 * Used to listen events if you just need it once
 */
function once(target, name) {
  var p = new Promise(function (resolve) {
    target.addEventListener(
      name,
      function () {
        resolve();
      },
      { once: true }
    );
  });
  return p;
}

/**
 * check if current wakelock is equal to expected state, if not, then wait until
 * the wakelock changes its state to expected state.
 * @param needLock
 *        the wakolock should be locked or not
 * @param isForegroundLock
 *        when the lock is on, the wakelock should be in the foreground or not
 */
async function waitForExpectedWakeLockState(
  topic,
  { needLock, isForegroundLock }
) {
  const powerManagerService = Cc["@mozilla.org/power/powermanagerservice;1"];
  const powerManager = powerManagerService.getService(
    Ci.nsIPowerManagerService
  );
  const wakelockState = powerManager.getWakeLockState(topic);
  let expectedLockState = "unlocked";
  if (needLock) {
    expectedLockState = isForegroundLock
      ? "locked-foreground"
      : "locked-background";
  }
  if (wakelockState != expectedLockState) {
    info(`wait until wakelock becomes ${expectedLockState}`);
    await wakeLockObserved(
      powerManager,
      topic,
      state => state == expectedLockState
    );
  }
  is(
    powerManager.getWakeLockState(topic),
    expectedLockState,
    `the wakelock state for '${topic}' is equal to '${expectedLockState}'`
  );
}

function wakeLockObserved(powerManager, observeTopic, checkFn) {
  return new Promise(resolve => {
    function wakeLockListener() {}
    wakeLockListener.prototype = {
      QueryInterface: ChromeUtils.generateQI(["nsIDOMMozWakeLockListener"]),
      callback(topic, state) {
        if (topic == observeTopic && checkFn(state)) {
          powerManager.removeWakeLockListener(wakeLockListener.prototype);
          resolve();
        }
      },
    };
    powerManager.addWakeLockListener(wakeLockListener.prototype);
  });
}

function getTestWebBasedURL(fileName, { crossOrigin = false } = {}) {
  const origin = crossOrigin ? "http://example.org" : "http://example.com";
  return (
    getRootDirectory(gTestPath).replace("chrome://mochitests/content", origin) +
    fileName
  );
}
