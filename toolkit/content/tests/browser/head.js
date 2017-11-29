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

/**
 * Helper class for testing datetime input picker widget
 */
class DateTimeTestHelper {
  constructor() {
    this.panel = document.getElementById("DateTimePickerPanel");
    this.panel.setAttribute("animate", false);
    this.tab = null;
    this.frame = null;
  }

  /**
   * Opens a new tab with the URL of the test page, and make sure the picker is
   * ready for testing.
   *
   * @param  {String} pageUrl
   */
  async openPicker(pageUrl) {
    this.tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, pageUrl);
    await BrowserTestUtils.synthesizeMouseAtCenter("input", {}, gBrowser.selectedBrowser);
    // If dateTimePopupFrame doesn't exist yet, wait for the binding to be attached
    if (!this.panel.dateTimePopupFrame) {
      await BrowserTestUtils.waitForEvent(this.panel, "DateTimePickerBindingReady");
    }
    this.frame = this.panel.dateTimePopupFrame;
    await this.waitForPickerReady();
  }

  async waitForPickerReady() {
    await BrowserTestUtils.waitForEvent(this.frame, "load", true);
    // Wait for picker elements to be ready
    await BrowserTestUtils.waitForEvent(this.frame.contentDocument, "PickerReady");
  }

  /**
   * Find an element on the picker.
   *
   * @param  {String} selector
   * @return {DOMElement}
   */
  getElement(selector) {
    return this.frame.contentDocument.querySelector(selector);
  }

  /**
   * Find the children of an element on the picker.
   *
   * @param  {String} selector
   * @return {Array<DOMElement>}
   */
  getChildren(selector) {
    return Array.from(this.getElement(selector).children);
  }

  /**
   * Click on an element
   *
   * @param  {DOMElement} element
   */
  click(element) {
    EventUtils.synthesizeMouseAtCenter(element, {}, this.frame.contentWindow);
  }

  /**
   * Close the panel and the tab
   */
  async tearDown() {
    if (!this.panel.hidden) {
      let pickerClosePromise = new Promise(resolve => {
        this.panel.addEventListener("popuphidden", resolve, {once: true});
      });
      this.panel.hidePopup();
      this.panel.closePicker();
      await pickerClosePromise;
    }
    await BrowserTestUtils.removeTab(this.tab);
    this.tab = null;
  }

  /**
   * Clean up after tests. Remove the frame to prevent leak.
   */
  cleanup() {
    this.frame.remove();
    this.frame = null;
    this.panel.removeAttribute("animate");
    this.panel = null;
  }
}

/**
 * Used to listen events if you just need it once
 */
function once(target, name) {
  var p = new Promise(function(resolve, reject) {
    target.addEventListener(name, function() {
      resolve();
    }, {once: true});
  });
  return p;
}
