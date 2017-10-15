/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  EventDispatcher: "resource://gre/modules/Messaging.jsm",
  GeckoViewUtils: "resource://gre/modules/GeckoViewUtils.jsm",
  Services: "resource://gre/modules/Services.jsm",
});

this.EXPORTED_SYMBOLS = ["Prompt", "DoorHanger"];

function log(msg) {
  Services.console.logStringMessage(msg);
}

function getRootWindow(win) {
  // Get the root xul window.
  return win.QueryInterface(Ci.nsIInterfaceRequestor)
            .getInterface(Ci.nsIDocShell).QueryInterface(Ci.nsIDocShellTreeItem)
            .rootTreeItem.QueryInterface(Ci.nsIInterfaceRequestor)
            .getInterface(Ci.nsIDOMWindow);
}

function Prompt(aOptions) {
  this.window = "window" in aOptions ? aOptions.window : null;

  this.msg = { async: true };

  if (this.window) {
    let window = getRootWindow(this.window);
    var tab = window &&
              window.BrowserApp &&
              window.BrowserApp.getTabForWindow(this.window);
    if (tab) {
      this.msg.tabId = tab.id;
    }
  }

  if (aOptions.priority === 1)
    this.msg.type = "Prompt:ShowTop";
  else
    this.msg.type = "Prompt:Show";

  if ("title" in aOptions && aOptions.title != null)
    this.msg.title = aOptions.title;

  if ("message" in aOptions && aOptions.message != null)
    this.msg.text = aOptions.message;

  if ("buttons" in aOptions && aOptions.buttons != null)
    this.msg.buttons = aOptions.buttons;

  if ("doubleTapButton" in aOptions && aOptions.doubleTapButton != null)
    this.msg.doubleTapButton = aOptions.doubleTapButton;

  if ("hint" in aOptions && aOptions.hint != null)
    this.msg.hint = aOptions.hint;
}

Prompt.prototype = {
  setHint: function(aHint) {
    if (!aHint)
      delete this.msg.hint;
    else
      this.msg.hint = aHint;
    return this;
  },

  addButton: function(aOptions) {
    if (!this.msg.buttons)
      this.msg.buttons = [];
    this.msg.buttons.push(aOptions.label);
    return this;
  },

  _addInput: function(aOptions) {
    let obj = aOptions;
    if (this[aOptions.type + "_count"] === undefined)
      this[aOptions.type + "_count"] = 0;

    obj.id = aOptions.id || (aOptions.type + this[aOptions.type + "_count"]);
    this[aOptions.type + "_count"]++;

    if (!this.msg.inputs)
      this.msg.inputs = [];
    this.msg.inputs.push(obj);
    return this;
  },

  addCheckbox: function(aOptions) {
    return this._addInput({
      type: "checkbox",
      label: aOptions.label,
      checked: aOptions.checked,
      id: aOptions.id
    });
  },

  addTextbox: function(aOptions) {
    return this._addInput({
      type: "textbox",
      value: aOptions.value,
      hint: aOptions.hint,
      autofocus: aOptions.autofocus,
      id: aOptions.id
    });
  },

  addNumber: function(aOptions) {
    return this._addInput({
      type: "number",
      value: aOptions.value,
      hint: aOptions.hint,
      autofocus: aOptions.autofocus,
      id: aOptions.id
    });
  },

  addPassword: function(aOptions) {
    return this._addInput({
      type: "password",
      value: aOptions.value,
      hint: aOptions.hint,
      autofocus: aOptions.autofocus,
      id: aOptions.id
    });
  },

  addDatePicker: function(aOptions) {
    return this._addInput({
      type: aOptions.type || "date",
      value: aOptions.value,
      id: aOptions.id,
      max: aOptions.max,
      min: aOptions.min
    });
  },

  addColorPicker: function(aOptions) {
    return this._addInput({
      type: "color",
      value: aOptions.value,
      id: aOptions.id
    });
  },

  addLabel: function(aOptions) {
    return this._addInput({
      type: "label",
      label: aOptions.label,
      id: aOptions.id
    });
  },

  addMenulist: function(aOptions) {
    return this._addInput({
      type: "menulist",
      values: aOptions.values,
      id: aOptions.id
    });
  },

  addIconGrid: function(aOptions) {
    return this._addInput({
      type: "icongrid",
      items: aOptions.items,
      id: aOptions.id
    });
  },

  addTabs: function(aOptions) {
    return this._addInput({
      type: "tabs",
      items: aOptions.items,
      id: aOptions.id
    });
  },

  show: function(callback) {
    this.callback = callback;
    log("Sending message");
    this._innerShow();
  },

  _innerShow: function() {
    let dispatcher = GeckoViewUtils.getDispatcherForWindow(this.window) ||
                     GeckoViewUtils.getActiveDispatcher();
    dispatcher.sendRequestForResult(this.msg).then((data) => {
      if (this.callback) {
        this.callback(data);
      }
    });
  },

  _setListItems: function(aItems) {
    let hasSelected = false;
    this.msg.listitems = [];

    aItems.forEach(function(item) {
      let obj = { id: item.id };

      obj.label = item.label;

      if (item.disabled)
        obj.disabled = true;

      if (item.selected) {
        if (!this.msg.choiceMode) {
          this.msg.choiceMode = "single";
        }
        obj.selected = item.selected;
      }

      if (item.header)
        obj.isGroup = true;

      if (item.menu)
        obj.isParent = true;

      if (item.child)
        obj.inGroup = true;

      if (item.showAsActions)
        obj.showAsActions = item.showAsActions;

      if (item.icon)
        obj.icon = item.icon;

      this.msg.listitems.push(obj);

    }, this);
    return this;
  },

  setSingleChoiceItems: function(aItems) {
    return this._setListItems(aItems);
  },

  setMultiChoiceItems: function(aItems) {
    this.msg.choiceMode = "multiple";
    return this._setListItems(aItems);
  },

};

