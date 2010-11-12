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

/**
 * Special Powers Exception - used to throw exceptions nicely
 **/
function SpecialPowersException(aMsg) {
  this.message = aMsg;
  this.name = "SpecialPowersException";
}

SpecialPowersException.prototype.toString = function() {
  return this.name + ': "' + this.message + '"';
};

/* XPCOM gunk */
function SpecialPowersObserver() {}

SpecialPowersObserver.prototype = {
  classDescription: "Special powers Observer for use in testing.",
  classID:          Components.ID("{59a52458-13e0-4d93-9d85-a637344f29a1}"),
  contractID:       "@mozilla.org/special-powers-observer;1",
  QueryInterface:   XPCOMUtils.generateQI([Components.interfaces.nsIObserver]),
  _xpcom_categories: [{category: "profile-after-change", service: true }],

  observe: function(aSubject, aTopic, aData)
  {
    if (aTopic == "profile-after-change") {
      this.init();
    } else if (aTopic == "content-document-global-created") {
      var w = aSubject.wrappedJSObject;

      if (w) {
        var messageManager = Cc["@mozilla.org/globalmessagemanager;1"].
                             getService(Ci.nsIChromeFrameMessageManager);
        // Register for any messages our API needs us to handle
        messageManager.addMessageListener("SPPrefService", this);

        // Load the frame script into the window
        messageManager.loadFrameScript(CHILD_SCRIPT, true);
        //dump("\n\n TESTME!! Loaded SPPrefService as well as frame script now\n\n");
      } else {
        dump("TEST-INFO | specialpowers | Can't attach special powers to window " + aSubject + "\n");
      }
    } else if (aTopic == "xpcom-shutdown") {
      this.uninit();
    }
  },

  init: function()
  {
    var obs = Services.obs;
    obs.addObserver(this, "xpcom-shutdown", false);
    obs.addObserver(this, "content-document-global-created", false);
  },

  uninit: function()
  {
    var obs = Services.obs;
    obs.removeObserver(this, "content-document-global-created"); 
  },
  
  /**
   * messageManager callback function
   * This will get requests from our API in the window and process them in chrome for it
   **/
  receiveMessage: function(aMessage) {
    switch(aMessage.name) {
      case "SPPrefService":
        var prefs = Services.prefs;
        var prefType = aMessage.json.prefType.toUpperCase();
        var prefName = aMessage.json.prefName;
        var prefValue = aMessage.json.prefValue ? aMessage.json.prefValue : null;

        if (aMessage.json.op == "get") {
          if (!prefName || !prefType)
            throw new SpecialPowersException("Invalid parameters for get in SPPrefService");
        } else if (aMessage.json.op == "set") {
          if (!prefName || !prefType  || !prefValue)
            throw new SpecialPowersException("Invalid parameters for set in SPPrefService");
        } else {
          throw new SpecialPowersException("Invalid operation for SPPrefService");
        }
        // Now we make the call
        switch(prefType) {
          case "BOOL":
            if (aMessage.json.op == "get")
              return(prefs.getBoolPref(prefName));
            else 
              return(prefs.setBoolPref(prefName, prefValue));
          case "INT":
            if (aMessage.json.op == "get") 
              return(prefs.getIntPref(prefName));
            else
              return(prefs.setIntPref(prefName, prefValue));
          case "CHAR":
            if (aMessage.json.op == "get")
              return(prefs.getCharPref(prefName));
            else
              return(prefs.setCharPref(prefName, prefValue));
          case "COMPLEX":
            if (aMessage.json.op == "get")
              return(prefs.getComplexValue(prefName, prefValue[0]));
            else
              return(prefs.setComplexValue(prefName, prefValue[0], prefValue[1]));
        }
        break;
      default:
        throw new SpecialPowersException("Unrecognized Special Powers API");
    }
  }
};

const NSGetFactory = XPCOMUtils.generateNSGetFactory([SpecialPowersObserver]);
