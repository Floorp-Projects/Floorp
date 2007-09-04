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
 * The Original Code is the Mozilla browser.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Shawn Wilsher <me@shawnwilsher.com>
 *   Myk Melez <myk@mozilla.org>
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

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

////////////////////////////////////////////////////////////////////////////////
//// Constants

const Ci = Components.interfaces;
const Cr = Components.results;

////////////////////////////////////////////////////////////////////////////////
//// nsWebHandler class

function nsWebHandlerApp() {}

nsWebHandlerApp.prototype = {
  //////////////////////////////////////////////////////////////////////////////
  //// nsWebHandler

  classDescription: "A web handler for protocols and content",
  classID: Components.ID("8b1ae382-51a9-4972-b930-56977a57919d"),
  contractID: "@mozilla.org/uriloader/web-handler-app;1",

  _name: null,
  _uriTemplate: null,

  //////////////////////////////////////////////////////////////////////////////
  //// nsIHandlerApp

  get name() {
    return this._name;
  },

  set name(aName) {
    this._name = aName;
  },

  equals: function(aHandlerApp) {
    if (!aHandlerApp)
      throw Cr.NS_ERROR_NULL_POINTER;

    if (aHandlerApp instanceof Ci.nsIWebHandlerApp &&
        aHandlerApp.uriTemplate &&
        this.uriTemplate &&
        aHandlerApp.uriTemplate == this.uriTemplate)
      return true;

    return false;
  },

  //////////////////////////////////////////////////////////////////////////////
  //// nsIWebHandlerApp

  get uriTemplate() {
    return this._uriTemplate;
  },

  set uriTemplate(aURITemplate) {
    this._uriTemplate = aURITemplate;
  },

  //////////////////////////////////////////////////////////////////////////////
  //// nsISupports

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIWebHandlerApp, Ci.nsIHandlerApp])
};

////////////////////////////////////////////////////////////////////////////////
//// Module

let components = [nsWebHandlerApp];

function NSGetModule(compMgr, fileSpec)
{
  return XPCOMUtils.generateModule(components);
}

