/* vim: set ts=2 sw=2 sts=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["ActorChild"];

/**
 * This should be the base class of any actor class registered via
 * ActorManagerParent and implemented in the child process. It currently takes
 * care of setting the `mm`, `content`, and `docShell` properties based on the
 * message manager it's bound to, but may do more in the future.
 *
 * If Fission is being simulated, and the actor is registered as "allFrames",
 * the `content` property of this class will be bound to a specific subframe.
 * Otherwise, the `content` is always the top-level content tied to the `mm`.
 */
this.ActorChild = class ActorChild {
  constructor(dispatcher) {
    this._dispatcher = dispatcher;
    this.mm = dispatcher.mm;
  }

  get content() {
    return this._dispatcher.window;
  }

  get docShell() {
    return this.mm.docShell;
  }

  addEventListener(event, options) {
    this._dispatcher.addEventListener(event, this.constructor.name, options);
  }

  addMessageListener(msg) {
    this._dispatcher.addMessageListener(msg, this.constructor.name);
  }

  sendAsyncMessage(msg, data = {}) {
    data.frameId = this._dispatcher.frameId;
    data.browsingContextId = this._dispatcher.browsingContextId;
    this.mm.sendAsyncMessage(msg, data);
  }
};
