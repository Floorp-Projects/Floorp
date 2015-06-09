/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { method } = require("devtools/server/protocol");

/**
 * Proxies a call from an actor to an underlying module, stored
 * as `bridge` on the actor. This allows a module to be defined in one
 * place, usable by other modules/actors on the server, but a separate
 * module defining the actor/RDP definition.
 *
 * @see Framerate implementation: toolkit/devtools/shared/framerate.js
 * @see Framerate actor definition: toolkit/devtools/server/actors/framerate.js
 */
exports.actorBridge = function actorBridge (methodName, definition={}) {
  return method(function () {
    return this.bridge[methodName].apply(this.bridge, arguments);
  }, definition);
}
