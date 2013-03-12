/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

function MetroUIUtils() {
}

const URLElements = {
  "a": "href",
  "applet": ["archive", "code", "codebase"],
  "area": "href",
  "audio": "src",
  "base": "href",
  "blockquote": ["cite"],
  "body": "background",
  "button": "formaction",
  "command": "icon",
  "del": ["cite"],
  "embed": "src",
  "form": "action",
  "frame": ["longdesc", "src"],
  "iframe": ["longdesc", "src"],
  "img": ["longdesc", "src"],
  "input": ["formaction", "src"],
  "ins": ["cite"],
  "link": "href",
  "object": ["archive", "codebase", "data"],
  "q": ["cite"],
  "script": "src",
  "source": "src",
};

MetroUIUtils.prototype = {
  classID : Components.ID("e4626085-17f7-4068-a225-66c1acc0485c"),
  QueryInterface : XPCOMUtils.generateQI([Ci.nsIMetroUIUtils]),
  /**
   * Loads the specified panel in the browser.
   * @ param aPanelId The identifier of the pane to load
  */
  showPanel: function(aPanelId) {
    let browserWin = Services.wm.getMostRecentWindow("navigator:browser");
    browserWin.PanelUI.show(aPanelId);
  },

  /**
   * Determines if the browser has selected content
  */
  get hasSelectedContent() {
    try {
      let browserWin = Services.wm.getMostRecentWindow("navigator:browser");
      let tabBrowser = browserWin.getBrowser();
      if (!browserWin || !tabBrowser || !tabBrowser.contentWindow) {
        return false;
      }

      let sel = tabBrowser.contentWindow.getSelection();
      return sel && sel.toString();
    } catch(e) {
      return false;
    }
  },

  /**
   * Obtains the current page title
  */
  get currentPageTitle() {
    let browserWin = Services.wm.getMostRecentWindow("navigator:browser");
    if (!browserWin || !browserWin.content || !browserWin.content.document) {
      throw Cr.NS_ERROR_FAILURE;
    }
    return browserWin.content.document.title || "";
  },

  /**
   * Obtains the current page URI
  */
  get currentPageURI() {
    let browserWin = Services.wm.getMostRecentWindow("navigator:browser");
    if (!browserWin || !browserWin.content || !browserWin.content.document) {
      throw Cr.NS_ERROR_FAILURE;
    }
    return browserWin.content.document.URL || "";
  },

  /**
   * Determines the text that should be shared
  */
  get shareText() {
    let browserWin = Services.wm.getMostRecentWindow("navigator:browser");
    let tabBrowser = browserWin.getBrowser();
    if (browserWin && tabBrowser && tabBrowser.contentWindow) {
      let sel = tabBrowser.contentWindow.getSelection();
      if (sel && sel.rangeCount)
        return sel;
    }

    throw Cr.NS_ERROR_FAILURE;
  },

  /**
   * Replaces the node's attribute value to be a fully qualified URL
  */
  _expandAttribute : function(ioService, doc, node, attrName) {
    let attrValue = node.getAttribute(attrName);
    if (!attrValue)
      return;

    try {
      let uri = ioService.newURI(attrValue, null, doc.baseURIObject);
      node.setAttribute(attrName, uri.spec);
    } catch (e) {
    }
  },

  /*
   * Replaces all attribute values in 'n' which contain URLs recursiely
   * to fully qualified URLs.
  */
  _expandURLs: function(doc, n) {
    let ioService = Cc["@mozilla.org/network/io-service;1"].
                    getService(Ci.nsIIOService);
    for (let i = 0; i < n.children.length; i++) {
      let child = n.children[i];
      let childTagName = child.tagName.toLowerCase();
      
      // Iterate through all known tags which can contain URLs. A tag either
      // contains a single attribute name or an array of attribute names.
      for (let tagName in URLElements) {
        if (tagName === childTagName) {
          if (URLElements[tagName] instanceof Array) {
            URLElements[tagName].forEach(function(attrName) {
              this._expandAttribute(ioService ,doc, child, attrName);
            }, this);
          } else {
            this._expandAttribute(ioService ,doc, child, URLElements[tagName]);
          }
        }
      }

      this._expandURLs(doc, child);
    }
  },

  /**
   * Determines the HTML that should be shared
  */
  get shareHTML() {
    let browserWin = Services.wm.getMostRecentWindow("navigator:browser");
    let tabBrowser = browserWin.getBrowser();
    let sel;
    if (browserWin && tabBrowser && tabBrowser.contentWindow &&
        (sel = tabBrowser.contentWindow.getSelection()) && sel.rangeCount) {
      let div = tabBrowser.contentWindow.document.createElement("DIV");
      for (let i = 0; i < sel.rangeCount; i++) {
        let contents = sel.getRangeAt(i).cloneContents(true);
        div.appendChild(contents);
      }
      this._expandURLs(tabBrowser.contentWindow.document, div);
      return div.outerHTML;
    }

    throw Cr.NS_ERROR_FAILURE;
  }
};

var component = [MetroUIUtils];
this.NSGetFactory = XPCOMUtils.generateNSGetFactory(component);
