/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Blake Ross <blakeross@telocity.com>
 *   Peter Annema <disttsc@bart.nl>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

const NS_ERROR_MODULE_NETWORK = 2152398848;
const NS_NET_STATUS_READ_FROM = NS_ERROR_MODULE_NETWORK + 8;
const NS_NET_STATUS_WROTE_TO  = NS_ERROR_MODULE_NETWORK + 9;


function nsBrowserStatusHandler()
{
  this.init();
}

nsBrowserStatusHandler.prototype =
{
  useRealProgressFlag : false,
  totalRequests : 0,
  finishedRequests : 0,

  // Stored Status, Link and Loading values
  status : "",
  defaultStatus : "",
  jsStatus : "",
  jsDefaultStatus : "",
  overLink : "",
  startTime : 0,

  statusTimeoutInEffect : false,

  hideAboutBlank : true,

  QueryInterface : function(aIID)
  {
    if (aIID.equals(Components.interfaces.nsIWebProgressListener) ||
        aIID.equals(Components.interfaces.nsISupportsWeakReference) ||
        aIID.equals(Components.interfaces.nsIXULBrowserWindow) ||
        aIID.equals(Components.interfaces.nsISupports))
      return this;
    throw Components.results.NS_NOINTERFACE;
  },

  init : function()
  {
    // XXXjag is this still needed? It's currently just ""
    this.defaultStatus = gNavigatorBundle.getString("defaultStatus");

    this.urlBar          = document.getElementById("urlbar");
    this.throbberElement = document.getElementById("navigator-throbber");
    this.statusMeter     = document.getElementById("statusbar-icon");
    this.stopButton      = document.getElementById("stop-button");
    this.stopMenu        = document.getElementById("menuitem-stop");
    this.stopContext     = document.getElementById("context-stop");
    this.statusTextField = document.getElementById("statusbar-display");

  },

  destroy : function()
  {
    // XXXjag to avoid leaks :-/, see bug 60729
    this.urlBar          = null;
    this.throbberElement = null;
    this.statusMeter     = null;
    this.stopButton      = null;
    this.stopMenu        = null;
    this.stopContext     = null;
    this.statusTextField = null;
  },

  setJSStatus : function(status)
  {
    this.jsStatus = status;
    this.updateStatusField();
    // set empty so defaults show up next change
    this.jsStatus = "";
  },

  setJSDefaultStatus : function(status)
  {
    this.jsDefaultStatus = status;
    this.updateStatusField();
  },

  setDefaultStatus : function(status)
  {
    this.defaultStatus = status;
    this.updateStatusField();
  },

  setOverLink : function(link, b)
  {
    this.overLink = link;
    this.updateStatusField();
    // set empty so defaults show up next change
    this.overLink = "";
  },

  updateStatusField : function()
  {
    var text = this.overLink || this.status || this.jsStatus || this.jsDefaultStatus || this.defaultStatus;

    // check the current value so we don't trigger an attribute change
    // and cause needless (slow!) UI updates
    if (this.statusTextField.label != text) {
      this.statusTextField.label = text;
    }
  },

  onLinkIconAvailable : function(aHref) {
    if (gProxyFavIcon)
      gProxyFavIcon.setAttribute("src", aHref);
  },

  onProgressChange : function (aWebProgress, aRequest,
                               aCurSelfProgress, aMaxSelfProgress,
                               aCurTotalProgress, aMaxTotalProgress)
  {
    if (!this.useRealProgressFlag && aRequest)
      return;

    if (aMaxTotalProgress > 0) {
      // This is highly optimized.  Don't touch this code unless
      // you are intimately familiar with the cost of setting
      // attrs on XUL elements. -- hyatt
      var percentage = (aCurTotalProgress * 100) / aMaxTotalProgress;
      this.statusMeter.value = percentage;
    } 
  },

  onStateChange : function(aWebProgress, aRequest, aStateFlags, aStatus)
  {  
    //ignore local/resource:/chrome: files
    if (aStatus == NS_NET_STATUS_READ_FROM || aStatus == NS_NET_STATUS_WROTE_TO)
      return;

    const nsIWebProgressListener = Components.interfaces.nsIWebProgressListener;
    const nsIChannel = Components.interfaces.nsIChannel;
    var domWindow;
    if (aStateFlags & nsIWebProgressListener.STATE_START) {
      if (aStateFlags & nsIWebProgressListener.STATE_IS_NETWORK) {
        // Remember when loading commenced.
        this.startTime = (new Date()).getTime();

        domWindow = aWebProgress.DOMWindow;
        if (aRequest && domWindow == _content)
          this.startDocumentLoad(aRequest);

        // Turn the throbber on.
        this.throbberElement.setAttribute("busy", true);

        // XXX: These need to be based on window activity...
        this.stopButton.disabled = false;
        this.stopMenu.removeAttribute('disabled');
        this.stopContext.removeAttribute('disabled');

        // Initialize the progress stuff...
        this.useRealProgressFlag = false;
        this.totalRequests = 0;
        this.finishedRequests = 0;
      }

      if (aStateFlags & nsIWebProgressListener.STATE_IS_REQUEST) {
        this.totalRequests += 1;
      }
    }
    else if (aStateFlags & nsIWebProgressListener.STATE_STOP) {
      if (aStateFlags & nsIWebProgressListener.STATE_IS_REQUEST) {
        this.finishedRequests += 1;
        if (!this.useRealProgressFlag)
          this.onProgressChange(null, null, 0, 0, this.finishedRequests, this.totalRequests);
      }
      if (aStateFlags & nsIWebProgressListener.STATE_IS_NETWORK) {
        domWindow = aWebProgress.DOMWindow;
        if (aRequest) {
          if (domWindow == domWindow.top)
            this.endDocumentLoad(aRequest, aStatus);

          var location = aRequest.QueryInterface(nsIChannel).URI.spec;
          var msg = "";
          if (location != "about:blank") {
            // Record page loading time.
            var elapsed = ((new Date()).getTime() - this.startTime) / 1000;
            msg = gNavigatorBundle.getString("nv_done");
            msg = msg.replace(/%elapsed%/, elapsed);
          }
          this.status = "";
          this.setDefaultStatus(msg);
        }

        // Turn the progress meter and throbber off.
        this.statusMeter.value = 0;  // be sure to clear the progress bar
        this.throbberElement.removeAttribute("busy");

        // XXX: These need to be based on window activity...
        // XXXjag: <command id="cmd_stop"/> ?
        this.stopButton.disabled = true;
        this.stopMenu.setAttribute('disabled', 'true');
        this.stopContext.setAttribute('disabled', 'true');

      }
    }
    else if (aStateFlags & nsIWebProgressListener.STATE_TRANSFERRING) {
      if (aStateFlags & nsIWebProgressListener.STATE_IS_DOCUMENT) {
        var ctype = aRequest.QueryInterface(nsIChannel).contentType;

        if (ctype != "text/html")
          this.useRealProgressFlag = true;
      }

      if (aStateFlags & nsIWebProgressListener.STATE_IS_REQUEST) {
        if (!this.useRealProgressFlag)
          this.onProgressChange(null, null, 0, 0, this.finishedRequests, this.totalRequests);
      }
    }
  },

  onLocationChange : function(aWebProgress, aRequest, aLocation)
  {
    var location = aLocation.spec;
    domWindow = aWebProgress.DOMWindow;

    if (this.hideAboutBlank) {
      this.hideAboutBlank = false;
      if (location == "about:blank")
        location = "";
    }

    // We should probably not do this if the value has changed since the user
    // searched
    // Update urlbar only if a new page was loaded on the primary content area
    // Do not update urlbar if there was a subframe navigation
    if (domWindow == domWindow.top) {
      this.urlBar.value = location;
      SetPageProxyState("valid", aLocation);
    }
    UpdateBackForwardButtons();
  },

  onStatusChange : function(aWebProgress, aRequest, aStatus, aMessage)
  {
    //ignore local/resource:/chrome: files
    if (aStatus == NS_NET_STATUS_READ_FROM || aStatus == NS_NET_STATUS_WROTE_TO)
      return;

    if (!this.statusTimeoutInEffect) {
      this.statusTimeoutInEffect = true;
      this.status = aMessage;
      this.updateStatusField();
      setTimeout(function(aClosure) { aClosure.statusTimeoutInEffect = false; }, 400, this);
    }
  },

  onSecurityChange : function(aWebProgress, aRequest, aState)
  {
  },

  startDocumentLoad : function(aRequest)
  {
    const nsIChannel = Components.interfaces.nsIChannel;
    var urlStr = aRequest.QueryInterface(nsIChannel).URI.spec;
    var observerService = Components.classes["@mozilla.org/observer-service;1"]
                                    .getService(Components.interfaces.nsIObserverService);
    try {
      observerService.notifyObservers(_content, "StartDocumentLoad", urlStr);
    } catch (e) {
    }
  },

  endDocumentLoad : function(aRequest, aStatus)
  {
    const nsIChannel = Components.interfaces.nsIChannel;
    var urlStr = aRequest.QueryInterface(nsIChannel).originalURI.spec;

    if (Components.isSuccessCode(aStatus))
      dump("Document "+urlStr+" loaded successfully\n"); // per QA request
    else {
      // per QA request
      var e = new Components.Exception("", aStatus);
      var name = e.name;
      dump("Error loading URL "+urlStr+" : "+
           Number(aStatus).toString(16));
      if (name)
           dump(" ("+name+")");
      dump('\n'); 
    }

    var observerService = Components.classes["@mozilla.org/observer-service;1"]
                                    .getService(Components.interfaces.nsIObserverService);

    var notification = Components.isSuccessCode(aStatus) ? "EndDocumentLoad" : "FailDocumentLoad";
    try {
      observerService.notifyObservers(_content, notification, urlStr);
    } catch (e) {
    }
  }
}

