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
}

const { debug, warn } = GeckoViewContentBlockingChild.initLogging(
  "GeckoViewContentBlocking"
); // eslint-disable-line no-unused-vars
const module = GeckoViewContentBlockingChild.create(this);
