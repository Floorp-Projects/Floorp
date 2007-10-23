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
 *   Dan Mosedale <dmose@mozilla.org>
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
const Cc = Components.classes;

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

  launchWithURI: function nWHA__launchWithURI(aURI, aWindowContext) {

    // XXX need to strip passwd & username from URI to handle, as per the
    // WhatWG HTML5 draft.  nsSimpleURL, which is what we're going to get,
    // can't do this directly.  Ideally, we'd fix nsStandardURL to make it
    // possible to turn off all of its quirks handling, and use that...

    // encode the URI to be handled
    var escapedUriSpecToHandle = encodeURIComponent(aURI.spec);

    // insert the encoded URI and create the object version 
    var uriSpecToSend = this.uriTemplate.replace("%s", escapedUriSpecToHandle);
    var ioService = Cc["@mozilla.org/network/io-service;1"].
                    getService(Ci.nsIIOService);
    var uriToSend = ioService.newURI(uriSpecToSend, null, null);
    
    // if we have a window context, use the URI loader to load there
    if (aWindowContext) {

      // create a channel from this URI
      var channel = ioService.newChannelFromURI(uriToSend);
      channel.loadFlags = Ci.nsIChannel.LOAD_DOCUMENT_URI;

      // load the channel
      var uriLoader = Cc["@mozilla.org/uriloader;1"].
                      getService(Ci.nsIURILoader);
      // XXX ideally, aIsContentPreferred (the second param) should really be
      // passed in from above.  Practically, true is probably a reasonable
      // default since browsers don't care much, and link click is likely to be
      // the more interesting case for non-browser apps.  See 
      // <https://bugzilla.mozilla.org/show_bug.cgi?id=392957#c9> for details.
      uriLoader.openURI(channel, true, aWindowContext);
      return;
    } 

    // since we don't have a window context, hand it off to a browser
    var windowMediator = Cc["@mozilla.org/appshell/window-mediator;1"].
      getService(Ci.nsIWindowMediator);

    // get browser dom window
    var browserDOMWin = windowMediator.getMostRecentWindow("navigator:browser")
                        .QueryInterface(Ci.nsIDOMChromeWindow)
                        .browserDOMWindow;

    // if we got an exception, there are several possible reasons why:
    // a) this gecko embedding doesn't provide an nsIBrowserDOMWindow
    //    implementation (i.e. doesn't support browser-style functionality),
    //    so we need to kick the URL out to the OS default browser.  This is
    //    the subject of bug 394479.
    // b) this embedding does provide an nsIBrowserDOMWindow impl, but
    //    there doesn't happen to be a browser window open at the moment; one
    //    should be opened.  It's not clear whether this situation will really
    //    ever occur in real life.  If it does, the only API that I can find
    //    that seems reasonably likely to work for most embedders is the
    //    command line handler.  
    // c) something else went wrong 
    //
    // it's not clear how one would differentiate between the three cases
    // above, so for now we don't catch the exception

    // openURI
    browserDOMWin.openURI(uriToSend,
                          null, // no window.opener 
                          Ci.nsIBrowserDOMWindow.OPEN_DEFAULT_WINDOW,
                          Ci.nsIBrowserDOMWindow.OPEN_NEW);
      
    return;
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

