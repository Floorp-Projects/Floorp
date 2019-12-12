/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { GeckoViewChildModule } = ChromeUtils.import(
  "resource://gre/modules/GeckoViewChildModule.jsm"
);

class GeckoViewContentBlockingChild extends GeckoViewChildModule {
  onEnable() {
    debug`onEnable`;

    const flags = Ci.nsIWebProgress.NOTIFY_CONTENT_BLOCKING;
    this.progressFilter = Cc[
      "@mozilla.org/appshell/component/browser-status-filter;1"
    ].createInstance(Ci.nsIWebProgress);
    this.progressFilter.addProgressListener(this, flags);
    const webProgress = docShell
      .QueryInterface(Ci.nsIInterfaceRequestor)
      .getInterface(Ci.nsIWebProgress);
    webProgress.addProgressListener(this.progressFilter, flags);

    this.messageManager.addMessageListener("ContentBlocking:RequestLog", this);
  }

  receiveMessage(aMsg) {
    debug`receiveMessage: ${aMsg.name}`;

    switch (aMsg.name) {
      case "ContentBlocking:RequestLog": {
        docShell.getContentBlockingLog().then(
          val =>
            sendAsyncMessage("ContentBlocking:ExportLog", {
              log: JSON.parse(val),
              id: aMsg.data.id,
            }),
          reason =>
            sendAsyncMessage("ContentBlocking:ExportLog", {
              error: reason,
              id: aMsg.data.id,
            })
        );

        break;
      }
    }
  }

  onContentBlockingEvent(aWebProgress, aRequest, aEvent) {
    debug`onContentBlockingEvent ${aEvent.toString(16)}`;

    if (!aRequest || !(aRequest instanceof Ci.nsIClassifiedChannel)) {
      return;
    }

    const channel = aRequest.QueryInterface(Ci.nsIChannel);
    const uri = channel.URI && channel.URI.spec;
    const classChannel = aRequest.QueryInterface(Ci.nsIClassifiedChannel);

    if (!uri) {
      return;
    }

    debug`onContentBlockingEvent matchedList: ${classChannel.matchedList}`;
    debug`onContentBlockingEvent matchedTrackingLists: ${
      classChannel.matchedTrackingLists
    }`;

    const message = {
      type: "GeckoView:ContentBlockingEvent",
      uri: uri,
      category: aEvent,
      blockedList: classChannel.matchedList || null,
      loadedLists: classChannel.matchedTrackingLists,
    };

    this.eventDispatcher.sendRequest(message);
  }
}

const { debug, warn } = GeckoViewContentBlockingChild.initLogging(
  "GeckoViewContentBlocking"
); // eslint-disable-line no-unused-vars
const module = GeckoViewContentBlockingChild.create(this);
