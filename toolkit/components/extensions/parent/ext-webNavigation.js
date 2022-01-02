/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// This file expects tabTracker to be defined in the global scope (e.g.
// by ext-browser.js or ext-android.js).
/* global tabTracker */

ChromeUtils.defineModuleGetter(
  this,
  "MatchURLFilters",
  "resource://gre/modules/MatchURLFilters.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "WebNavigation",
  "resource://gre/modules/WebNavigation.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "WebNavigationFrames",
  "resource://gre/modules/WebNavigationFrames.jsm"
);
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

// Similar to WebRequestEventManager but for WebNavigation.
class WebNavigationEventManager extends EventManager {
  constructor(context, eventName) {
    let { tabManager } = context.extension;
    let name = `webNavigation.${eventName}`;
    let register = (fire, urlFilters) => {
      // Don't create a MatchURLFilters instance if the listener does not include any filter.
      let filters = urlFilters ? new MatchURLFilters(urlFilters.url) : null;

      let listener = data => {
        if (!data.browser) {
          return;
        }

        let data2 = {
          url: data.url,
          timeStamp: Date.now(),
        };

        if (eventName == "onErrorOccurred") {
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

        fillTransitionProperties(eventName, data, data2);

        fire.async(data2);
      };

      WebNavigation[eventName].addListener(listener, filters, context);
      return () => {
        WebNavigation[eventName].removeListener(listener);
      };
    };

    super({ context, name, register });
  }
}

this.webNavigation = class extends ExtensionAPI {
  getAPI(context) {
    let { tabManager } = context.extension;

    return {
      webNavigation: {
        onTabReplaced: new EventManager({
          context,
          name: "webNavigation.onTabReplaced",
          register: fire => {
            return () => {};
          },
        }).api(),
        onBeforeNavigate: new WebNavigationEventManager(
          context,
          "onBeforeNavigate"
        ).api(),
        onCommitted: new WebNavigationEventManager(
          context,
          "onCommitted"
        ).api(),
        onDOMContentLoaded: new WebNavigationEventManager(
          context,
          "onDOMContentLoaded"
        ).api(),
        onCompleted: new WebNavigationEventManager(
          context,
          "onCompleted"
        ).api(),
        onErrorOccurred: new WebNavigationEventManager(
          context,
          "onErrorOccurred"
        ).api(),
        onReferenceFragmentUpdated: new WebNavigationEventManager(
          context,
          "onReferenceFragmentUpdated"
        ).api(),
        onHistoryStateUpdated: new WebNavigationEventManager(
          context,
          "onHistoryStateUpdated"
        ).api(),
        onCreatedNavigationTarget: new WebNavigationEventManager(
          context,
          "onCreatedNavigationTarget"
        ).api(),
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
