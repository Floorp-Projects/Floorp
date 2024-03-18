/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Bug 1738220 - Shim Conversant FastClick
 *
 * Sites assuming FastClick will load can break if it is blocked.
 * This shim mitigates that breakage.
 */

// FastClick bundles nodeJS packages/core-js/internals/dom-iterables.js
// which is known to be needed by at least one site.
if (!HTMLCollection.prototype.forEach) {
  const DOMIterables = [
    "CSSRuleList",
    "CSSStyleDeclaration",
    "CSSValueList",
    "ClientRectList",
    "DOMRectList",
    "DOMStringList",
    "DOMTokenList",
    "DataTransferItemList",
    "FileList",
    "HTMLAllCollection",
    "HTMLCollection",
    "HTMLFormElement",
    "HTMLSelectElement",
    "MediaList",
    "MimeTypeArray",
    "NamedNodeMap",
    "NodeList",
    "PaintRequestList",
    "Plugin",
    "PluginArray",
    "SVGLengthList",
    "SVGNumberList",
    "SVGPathSegList",
    "SVGPointList",
    "SVGStringList",
    "SVGTransformList",
    "SourceBufferList",
    "StyleSheetList",
    "TextTrackCueList",
    "TextTrackList",
    "TouchList",
  ];

  const forEach = Array.prototype.forEach;

  const handlePrototype = proto => {
    if (!proto || proto.forEach === forEach) {
      return;
    }
    try {
      Object.defineProperty(proto, "forEach", {
        enumerable: false,
        get: () => forEach,
      });
    } catch (_) {
      proto.forEach = forEach;
    }
  };

  for (const name of DOMIterables) {
    handlePrototype(window[name]?.prototype);
  }
}

if (!window.conversant?.launch) {
  const c = (window.conversant = window.conversant || {});
  c.launch = () => {};
}
