/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict"

let Cc = Components.classes;
let Ci = Components.interfaces;

Components.utils.import("resource://gre/modules/Services.jsm");

this.EXPORTED_SYMBOLS = ["Prompt"];

function log(msg) {
  //Services.console.logStringMessage(msg);
}

function Prompt(aOptions) {
  this.window = "window" in aOptions ? aOptions.window : null;
  this.msg = { type: "Prompt:Show", async: true };

  if ("title" in aOptions)
    this.msg.title = aOptions.title;

  if ("message" in aOptions)
    this.msg.text = aOptions.message;

  if ("buttons" in aOptions)
    this.msg.buttons = aOptions.buttons;

  let idService = Cc["@mozilla.org/uuid-generator;1"].getService(Ci.nsIUUIDGenerator); 
  this.guid = idService.generateUUID().toString();
  this.msg.guid = this.guid;
}

Prompt.prototype = {
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

  addPassword: function(aOptions) {
    return this._addInput({
      type: "password",
      value: aOptions.value,
      hint: aOptions.hint,
      autofocus: aOptions.autofocus,
      id : aOptions.id
    });
  },

  addDatePicker: function(aOptions) {
    return this._addInput({
      type: aOptions.type || "date",
      value: aOptions.value,
    });
  },

  addMenulist: function(aOptions) {
    return this._addInput({
      type: "menulist",
      values: aOptions.values,
      id: aOptions.id
    });
  },

  show: function(callback) {
    this.callback = callback;
    log("Sending message");
    Services.obs.addObserver(this, "Prompt:Reply", false);
    this._innerShow();
  },

  _innerShow: function() {
    this.bridge.handleGeckoMessage(JSON.stringify(this.msg));
  },

  observe: function(aSubject, aTopic, aData) {
    log("observe " + aData);
    let data = JSON.parse(aData);
    if (data.guid != this.guid)
      return;

    Services.obs.removeObserver(this, "Prompt:Reply", false);

    if (this.callback)
      this.callback(data);
  },

  _setListItems: function(aItems) {
    let hasSelected = false;
    this.msg.listitems = [];

    aItems.forEach(function(item) {
      let obj = { id: item.id };

      obj.label = item.label;

      if (item.disabled)
        obj.disabled = true;

      if (item.selected || hasSelected || this.msg.multiple) {
        if (!this.msg.selected) {
          this.msg.selected = new Array(this.msg.listitems.length);
          hasSelected = true;
        }
        this.msg.selected[this.msg.listitems.length] = item.selected;
      }

      if (item.header)
        obj.isGroup = true;

      if (item.menu)
        obj.isParent = true;

      if (item.child)
        obj.inGroup = true;

      this.msg.listitems.push(obj);

    }, this);
    return this;
  },

  setSingleChoiceItems: function(aItems) {
    return this._setListItems(aItems);
  },

  setMultiChoiceItems: function(aItems) {
    this.msg.multiple = true;
    return this._setListItems(aItems);
  },

  get bridge() {
    return Cc["@mozilla.org/android/bridge;1"].getService(Ci.nsIAndroidBridge);
  },

}
