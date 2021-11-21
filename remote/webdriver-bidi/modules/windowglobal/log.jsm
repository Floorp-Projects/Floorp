/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["log"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  Services: "resource://gre/modules/Services.jsm",

  Module: "chrome://remote/content/shared/messagehandler/Module.jsm",
  serialize: "chrome://remote/content/webdriver-bidi/RemoteValue.jsm",
});

class Log extends Module {
  constructor(messageHandler) {
    super(messageHandler);

    this._onConsoleAPILogEvent = this._onConsoleAPILogEvent.bind(this);
  }

  destroy() {
    Services.obs.removeObserver(
      this._onConsoleAPILogEvent,
      "console-api-log-event"
    );
  }

  /**
   * Private methods
   */
  _applySessionData(params) {
    // TODO: Bug 1741861. Move this logic to a shared module or the an abstract
    // class.
    if (params.category === "event") {
      for (const event of params.values) {
        this._subscribeEvent(event);
      }
    }
  }

  _subscribeEvent(event) {
    if (event === "log.entryAdded") {
      Services.obs.addObserver(
        this._onConsoleAPILogEvent,
        "console-api-log-event"
      );
    }
  }

  _onConsoleAPILogEvent(message) {
    const browsingContext = BrowsingContext.get(this.messageHandler.contextId);
    const innerWindowId =
      browsingContext.window.windowGlobalChild?.innerWindowId;

    const messageObject = message.wrappedJSObject;
    if (innerWindowId !== messageObject.innerID) {
      // If the message doesn't match the innerWindowId of the current context
      // ignore it.
      return;
    }

    // Step numbers below refer to the specifications at
    //   https://w3c.github.io/webdriver-bidi/#event-log-entryAdded

    // 1. The console method used to create the messageObject is stored in the
    // `level` property. Translate it to a log.LogEntry level
    const method = messageObject.level;
    const level = this._getLogEntryLevelFromConsoleMethod(method);

    // 2. Use the message's timeStamp or fallback on the current time value.
    const timestamp = messageObject.timeStamp || Date.now();

    // 3. Start assembling the text representation of the message.
    let text = "";

    // 4. Formatters have already been applied at this points.
    // messageObject.arguments corresponds to the "formatted args" from the
    // specifications.

    // 5. Concatenate all formatted arguments in text
    // TODO: For m1 we only support string arguments, so we rely on the builtin
    // toString for each argument which will be available in message.arguments.
    const args = messageObject.arguments || [];
    text += args.map(String).join(" ");

    // Step 6 and 7: Serialize each arg as remote value.
    const serializedArgs = [];
    for (const arg of args) {
      serializedArgs.push(serialize(arg /*, null, true, new Set() */));
    }

    // 8. TODO: set realm to the current realm id.

    // 9. TODO: set stack to the current stack trace.

    // 10. Build the ConsoleLogEntry
    const entry = {
      level,
      text,
      timestamp,
      method,
      realm: null,
      args: serializedArgs,
      type: "console",
    };

    // TODO: steps 11. to 15. Those steps relate to:
    // - emitting associated BrowsingContext. See log.entryAdded full support
    //   in https://bugzilla.mozilla.org/show_bug.cgi?id=1724669#c0
    // - handling cases where session doesn't exist or the event is not
    //   monitored. The implementation differs from the spec here because we
    //   only react to events if there is a session & if the session subscribed
    //   to those events.

    this.messageHandler.emitMessageHandlerEvent("log.entryAdded", entry);
  }

  _getLogEntryLevelFromConsoleMethod(method) {
    switch (method) {
      case "assert":
      case "error":
        return "error";
      case "debug":
      case "trace":
        return "debug";
      case "warn":
      case "warning":
        return "warning";
      default:
        return "info";
    }
  }
}

const log = Log;
