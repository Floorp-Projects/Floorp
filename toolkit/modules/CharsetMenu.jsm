/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = ["CharsetMenu"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
XPCOMUtils.defineLazyGetter(this, "gBundle", function() {
  const kUrl = "chrome://global/locale/charsetMenu.properties";
  return Services.strings.createBundle(kUrl);
});

ChromeUtils.defineModuleGetter(
  this,
  "Deprecated",
  "resource://gre/modules/Deprecated.jsm"
);

/**
 * This set contains encodings that are in the Encoding Standard, except:
 *  - Japanese encodings are represented by one autodetection item
 *  - x-user-defined, which practically never makes sense as an end-user-chosen
 *    override.
 *  - Encodings that IE11 doesn't have in its corresponding menu.
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
  "GBK",
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
  // Japanese (NOT AN ENCODING NAME)
  "Japanese",
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
const kPinned = ["UTF-8", "windows-1252"];

kPinned.forEach(x => kEncodings.delete(x));

function CharsetComparator(a, b) {
  // Normal sorting sorts the part in parenthesis in an order that
  // happens to make the less frequently-used items first.
  let titleA = a.label.replace(/\(.*/, "") + b.value;
  let titleB = b.label.replace(/\(.*/, "") + a.value;
  // Secondarily reverse sort by encoding name to sort "windows"
  return titleA.localeCompare(titleB) || b.value.localeCompare(a.value);
}

var gCharsetInfoCache, gPinnedInfoCache;

var CharsetMenu = {
  build(parent, deprecatedShowAccessKeys = true) {
    if (!deprecatedShowAccessKeys) {
      Deprecated.warning(
        "CharsetMenu no longer supports building a menu with no access keys.",
        "https://bugzilla.mozilla.org/show_bug.cgi?id=1088710"
      );
    }
    function createDOMNode(doc, nodeInfo) {
      let node = doc.createXULElement("menuitem");
      node.setAttribute("type", "radio");
      node.setAttribute("name", nodeInfo.name + "Group");
      node.setAttribute(nodeInfo.name, nodeInfo.value);
      node.setAttribute("label", nodeInfo.label);
      if (nodeInfo.accesskey) {
        node.setAttribute("accesskey", nodeInfo.accesskey);
      }
      return node;
    }

    if (parent.hasChildNodes()) {
      // Charset menu already built
      return;
    }
    this._ensureDataReady();
    let doc = parent.ownerDocument;

    gPinnedInfoCache.forEach(charsetInfo =>
      parent.appendChild(createDOMNode(doc, charsetInfo))
    );
    parent.appendChild(doc.createXULElement("menuseparator"));
    gCharsetInfoCache.forEach(charsetInfo =>
      parent.appendChild(createDOMNode(doc, charsetInfo))
    );
  },

  getData() {
    this._ensureDataReady();
    return {
      pinnedCharsets: gPinnedInfoCache,
      otherCharsets: gCharsetInfoCache,
    };
  },

  _ensureDataReady() {
    if (!gCharsetInfoCache) {
      gPinnedInfoCache = this.getCharsetInfo(kPinned, false);
      gCharsetInfoCache = this.getCharsetInfo(kEncodings);
    }
  },

  getCharsetInfo(charsets, sort = true) {
    let list = Array.from(charsets, charset => ({
      label: this._getCharsetLabel(charset),
      accesskey: this._getCharsetAccessKey(charset),
      name: "charset",
      value: charset,
    }));

    if (sort) {
      list.sort(CharsetComparator);
    }
    return list;
  },

  _getCharsetLabel(charset) {
    if (charset == "GBK") {
      // Localization key has been revised
      charset = "gbk.bis";
    }
    try {
      return gBundle.GetStringFromName(charset);
    } catch (ex) {}
    return charset;
  },
  _getCharsetAccessKey(charset) {
    if (charset == "GBK") {
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
  foldCharset(charset, isAutodetected) {
    if (isAutodetected) {
      switch (charset) {
        case "Shift_JIS":
        case "EUC-JP":
        case "ISO-2022-JP":
          return "Japanese";
        default:
        // fall through
      }
    }
    switch (charset) {
      case "ISO-8859-8-I":
        return "windows-1255";

      case "gb18030":
        return "GBK";

      default:
        return charset;
    }
  },

  /**
   * This method is for comm-central callers only.
   */
  update(parent, charset) {
    let menuitem = parent
      .getElementsByAttribute("charset", this.foldCharset(charset, false))
      .item(0);
    if (menuitem) {
      menuitem.setAttribute("checked", "true");
    }
  },
};

Object.freeze(CharsetMenu);
