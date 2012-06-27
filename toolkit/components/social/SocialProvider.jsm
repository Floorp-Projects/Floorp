/* -*- Mode: JavaScript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/FrameWorker.jsm");

const EXPORTED_SYMBOLS = ["SocialProvider"];

/**
 * The SocialProvider object represents a social provider, and allows
 * controlling its FrameWorker.
 *
 * @constructor
 * @param {jsobj} object representing the manifest file describing this provider
 */
function SocialProvider(input) {
  if (!input.name)
    throw new Error("SocialProvider must be passed a name");
  if (!input.workerURL)
    throw new Error("SocialProvider must be passed a workerURL");
  if (!input.origin)
    throw new Error("SocialProvider must be passed an origin");

  this.name = input.name;
  this.workerURL = input.workerURL;
  this.origin = input.origin;
}

SocialProvider.prototype = {
  /**
   * Terminate's the provider's FrameWorker, closing all of its ports.
   */
  terminate: function shutdown() {
    try {
      getFrameWorkerHandle(this.workerURL, null).terminate();
    } catch (e) {
      Cu.reportError("SocialProvider termination failed: " + e);
    }
  },

  /**
   * Instantiates a FrameWorker for the provider if one doesn't exist, and
   * returns a reference to a port to that FrameWorker.
   *
   * @param {DOMWindow} window (optional)
   */
  getWorkerPort: function getWorkerPort(window) {
    return getFrameWorkerHandle(this.workerURL, window).port;
  }
}
