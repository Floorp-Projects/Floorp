/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["GeckoViewContentBlocking"];

const { GeckoViewModule } = ChromeUtils.import(
  "resource://gre/modules/GeckoViewModule.jsm"
);

class GeckoViewContentBlocking extends GeckoViewModule {
  onEnable() {
    debug`onEnable`;

    const flags = Ci.nsIWebProgress.NOTIFY_CONTENT_BLOCKING;
    this.progressFilter = Cc[
      "@mozilla.org/appshell/component/browser-status-filter;1"
    ].createInstance(Ci.nsIWebProgress);
    this.progressFilter.addProgressListener(this, flags);
    this.browser.addProgressListener(this.progressFilter, flags);
  }

  onDisable() {
    debug`onDisable`;

    if (!this.progressFilter) {
      return;
    }
    this.progressFilter.removeProgressListener(this);
    this.browser.removeProgressListener(this.progressFilter);
  }

  onContentBlockingEvent(aWebProgress, aRequest, aEvent) {
    debug`onContentBlockingEvent`;

    if (!aRequest || !(aRequest instanceof Ci.nsIClassifiedChannel)) {
      return;
    }

    const channel = aRequest.QueryInterface(Ci.nsIChannel);
    const uri = channel.URI && channel.URI.spec;
    const classChannel = aRequest.QueryInterface(Ci.nsIClassifiedChannel);

    if (!uri || !classChannel.matchedList) {
      return;
    }

    const message = {
      type: "GeckoView:ContentBlocked",
      uri: uri,
      matchedList: classChannel.matchedList,
    };

    this.eventDispatcher.sendRequest(message);
  }
}

// eslint-disable-next-line no-unused-vars
const { debug, warn } = GeckoViewContentBlocking.initLogging(
  "GeckoViewContentBlocking"
);
