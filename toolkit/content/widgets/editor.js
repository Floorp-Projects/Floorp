/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// This is loaded into chrome windows with the subscript loader. Wrap in
// a block to prevent accidentally leaking globals onto `window`.
{
  /* globals XULFrameElement */

  class MozEditor extends XULFrameElement {
    connectedCallback() {
      this._editorContentListener = {
        QueryInterface: ChromeUtils.generateQI([
          "nsIURIContentListener",
          "nsISupportsWeakReference",
        ]),
        doContent(contentType, isContentPreferred, request, contentHandler) {
          return false;
        },
        isPreferred(contentType, desiredContentType) {
          return false;
        },
        canHandleContent(contentType, isContentPreferred, desiredContentType) {
          return false;
        },
        loadCookie: null,
        parentContentListener: null,
      };

      this._finder = null;

      this._fastFind = null;

      this._lastSearchString = null;

      // Make window editable immediately only
      //   if the "editortype" attribute is supplied
      // This allows using same contentWindow for different editortypes,
      //   where the type is determined during the apps's window.onload handler.
      if (this.editortype) {
        this.makeEditable(this.editortype, true);
      }
    }

    get finder() {
      if (!this._finder) {
        if (!this.docShell) {
          return null;
        }

        let Finder = ChromeUtils.import("resource://gre/modules/Finder.jsm", {})
          .Finder;
        this._finder = new Finder(this.docShell);
      }
      return this._finder;
    }

    get fastFind() {
      if (!this._fastFind) {
        if (!("@mozilla.org/typeaheadfind;1" in Cc)) {
          return null;
        }

        if (!this.docShell) {
          return null;
        }

        this._fastFind = Cc["@mozilla.org/typeaheadfind;1"].createInstance(
          Ci.nsITypeAheadFind
        );
        this._fastFind.init(this.docShell);
      }
      return this._fastFind;
    }

    set editortype(val) {
      this.setAttribute("editortype", val);
    }

    get editortype() {
      return this.getAttribute("editortype");
    }

    get currentURI() {
      return this.webNavigation.currentURI;
    }

    get webBrowserFind() {
      return this.docShell
        .QueryInterface(Ci.nsIInterfaceRequestor)
        .getInterface(Ci.nsIWebBrowserFind);
    }

    get markupDocumentViewer() {
      return this.docShell.contentViewer;
    }

    get editingSession() {
      return this.docShell.editingSession;
    }

    get commandManager() {
      return this.webNavigation
        .QueryInterface(Ci.nsIInterfaceRequestor)
        .getInterface(Ci.nsICommandManager);
    }

    set fullZoom(val) {
      this.browsingContext.fullZoom = val;
    }

    get fullZoom() {
      return this.browsingContext.fullZoom;
    }

    set textZoom(val) {
      this.browsingContext.textZoom = val;
    }

    get textZoom() {
      return this.browsingContext.textZoom;
    }

    get isSyntheticDocument() {
      return this.contentDocument.isSyntheticDocument;
    }

    get messageManager() {
      if (this.frameLoader) {
        return this.frameLoader.messageManager;
      }
      return null;
    }

    // Copied from toolkit/content/widgets/browser-custom-element.js.
    // Send an asynchronous message to the remote child via an actor.
    // Note: use this only for messages through an actor. For old-style
    // messages, use the message manager.
    // The value of the scope argument determines which browsing contexts
    // are sent to:
    //   'all' - send to actors associated with all descendant child frames.
    //   'roots' - send only to actors associated with process roots.
    //   undefined/'' - send only to the top-level actor and not any descendants.
    sendMessageToActor(messageName, args, actorName, scope) {
      if (!this.frameLoader) {
        return;
      }

      function sendToChildren(browsingContext, childScope) {
        let windowGlobal = browsingContext.currentWindowGlobal;
        // If 'roots' is set, only send if windowGlobal.isProcessRoot is true.
        if (
          windowGlobal &&
          (childScope != "roots" || windowGlobal.isProcessRoot)
        ) {
          windowGlobal.getActor(actorName).sendAsyncMessage(messageName, args);
        }

        // Iterate as long as scope in assigned. Note that we use the original
        // passed in scope, not childScope here.
        if (scope) {
          for (let context of browsingContext.children) {
            sendToChildren(context, scope);
          }
        }
      }

      // Pass no second argument to always send to the top-level browsing context.
      sendToChildren(this.browsingContext);
    }

    get outerWindowID() {
      return this.contentWindow.windowUtils.outerWindowID;
    }

    makeEditable(editortype, waitForUrlLoad) {
      let win = this.contentWindow;
      let winUtils = win.windowUtils;
      this.editingSession.makeWindowEditable(
        win,
        editortype,
        waitForUrlLoad,
        true,
        false
      );
      winUtils.loadSheetUsingURIString(
        "resource://gre/res/EditorOverride.css",
        winUtils.AGENT_SHEET
      );
      this.setAttribute("editortype", editortype);

      this.docShell
        .QueryInterface(Ci.nsIInterfaceRequestor)
        .getInterface(
          Ci.nsIURIContentListener
        ).parentContentListener = this._editorContentListener;
    }

    getEditor(containingWindow) {
      return this.editingSession.getEditorForWindow(containingWindow);
    }

    getHTMLEditor(containingWindow) {
      var editor = this.editingSession.getEditorForWindow(containingWindow);
      return editor.QueryInterface(Ci.nsIHTMLEditor);
    }
  }

  customElements.define("editor", MozEditor);
}
