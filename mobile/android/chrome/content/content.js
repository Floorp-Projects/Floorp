/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var { classes: Cc, interfaces: Ci, utils: Cu } = Components;

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

  _isLeavingReaderableReaderMode: false,

  init: function() {
    addEventListener("AboutReaderContentLoaded", this, false, true);
    addEventListener("DOMContentLoaded", this, false);
    addEventListener("pageshow", this, false);
    addEventListener("pagehide", this, false);
    addMessageListener("Reader:ToggleReaderMode", this);
    addMessageListener("Reader:PushState", this);
  },

  receiveMessage: function(message) {
    switch (message.name) {
      case "Reader:ToggleReaderMode":
        let url = content.document.location.href;
        if (!this.isAboutReader) {
          this._articlePromise = ReaderMode.parseDocument(content.document).catch(Cu.reportError);
          ReaderMode.enterReaderMode(docShell, content);
        } else {
          this._isLeavingReaderableReaderMode = this.isReaderableAboutReader;
          ReaderMode.leaveReaderMode(docShell, content);
        }
        break;

      case "Reader:PushState":
        this.updateReaderButton(!!(message.data && message.data.isArticle));
        break;
    }
  },

  get isAboutReader() {
    return content.document.documentURI.startsWith("about:reader");
  },

  get isReaderableAboutReader() {
    return this.isAboutReader &&
      !content.document.documentElement.dataset.isError;
  },

  get isErrorPage() {
    return content.document.documentURI.startsWith("about:neterror") ||
        content.document.documentURI.startsWith("about:certerror") ||
        content.document.documentURI.startsWith("about:blocked");
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
        // this._isLeavingReaderableReaderMode is used here to keep the Reader Mode icon
        // visible in the location bar when transitioning from reader-mode page
        // back to the source page.
        sendAsyncMessage("Reader:UpdateReaderButton", { isArticle: this._isLeavingReaderableReaderMode });
        if (this._isLeavingReaderableReaderMode) {
          this._isLeavingReaderableReaderMode = false;
        }
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
    // Do not show Reader View icon on error pages (bug 1320900)
    if (this.isErrorPage) {
        sendAsyncMessage("Reader:UpdateReaderButton", { isArticle: false });
    } else if (!ReaderMode.isEnabledForParseOnLoad || this.isAboutReader ||
        !(content.document instanceof content.HTMLDocument) ||
        content.document.mozSyntheticDocument) {
      return;
    } else {
        this.scheduleReadabilityCheckPostPaint(forceNonArticle);
    }
  },

  cancelPotentialPendingReadabilityCheck: function() {
    if (this._pendingReadabilityCheck) {
      removeEventListener("MozAfterPaint", this._pendingReadabilityCheck);
      delete this._pendingReadabilityCheck;
    }
  },

  scheduleReadabilityCheckPostPaint: function(forceNonArticle) {
    if (this._pendingReadabilityCheck) {
      // We need to stop this check before we re-add one because we don't know
      // if forceNonArticle was true or false last time.
      this.cancelPotentialPendingReadabilityCheck();
    }
    this._pendingReadabilityCheck = this.onPaintWhenWaitedFor.bind(this, forceNonArticle);
    addEventListener("MozAfterPaint", this._pendingReadabilityCheck);
  },

  onPaintWhenWaitedFor: function(forceNonArticle, event) {
    // In non-e10s, we'll get called for paints other than ours, and so it's
    // possible that this page hasn't been laid out yet, in which case we
    // should wait until we get an event that does relate to our layout. We
    // determine whether any of our content got painted by checking if there
    // are any painted rects.
    if (!event.clientRects.length) {
      return;
    }

    this.cancelPotentialPendingReadabilityCheck();

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

Services.obs.notifyObservers(this, "tab-content-frameloader-created");
