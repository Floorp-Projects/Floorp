/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "AboutReader", "resource://gre/modules/AboutReader.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "ReaderMode", "resource://gre/modules/ReaderMode.jsm");

let dump = Cu.import("resource://gre/modules/AndroidLog.jsm", {}).AndroidLog.d.bind(null, "Content");

let global = this;

let AboutReaderListener = {
  _savedArticle: null,

  init: function() {
    addEventListener("AboutReaderContentLoaded", this, false, true);
    addEventListener("pageshow", this, false);
    addMessageListener("Reader:SavedArticleGet", this);
  },

  receiveMessage: function(message) {
    switch (message.name) {
      case "Reader:SavedArticleGet":
        sendAsyncMessage("Reader:SavedArticleData", { article: this._savedArticle });
        break;
    }
  },

  get isAboutReader() {
    return content.document.documentURI.startsWith("about:reader");
  },

  handleEvent: function(event) {
    if (event.originalTarget.defaultView != content) {
      return;
    }

    switch (event.type) {
      case "AboutReaderContentLoaded":
        if (!this.isAboutReader) {
          return;
        }

        // If we are restoring multiple reader mode tabs during session restore, duplicate "DOMContentLoaded"
        // events may be fired for the visible tab. The inital "DOMContentLoaded" may be received before the
        // document body is available, so we avoid instantiating an AboutReader object, expecting that a
        // valid message will follow. See bug 925983.
        if (content.document.body) {
          new AboutReader(global, content);
        }
        break;

      case "pageshow":
        if (!ReaderMode.isEnabledForParseOnLoad || this.isAboutReader) {
          return;
        }

        // Reader mode is disabled until proven enabled.
        this._savedArticle = null;
        sendAsyncMessage("Reader:UpdateReaderButton", { isArticle: false });

        ReaderMode.parseDocument(content.document).then(article => {
          // Do nothing if there is no article, or if the content window has been destroyed.
          if (article === null || content === null) {
            return;
          }

          // The loaded page may have changed while we were parsing the document.
          // Make sure we've got the current one.
          let currentURL = Services.io.newURI(content.document.documentURI, null, null).specIgnoringRef;
          if (article.url !== currentURL) {
            return;
          }

          this._savedArticle = article;
          sendAsyncMessage("Reader:UpdateReaderButton", { isArticle: true });

        }).catch(e => Cu.reportError("Error parsing document: " + e));
        break;
    }
  }
};
AboutReaderListener.init();
