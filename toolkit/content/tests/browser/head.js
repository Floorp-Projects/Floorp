"use strict";

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");


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
  return new Promise(resolve => {
    SpecialPowers.pushPrefEnv({"set": aPrefs}, resolve);
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
    await BrowserTestUtils.waitForEvent(tab, "TabAttrModified", false, (event) => {
      if (event.detail.changed.indexOf("activemedia-blocked") >= 0) {
        is(tab.activeMediaBlocked, expectBlocked, "The tab should " + (expectBlocked ? "" : "not ") + "be blocked");
        return true;
      }
      return false;
    });
  }
}

/**
 * Used to check whether the tab has soundplaying attribute.
 */
async function waitForTabPlayingEvent(tab, expectPlaying) {
  if (tab.soundPlaying == expectPlaying) {
    ok(true, "The tab should " + (expectPlaying ? "" : "not ") + "be playing");
  } else {
    info("Playing state doens't match, wait for attributes changes.");
    await BrowserTestUtils.waitForEvent(tab, "TabAttrModified", false, (event) => {
      if (event.detail.changed.indexOf("soundplaying") >= 0) {
        is(tab.soundPlaying, expectPlaying, "The tab should " + (expectPlaying ? "" : "not ") + "be playing");
        return true;
      }
      return false;
    });
  }
}

function getTestPlugin(pluginName) {
  var ph = SpecialPowers.Cc["@mozilla.org/plugin/host;1"]
                                 .getService(SpecialPowers.Ci.nsIPluginHost);
  var tags = ph.getPluginTags();
  var name = pluginName || "Test Plug-in";
  for (var tag of tags) {
    if (tag.name == name) {
      return tag;
    }
  }

  ok(false, "Could not find plugin tag with plugin name '" + name + "'");
  return null;
}

function setTestPluginEnabledState(newEnabledState, pluginName) {
  var oldEnabledState = SpecialPowers.setTestPluginEnabledState(newEnabledState, pluginName);
  if (!oldEnabledState) {
    return;
  }
  var plugin = getTestPlugin(pluginName);
  // Run a nested event loop to wait for the preference change to
  // propagate to the child. Yuck!
  SpecialPowers.Services.tm.spinEventLoopUntil(() => {
    return plugin.enabledState == newEnabledState;
  });
  SimpleTest.registerCleanupFunction(function() {
    SpecialPowers.setTestPluginEnabledState(oldEnabledState, pluginName);
  });
}

function disable_non_test_mouse(disable) {
  let utils = window.QueryInterface(Ci.nsIInterfaceRequestor)
                    .getInterface(Ci.nsIDOMWindowUtils);
  utils.disableNonTestMouseEvents(disable);
}

function hover_icon(icon, tooltip) {
  disable_non_test_mouse(true);

  let popupShownPromise = BrowserTestUtils.waitForEvent(tooltip, "popupshown");
  EventUtils.synthesizeMouse(icon, 1, 1, {type: "mouseover"});
  EventUtils.synthesizeMouse(icon, 2, 2, {type: "mousemove"});
  EventUtils.synthesizeMouse(icon, 3, 3, {type: "mousemove"});
  EventUtils.synthesizeMouse(icon, 4, 4, {type: "mousemove"});
  return popupShownPromise;
}

function leave_icon(icon) {
  EventUtils.synthesizeMouse(icon, 0, 0, {type: "mouseout"});
  EventUtils.synthesizeMouseAtCenter(document.documentElement, {type: "mousemove"});
  EventUtils.synthesizeMouseAtCenter(document.documentElement, {type: "mousemove"});
  EventUtils.synthesizeMouseAtCenter(document.documentElement, {type: "mousemove"});

  disable_non_test_mouse(false);
}
