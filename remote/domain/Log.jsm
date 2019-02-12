/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["Log"];

const {Domain} = ChromeUtils.import("chrome://remote/content/Domain.jsm");
const {Services} = ChromeUtils.import("resource://gre/modules/Services.jsm");
const {t} = ChromeUtils.import("chrome://remote/content/Protocol.jsm");

const {Network, Runtime} = Domain;

const ALLOWED_SOURCES = [
  "xml",
  "javascript",
  "network",
  "storage",
  "appcache",
  "rendering",
  "security",
  "deprecation",
  "worker",
  "violation",
  "intervention",
  "recommendation",
  "other",
];

const ALLOWED_LEVELS = [
  "verbose",
  "info",
  "warning",
  "error",
];

class Log extends Domain {
  constructor(session, target) {
    super(session, target);
    this.enabled = false;
  }

  destructor() {
    this.disable();
  }

  enable() {
    if (!this.enabled) {
      this.enabled = true;

      // TODO(ato): Using the target content browser's MM here does not work
      // because it disconnects when it suffers a host process change.
      // That forces us to listen for Everything
      // and do a target check in receiveMessage() below.
      // Perhaps we can solve reattaching message listeners in a ParentActor?
      Services.mm.addMessageListener("RemoteAgent:Log:OnConsoleAPIEvent", this);
      Services.mm.addMessageListener("RemoteAgent:Log:OnConsoleServiceMessage", this);
    }
  }

  disable() {
    if (this.enabled) {
      Services.mm.removeMessageListener("RemoteAgent:Log:OnConsoleAPIEvent", this);
      Services.mm.removeMessageListener("RemoteAgent:Log:OnConsoleServiceMessage", this);
      this.enabled = false;
    }
  }

  // nsIMessageListener

  receiveMessage({target, name, data}) {
    // filter out Console API events that do not belong
    // to the browsing context we are operating on
    if (name == "RemoteAgent:Log:OnConsoleAPIEvent" &&
        this.target.id !== data.browsingContextId) {
      return;
    }

    this.emit("Log.entryAdded", {entry: data});
  }

  static get schema() {
    return {
      methods: {
        enable: {},
        disable: {},
      },
      events: {
        entryAdded: Log.LogEntry.schema,
      },
    };
  }
};

Log.LogEntry = {
  schema: {
    source: t.Enum(ALLOWED_SOURCES),
    level: t.Enum(ALLOWED_LEVELS),
    text: t.String,
    timestamp: Runtime.Timestamp,
    url: t.Optional(t.String),
    lineNumber: t.Optional(t.Number),
    stackTrace: t.Optional(Runtime.StackTrace.schema),
    networkRequestId: t.Optional(Network.RequestId.schema),
    workerId: t.Optional(t.String),
    args: t.Optional(t.Array(Runtime.RemoteObject.schema)),
  },
};
