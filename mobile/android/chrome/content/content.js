/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/ExtensionContent.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "AboutReader", "resource://gre/modules/AboutReader.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "ReaderMode", "resource://gre/modules/ReaderMode.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "LoginManagerContent", "resource://gre/modules/LoginManagerContent.jsm");

var dump = Cu.import("resource://gre/modules/AndroidLog.jsm", {}).AndroidLog.d.bind(null, "Content");

var global = this;

// This is copied from desktop's tab-content.js. See bug 1153485 about sharing this code somehow.
var AboutReaderListener = {

  _articlePromise: null,

  init: function() {
    addEventListener("AboutReaderContentLoaded", this, false, true);
    addEventListener("DOMContentLoaded", this, false);
    addEventListener("pageshow", this, false);
    addEventListener("pagehide", this, false);
    addMessageListener("Reader:ParseDocument", this);
    addMessageListener("Reader:PushState", this);
  },

  receiveMessage: function(message) {
    switch (message.name) {
      case "Reader:ParseDocument":
        this._articlePromise = ReaderMode.parseDocument(content.document).catch(Cu.reportError);
        content.document.location = "about:reader?url=" + encodeURIComponent(message.data.url);
        break;

      case "Reader:PushState":
        this.updateReaderButton(!!(message.data && message.data.isArticle));
        break;
    }
  },

  get isAboutReader() {
    return content.document.documentURI.startsWith("about:reader");
  },

  handleEvent: function(aEvent) {
    if (aEvent.originalTarget.defaultView != content) {
      return;
    }

    switch (aEvent.type) {
      case "AboutReaderContentLoaded":
        if (!this.isAboutReader) {
          return;
        }

        // If we are restoring multiple reader mode tabs during session restore, duplicate "DOMContentLoaded"
        // events may be fired for the visible tab. The inital "DOMContentLoaded" may be received before the
        // document body is available, so we avoid instantiating an AboutReader object, expecting that a
        // valid message will follow. See bug 925983.
        if (content.document.body) {
          new AboutReader(global, content, this._articlePromise);
          this._articlePromise = null;
        }
        break;

      case "pagehide":
        sendAsyncMessage("Reader:UpdateReaderButton", { isArticle: false });
        break;

      case "pageshow":
        // If a page is loaded from the bfcache, we won't get a "DOMContentLoaded"
        // event, so we need to rely on "pageshow" in this case.
        if (aEvent.persisted) {
          this.updateReaderButton();
        }
        break;
      case "DOMContentLoaded":
        this.updateReaderButton();
        break;
    }
  },
  updateReaderButton: function(forceNonArticle) {
    if (!ReaderMode.isEnabledForParseOnLoad || this.isAboutReader ||
        !(content.document instanceof content.HTMLDocument) ||
        content.document.mozSyntheticDocument) {
      return;
    }
    // Only send updates when there are articles; there's no point updating with
    // |false| all the time.
    if (ReaderMode.isProbablyReaderable(content.document)) {
      sendAsyncMessage("Reader:UpdateReaderButton", { isArticle: true });
    } else if (forceNonArticle) {
      sendAsyncMessage("Reader:UpdateReaderButton", { isArticle: false });
    }
  },
};
AboutReaderListener.init();

addMessageListener("RemoteLogins:fillForm", function(message) {
  LoginManagerContent.receiveMessage(message, content);
});

ExtensionContent.init(this);
addEventListener("unload", () => {
  ExtensionContent.uninit(this);
});

