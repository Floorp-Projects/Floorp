/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var frame = {};   Components.utils.import('resource://mozmill/modules/frame.js', frame);

/**
* Console listener which listens for error messages in the console and forwards
* them to the Mozmill reporting system for output.
*/
function ConsoleListener() {
 this.register();
}
ConsoleListener.prototype = {
 observe: function(aMessage) {
   var msg = aMessage.message;
   var re = /^\[.*Error:.*(chrome|resource):\/\/.*/i;
   if (msg.match(re)) {
     frame.events.fail(aMessage);
   }
 },
 QueryInterface: function (iid) {
	if (!iid.equals(Components.interfaces.nsIConsoleListener) && !iid.equals(Components.interfaces.nsISupports)) {
		throw Components.results.NS_ERROR_NO_INTERFACE;
   }
   return this;
 },
 register: function() {
   var aConsoleService = Components.classes["@mozilla.org/consoleservice;1"]
                              .getService(Components.interfaces.nsIConsoleService);
   aConsoleService.registerListener(this);
 },
 unregister: function() {
   var aConsoleService = Components.classes["@mozilla.org/consoleservice;1"]
                              .getService(Components.interfaces.nsIConsoleService);
   aConsoleService.unregisterListener(this);
 }
}

// start listening
var consoleListener = new ConsoleListener();

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
  // These are the event handlers
  function pageShowHandler(event) {
    var doc = event.originalTarget;
    var tab = window.gBrowser.getBrowserForDocument(doc);

    if (tab) {
      //log("*** Loaded tab: location=" + doc.location + ", baseURI=" + doc.baseURI + "\n");
      tab.mozmillDocumentLoaded = true;
    } else {
      //log("*** Loaded HTML location=" + doc.location + ", baseURI=" + doc.baseURI + "\n");
      doc.defaultView.mozmillDocumentLoaded = true;
    }

    // We need to add/remove the unload/pagehide event listeners to preserve caching.
    window.gBrowser.addEventListener("beforeunload", beforeUnloadHandler, true);
    window.gBrowser.addEventListener("pagehide", pageHideHandler, true);
  };

  var DOMContentLoadedHandler = function(event) {
    var errorRegex = /about:.+(error)|(blocked)\?/;
    if (errorRegex.exec(event.target.baseURI)) {
      // Wait about 1s to be sure the DOM is ready
      mozmill.utils.sleep(1000);

      var tab = window.gBrowser.getBrowserForDocument(event.target);
      if (tab)
        tab.mozmillDocumentLoaded = true;

      // We need to add/remove the unload event listener to preserve caching.
      window.gBrowser.addEventListener("beforeunload", beforeUnloadHandler, true);
    }
  };

  // beforeunload is still needed because pagehide doesn't fire before the page is unloaded.
  // still use pagehide for cases when beforeunload doesn't get fired
  function beforeUnloadHandler(event) {
    var doc = event.originalTarget;
    var tab = window.gBrowser.getBrowserForDocument(event.target);

    if (tab) {
      //log("*** Unload tab: location=" + doc.location + ", baseURI=" + doc.baseURI + "\n");
      tab.mozmillDocumentLoaded = false;
    } else {
      //log("*** Unload HTML location=" + doc.location + ", baseURI=" + doc.baseURI + "\n");
      doc.defaultView.mozmillDocumentLoaded = false;
    }

    window.gBrowser.removeEventListener("beforeunload", beforeUnloadHandler, true);
  };

  var pageHideHandler = function(event) {
    // If event.persisted is false, the beforeUnloadHandler should fire
    // and there is no need for this event handler.
    if (event.persisted) {
      var doc = event.originalTarget;
      var tab = window.gBrowser.getBrowserForDocument(event.target);

      if (tab) {
        //log("*** Unload tab: location=" + doc.location + ", baseURI=" + doc.baseURI + "\n");
        tab.mozmillDocumentLoaded = false;
      } else {
        //log("*** Unload HTML location=" + doc.location + ", baseURI=" + doc.baseURI + "\n");
        doc.defaultView.mozmillDocumentLoaded = false;
      }

      window.gBrowser.removeEventListener("beforeunload", beforeUnloadHandler, true);
    }

  };

  // Add the event handlers to the tabbedbrowser once its window has loaded
  window.addEventListener("load", function(event) {
    window.mozmillDocumentLoaded = true;


    if (window.gBrowser) {
      // Page is ready
      window.gBrowser.addEventListener("pageshow", pageShowHandler, true);

      // Note: Error pages will never fire a "load" event. For those we
      // have to wait for the "DOMContentLoaded" event. That's the final state.
      // Error pages will always have a baseURI starting with
      // "about:" followed by "error" or "blocked".
      window.gBrowser.addEventListener("DOMContentLoaded", DOMContentLoadedHandler, true);

      // Leave page (use caching)
      window.gBrowser.addEventListener("pagehide", pageHideHandler, true);
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
    win.mozmillDocumentLoaded = true;
  };
}

initialize();

