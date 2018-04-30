/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");

function TooltipTextProvider() {}

TooltipTextProvider.prototype = {
  getNodeText(tipElement, textOut, directionOut) {
    // Don't show the tooltip if the tooltip node is a document, browser, or disconnected.
    if (!tipElement || !tipElement.ownerDocument ||
        tipElement.localName == "browser" ||
        (tipElement.ownerDocument.compareDocumentPosition(tipElement) &
         tipElement.ownerDocument.DOCUMENT_POSITION_DISCONNECTED)) {
      return false;
    }

    var defView = tipElement.ownerGlobal;
    // XXX Work around bug 350679:
    // "Tooltips can be fired in documents with no view".
    if (!defView)
      return false;

    const XLinkNS = "http://www.w3.org/1999/xlink";
    const XULNS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

    var titleText = null;
    var XLinkTitleText = null;
    var SVGTitleText = null;
    var XULtooltiptextText = null;
    var lookingForSVGTitle = true;
    var direction = tipElement.ownerDocument.dir;

    // If the element is invalid per HTML5 Forms specifications and has no title,
    // show the constraint validation error message.
    if ((tipElement instanceof defView.HTMLInputElement ||
         tipElement instanceof defView.HTMLTextAreaElement ||
         tipElement instanceof defView.HTMLSelectElement ||
         tipElement instanceof defView.HTMLButtonElement) &&
        !tipElement.hasAttribute("title") &&
        (!tipElement.form || !tipElement.form.noValidate)) {
      // If the element is barred from constraint validation or valid,
      // the validation message will be the empty string.
      titleText = tipElement.validationMessage || null;
    }

    // If the element is an <input type='file'> without a title, we should show
    // the current file selection.
    if (!titleText &&
        tipElement instanceof defView.HTMLInputElement &&
        tipElement.type == "file" &&
        !tipElement.hasAttribute("title")) {
      let files = tipElement.files;

      try {
        var bundle =
          Services.strings.createBundle("chrome://global/locale/layout/HtmlForm.properties");
        if (files.length == 0) {
          if (tipElement.multiple) {
            titleText = bundle.GetStringFromName("NoFilesSelected");
          } else {
            titleText = bundle.GetStringFromName("NoFileSelected");
          }
        } else {
          titleText = files[0].name;
          // For UX and performance (jank) reasons we cap the number of
          // files that we list in the tooltip to 20 plus a "and xxx more"
          // line, or to 21 if exactly 21 files were picked.
          const TRUNCATED_FILE_COUNT = 20;
          let count = Math.min(files.length, TRUNCATED_FILE_COUNT);
          for (let i = 1; i < count; ++i) {
            titleText += "\n" + files[i].name;
          }
          if (files.length == TRUNCATED_FILE_COUNT + 1) {
            titleText += "\n" + files[TRUNCATED_FILE_COUNT].name;
          } else if (files.length > TRUNCATED_FILE_COUNT + 1) {
            let xmoreStr = bundle.GetStringFromName("AndNMoreFiles");
            let xmoreNum = files.length - TRUNCATED_FILE_COUNT;
            let tmp = {};
            ChromeUtils.import("resource://gre/modules/PluralForm.jsm", tmp);
            let andXMoreStr = tmp.PluralForm.get(xmoreNum, xmoreStr).replace("#1", xmoreNum);
            titleText += "\n" + andXMoreStr;
          }
        }
      } catch (e) {}
    }

    // Check texts against null so that title="" can be used to undefine a
    // title on a child element.
    let usedTipElement = null;
    while (tipElement &&
           (titleText == null) && (XLinkTitleText == null) &&
           (SVGTitleText == null) && (XULtooltiptextText == null)) {

      if (tipElement.nodeType == defView.Node.ELEMENT_NODE) {
        if (tipElement.namespaceURI == XULNS)
          XULtooltiptextText = tipElement.getAttribute("tooltiptext");
        else if (!(tipElement instanceof defView.SVGElement))
          titleText = tipElement.getAttribute("title");

        if ((tipElement instanceof defView.HTMLAnchorElement ||
             tipElement instanceof defView.HTMLAreaElement ||
             tipElement instanceof defView.HTMLLinkElement ||
             tipElement instanceof defView.SVGAElement) && tipElement.href) {
          XLinkTitleText = tipElement.getAttributeNS(XLinkNS, "title");
        }
        if (lookingForSVGTitle &&
            (!(tipElement instanceof defView.SVGElement) ||
             tipElement.parentNode.nodeType == defView.Node.DOCUMENT_NODE)) {
          lookingForSVGTitle = false;
        }
        if (lookingForSVGTitle) {
          for (let childNode of tipElement.childNodes) {
            if (childNode instanceof defView.SVGTitleElement) {
              SVGTitleText = childNode.textContent;
              break;
            }
          }
        }

        usedTipElement = tipElement;
      }

      tipElement = tipElement.parentNode;
    }

    return [titleText, XLinkTitleText, SVGTitleText, XULtooltiptextText].some(function(t) {
      if (t && /\S/.test(t)) {
        // Make CRLF and CR render one line break each.
        textOut.value = t.replace(/\r\n?/g, "\n");

        if (usedTipElement) {
          direction = defView.getComputedStyle(usedTipElement)
                             .getPropertyValue("direction");
        }

        directionOut.value = direction;
        return true;
      }

      return false;
    });
  },

  classID: Components.ID("{f376627f-0bbc-47b8-887e-fc92574cc91f}"),
  QueryInterface: ChromeUtils.generateQI([Ci.nsITooltipTextProvider]),
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([TooltipTextProvider]);

