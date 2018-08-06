/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

{

/* globals XULFrameElement */

class MozEditor extends XULFrameElement {
  connectedCallback() {
    this._editorContentListener = {
      QueryInterface: ChromeUtils.generateQI([
        "nsIURIContentListener",
        "nsISupportsWeakReference",
      ]),
      onStartURIOpen(uri) {
        return false;
      },
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
      parentContentListener: null
    };

    this._finder = null;

    this._fastFind = null;

    this._lastSearchString = null;

    // Make window editable immediately only
    //   if the "editortype" attribute is supplied
    // This allows using same contentWindow for different editortypes,
    //   where the type is determined during the apps's window.onload handler.
    if (this.editortype)
      this.makeEditable(this.editortype, true);
  }

  get finder() {
    if (!this._finder) {
      if (!this.docShell)
        return null;

      let Finder = ChromeUtils.import("resource://gre/modules/Finder.jsm", {}).Finder;
      this._finder = new Finder(this.docShell);
    }
    return this._finder;
  }

  get fastFind() {
    if (!this._fastFind) {
      if (!("@mozilla.org/typeaheadfind;1" in Cc))
        return null;

      if (!this.docShell)
        return null;

      this._fastFind = Cc["@mozilla.org/typeaheadfind;1"]
        .createInstance(Ci.nsITypeAheadFind);
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

  get contentWindowAsCPOW() {
    return this.contentWindow;
  }

  get webBrowserFind() {
    return this.docShell.QueryInterface(Ci.nsIInterfaceRequestor)
                        .getInterface(Ci.nsIWebBrowserFind);
  }

  get markupDocumentViewer() {
    return this.docShell.contentViewer;
  }

  get editingSession() {
    return this.docShell.editingSession;
  }

  get commandManager() {
    return this.webNavigation.QueryInterface(Ci.nsIInterfaceRequestor)
                             .getInterface(Ci.nsICommandManager);
  }

  set fullZoom(val) {
    this.markupDocumentViewer.fullZoom = val;
  }

  get fullZoom() {
    return this.markupDocumentViewer.fullZoom;
  }

  set textZoom(val) {
    this.markupDocumentViewer.textZoom = val;
  }

  get textZoom() {
    return this.markupDocumentViewer.textZoom;
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

  get outerWindowID() {
    return this.contentWindow.windowUtils.outerWindowID;
  }

  makeEditable(editortype, waitForUrlLoad) {
    this.editingSession.makeWindowEditable(
      this.contentWindow,
      editortype,
      waitForUrlLoad,
      true,
      false
    );
    this.setAttribute("editortype", editortype);

    this.docShell.QueryInterface(Ci.nsIInterfaceRequestor)
                 .getInterface(Ci.nsIURIContentListener)
                 .parentContentListener = this._editorContentListener;
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