var DoorHanger = {
  _getTabId: function(aWindow, aBrowserApp) {
      let tab = aBrowserApp.getTabForWindow(aWindow.top) ||
                aBrowserApp.selectedTab;
      return tab ? tab.id : -1;
  },

  show: function(aWindow, aMessage, aValue, aButtons, aOptions, aCategory) {
    let chromeWin = getRootWindow(aWindow);
    if (chromeWin.NativeWindow && chromeWin.NativeWindow.doorhanger) {
      // We're dealing with browser.js.
      return chromeWin.NativeWindow.doorhanger.show(
          aMessage, aValue, aButtons, this._getTabId(aWindow, chromeWin.BrowserApp),
          aOptions, aCategory);
    }

    // We're dealing with GeckoView (e.g. custom tabs).
    aButtons = aButtons || [];

    // Extract callbacks into a separate array, and replace each callback in
    // the buttons array with an index into the callback array.
    let callbacks = aButtons.map((aButton, aIndex) => {
      let cb = aButton.callback;
      aButton.callback = aIndex;
      return cb;
    });

    EventDispatcher.for(chromeWin).sendRequestForResult({
      type: "Doorhanger:Add",
      message: aMessage,
      value: aValue,
      buttons: aButtons,
      options: aOptions || {},
      category: aCategory,
    }).then(response => {
      // Pass the value of the optional checkbox to the callback
      callbacks[response.callback](response.checked, response.inputs);
    });
  },

  hide: function(aWindow, aValue) {
    let chromeWin = getRootWindow(aWindow);
    if (chromeWin.NativeWindow && chromeWin.NativeWindow.doorhanger) {
      // We're dealing with browser.js.
      return chromeWin.NativeWindow.doorhanger.hide(
          aValue, this._getTabId(aWindow, chromeWin.BrowserApp));
    }

    EventDispatcher.for(chromeWin).sendRequest({
      type: "Doorhanger:Remove",
      value: aValue,
    });
  },
};
