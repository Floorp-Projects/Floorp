/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var SharingUI = {
  _dialog: null,

  show: function show(aURL, aTitle) {
    try {
      this.showSharingUI(aURL, aTitle);
    } catch (ex) {
      this.showFallback(aURL, aTitle);
    }
  },

  showSharingUI: function showSharingUI(aURL, aTitle) {
    let sharingSvc = Cc["@mozilla.org/uriloader/external-sharing-app-service;1"].getService(Ci.nsIExternalSharingAppService);
    sharingSvc.shareWithDefault(aURL, "text/plain", aTitle);
  },

  showFallback: function showFallback(aURL, aTitle) {
    this._dialog = importDialog(window, "chrome://browser/content/share.xul", null);
    document.getElementById("share-title").value = aTitle || aURL;

    BrowserUI.pushPopup(this, this._dialog);

    let bbox = document.getElementById("share-buttons-box");
    this._handlers.forEach(function(handler) {
      let button = document.createElement("button");
      button.className = "action-button";
      button.setAttribute("label", handler.name);
      button.addEventListener("command", function() {
        SharingUI.hide();
        handler.callback(aURL || "", aTitle || "");
      }, false);
      bbox.appendChild(button);
    });

    this._dialog.waitForClose();
    BrowserUI.popPopup(this);
  },

  hide: function hide() {
    this._dialog.close();
    this._dialog = null;
  },

  _handlers: [
    {
      name: "Email",
      callback: function callback(aURL, aTitle) {
        let url = "mailto:?subject=" + encodeURIComponent(aTitle) +
                  "&body=" + encodeURIComponent(aURL);
        let uri = Services.io.newURI(url, null, null);
        let extProtocolSvc = Cc["@mozilla.org/uriloader/external-protocol-service;1"]
                             .getService(Ci.nsIExternalProtocolService);
        extProtocolSvc.loadUrl(uri);
      }
    },
    {
      name: "Twitter",
      callback: function callback(aURL, aTitle) {
        let url = "http://twitter.com/home?status=" + encodeURIComponent((aTitle ? aTitle+": " : "")+aURL);
        BrowserUI.newTab(url, Browser.selectedTab);
      }
    },
    {
      name: "Google Reader",
      callback: function callback(aURL, aTitle) {
        let url = "http://www.google.com/reader/link?url=" + encodeURIComponent(aURL) +
                  "&title=" + encodeURIComponent(aTitle);
        BrowserUI.newTab(url, Browser.selectedTab);
      }
    },
    {
      name: "Facebook",
      callback: function callback(aURL, aTitle) {
        let url = "http://www.facebook.com/share.php?u=" + encodeURIComponent(aURL);
        BrowserUI.newTab(url, Browser.selectedTab);
      }
    }
  ]
};
