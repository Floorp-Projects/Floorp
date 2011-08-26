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
 * The Original Code is Special Powers code
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jesse Ruderman <jruderman@mozilla.com>
 *   Robert Sayre <sayrer@gmail.com>
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
 * ***** END LICENSE BLOCK *****/

// Based on:
// https://bugzilla.mozilla.org/show_bug.cgi?id=549539
// https://bug549539.bugzilla.mozilla.org/attachment.cgi?id=429661
// https://developer.mozilla.org/en/XPCOM/XPCOM_changes_in_Gecko_1.9.3
// http://mxr.mozilla.org/mozilla-central/source/toolkit/components/console/hudservice/HUDService.jsm#3240
// https://developer.mozilla.org/en/how_to_build_an_xpcom_component_in_javascript

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");
Components.utils.import("resource://gre/modules/Services.jsm");

const Cc = Components.classes;
const Ci = Components.interfaces;

const CHILD_SCRIPT = "chrome://specialpowers/content/specialpowers.js"
const CHILD_SCRIPT_API = "chrome://specialpowers/content/specialpowersAPI.js"
const CHILD_LOGGER_SCRIPT = "chrome://specialpowers/content/MozillaLogger.js"


//glue to add in the observer API to this object.  This allows us to share code with chrome tests
var loader = Components.classes["@mozilla.org/moz/jssubscript-loader;1"]
                       .getService(Components.interfaces.mozIJSSubScriptLoader);
loader.loadSubScript("chrome://specialpowers/content/SpecialPowersObserverAPI.js");


/* XPCOM gunk */
function SpecialPowersObserver() {
  this._isFrameScriptLoaded = false;
  this._messageManager = Cc["@mozilla.org/globalmessagemanager;1"].
                         getService(Ci.nsIChromeFrameMessageManager);
}


SpecialPowersObserver.prototype = new SpecialPowersObserverAPI();

  SpecialPowersObserver.prototype.classDescription = "Special powers Observer for use in testing.";
  SpecialPowersObserver.prototype.classID = Components.ID("{59a52458-13e0-4d93-9d85-a637344f29a1}");
  SpecialPowersObserver.prototype.contractID = "@mozilla.org/special-powers-observer;1";
  SpecialPowersObserver.prototype.QueryInterface = XPCOMUtils.generateQI([Components.interfaces.nsIObserver]);
  SpecialPowersObserver.prototype._xpcom_categories = [{category: "profile-after-change", service: true }];

  SpecialPowersObserver.prototype.observe = function(aSubject, aTopic, aData)
  {
    switch (aTopic) {
      case "profile-after-change":
        this.init();
        break;

      case "chrome-document-global-created":
        if (!this._isFrameScriptLoaded) {
          // Register for any messages our API needs us to handle
          this._messageManager.addMessageListener("SPPrefService", this);
          this._messageManager.addMessageListener("SPProcessCrashService", this);
          this._messageManager.addMessageListener("SPPingService", this);

          this._messageManager.loadFrameScript(CHILD_LOGGER_SCRIPT, true);
          this._messageManager.loadFrameScript(CHILD_SCRIPT_API, true);
          this._messageManager.loadFrameScript(CHILD_SCRIPT, true);
          this._isFrameScriptLoaded = true;
        }
        break;

      case "xpcom-shutdown":
        this.uninit();
        break;

      default:
        this._observe(aSubject, aTopic, aData);
        break;
    }
  };

  SpecialPowersObserver.prototype._sendAsyncMessage = function(msgname, msg)
  {
    this._messageManager.sendAsyncMessage(msgname, msg);
  };

  SpecialPowersObserver.prototype._receiveMessage = function(aMessage) {
    return this._receiveMessageAPI(aMessage);
  };

  SpecialPowersObserver.prototype.init = function()
  {
    var obs = Services.obs;
    obs.addObserver(this, "xpcom-shutdown", false);
    obs.addObserver(this, "chrome-document-global-created", false);
  };

  SpecialPowersObserver.prototype.uninit = function()
  {
    var obs = Services.obs;
    obs.removeObserver(this, "chrome-document-global-created", false);
    this._removeProcessCrashObservers();
  };

  /**
   * messageManager callback function
   * This will get requests from our API in the window and process them in chrome for it
   **/
  SpecialPowersObserver.prototype.receiveMessage = function(aMessage) {
    switch(aMessage.name) {
      case "SPPingService":
        if (aMessage.json.op == "ping") {
          aMessage.target
                  .QueryInterface(Ci.nsIFrameLoaderOwner)
                  .frameLoader
                  .messageManager
                  .sendAsyncMessage("SPPingService", { op: "pong" });
        }
        break;
      default:
        return this._receiveMessage(aMessage);
    }
  };

const NSGetFactory = XPCOMUtils.generateNSGetFactory([SpecialPowersObserver]);
