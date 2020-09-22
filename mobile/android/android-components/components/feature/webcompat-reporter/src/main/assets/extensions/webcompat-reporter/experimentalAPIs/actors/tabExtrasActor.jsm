/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

var EXPORTED_SYMBOLS = ["ReportSiteIssueHelperChild"];

const PREVIEW_MAX_ITEMS = 10;
const LOG_LEVELS = ["debug", "info", "warn", "error"];

function getPreview(value) {
  switch (typeof value) {
    case "function":
      return "function ()";

    case "object":
      if (value === null) {
        return null;
      }

      if (Array.isArray(value)) {
        return `(${value.length})[...]`;
      }

      return "{...}";

    case "undefined":
      return "undefined";

    default:
      return value;
  }
}

function getArrayPreview(arr) {
  const preview = [];
  let count = 0;
  for (const value of arr) {
    if (++count > PREVIEW_MAX_ITEMS) {
      break;
    }
    preview.push(getPreview(value));
  }

  return preview;
}

function getObjectPreview(obj) {
  const preview = {};
  let count = 0;
  for (const key of Object.keys(obj)) {
    if (++count > PREVIEW_MAX_ITEMS) {
      break;
    }
    preview[key] = getPreview(obj[key]);
  }

  return preview;
}

function getArgs(value) {
  if (typeof value === "object" && value !== null) {
    if (Array.isArray(value)) {
      return getArrayPreview(value);
    }

    return getObjectPreview(value);
  }

  return getPreview(value);
}

class ReportSiteIssueHelperChild extends JSWindowActorChild {
  _getConsoleMessages(windowId) {
    const ConsoleAPIStorage = Cc[
      "@mozilla.org/consoleAPI-storage;1"
    ].getService(Ci.nsIConsoleAPIStorage);
    let messages = ConsoleAPIStorage.getEvents(windowId);
    return messages.map(evt => {
      const { columnNumber, filename, level, lineNumber, timeStamp } = evt;
      const args = evt.arguments.map(getArgs);

      const message = {
        level,
        log: args,
        uri: filename,
        pos: `${lineNumber}:${columnNumber}`,
      };

      return { timeStamp, message };
    });
  }

  _getScriptErrors(windowId, includePrivate) {
    const messages = Services.console.getMessageArray();
    return messages
      .filter(message => {
        if (message instanceof Ci.nsIScriptError) {
          if (!includePrivate && message.isFromPrivateWindow) {
            return false;
          }

          if (windowId && windowId !== message.innerWindowID) {
            return false;
          }

          return true;
        }

        // If this is not an nsIScriptError and we need to do window-based
        // filtering we skip this message.
        return false;
      })
      .map(error => {
        const {
          timeStamp,
          errorMessage,
          sourceName,
          lineNumber,
          columnNumber,
          logLevel,
        } = error;
        const message = {
          level: LOG_LEVELS[logLevel],
          log: [errorMessage],
          uri: sourceName,
          pos: `${lineNumber}:${columnNumber}`,
        };
        return { timeStamp, message };
      });
  }

  _getLoggedMessages(includePrivate = false) {
    const windowId = this.contentWindow.windowGlobalChild.innerWindowId;
    return this._getConsoleMessages(windowId).concat(
      this._getScriptErrors(windowId, includePrivate)
    );
  }

  receiveMessage(msg) {
    switch (msg.name) {
      case "GetLog":
        return this._getLoggedMessages();
      case "GetBlockingStatus":
        const { docShell } = this;
        return {
          hasTrackingContentBlocked: docShell.hasTrackingContentBlocked,
        };
    }
    return null;
  }
}
