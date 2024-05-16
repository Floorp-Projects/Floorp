/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import { GeckoViewModule } from "resource://gre/modules/GeckoViewModule.sys.mjs";

export class GeckoViewContentBlocking extends GeckoViewModule {
  onEnable() {
    const flags = Ci.nsIWebProgress.NOTIFY_CONTENT_BLOCKING;
    this.progressFilter = Cc[
      "@mozilla.org/appshell/component/browser-status-filter;1"
    ].createInstance(Ci.nsIWebProgress);
    this.progressFilter.addProgressListener(this, flags);
    this.browser.addProgressListener(this.progressFilter, flags);

    this.registerListener(["ContentBlocking:RequestLog"]);
  }

  onDisable() {
    if (this.progressFilter) {
      this.progressFilter.removeProgressListener(this);
      this.browser.removeProgressListener(this.progressFilter);
      delete this.progressFilter;
    }

    this.unregisterListener(["ContentBlocking:RequestLog"]);
  }

  // Bundle event handler.
  onEvent(aEvent, aData, aCallback) {
    debug`onEvent: event=${aEvent}, data=${aData}`;

    switch (aEvent) {
      case "ContentBlocking:RequestLog": {
        let bc = this.browser.browsingContext;

        if (!bc) {
          warn`Failed to export content blocking log.`;
          break;
        }

        // Get the top-level browsingContext. The ContentBlockingLog is located
        // in its current window global.
        bc = bc.top;

        const topWindowGlobal = bc.currentWindowGlobal;

        if (!topWindowGlobal) {
          warn`Failed to export content blocking log.`;
          break;
        }

        const log = JSON.parse(topWindowGlobal.contentBlockingLog);
        const res = Object.keys(log).map(key => {
          const blockData = log[key].map(data => {
            return {
              category: data[0],
              blocked: data[1],
              count: data[2],
            };
          });
          return {
            origin: key,
            blockData,
          };
        });

        aCallback.onSuccess({ log: res });
        break;
      }
    }
  }

  onContentBlockingEvent(aWebProgress, aRequest, aEvent) {
    debug`onContentBlockingEvent ${aEvent.toString(16)}`;

    if (!(aRequest instanceof Ci.nsIClassifiedChannel)) {
      return;
    }

    const channel = aRequest.QueryInterface(Ci.nsIChannel);
    const uri = channel.URI && channel.URI.spec;

    if (!uri) {
      return;
    }

    const classChannel = aRequest.QueryInterface(Ci.nsIClassifiedChannel);
    const blockedList = classChannel.matchedList || null;
    let loadedLists = [];

    if (aRequest instanceof Ci.nsIHttpChannel) {
      loadedLists = classChannel.matchedTrackingLists || [];
    }

    debug`onContentBlockingEvent matchedList: ${blockedList}`;
    debug`onContentBlockingEvent matchedTrackingLists: ${loadedLists}`;

    const message = {
      type: "GeckoView:ContentBlockingEvent",
      uri,
      category: aEvent,
      blockedList,
      loadedLists,
    };

    this.eventDispatcher.sendRequest(message);
  }
}

const { debug, warn } = GeckoViewContentBlocking.initLogging(
  "GeckoViewContentBlocking"
);
