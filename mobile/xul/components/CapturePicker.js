/* -*- Mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil; -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is CapturePicker.js
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Kyle Huey <me@kylehuey.com>
 *   Fabrice Desré <fabrice@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
