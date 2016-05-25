/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["WebNavigation"];

const Ci = Components.interfaces;
const Cc = Components.classes;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "RecentWindow",
                                  "resource:///modules/RecentWindow.jsm");

// Maximum amount of time that can be passed and still consider
// the data recent (similar to how is done in nsNavHistory,
// e.g. nsNavHistory::CheckIsRecentEvent, but with a lower threshold value).
const RECENT_DATA_THRESHOLD = 5 * 1000000;

// TODO:
// onCreatedNavigationTarget

var Manager = {
  listeners: new Map(),

  init() {
    // Collect recent tab transition data in a WeakMap:
    //   browser -> tabTransitionData
    this.recentTabTransitionData = new WeakMap();
    Services.obs.addObserver(this, "autocomplete-did-enter-text", true);

    Services.mm.addMessageListener("Content:Click", this);
    Services.mm.addMessageListener("Extension:DOMContentLoaded", this);
    Services.mm.addMessageListener("Extension:StateChange", this);
    Services.mm.addMessageListener("Extension:DocumentChange", this);
    Services.mm.addMessageListener("Extension:HistoryChange", this);

    Services.mm.loadFrameScript("resource://gre/modules/WebNavigationContent.js", true);
  },

  uninit() {
    // Stop collecting recent tab transition data and reset the WeakMap.
    Services.obs.removeObserver(this, "autocomplete-did-enter-text", true);
    this.recentTabTransitionData = new WeakMap();

    Services.mm.removeMessageListener("Content:Click", this);
    Services.mm.removeMessageListener("Extension:StateChange", this);
    Services.mm.removeMessageListener("Extension:DocumentChange", this);
    Services.mm.removeMessageListener("Extension:HistoryChange", this);
    Services.mm.removeMessageListener("Extension:DOMContentLoaded", this);

    Services.mm.removeDelayedFrameScript("resource://gre/modules/WebNavigationContent.js");
    Services.mm.broadcastAsyncMessage("Extension:DisableWebNavigation");
  },

  addListener(type, listener) {
    if (this.listeners.size == 0) {
      this.init();
    }

    if (!this.listeners.has(type)) {
      this.listeners.set(type, new Set());
    }
    let listeners = this.listeners.get(type);
    listeners.add(listener);
  },

  removeListener(type, listener) {
    let listeners = this.listeners.get(type);
    if (!listeners) {
      return;
    }
    listeners.delete(listener);
    if (listeners.size == 0) {
      this.listeners.delete(type);
    }

    if (this.listeners.size == 0) {
      this.uninit();
    }
  },

  /**
   *  Support nsIObserver interface to observe the urlbar autocomplete events used
   *  to keep track of the urlbar user interaction.
   */
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver, Ci.nsISupportsWeakReference]),

  /**
   *  Observe autocomplete-did-enter-text topic to track the user interaction with
   *  the awesome bar.
   */
  observe: function(subject, topic, data) {
    if (topic == "autocomplete-did-enter-text") {
      this.onURLBarAutoCompletion(subject, topic, data);
    }
  },

  /**
   *  Recognize the type of urlbar user interaction (e.g. typing a new url,
   *  clicking on an url generated from a searchengine or a keyword, or a
   *  bookmark found by the urlbar autocompletion).
   */
  onURLBarAutoCompletion(subject, topic, data) {
    if (subject && subject instanceof Ci.nsIAutoCompleteInput) {
      // We are only interested in urlbar autocompletion events
      if (subject.id !== "urlbar") {
        return;
      }

      let controller = subject.popup.view.QueryInterface(Ci.nsIAutoCompleteController);
      let idx = subject.popup.selectedIndex;

      let tabTransistionData = {
        from_address_bar: true,
      };

      if (idx < 0 || idx >= controller.matchCount) {
        // Recognize when no valid autocomplete results has been selected.
        tabTransistionData.typed = true;
      } else {
        let value = controller.getValueAt(idx);
        let action = subject._parseActionUrl(value);

        if (action) {
          // Detect keywork and generated and more typed scenarios.
          switch (action.type) {
            case "keyword":
              tabTransistionData.keyword = true;
              break;
            case "searchengine":
            case "searchsuggestion":
              tabTransistionData.generated = true;
              break;
            case "visiturl":
              // Visiturl are autocompletion results related to
              // history suggestions.
              tabTransistionData.typed = true;
              break;
            case "remotetab":
              // Remote tab are autocomplete results related to
              // tab urls from a remote synchronized Firefox.
              tabTransistionData.typed = true;
              break;
            case "switchtab":
              // This "switchtab" autocompletion should be ignored, because
              // it is not related to a navigation.
              return;
            default:
              // Fallback on "typed" if unable to detect a known moz-action type.
              tabTransistionData.typed = true;
          }
        } else {
          // Special handling for bookmark urlbar autocompletion
          // (which happens when we got a null action and a valid selectedIndex)
          let styles = new Set(controller.getStyleAt(idx).split(/\s+/));

          if (styles.has("bookmark")) {
            tabTransistionData.auto_bookmark = true;
          } else {
            // Fallback on "typed" if unable to detect a specific actionType
            // (and when in the styles there are "autofill" or "history").
            tabTransistionData.typed = true;
          }
        }
      }

      this.setRecentTabTransitionData(tabTransistionData);
    }
  },

  /**
   *  Keep track of a recent user interaction and cache it in a
   *  map associated to the current selected tab.
   */
  setRecentTabTransitionData(tabTransitionData) {
    let window = RecentWindow.getMostRecentBrowserWindow();
    if (window && window.gBrowser && window.gBrowser.selectedTab &&
        window.gBrowser.selectedTab.linkedBrowser) {
      let browser = window.gBrowser.selectedTab.linkedBrowser;

      // Get recent tab transition data to update if any.
      let prevData = this.getAndForgetRecentTabTransitionData(browser);

      let newData = Object.assign(
        {time: Date.now()},
        prevData,
        tabTransitionData
      );
      this.recentTabTransitionData.set(browser, newData);
    }
  },

  /**
   *  Retrieve recent data related to a recent user interaction give a
   *  given tab's linkedBrowser (only if is is more recent than the
   *  `RECENT_DATA_THRESHOLD`).
   *
   *  NOTE: this method is used to retrieve the tab transition data
   *  collected when one of the `onCommitted`, `onHistoryStateUpdated`
   *  or `onReferenceFragmentUpdated` events has been received.
   */
  getAndForgetRecentTabTransitionData(browser) {
    let data = this.recentTabTransitionData.get(browser);
    this.recentTabTransitionData.delete(browser);

    // Return an empty object if there isn't any tab transition data
    // or if it's less recent than RECENT_DATA_THRESHOLD.
    if (!data || (data.time - Date.now()) > RECENT_DATA_THRESHOLD) {
      return {};
    }

    return data;
  },

  /**
   *  Receive messages from the WebNavigationContent.js framescript
   *  over message manager events.
   */
  receiveMessage({name, data, target}) {
    switch (name) {
      case "Extension:StateChange":
        this.onStateChange(target, data);
        break;

      case "Extension:DocumentChange":
        this.onDocumentChange(target, data);
        break;

      case "Extension:HistoryChange":
        this.onHistoryChange(target, data);
        break;

      case "Extension:DOMContentLoaded":
        this.onLoad(target, data);
        break;

      case "Content:Click":
        this.onContentClick(target, data);
        break;
    }
  },

  onContentClick(target, data) {
    // We are interested only on clicks to links which are not "add to bookmark" commands
    if (data.href && !data.bookmark) {
      let ownerWin = target.ownerDocument.defaultView;
      let where = ownerWin.whereToOpenLink(data);
      if (where == "current") {
        this.setRecentTabTransitionData({link: true});
      }
    }
  },

  onStateChange(browser, data) {
    let stateFlags = data.stateFlags;
    if (stateFlags & Ci.nsIWebProgressListener.STATE_IS_WINDOW) {
      let url = data.requestURL;
      if (stateFlags & Ci.nsIWebProgressListener.STATE_START) {
        this.fire("onBeforeNavigate", browser, data, {url});
      } else if (stateFlags & Ci.nsIWebProgressListener.STATE_STOP) {
        if (Components.isSuccessCode(data.status)) {
          this.fire("onCompleted", browser, data, {url});
        } else {
          let error = `Error code ${data.status}`;
          this.fire("onErrorOccurred", browser, data, {error, url});
        }
      }
    }
  },

  onDocumentChange(browser, data) {
    let extra = {
      url: data.location,
      // Transition data which is coming from the content process.
      frameTransitionData: data.frameTransitionData,
      tabTransitionData: this.getAndForgetRecentTabTransitionData(browser),
    };

    this.fire("onCommitted", browser, data, extra);
  },

  onHistoryChange(browser, data) {
    let extra = {
      url: data.location,
      // Transition data which is coming from the content process.
      frameTransitionData: data.frameTransitionData,
      tabTransitionData: this.getAndForgetRecentTabTransitionData(browser),
    };

    if (data.isReferenceFragmentUpdated) {
      this.fire("onReferenceFragmentUpdated", browser, data, extra);
    } else if (data.isHistoryStateUpdated) {
      this.fire("onHistoryStateUpdated", browser, data, extra);
    }
  },

  onLoad(browser, data) {
    this.fire("onDOMContentLoaded", browser, data, {url: data.url});
  },

  fire(type, browser, data, extra) {
    let listeners = this.listeners.get(type);
    if (!listeners) {
      return;
    }

    let details = {
      browser,
      windowId: data.windowId,
    };

    if (data.parentWindowId) {
      details.parentWindowId = data.parentWindowId;
    }

    for (let prop in extra) {
      details[prop] = extra[prop];
    }

    for (let listener of listeners) {
      listener(details);
    }
  },
};

const EVENTS = [
  "onBeforeNavigate",
  "onCommitted",
  "onDOMContentLoaded",
  "onCompleted",
  "onErrorOccurred",
  "onReferenceFragmentUpdated",
  "onHistoryStateUpdated",
  // "onCreatedNavigationTarget",
];

var WebNavigation = {};

for (let event of EVENTS) {
  WebNavigation[event] = {
    addListener: Manager.addListener.bind(Manager, event),
    removeListener: Manager.removeListener.bind(Manager, event),
  };
}
