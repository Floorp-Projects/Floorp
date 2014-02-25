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
  ["off", ""],
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

function CharsetComparator(a, b) {
  // Normal sorting sorts the part in parenthesis in an order that
  // happens to make the less frequently-used items first.
  let titleA = a.label.replace(/\(.*/, "") + b.value;
  let titleB = b.label.replace(/\(.*/, "") + a.value;
  // Secondarily reverse sort by encoding name to sort "windows" or
  // "shift_jis" first.
  return titleA.localeCompare(titleB) || b.value.localeCompare(a.value);
}

function SetDetector(event) {
  let str = Cc["@mozilla.org/supports-string;1"].createInstance(Ci.nsISupportsString);
  str.data = event.target.getAttribute("detector");
  Services.prefs.setComplexValue("intl.charset.detector", Ci.nsISupportsString, str);
}

function UpdateDetectorMenu(event) {
  event.stopPropagation();
  let detector = Services.prefs.getComplexValue("intl.charset.detector", Ci.nsIPrefLocalizedString);
  let menuitem = this.getElementsByAttribute("detector", detector).item(0);
  if (menuitem) {
    menuitem.setAttribute("checked", "true");
  }
}

let gDetectorInfoCache, gCharsetInfoCache, gPinnedInfoCache;

let CharsetMenu = {
  build: function(parent, showAccessKeys=true, showDetector=true) {
    function createDOMNode(doc, nodeInfo) {
      let node = doc.createElement("menuitem");
      node.setAttribute("type", "radio");
      node.setAttribute("name", nodeInfo.name + "Group");
      node.setAttribute(nodeInfo.name, nodeInfo.value);
      node.setAttribute("label", nodeInfo.label);
      if (showAccessKeys && nodeInfo.accesskey) {
        node.setAttribute("accesskey", nodeInfo.accesskey);
      }
      return node;
    }

    if (parent.hasChildNodes()) {
      // Detector menu or charset menu already built
      return;
    }
    this._ensureDataReady();
    let doc = parent.ownerDocument;

    if (showDetector) {
      let menuNode = doc.createElement("menu");
      menuNode.setAttribute("label", gBundle.GetStringFromName("charsetMenuAutodet"));
      if (showAccessKeys) {
        menuNode.setAttribute("accesskey", gBundle.GetStringFromName("charsetMenuAutodet.key"));
      }
      parent.appendChild(menuNode);

      let menuPopupNode = doc.createElement("menupopup");
      menuNode.appendChild(menuPopupNode);
      menuPopupNode.addEventListener("command", SetDetector);
      menuPopupNode.addEventListener("popupshown", UpdateDetectorMenu);

      gDetectorInfoCache.forEach(detectorInfo => menuPopupNode.appendChild(createDOMNode(doc, detectorInfo)));
      parent.appendChild(doc.createElement("menuseparator"));
    }

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
      gCharsetInfoCache = this.getCharsetInfo(kEncodings);
    }
  },

  getDetectorInfo: function() {
    return kAutoDetectors.map(([detectorName, nodeId]) => ({
      label: this._getDetectorLabel(detectorName),
      accesskey: this._getDetectorAccesskey(detectorName),
      name: "detector",
      value: nodeId
    }));
  },

  getCharsetInfo: function(charsets, sort=true) {
    let list = [{
      label: this._getCharsetLabel(charset),
      accesskey: this._getCharsetAccessKey(charset),
      name: "charset",
      value: charset
    } for (charset of charsets)];

    if (sort) {
      list.sort(CharsetComparator);
    }
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

  /**
   * For substantially similar encodings, treat two encodings as the same
   * for the purpose of the check mark.
   */
  foldCharset: function(charset) {
    switch (charset) {
      case "ISO-8859-8-I":
        return "windows-1255";

      case "gb18030":
        return "gbk";

      default:
        return charset;
    }
  },

  update: function(parent, charset) {
    let menuitem = parent.getElementsByAttribute("charset", this.foldCharset(charset)).item(0);
    if (menuitem) {
      menuitem.setAttribute("checked", "true");
    }
  },
};

Object.freeze(CharsetMenu);

