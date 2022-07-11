/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function TooltipTextProvider() {}

TooltipTextProvider.prototype = {
  getNodeText(tipElement, textOut, directionOut) {
    // Don't show the tooltip if the tooltip node is a document or browser.
    // Caller should ensure the node is in (composed) document.
    if (
      !tipElement ||
      !tipElement.ownerDocument ||
      tipElement.localName == "browser"
    ) {
      return false;
    }

    var defView = tipElement.ownerGlobal;
    // XXX Work around bug 350679:
    // "Tooltips can be fired in documents with no view".
    if (!defView) {
      return false;
    }

    const XLinkNS = "http://www.w3.org/1999/xlink";
    const XUL_NS =
      "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

    var titleText = null;
    var XLinkTitleText = null;
    var SVGTitleText = null;
    var XULtooltiptextText = null;
    var lookingForSVGTitle = true;
    var direction = tipElement.ownerDocument.dir;

    // If the element is invalid per HTML5 Forms specifications and has no title,
    // show the constraint validation error message.
    if (
      (defView.HTMLInputElement.isInstance(tipElement) ||
        defView.HTMLTextAreaElement.isInstance(tipElement) ||
        defView.HTMLSelectElement.isInstance(tipElement) ||
        defView.HTMLButtonElement.isInstance(tipElement)) &&
      !tipElement.hasAttribute("title") &&
      (!tipElement.form || !tipElement.form.noValidate)
    ) {
      // If the element is barred from constraint validation or valid,
      // the validation message will be the empty string.
      titleText = tipElement.validationMessage || null;
    }

    // If the element is an <input type='file'> without a title, we should show
    // the current file selection.
    if (
      !titleText &&
      defView.HTMLInputElement.isInstance(tipElement) &&
      tipElement.type == "file" &&
      !tipElement.hasAttribute("title")
    ) {
      let files = tipElement.files;

      try {
        var bundle = Services.strings.createBundle(
          "chrome://global/locale/layout/HtmlForm.properties"
        );
        if (!files.length) {
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
            let { PluralForm } = ChromeUtils.import(
              "resource://gre/modules/PluralForm.jsm"
            );
            let andXMoreStr = PluralForm.get(xmoreNum, xmoreStr).replace(
              "#1",
              xmoreNum
            );
            titleText += "\n" + andXMoreStr;
          }
        }
      } catch (e) {}
    }

    // Check texts against null so that title="" can be used to undefine a
    // title on a child element.
    let usedTipElement = null;
    while (
      tipElement &&
      titleText == null &&
      XLinkTitleText == null &&
      SVGTitleText == null &&
      XULtooltiptextText == null
    ) {
      if (tipElement.nodeType == defView.Node.ELEMENT_NODE) {
        if (tipElement.namespaceURI == XUL_NS) {
          XULtooltiptextText = tipElement.hasAttribute("tooltiptext")
            ? tipElement.getAttribute("tooltiptext")
            : null;
        } else if (!defView.SVGElement.isInstance(tipElement)) {
          titleText = tipElement.getAttribute("title");
        }

        if (
          (defView.HTMLAnchorElement.isInstance(tipElement) ||
            defView.HTMLAreaElement.isInstance(tipElement) ||
            defView.HTMLLinkElement.isInstance(tipElement) ||
            defView.SVGAElement.isInstance(tipElement)) &&
          tipElement.href
        ) {
          XLinkTitleText = tipElement.getAttributeNS(XLinkNS, "title");
        }
        if (
          lookingForSVGTitle &&
          (!defView.SVGElement.isInstance(tipElement) ||
            tipElement.parentNode.nodeType == defView.Node.DOCUMENT_NODE)
        ) {
          lookingForSVGTitle = false;
        }
        if (lookingForSVGTitle) {
          for (let childNode of tipElement.childNodes) {
            if (defView.SVGTitleElement.isInstance(childNode)) {
              SVGTitleText = childNode.textContent;
              break;
            }
          }
        }

        usedTipElement = tipElement;
      }

      tipElement = tipElement.flattenedTreeParentNode;
    }

    return [titleText, XLinkTitleText, SVGTitleText, XULtooltiptextText].some(
      function(t) {
        if (t && /\S/.test(t)) {
          // Make CRLF and CR render one line break each.
          textOut.value = t.replace(/\r\n?/g, "\n");

          if (usedTipElement) {
            direction = defView
              .getComputedStyle(usedTipElement)
              .getPropertyValue("direction");
          }

          directionOut.value = direction;
          return true;
        }

        return false;
      }
    );
  },

  classID: Components.ID("{f376627f-0bbc-47b8-887e-fc92574cc91f}"),
  QueryInterface: ChromeUtils.generateQI(["nsITooltipTextProvider"]),
};

var EXPORTED_SYMBOLS = ["TooltipTextProvider"];
