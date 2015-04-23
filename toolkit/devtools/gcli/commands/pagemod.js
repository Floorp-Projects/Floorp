/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Cc, Ci, Cu } = require("chrome");
const l10n = require("gcli/l10n");

exports.items = [
  {
    name: "pagemod",
    description: l10n.lookup("pagemodDesc"),
  },
  {
    item: "command",
    runAt: "server",
    name: "pagemod replace",
    description: l10n.lookup("pagemodReplaceDesc"),
    params: [
      {
        name: "search",
        type: "string",
        description: l10n.lookup("pagemodReplaceSearchDesc"),
      },
      {
        name: "replace",
        type: "string",
        description: l10n.lookup("pagemodReplaceReplaceDesc"),
      },
      {
        name: "ignoreCase",
        type: "boolean",
        description: l10n.lookup("pagemodReplaceIgnoreCaseDesc"),
      },
      {
        name: "selector",
        type: "string",
        description: l10n.lookup("pagemodReplaceSelectorDesc"),
        defaultValue: "*:not(script):not(style):not(embed):not(object):not(frame):not(iframe):not(frameset)",
      },
      {
        name: "root",
        type: "node",
        description: l10n.lookup("pagemodReplaceRootDesc"),
        defaultValue: null,
      },
      {
        name: "attrOnly",
        type: "boolean",
        description: l10n.lookup("pagemodReplaceAttrOnlyDesc"),
      },
      {
        name: "contentOnly",
        type: "boolean",
        description: l10n.lookup("pagemodReplaceContentOnlyDesc"),
      },
      {
        name: "attributes",
        type: "string",
        description: l10n.lookup("pagemodReplaceAttributesDesc"),
        defaultValue: null,
      },
    ],
    // Make a given string safe to use in a regular expression.
    escapeRegex: function(aString) {
      return aString.replace(/[-[\]{}()*+?.,\\^$|#\s]/g, "\\$&");
    },
    exec: function(args, context) {
      let searchTextNodes = !args.attrOnly;
      let searchAttributes = !args.contentOnly;
      let regexOptions = args.ignoreCase ? "ig" : "g";
      let search = new RegExp(this.escapeRegex(args.search), regexOptions);
      let attributeRegex = null;
      if (args.attributes) {
        attributeRegex = new RegExp(args.attributes, regexOptions);
      }

      let root = args.root || context.environment.document;
      let elements = root.querySelectorAll(args.selector);
      elements = Array.prototype.slice.call(elements);

      let replacedTextNodes = 0;
      let replacedAttributes = 0;

      function replaceAttribute() {
        replacedAttributes++;
        return args.replace;
      }
      function replaceTextNode() {
        replacedTextNodes++;
        return args.replace;
      }

      for (let i = 0; i < elements.length; i++) {
        let element = elements[i];
        if (searchTextNodes) {
          for (let y = 0; y < element.childNodes.length; y++) {
            let node = element.childNodes[y];
            if (node.nodeType == node.TEXT_NODE) {
              node.textContent = node.textContent.replace(search, replaceTextNode);
            }
          }
        }

        if (searchAttributes) {
          if (!element.attributes) {
            continue;
          }
          for (let y = 0; y < element.attributes.length; y++) {
            let attr = element.attributes[y];
            if (!attributeRegex || attributeRegex.test(attr.name)) {
              attr.value = attr.value.replace(search, replaceAttribute);
            }
          }
        }
      }

      return l10n.lookupFormat("pagemodReplaceResult",
                              [elements.length, replacedTextNodes,
                                replacedAttributes]);
    }
  },
  {
    name: "pagemod remove",
    description: l10n.lookup("pagemodRemoveDesc"),
  },
  {
    item: "command",
    runAt: "server",
    name: "pagemod remove element",
    description: l10n.lookup("pagemodRemoveElementDesc"),
    params: [
      {
        name: "search",
        type: "string",
        description: l10n.lookup("pagemodRemoveElementSearchDesc"),
      },
      {
        name: "root",
        type: "node",
        description: l10n.lookup("pagemodRemoveElementRootDesc"),
        defaultValue: null,
      },
      {
        name: "stripOnly",
        type: "boolean",
        description: l10n.lookup("pagemodRemoveElementStripOnlyDesc"),
      },
      {
        name: "ifEmptyOnly",
        type: "boolean",
        description: l10n.lookup("pagemodRemoveElementIfEmptyOnlyDesc"),
      },
    ],
    exec: function(args, context) {
      let root = args.root || context.environment.document;
      let elements = Array.prototype.slice.call(root.querySelectorAll(args.search));

      let removed = 0;
      for (let i = 0; i < elements.length; i++) {
        let element = elements[i];
        let parentNode = element.parentNode;
        if (!parentNode || !element.removeChild) {
          continue;
        }
        if (args.stripOnly) {
          while (element.hasChildNodes()) {
            parentNode.insertBefore(element.childNodes[0], element);
          }
        }
        if (!args.ifEmptyOnly || !element.hasChildNodes()) {
          element.parentNode.removeChild(element);
          removed++;
        }
      }

      return l10n.lookupFormat("pagemodRemoveElementResultMatchedAndRemovedElements",
                              [elements.length, removed]);
    }
  },
  {
    item: "command",
    runAt: "server",
    name: "pagemod remove attribute",
    description: l10n.lookup("pagemodRemoveAttributeDesc"),
    params: [
      {
        name: "searchAttributes",
        type: "string",
        description: l10n.lookup("pagemodRemoveAttributeSearchAttributesDesc"),
      },
      {
        name: "searchElements",
        type: "string",
        description: l10n.lookup("pagemodRemoveAttributeSearchElementsDesc"),
      },
      {
        name: "root",
        type: "node",
        description: l10n.lookup("pagemodRemoveAttributeRootDesc"),
        defaultValue: null,
      },
      {
        name: "ignoreCase",
        type: "boolean",
        description: l10n.lookup("pagemodRemoveAttributeIgnoreCaseDesc"),
      },
    ],
    exec: function(args, context) {
      let root = args.root || context.environment.document;
      let regexOptions = args.ignoreCase ? "ig" : "g";
      let attributeRegex = new RegExp(args.searchAttributes, regexOptions);
      let elements = root.querySelectorAll(args.searchElements);
      elements = Array.prototype.slice.call(elements);

      let removed = 0;
      for (let i = 0; i < elements.length; i++) {
        let element = elements[i];
        if (!element.attributes) {
          continue;
        }

        var attrs = Array.prototype.slice.call(element.attributes);
        for (let y = 0; y < attrs.length; y++) {
          let attr = attrs[y];
          if (attributeRegex.test(attr.name)) {
            element.removeAttribute(attr.name);
            removed++;
          }
        }
      }

      return l10n.lookupFormat("pagemodRemoveAttributeResult",
                              [elements.length, removed]);
    }
  },
  // This command allows the user to export the page to HTML after DOM changes
  {
    name: "export",
    description: l10n.lookup("exportDesc"),
  },
  {
    item: "command",
    runAt: "server",
    name: "export html",
    description: l10n.lookup("exportHtmlDesc"),
    params: [
      {
        name: "destination",
        type: {
          name: "selection",
          data: [ "window", "stdout", "clipboard" ]
        },
        defaultValue: "window"
      }
    ],
    exec: function(args, context) {
      let html = context.environment.document.documentElement.outerHTML;
      if (args.destination === "stdout") {
        return html;
      }

      if (args.desination === "clipboard") {
        let clipboard = Cc["@mozilla.org/widget/clipboardhelper;1"]
                          .getService(Ci.nsIClipboardHelper);
        clipboard.copyString(url);
        return '';
      }

      let url = "data:text/plain;charset=utf8," + encodeURIComponent(html);
      context.environment.window.open(url);
      return '';
    }
  }
];
