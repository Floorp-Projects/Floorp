/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// This file expects tabTracker to be defined in the global scope (e.g.
// by ext-browser.js or ext-android.js).
/* global tabTracker */

ChromeUtils.defineESModuleGetters(this, {
  MatchURLFilters: "resource://gre/modules/MatchURLFilters.sys.mjs",
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.sys.mjs",
  WebNavigation: "resource://gre/modules/WebNavigation.sys.mjs",
  WebNavigationFrames: "resource://gre/modules/WebNavigationFrames.sys.mjs",
});

var { ExtensionError } = ExtensionUtils;

const defaultTransitionTypes = {
  topFrame: "link",
  subFrame: "auto_subframe",
};

const frameTransitions = {
  anyFrame: {
    qualifiers: ["server_redirect", "client_redirect", "forward_back"],
  },
  topFrame: {
    types: ["reload", "form_submit"],
  },
};

const tabTransitions = {
  topFrame: {
    qualifiers: ["from_address_bar"],
    types: ["auto_bookmark", "typed", "keyword", "generated", "link"],
  },
  subFrame: {
    types: ["manual_subframe"],
  },
};

const isTopLevelFrame = ({ frameId, parentFrameId }) => {
  return frameId == 0 && parentFrameId == -1;
};

const fillTransitionProperties = (eventName, src, dst) => {
  if (
    eventName == "onCommitted" ||
    eventName == "onHistoryStateUpdated" ||
    eventName == "onReferenceFragmentUpdated"
  ) {
    let frameTransitionData = src.frameTransitionData || {};
    let tabTransitionData = src.tabTransitionData || {};

    let transitionType,
      transitionQualifiers = [];

    // Fill transition properties for any frame.
    for (let qualifier of frameTransitions.anyFrame.qualifiers) {
      if (frameTransitionData[qualifier]) {
        transitionQualifiers.push(qualifier);
      }
    }

    if (isTopLevelFrame(dst)) {
      for (let type of frameTransitions.topFrame.types) {
        if (frameTransitionData[type]) {
          transitionType = type;
        }
      }

      for (let qualifier of tabTransitions.topFrame.qualifiers) {
        if (tabTransitionData[qualifier]) {
          transitionQualifiers.push(qualifier);
        }
      }

      for (let type of tabTransitions.topFrame.types) {
        if (tabTransitionData[type]) {
          transitionType = type;
        }
      }

      // If transitionType is not defined, defaults it to "link".
      if (!transitionType) {
        transitionType = defaultTransitionTypes.topFrame;
      }
    } else {
      // If it is sub-frame, transitionType defaults it to "auto_subframe",
      // "manual_subframe" is set only in case of a recent user interaction.
      transitionType = tabTransitionData.link
        ? "manual_subframe"
        : defaultTransitionTypes.subFrame;
    }

    // Fill the transition properties in the webNavigation event object.
    dst.transitionType = transitionType;
    dst.transitionQualifiers = transitionQualifiers;
  }
};

