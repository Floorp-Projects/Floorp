/* -*- Mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

function CapturePicker() {
  this.messageManager = Cc["@mozilla.org/childprocessmessagemanager;1"].getService(Ci.nsISyncMessageSender);
}

CapturePicker.prototype = {
  _file: null,
  _mode: -1,
  _result: -1,
  _shown: false,
  _title: "",
  _type: "",
  _window: null,

  //
  // nsICapturePicker
  //
  init: function(aWindow, aTitle, aMode) {
    this._window = aWindow;
    this._title = aTitle;
    this._mode = aMode;
  },

  show: function() {
    if (this._shown)
      throw Cr.NS_ERROR_UNEXPECTED;

    this._shown = true;

    let res = this.messageManager.sendSyncMessage("CapturePicker:Show", { title: this._title, mode: this._mode, type: this._type })[0];
    if (res.value)
      this._file = res.path;

    return (res.value ? Ci.nsICapturePicker.RETURN_OK : Ci.nsICapturePicker.RETURN_CANCEL);
  },

  modeMayBeAvailable: function(aMode) {
    if (aMode != Ci.nsICapturePicker.MODE_STILL)
      return false;
    return true;
  },

  get file() {
    if (this._file) { 
      let file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsILocalFile);
      file.initWithPath(this._file);
      let utils = this._window.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils);
      return utils.wrapDOMFile(file);
    } else {
      throw Cr.NS_ERROR_FAILURE;
    }
  },

  get type() {
    return this._type;
  },

  set type(aNewType) {
    if (this._shown)
      throw Cr.NS_ERROR_UNEXPECTED;
    else 
      this._type = aNewType;
  },

  // QI
  QueryInterface: XPCOMUtils.generateQI([Ci.nsICapturePicker]),

  // XPCOMUtils factory
  classID: Components.ID("{cb5a47f0-b58c-4fc3-b61a-358ee95f8238}"),
};

var components = [ CapturePicker ];
const NSGetFactory = XPCOMUtils.generateNSGetFactory(components);
