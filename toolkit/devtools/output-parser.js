/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {Cc, Ci, Cu} = require("chrome");
const {colorUtils} = require("devtools/css-color");
const {Services} = Cu.import("resource://gre/modules/Services.jsm", {});

const HTML_NS = "http://www.w3.org/1999/xhtml";

const MAX_ITERATIONS = 100;
const REGEX_QUOTES = /^".*?"|^".*/;
const REGEX_URL = /^url\(["']?(.+?)(?::(\d+))?["']?\)/;
const REGEX_WHITESPACE = /^\s+/;
const REGEX_FIRST_WORD_OR_CHAR = /^\w+|^./;
const REGEX_CSS_PROPERTY_VALUE = /(^[^;]+)/;

/**
 * This regex matches:
 *  - #F00
 *  - #FF0000
 *  - hsl()
 *  - hsla()
 *  - rgb()
 *  - rgba()
 *  - color names
 */
const REGEX_ALL_COLORS = /^#[0-9a-fA-F]{3}\b|^#[0-9a-fA-F]{6}\b|^hsl\(.*?\)|^hsla\(.*?\)|^rgba?\(.*?\)|^[a-zA-Z-]+/;

loader.lazyGetter(this, "DOMUtils", function () {
  return Cc["@mozilla.org/inspector/dom-utils;1"].getService(Ci.inIDOMUtils);
});

/**
 * This regular expression catches all css property names with their trailing
 * spaces and semicolon. This is used to ensure a value is valid for a property
 * name within style="" attributes.
 */
loader.lazyGetter(this, "REGEX_ALL_CSS_PROPERTIES", function () {
  let names = DOMUtils.getCSSPropertyNames();
    let pattern = "^(";

    for (let i = 0; i < names.length; i++) {
      if (i > 0) {
        pattern += "|";
      }
      pattern += names[i];
    }
    pattern += ")\\s*:\\s*";

    return new RegExp(pattern);
});

/**
 * This module is used to process text for output by developer tools. This means
 * linking JS files with the debugger, CSS files with the style editor, JS
 * functions with the debugger, placing color swatches next to colors and
 * adding doorhanger previews where possible (images, angles, lengths,
 * border radius, cubic-bezier etc.).
 *
 * Usage:
 *   const {devtools} = Cu.import("resource://gre/modules/devtools/Loader.jsm", {});
 *   const {OutputParser} = devtools.require("devtools/output-parser");
 *
 *   let parser = new OutputParser();
 *
 *   parser.parseCssProperty("color", "red"); // Returns document fragment.
 *   parser.parseHTMLAttribute("color:red; font-size: 12px;"); // Returns document
 *                                                             // fragment.
 */
function OutputParser() {
  this.parsed = [];
}

exports.OutputParser = OutputParser;

OutputParser.prototype = {
  /**
   * Parse a CSS property value given a property name.
   *
   * @param  {String} name
   *         CSS Property Name
   * @param  {String} value
   *         CSS Property value
   * @param  {Object} [options]
   *         Options object. For valid options and default values see
   *         _mergeOptions().
   * @return {DocumentFragment}
   *         A document fragment containing color swatches etc.
   */
  parseCssProperty: function(name, value, options={}) {
    options = this._mergeOptions(options);

    if (this._cssPropertySupportsValue(name, value)) {
      return this._parse(value, options);
    }
    this._appendTextNode(value);

    return this._toDOM();
  },

  /**
   * Parse a string.
   *
   * @param  {String} value
   *         Text to parse.
   * @param  {Object} [options]
   *         Options object. For valid options and default values see
   *         _mergeOptions().
   * @return {DocumentFragment}
   *         A document fragment. Colors will not be parsed.
   */
  parseHTMLAttribute: function(value, options={}) {
    options = this._mergeOptions(options);

    return this._parse(value, options);
  },

  /**
   * Parse a string.
   *
   * @param  {String} text
   *         Text to parse.
   * @param  {Object} [options]
   *         Options object. For valid options and default values see
   *         _mergeOptions().
   * @return {DocumentFragment}
   *         A document fragment.
   */
  _parse: function(text, options={}) {
    text = text.trim();
    this.parsed.length = 0;
    let dirty = false;
    let matched = null;
    let i = 0;

    let trimMatchFromStart = function(match) {
      text = text.substr(match.length);
      dirty = true;
      matched = null;
    };

    while (text.length > 0) {
      matched = text.match(REGEX_QUOTES);
      if (matched) {
        let match = matched[0];
        trimMatchFromStart(match);
        this._appendTextNode(match);
      }

      matched = text.match(REGEX_WHITESPACE);
      if (matched) {
        let match = matched[0];
        trimMatchFromStart(match);
        this._appendTextNode(match);
      }

      matched = text.match(REGEX_URL);
      if (matched) {
        let [match, url] = matched;
        trimMatchFromStart(match);
        this._appendURL(match, url, options);
      }

      matched = text.match(REGEX_ALL_CSS_PROPERTIES);
      if (matched) {
        let [match] = matched;
        trimMatchFromStart(match);
        this._appendTextNode(match);

        dirty = true;
      }

      matched = text.match(REGEX_ALL_COLORS);
      if (matched) {
        let match = matched[0];
        if (this._appendColor(match, options)) {
          trimMatchFromStart(match);
        }
      }

      if (!dirty) {
        // This test must always be last as it indicates use of an unknown
        // character that needs to be removed to prevent infinite loops.
        matched = text.match(REGEX_FIRST_WORD_OR_CHAR);
        if (matched) {
          let match = matched[0];
          trimMatchFromStart(match);
          this._appendTextNode(match);
        }
      }

      dirty = false;

      // Prevent this loop from slowing down the browser with too
      // many nodes being appended into output.
      i++;
      if (i > MAX_ITERATIONS) {
        trimMatchFromStart(text);
        this._appendTextNode(text);
      }
    }

    return this._toDOM();
  },

  /**
   * Check if a CSS property supports a specific value.
   *
   * @param  {String} name
   *         CSS Property name to check
   * @param  {String} value
   *         CSS Property value to check
   */
  _cssPropertySupportsValue: function(name, value) {
    let win = Services.appShell.hiddenDOMWindow;
    let doc = win.document;

    name = name.replace(/-\w{1}/g, function(match) {
      return match.charAt(1).toUpperCase();
    });

    let div = doc.createElement("div");
    div.style[name] = value;

    return !!div.style[name];
  },

  /**
   * Append a color to the output.
   *
   * @param  {String} color
   *         Color to append
   * @param  {Object} [options]
   *         Options object. For valid options and default values see
   *         _mergeOptions().
   * @returns {Boolean}
   *          true if the color passed in was valid, false otherwise. Special
   *          values such as transparent also return false.
   */
  _appendColor: function(color, options={}) {
    let colorObj = new colorUtils.CssColor(color);

    if (colorObj.valid && !colorObj.specialValue) {
      if (options.colorSwatchClass) {
        this._appendNode("span", {
          class: options.colorSwatchClass,
          style: "background-color:" + color
        });
      }
      if (options.defaultColorType) {
        color = colorObj.toString();
      }
      this._appendTextNode(color);
      return true;
    }
    return false;
  },

   /**
    * Append a URL to the output.
    *
    * @param  {String} match
    *         Complete match that may include "url(xxx)"
    * @param  {String} url
    *         Actual URL
    * @param  {Object} [options]
    *         Options object. For valid options and default values see
    *         _mergeOptions().
    */
  _appendURL: function(match, url, options={}) {
    if (options.urlClass) {
      // We use single quotes as this works inside html attributes (e.g. the
      // markup view).
      this._appendTextNode("url('");

      let href = url;
      if (options.baseURI) {
        href = options.baseURI.resolve(url);
      }

      this._appendNode("a",  {
        target: "_blank",
        class: options.urlClass,
        href: href
      }, url);

      this._appendTextNode("')");
    } else {
      this._appendTextNode("url('" + url + "')");
    }
  },

  /**
   * Append a node to the output.
   *
   * @param  {String} tagName
   *         Tag type e.g. "div"
   * @param  {Object} attributes
   *         e.g. {class: "someClass", style: "cursor:pointer"};
   * @param  {String} [value]
   *         If a value is included it will be appended as a text node inside
   *         the tag. This is useful e.g. for span tags.
   */
  _appendNode: function(tagName, attributes, value="") {
    let win = Services.appShell.hiddenDOMWindow;
    let doc = win.document;
    let node = doc.createElementNS(HTML_NS, tagName);
    let attrs = Object.getOwnPropertyNames(attributes);

    for (let attr of attrs) {
      node.setAttribute(attr, attributes[attr]);
    }

    if (value) {
      let textNode = doc.createTextNode(value);
      node.appendChild(textNode);
    }

    this.parsed.push(node);
  },

  /**
   * Append a text node to the output. If the previously output item was a text
   * node then we append the text to that node.
   *
   * @param  {String} text
   *         Text to append
   */
  _appendTextNode: function(text) {
    let lastItem = this.parsed[this.parsed.length - 1];
    if (typeof lastItem === "string") {
      this.parsed[this.parsed.length - 1] = lastItem + text
    } else {
      this.parsed.push(text);
    }
  },

  /**
   * Take all output and append it into a single DocumentFragment.
   *
   * @return {DocumentFragment}
   *         Document Fragment
   */
  _toDOM: function() {
    let win = Services.appShell.hiddenDOMWindow;
    let doc = win.document;
    let frag = doc.createDocumentFragment();

    for (let item of this.parsed) {
      if (typeof item === "string") {
        frag.appendChild(doc.createTextNode(item));
      } else {
        frag.appendChild(item);
      }
    }

    this.parsed.length = 0;
    return frag;
  },

  /**
   * Merges options objects. Default values are set here.
   *
   * @param  {Object} overrides
   *         The option values to override e.g. _mergeOptions({colors: false})
   *
   *         Valid options are:
   *           - defaultColorType: true // Convert colors to the default type
   *                                    // selected in the options panel.
   *           - colorSwatchClass: ""   // The class to use for color swatches.
   *           - urlClass: ""           // The class to be used for url() links.
   *           - baseURI: ""            // A string or nsIURI used to resolve
   *                                    // relative links.
   * @return {Object}
   *         Overridden options object
   */
  _mergeOptions: function(overrides) {
    let defaults = {
      defaultColorType: true,
      colorSwatchClass: "",
      urlClass: "",
      baseURI: ""
    };

    if (typeof overrides.baseURI === "string") {
      overrides.baseURI = Services.io.newURI(overrides.baseURI, null, null);
    }

    for (let item in overrides) {
      defaults[item] = overrides[item];
    }
    return defaults;
  },
};