this.webNavigation = class extends ExtensionAPIPersistent {
  makeEventHandler(event) {
    let { extension } = this;
    let { tabManager } = extension;
    return ({ fire }, params) => {
      // Don't create a MatchURLFilters instance if the listener does not include any filter.
      let [urlFilters] = params;
      let filters = urlFilters ? new MatchURLFilters(urlFilters.url) : null;

      let listener = data => {
        if (!data.browser) {
          return;
        }
        if (
          !extension.privateBrowsingAllowed &&
          PrivateBrowsingUtils.isBrowserPrivate(data.browser)
        ) {
          return;
        }
        if (filters && !filters.matches(data.url)) {
          return;
        }

        let data2 = {
          url: data.url,
          timeStamp: Date.now(),
        };

        if (event == "onErrorOccurred") {
          data2.error = data.error;
        }

        if (data.frameId != undefined) {
          data2.frameId = data.frameId;
          data2.parentFrameId = data.parentFrameId;
        }

        if (data.sourceFrameId != undefined) {
          data2.sourceFrameId = data.sourceFrameId;
        }

        // Do not send a webNavigation event when the data.browser is related to a tab from a
        // new window opened to adopt an existent tab (See Bug 1443221 for a rationale).
        const chromeWin = data.browser.ownerGlobal;

        if (
          chromeWin &&
          chromeWin.gBrowser &&
          chromeWin.gBrowserInit &&
          chromeWin.gBrowserInit.isAdoptingTab() &&
          chromeWin.gBrowser.selectedBrowser === data.browser
        ) {
          return;
        }

        // Fills in tabId typically.
        Object.assign(data2, tabTracker.getBrowserData(data.browser));
        if (data2.tabId < 0) {
          return;
        }
        let tab = tabTracker.getTab(data2.tabId);
        if (!tabManager.canAccessTab(tab)) {
          return;
        }

        if (data.sourceTabBrowser) {
          data2.sourceTabId = tabTracker.getBrowserData(
            data.sourceTabBrowser
          ).tabId;
        }

        fillTransitionProperties(event, data, data2);

        fire.async(data2);
      };

      WebNavigation[event].addListener(listener);
      return {
        unregister() {
          WebNavigation[event].removeListener(listener);
        },
        convert(_fire) {
          fire = _fire;
        },
      };
    };
  }

  makeEventManagerAPI(event, context) {
    let self = this;
    return new EventManager({
      context,
      module: "webNavigation",
      event,
      register(fire, ...params) {
        let fn = self.makeEventHandler(event);
        return fn({ fire }, params).unregister;
      },
    }).api();
  }

  PERSISTENT_EVENTS = {
    onBeforeNavigate: this.makeEventHandler("onBeforeNavigate"),
    onCommitted: this.makeEventHandler("onCommitted"),
    onDOMContentLoaded: this.makeEventHandler("onDOMContentLoaded"),
    onCompleted: this.makeEventHandler("onCompleted"),
    onErrorOccurred: this.makeEventHandler("onErrorOccurred"),
    onReferenceFragmentUpdated: this.makeEventHandler(
      "onReferenceFragmentUpdated"
    ),
    onHistoryStateUpdated: this.makeEventHandler("onHistoryStateUpdated"),
    onCreatedNavigationTarget: this.makeEventHandler(
      "onCreatedNavigationTarget"
    ),
  };

  getAPI(context) {
    let { extension } = context;
    let { tabManager } = extension;

    return {
      webNavigation: {
        // onTabReplaced does nothing, it exists for compat.
        onTabReplaced: new EventManager({
          context,
          name: "webNavigation.onTabReplaced",
          register: fire => {
            return () => {};
          },
        }).api(),
        onBeforeNavigate: this.makeEventManagerAPI("onBeforeNavigate", context),
        onCommitted: this.makeEventManagerAPI("onCommitted", context),
        onDOMContentLoaded: this.makeEventManagerAPI(
          "onDOMContentLoaded",
          context
        ),
        onCompleted: this.makeEventManagerAPI("onCompleted", context),
        onErrorOccurred: this.makeEventManagerAPI("onErrorOccurred", context),
        onReferenceFragmentUpdated: this.makeEventManagerAPI(
          "onReferenceFragmentUpdated",
          context
        ),
        onHistoryStateUpdated: this.makeEventManagerAPI(
          "onHistoryStateUpdated",
          context
        ),
        onCreatedNavigationTarget: this.makeEventManagerAPI(
          "onCreatedNavigationTarget",
          context
        ),
        getAllFrames({ tabId }) {
          let tab = tabManager.get(tabId);
          if (tab.discarded) {
            return null;
          }
          let frames = WebNavigationFrames.getAllFrames(tab.browsingContext);
          return frames.map(fd => ({ tabId, ...fd }));
        },
        getFrame({ tabId, frameId }) {
          let tab = tabManager.get(tabId);
          if (tab.discarded) {
            return null;
          }
          let fd = WebNavigationFrames.getFrame(tab.browsingContext, frameId);
          if (!fd) {
            throw new ExtensionError(`No frame found with frameId: ${frameId}`);
          }
          return { tabId, ...fd };
        },
      },
    };
  }
};
