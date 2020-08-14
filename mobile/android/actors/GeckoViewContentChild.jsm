/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { GeckoViewActorChild } = ChromeUtils.import(
  "resource://gre/modules/GeckoViewActorChild.jsm"
);

var { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  SessionHistory: "resource://gre/modules/sessionstore/SessionHistory.jsm",
  PrivacyFilter: "resource://gre/modules/sessionstore/PrivacyFilter.jsm",
});

var EXPORTED_SYMBOLS = ["GeckoViewContentChild"];

class GeckoViewContentChild extends GeckoViewActorChild {
  collectSessionState() {
    const { docShell, contentWindow } = this;
    const history = SessionHistory.collect(docShell);
    let formdata = SessionStoreUtils.collectFormData(contentWindow);
    let scrolldata = SessionStoreUtils.collectScrollPosition(contentWindow);

    // Save the current document resolution.
    let zoom = 1;
    const domWindowUtils = contentWindow.windowUtils;
    zoom = domWindowUtils.getResolution();
    scrolldata = scrolldata || {};
    scrolldata.zoom = {};
    scrolldata.zoom.resolution = zoom;

    // Save some data that'll help in adjusting the zoom level
    // when restoring in a different screen orientation.
    const displaySize = {};
    const width = {},
      height = {};
    domWindowUtils.getContentViewerSize(width, height);

    displaySize.width = width.value;
    displaySize.height = height.value;

    scrolldata.zoom.displaySize = displaySize;

    formdata = PrivacyFilter.filterFormData(formdata || {});

    return { history, formdata, scrolldata };
  }

  receiveMessage(message) {
    const { name } = message;
    switch (name) {
      case "CollectSessionState": {
        return this.collectSessionState();
      }
      default:
        return null;
    }
  }
}

const { debug, warn } = GeckoViewContentChild.initLogging("GeckoViewContent"); // eslint-disable-line no-unused-vars
