/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import { GeckoViewUtils } from "resource://gre/modules/GeckoViewUtils.sys.mjs";

const { debug, warn } = GeckoViewUtils.initLogging("Console");

export var GeckoViewConsole = {
  _isEnabled: false,

  get enabled() {
    return this._isEnabled;
  },

  set enabled(aVal) {
    debug`enabled = ${aVal}`;
    if (!!aVal === this._isEnabled) {
      return;
    }

    this._isEnabled = !!aVal;
    const ConsoleAPIStorage = Cc[
      "@mozilla.org/consoleAPI-storage;1"
    ].getService(Ci.nsIConsoleAPIStorage);
    if (this._isEnabled) {
      this._consoleMessageListener = this._handleConsoleMessage.bind(this);
      ConsoleAPIStorage.addLogEventListener(
        this._consoleMessageListener,
        Cc["@mozilla.org/systemprincipal;1"].createInstance(Ci.nsIPrincipal)
      );
    } else if (this._consoleMessageListener) {
      ConsoleAPIStorage.removeLogEventListener(this._consoleMessageListener);
      delete this._consoleMessageListener;
    }
  },

  observe(aSubject, aTopic, aData) {
    if (aTopic == "nsPref:changed") {
      this.enabled = Services.prefs.getBoolPref(aData, false);
    }
  },

  _handleConsoleMessage(aMessage) {
    aMessage = aMessage.wrappedJSObject;

    const mappedArguments = Array.from(
      aMessage.arguments,
      this.formatResult,
      this
    );
    const joinedArguments = mappedArguments.join(" ");

    if (aMessage.level == "error" || aMessage.level == "warn") {
      const flag =
        aMessage.level == "error"
          ? Ci.nsIScriptError.errorFlag
          : Ci.nsIScriptError.warningFlag;
      const consoleMsg = Cc["@mozilla.org/scripterror;1"].createInstance(
        Ci.nsIScriptError
      );
      consoleMsg.init(
        joinedArguments,
        null,
        null,
        0,
        0,
        flag,
        "content javascript"
      );
      Services.console.logMessage(consoleMsg);
    } else if (aMessage.level == "trace") {
      const bundle = Services.strings.createBundle(
        "chrome://browser/locale/browser.properties"
      );
      const args = aMessage.arguments;
      const msgDetails = args[0] ?? aMessage;
      const filename = this.abbreviateSourceURL(msgDetails.filename);
      const functionName =
        msgDetails.functionName ||
        bundle.GetStringFromName("stacktrace.anonymousFunction");
      const lineNumber = msgDetails.lineNumber;

      let body = bundle.formatStringFromName("stacktrace.outputMessage", [
        filename,
        functionName,
        lineNumber,
      ]);
      body += "\n";
      args.forEach(function (aFrame) {
        const functionName =
          aFrame.functionName ||
          bundle.GetStringFromName("stacktrace.anonymousFunction");
        body +=
          "  " +
          aFrame.filename +
          " :: " +
          functionName +
          " :: " +
          aFrame.lineNumber +
          "\n";
      });

      Services.console.logStringMessage(body);
    } else if (aMessage.level == "time" && aMessage.arguments) {
      const bundle = Services.strings.createBundle(
        "chrome://browser/locale/browser.properties"
      );
      const body = bundle.formatStringFromName("timer.start", [
        aMessage.arguments.name,
      ]);
      Services.console.logStringMessage(body);
    } else if (aMessage.level == "timeEnd" && aMessage.arguments) {
      const bundle = Services.strings.createBundle(
        "chrome://browser/locale/browser.properties"
      );
      const body = bundle.formatStringFromName("timer.end", [
        aMessage.arguments.name,
        aMessage.arguments.duration,
      ]);
      Services.console.logStringMessage(body);
    } else if (
      ["group", "groupCollapsed", "groupEnd"].includes(aMessage.level)
    ) {
      // Do nothing yet
    } else {
      Services.console.logStringMessage(joinedArguments);
    }
  },

  getResultType(aResult) {
    let type = aResult === null ? "null" : typeof aResult;
    if (type == "object" && aResult.constructor && aResult.constructor.name) {
      type = aResult.constructor.name;
    }
    return type.toLowerCase();
  },

  formatResult(aResult) {
    let output = "";
    const type = this.getResultType(aResult);
    switch (type) {
      case "string":
      case "boolean":
      case "date":
      case "error":
      case "number":
      case "regexp":
        output = aResult.toString();
        break;
      case "null":
      case "undefined":
        output = type;
        break;
      default:
        output = aResult.toString();
        break;
    }

    return output;
  },

  abbreviateSourceURL(aSourceURL) {
    // Remove any query parameters.
    const hookIndex = aSourceURL.indexOf("?");
    if (hookIndex > -1) {
      aSourceURL = aSourceURL.substring(0, hookIndex);
    }

    // Remove a trailing "/".
    if (aSourceURL[aSourceURL.length - 1] == "/") {
      aSourceURL = aSourceURL.substring(0, aSourceURL.length - 1);
    }

    // Remove all but the last path component.
    const slashIndex = aSourceURL.lastIndexOf("/");
    if (slashIndex > -1) {
      aSourceURL = aSourceURL.substring(slashIndex + 1);
    }

    return aSourceURL;
  },
};
