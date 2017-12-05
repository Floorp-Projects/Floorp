/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {results: Cr, utils: Cu} = Components;

Cu.import("resource://gre/modules/Services.jsm");

this.EXPORTED_SYMBOLS = ["frame"];

/** @namespace */
this.frame = {};

/**
 * The FrameManager will maintain the list of Out Of Process (OOP)
 * frames and will handle frame switching between them.
 *
 * @param {GeckoDriver} driver
 *     Reference to the driver instance.
 */
frame.Manager = class {
  constructor(driver) {
    this.driver = driver;
  }

  /**
   * Adds message listeners to the driver,  listening for
   * messages from content frame scripts.
   *
   * @param {nsIMessageListenerManager} mm
   *     The message manager object, typically
   *     ChromeMessageBroadcaster or ChromeMessageSender.
   */
  addMessageManagerListeners(mm) {
    mm.addWeakMessageListener("Marionette:switchedToFrame", this.driver);
    mm.addWeakMessageListener("Marionette:getVisibleCookies", this.driver);
    mm.addWeakMessageListener("Marionette:register", this.driver);
    mm.addWeakMessageListener("Marionette:listenersAttached", this.driver);
    mm.addWeakMessageListener("Marionette:GetLogLevel", this.driver);
  }

  /**
   * Removes listeners for messages from content frame scripts.
   *
   * @param {nsIMessageListenerManager} mm
   *     The message manager object, typically
   *     ChromeMessageBroadcaster or ChromeMessageSender.
   */
  removeMessageManagerListeners(mm) {
    mm.removeWeakMessageListener("Marionette:switchedToFrame", this.driver);
    mm.removeWeakMessageListener("Marionette:getVisibleCookies", this.driver);
    mm.removeWeakMessageListener("Marionette:GetLogLevel", this.driver);
    mm.removeWeakMessageListener("Marionette:listenersAttached", this.driver);
    mm.removeWeakMessageListener("Marionette:register", this.driver);
  }
};
