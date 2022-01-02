/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["WebNavigation", "WebNavigationManager"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);

ChromeUtils.defineModuleGetter(
  this,
  "BrowserWindowTracker",
  "resource:///modules/BrowserWindowTracker.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "PrivateBrowsingUtils",
  "resource://gre/modules/PrivateBrowsingUtils.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "UrlbarUtils",
  "resource:///modules/UrlbarUtils.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "WebNavigationFrames",
  "resource://gre/modules/WebNavigationFrames.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "ClickHandlerParent",
  "resource:///actors/ClickHandlerParent.jsm"
);

// Maximum amount of time that can be passed and still consider
// the data recent (similar to how is done in nsNavHistory,
// e.g. nsNavHistory::CheckIsRecentEvent, but with a lower threshold value).
const RECENT_DATA_THRESHOLD = 5 * 1000000;

function getBrowser(bc) {
  return bc.top.embedderElement;
}

var WebNavigationManager = {
  // Map[string -> Map[listener -> URLFilter]]
  listeners: new Map(),

  init() {
    // Collect recent tab transition data in a WeakMap:
    //   browser -> tabTransitionData
    this.recentTabTransitionData = new WeakMap();

    Services.obs.addObserver(this, "urlbar-user-start-navigation", true);

    Services.obs.addObserver(this, "webNavigation-createdNavigationTarget");

    if (AppConstants.MOZ_BUILD_APP == "browser") {
      ClickHandlerParent.addContentClickListener(this);
    }
  },

  uninit() {
    // Stop collecting recent tab transition data and reset the WeakMap.
    Services.obs.removeObserver(this, "urlbar-user-start-navigation");
    Services.obs.removeObserver(this, "webNavigation-createdNavigationTarget");

    if (AppConstants.MOZ_BUILD_APP == "browser") {
      ClickHandlerParent.removeContentClickListener(this);
    }

    this.recentTabTransitionData = new WeakMap();
  },

  addListener(type, listener, filters, context) {
    if (this.listeners.size == 0) {
      this.init();
    }

    if (!this.listeners.has(type)) {
      this.listeners.set(type, new Map());
    }
    let listeners = this.listeners.get(type);
    listeners.set(listener, { filters, context });
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
   * Support nsIObserver interface to observe the urlbar autocomplete events used
   * to keep track of the urlbar user interaction.
   */
  QueryInterface: ChromeUtils.generateQI([
    "extIWebNavigation",
    "nsIObserver",
    "nsISupportsWeakReference",
  ]),

  /**
   * Observe webNavigation-createdNavigationTarget (to fire the onCreatedNavigationTarget
   * related to windows or tabs opened from the main process) topics.
   *
   * @param {nsIAutoCompleteInput|Object} subject
   * @param {string} topic
   * @param {string|undefined} data
   */
  observe: function(subject, topic, data) {
    if (topic == "urlbar-user-start-navigation") {
      this.onURLBarUserStartNavigation(subject.wrappedJSObject);
    } else if (topic == "webNavigation-createdNavigationTarget") {
      // The observed notification is coming from privileged JavaScript components running
      // in the main process (e.g. when a new tab or window is opened using the context menu
      // or Ctrl/Shift + click on a link).
      const {
        createdTabBrowser,
        url,
        sourceFrameID,
        sourceTabBrowser,
      } = subject.wrappedJSObject;

      this.fire("onCreatedNavigationTarget", createdTabBrowser, null, {
        sourceTabBrowser,
        sourceFrameId: sourceFrameID,
        url,
      });
    }
  },

  /**
   * Recognize the type of urlbar user interaction (e.g. typing a new url,
   * clicking on an url generated from a searchengine or a keyword, or a
   * bookmark found by the urlbar autocompletion).
   *
   * @param {object} acData
   *   The data for the autocompleted item.
   * @param {object} [acData.result]
   *   The result information associated with the navigation action.
   * @param {UrlbarUtils.RESULT_TYPE} [acData.result.type]
   *   The result type associated with the navigation action.
   * @param {UrlbarUtils.RESULT_SOURCE} [acData.result.source]
   *   The result source associated with the navigation action.
   */
  onURLBarUserStartNavigation(acData) {
    let tabTransitionData = {
      from_address_bar: true,
    };

    if (!acData.result) {
      tabTransitionData.typed = true;
    } else {
      switch (acData.result.type) {
        case UrlbarUtils.RESULT_TYPE.KEYWORD:
          tabTransitionData.keyword = true;
          break;
        case UrlbarUtils.RESULT_TYPE.SEARCH:
          tabTransitionData.generated = true;
          break;
        case UrlbarUtils.RESULT_TYPE.URL:
          if (acData.result.source == UrlbarUtils.RESULT_SOURCE.BOOKMARKS) {
            tabTransitionData.auto_bookmark = true;
          } else {
            tabTransitionData.typed = true;
          }
          break;
        case UrlbarUtils.RESULT_TYPE.REMOTE_TAB:
          // Remote tab are autocomplete results related to
          // tab urls from a remote synchronized Firefox.
          tabTransitionData.typed = true;
          break;
        case UrlbarUtils.RESULT_TYPE.TAB_SWITCH:
        // This "switchtab" autocompletion should be ignored, because
        // it is not related to a navigation.
        // Fall through.
        case UrlbarUtils.RESULT_TYPE.OMNIBOX:
        // "Omnibox" should be ignored as the add-on may or may not initiate
        // a navigation on the item being selected.
        // Fall through.
        case UrlbarUtils.RESULT_TYPE.TIP:
          // "Tip" should be ignored since the tip will only initiate navigation
          // if there is a valid buttonUrl property, which is optional.
          throw new Error(
            `Unexpectedly received notification for ${acData.result.type}`
          );
        default:
          Cu.reportError(
            `Received unexpected result type ${acData.result.type}, falling back to typed transition.`
          );
          // Fallback on "typed" if the type is unknown.
          tabTransitionData.typed = true;
      }
    }

    this.setRecentTabTransitionData(tabTransitionData);
  },

  /**
   * Keep track of a recent user interaction and cache it in a
   * map associated to the current selected tab.
   *
   * @param {object} tabTransitionData
   * @param {boolean} [tabTransitionData.auto_bookmark]
   * @param {boolean} [tabTransitionData.from_address_bar]
   * @param {boolean} [tabTransitionData.generated]
   * @param {boolean} [tabTransitionData.keyword]
   * @param {boolean} [tabTransitionData.link]
   * @param {boolean} [tabTransitionData.typed]
   */
  setRecentTabTransitionData(tabTransitionData) {
    let window = BrowserWindowTracker.getTopWindow();
    if (
      window &&
      window.gBrowser &&
      window.gBrowser.selectedTab &&
      window.gBrowser.selectedTab.linkedBrowser
    ) {
      let browser = window.gBrowser.selectedTab.linkedBrowser;

      // Get recent tab transition data to update if any.
      let prevData = this.getAndForgetRecentTabTransitionData(browser);

      let newData = Object.assign(
        { time: Date.now() },
        prevData,
        tabTransitionData
      );
      this.recentTabTransitionData.set(browser, newData);
    }
  },

  /**
   * Retrieve recent data related to a recent user interaction give a
   * given tab's linkedBrowser (only if is is more recent than the
   * `RECENT_DATA_THRESHOLD`).
   *
   * NOTE: this method is used to retrieve the tab transition data
   * collected when one of the `onCommitted`, `onHistoryStateUpdated`
   * or `onReferenceFragmentUpdated` events has been received.
   *
   * @param {XULBrowserElement} browser
   * @returns {object}
   */
  getAndForgetRecentTabTransitionData(browser) {
    let data = this.recentTabTransitionData.get(browser);
    this.recentTabTransitionData.delete(browser);

    // Return an empty object if there isn't any tab transition data
    // or if it's less recent than RECENT_DATA_THRESHOLD.
    if (!data || data.time - Date.now() > RECENT_DATA_THRESHOLD) {
      return {};
    }

    return data;
  },

  onContentClick(target, data) {
    // We are interested only on clicks to links which are not "add to bookmark" commands
    if (data.href && !data.bookmark) {
      let ownerWin = target.ownerGlobal;
      let where = ownerWin.whereToOpenLink(data);
      if (where == "current") {
        this.setRecentTabTransitionData({ link: true });
      }
    }
  },

  onCreatedNavigationTarget(bc, sourceBC, url) {
    if (!this.listeners.size) {
      return;
    }

    let browser = getBrowser(bc);

    this.fire("onCreatedNavigationTarget", browser, null, {
      sourceTabBrowser: getBrowser(sourceBC),
      sourceFrameId: WebNavigationFrames.getFrameId(sourceBC),
      url,
    });
  },

  onStateChange(bc, requestURI, status, stateFlags) {
    if (!this.listeners.size) {
      return;
    }

    let browser = getBrowser(bc);

    if (stateFlags & Ci.nsIWebProgressListener.STATE_IS_WINDOW) {
      let url = requestURI.spec;
      if (stateFlags & Ci.nsIWebProgressListener.STATE_START) {
        this.fire("onBeforeNavigate", browser, bc, { url });
      } else if (stateFlags & Ci.nsIWebProgressListener.STATE_STOP) {
        if (Components.isSuccessCode(status)) {
          this.fire("onCompleted", browser, bc, { url });
        } else {
          let error = `Error code ${status}`;
          this.fire("onErrorOccurred", browser, bc, { error, url });
        }
      }
    }
  },

  onDocumentChange(bc, frameTransitionData, location) {
    if (!this.listeners.size) {
      return;
    }

    let browser = getBrowser(bc);

    let extra = {
      url: location ? location.spec : "",
      // Transition data which is coming from the content process.
      frameTransitionData,
      tabTransitionData: this.getAndForgetRecentTabTransitionData(browser),
    };

    this.fire("onCommitted", browser, bc, extra);
  },

  onHistoryChange(
    bc,
    frameTransitionData,
    location,
    isHistoryStateUpdated,
    isReferenceFragmentUpdated
  ) {
    if (!this.listeners.size) {
      return;
    }

    let browser = getBrowser(bc);

    let extra = {
      url: location ? location.spec : "",
      // Transition data which is coming from the content process.
      frameTransitionData,
      tabTransitionData: this.getAndForgetRecentTabTransitionData(browser),
    };

    if (isReferenceFragmentUpdated) {
      this.fire("onReferenceFragmentUpdated", browser, bc, extra);
    } else if (isHistoryStateUpdated) {
      this.fire("onHistoryStateUpdated", browser, bc, extra);
    }
  },

  onDOMContentLoaded(bc, documentURI) {
    if (!this.listeners.size) {
      return;
    }

    let browser = getBrowser(bc);

    this.fire("onDOMContentLoaded", browser, bc, { url: documentURI.spec });
  },

  fire(type, browser, bc, extra) {
    if (!browser) {
      return;
    }

    let listeners = this.listeners.get(type);
    if (!listeners) {
      return;
    }

    let details = {
      browser,
    };

    if (bc) {
      details.frameId = WebNavigationFrames.getFrameId(bc);
      details.parentFrameId = WebNavigationFrames.getParentFrameId(bc);
    }

    for (let prop in extra) {
      details[prop] = extra[prop];
    }

    for (let [listener, { filters, context }] of listeners) {
      if (
        context &&
        !context.privateBrowsingAllowed &&
        PrivateBrowsingUtils.isBrowserPrivate(browser)
      ) {
        continue;
      }
      // Call the listener if the listener has no filter or if its filter matches.
      if (!filters || filters.matches(extra.url)) {
        listener(details);
      }
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
  "onCreatedNavigationTarget",
];

var WebNavigation = {};

for (let event of EVENTS) {
  WebNavigation[event] = {
    addListener: WebNavigationManager.addListener.bind(
      WebNavigationManager,
      event
    ),
    removeListener: WebNavigationManager.removeListener.bind(
      WebNavigationManager,
      event
    ),
  };
}
