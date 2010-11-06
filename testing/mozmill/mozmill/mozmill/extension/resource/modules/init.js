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
 * The Original Code is Mozilla Corporation Code.
 * 
 * The Initial Developer of the Original Code is
 * Adam Christian.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 * 
 * Contributor(s):
 *  Adam Christian <adam.christian@gmail.com>
 *  Mikeal Rogers <mikeal.rogers@gmail.com>
 *  Henrik Skupin <hskupin@mozilla.com>
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

var EXPORTED_SYMBOLS = ["mozmill"];
  
const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
  
var mozmill = Cu.import('resource://mozmill/modules/mozmill.js');
  
// Observer for new top level windows
var windowObserver = {
  observe: function(subject, topic, data) {
    attachEventListeners(subject);
  }
};
  
/**
 * Attach event listeners
 */
function attachEventListeners(window) {
  window.addEventListener("load", function(event) {
    window.documentLoaded = true;
 
    if (window.gBrowser) {
      // Page is ready
      window.gBrowser.addEventListener("load", function(event) {
        event.target.documentLoaded = true;
      }, true);
 
      // Note: Error pages will never fire a "load" event. For those we
      // have to wait for the "DOMContentLoaded" event. That's the final state.
      // Error pages will always have a baseURI starting with
      // "about:" followed by "error" or "blocked".
      window.gBrowser.addEventListener("DOMContentLoaded", function(event) {
        var errorRegex = /about:.+(error)|(blocked)\?/;
        if (errorRegex.exec(event.target.baseURI)) {
          // Wait about 1s to be sure the DOM is ready
          mozmill.utils.sleep(1000);
          event.target.documentLoaded = true;
        }
      }, true);
  
      // Page is about to get unloaded
      window.gBrowser.addEventListener("beforeunload", function(event) {
        event.target.documentLoaded = false;
      }, true);
    }
  }, false);
}
  
/**
 * Initialize Mozmill
 */
function initialize() {
  // Activate observer for new top level windows
  var observerService = Cc["@mozilla.org/observer-service;1"].
                        getService(Ci.nsIObserverService);
  observerService.addObserver(windowObserver, "toplevel-window-ready", false);

  // Attach event listeners to all open windows
  var enumerator = Cc["@mozilla.org/appshell/window-mediator;1"].
                   getService(Ci.nsIWindowMediator).getEnumerator("");
  while (enumerator.hasMoreElements()) {
    var win = enumerator.getNext();
    attachEventListeners(win);

    // For windows or dialogs already open we have to explicitly set the property
    // otherwise windows which load really quick never gets the property set and
    // we fail to create the controller
    win.documentLoaded = true;
  };
}

initialize();

