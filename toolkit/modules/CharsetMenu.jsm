/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

this.EXPORTED_SYMBOLS = [ "CharsetMenu" ];

const { classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
XPCOMUtils.defineLazyGetter(this, "gBundle", function() {
  const kUrl = "chrome://global/locale/charsetMenu.properties";
  return Services.strings.createBundle(kUrl);
});

const kAutoDetectors = [
  ["off", "off"],
  ["ja", "ja_parallel_state_machine"],
  ["ru", "ruprob"],
  ["uk", "ukprob"]
];

/**
 * This set contains encodings that are in the Encoding Standard, except:
 *  - XSS-dangerous encodings (except ISO-2022-JP which is assumed to be
 *    too common not to be included).
 *  - x-user-defined, which practically never makes sense as an end-user-chosen
 *    override.
 *  - Encodings that IE11 doesn't have in its correspoding menu.
 */
const kEncodings = new Set([
  // Globally relevant
  "UTF-8",
  "windows-1252",
  // Arabic
  "windows-1256",
  "ISO-8859-6",
  // Baltic
  "windows-1257",
  "ISO-8859-4",
  // "ISO-8859-13", // Hidden since not in menu in IE11
  // Central European
  "windows-1250",
  "ISO-8859-2",
  // Chinese, Simplified
  "gbk",
  // Chinese, Traditional
  "Big5",
  // Cyrillic
  "windows-1251",
  "ISO-8859-5",
  "KOI8-R",
  "KOI8-U",
  "IBM866", // Not in menu in Chromium. Maybe drop this?
  // "x-mac-cyrillic", // Not in menu in IE11 or Chromium.
  // Greek
  "windows-1253",
  "ISO-8859-7",
  // Hebrew
  "windows-1255",
  "ISO-8859-8",
  // Japanese
  "Shift_JIS",
  "EUC-JP",
  "ISO-2022-JP",
  // Korean
  "EUC-KR",
  // Thai
  "windows-874",
  // Turkish
  "windows-1254",
  // Vietnamese
  "windows-1258",
  // Hiding rare European encodings that aren't in the menu in IE11 and would
  // make the menu messy by sorting all over the place
  // "ISO-8859-3",
  // "ISO-8859-10",
  // "ISO-8859-14",
  // "ISO-8859-15",
  // "ISO-8859-16",
  // "macintosh"
]);

// Always at the start of the menu, in this order, followed by a separator.
const kPinned = [
  "UTF-8",
  "windows-1252"
];

kPinned.forEach(x => kEncodings.delete(x));


let gDetectorInfoCache, gCharsetInfoCache, gPinnedInfoCache;

let CharsetMenu = {
  build: function(parent, idPrefix="", showAccessKeys=true) {
    function createDOMNode(doc, nodeInfo) {
      let node = doc.createElement("menuitem");
      node.setAttribute("type", "radio");
      node.setAttribute("name", nodeInfo.name);
      node.setAttribute("label", nodeInfo.label);
      if (showAccessKeys && nodeInfo.accesskey) {
        node.setAttribute("accesskey", nodeInfo.accesskey);
      }
      if (idPrefix) {
        node.id = idPrefix + nodeInfo.id;
      } else {
        node.id = nodeInfo.id;
      }
      return node;
    }

    if (parent.childElementCount > 0) {
      // Detector menu or charset menu already built
      return;
    }
    let doc = parent.ownerDocument;

    let menuNode = doc.createElement("menu");
    menuNode.setAttribute("label", gBundle.GetStringFromName("charsetMenuAutodet"));
    if (showAccessKeys) {
      menuNode.setAttribute("accesskey", gBundle.GetStringFromName("charsetMenuAutodet.key"));
    }
    parent.appendChild(menuNode);

    let menuPopupNode = doc.createElement("menupopup");
    menuNode.appendChild(menuPopupNode);

    this._ensureDataReady();
    gDetectorInfoCache.forEach(detectorInfo => menuPopupNode.appendChild(createDOMNode(doc, detectorInfo)));
    parent.appendChild(doc.createElement("menuseparator"));
    gPinnedInfoCache.forEach(charsetInfo => parent.appendChild(createDOMNode(doc, charsetInfo)));
    parent.appendChild(doc.createElement("menuseparator"));
    gCharsetInfoCache.forEach(charsetInfo => parent.appendChild(createDOMNode(doc, charsetInfo)));
  },

  getData: function() {
    this._ensureDataReady();
    return {
      detectors: gDetectorInfoCache,
      pinnedCharsets: gPinnedInfoCache,
      otherCharsets: gCharsetInfoCache
    };
  },

  _ensureDataReady: function() {
    if (!gDetectorInfoCache) {
      gDetectorInfoCache = this.getDetectorInfo();
      gPinnedInfoCache = this.getCharsetInfo(kPinned, false);
      gCharsetInfoCache = this.getCharsetInfo([...kEncodings]);
    }
  },

  getDetectorInfo: function() {
    return kAutoDetectors.map(([detectorName, nodeId]) => ({
      id: "chardet." + nodeId,
      label: this._getDetectorLabel(detectorName),
      accesskey: this._getDetectorAccesskey(detectorName),
      name: "detectorGroup",
    }));
  },

  getCharsetInfo: function(charsets, sort=true) {
    let list = charsets.map(charset => ({
      id: "charset." + charset,
      label: this._getCharsetLabel(charset),
      accesskey: this._getCharsetAccessKey(charset),
      name: "charsetGroup",
    }));

    if (!sort) {
      return list;
    }

    list.sort(function (a, b) {
      let titleA = a.label;
      let titleB = b.label;
      // Normal sorting sorts the part in parenthesis in an order that
      // happens to make the less frequently-used items first.
      let index;
      if ((index = titleA.indexOf("(")) > -1) {
        titleA = titleA.substring(0, index);
      }
      if ((index = titleB.indexOf("(")) > -1) {
        titleA = titleB.substring(0, index);
      }
      let comp = titleA.localeCompare(titleB);
      if (comp) {
        return comp;
      }
      // secondarily reverse sort by encoding name to sort "windows" or
      // "shift_jis" first. This works regardless of localization, because
      // the ids aren't localized.
      if (a.id < b.id) {
        return 1;
      }
      if (b.id < a.id) {
        return -1;
      }
      return 0;
    });
    return list;
  },

  _getDetectorLabel: function(detector) {
    try {
      return gBundle.GetStringFromName("charsetMenuAutodet." + detector);
    } catch (ex) {}
    return detector;
  },
  _getDetectorAccesskey: function(detector) {
    try {
      return gBundle.GetStringFromName("charsetMenuAutodet." + detector + ".key");
    } catch (ex) {}
    return "";
  },

  _getCharsetLabel: function(charset) {
    if (charset == "gbk") {
      // Localization key has been revised
      charset = "gbk.bis";
    }
    try {
      return gBundle.GetStringFromName(charset);
    } catch (ex) {}
    return charset;
  },
  _getCharsetAccessKey: function(charset) {
    if (charset == "gbk") {
      // Localization key has been revised
      charset = "gbk.bis";
    }
    try {
      return gBundle.GetStringFromName(charset + ".key");
    } catch (ex) {}
    return "";
  },
};

Object.freeze(CharsetMenu);

