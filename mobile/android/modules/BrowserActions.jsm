/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Messaging.jsm");

this.EXPORTED_SYMBOLS = ["BrowserActions"];

var BrowserActions = {
  _browserActions: {},

  _initialized: false,

  /**
   * Registers the listeners only if they have not been initialized
   * already and there is at least one browser action.
   */
  _maybeRegisterListeners() {
    if (!this._initialized && Object.keys(this._browserActions).length) {
      this._initialized = true;
      EventDispatcher.instance.registerListener(this, "Menu:Clicked");
    }
  },

  /**
   * Unregisters the listeners if they are already initizliaed and
   * all of the browser actions have been removed.
   */
  _maybeUnregisterListeners: function() {
    if (this._initialized && !Object.keys(this._browserActions).length) {
      this._initialized = false;
      EventDispatcher.instance.unregisterListener(this, "Menu:Clicked");
    }
  },

  /**
   * Called when a browser action is clicked on.
   * @param {string} event The name of the event, which should always
   *    be "Menu:Clicked".
   * @param {Object} data An object containing information about the
   *    browser action, which in this case should contain an `item`
   *    property which is browser action's ID.
   */
  onEvent(event, data) {
    if (event !== "Menu:Clicked") {
      throw new Error(`Expected "Menu:Clicked" event - received "${event}" instead`);
    }

    let browserAction = this._browserActions[data.item];
    if (!browserAction) {
      throw new Error(`No browser action found with id ${data.item}`);
    }
    browserAction.onClicked();
  },

  /**
   * Registers a new browser action.
   * @param {Object} browserAction The browser action to add.
   */
  register(browserAction) {
    EventDispatcher.instance.sendRequest({
      type: "Menu:Add",
      id: browserAction.id,
      name: browserAction.name,
    });

    this._browserActions[browserAction.id] = browserAction;
    this._maybeRegisterListeners();
  },

  /**
   * Checks to see if the browser action is shown. Used for testing only.
   * @param {string} id The ID of the browser action.
   * @returns True if the browser action is shown; false otherwise.
   */
  isShown: function(id) {
    return !!this._browserActions[id];
  },

  /**
   * Synthesizes a click on the browser action. Used for testing only.
   * @param {string} id The ID of the browser action.
   */
  synthesizeClick: function(id) {
    let browserAction = this._browserActions[id];
    if (!browserAction) {
      throw new Error(`No browser action found with id ${id}`);
    }
    browserAction.onClicked();
  },

  /**
   * Unregisters the browser action with the specified ID.
   * @param {string} id The browser action ID.
   */
  unregister(id) {
    let browserAction = this._browserActions[id];
    if (!browserAction) {
      throw new Error(`No BrowserAction with ID ${id} was found`);
    }
    EventDispatcher.instance.sendRequest({
      type: "Menu:Remove",
      id,
    });
    delete this._browserActions[id];
    this._maybeUnregisterListeners();
  }
}