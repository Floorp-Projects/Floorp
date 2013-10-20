/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {Cc, Ci, Cu} = require("chrome");
const {colorUtils} = require("devtools/css-color");
const {Services} = Cu.import("resource://gre/modules/Services.jsm", {});

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
    options.cssPropertyName = name;

    // Detect if "name" supports colors by checking if "papayawhip" is a valid
    // value.
    options.colors = this._cssPropertySupportsValue(name, "papayawhip");

    return this._parse(value, options);
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
   *         A document fragment containing events etc. Colors will not be
   *         parsed.
   */
  parseHTMLAttribute: function(value, options={}) {
    options = this._mergeOptions(options);
    options.colors = false;

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
   *         A document fragment containing events etc. Colors will not be
   *         parsed.
   */
  _parse: function(text, options={}) {
    text = text.trim();
    this.parsed.length = 0;
    let dirty = false;
    let matched = null;
    let nameValueSupported = false;

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
        let [match, url, line] = matched;
        trimMatchFromStart(match);
        this._appendURL(match, url, line, options);
      }

      // This block checks for valid name and value combinations setting
      // nameValueSupported to true as appropriate.
      matched = text.match(REGEX_ALL_CSS_PROPERTIES);
      if (matched) {
        let [match, propertyName] = matched;
        trimMatchFromStart(match);
        this._appendTextNode(match);

        matched = text.match(REGEX_CSS_PROPERTY_VALUE);
        if (matched) {
          let [, value] = matched;
          nameValueSupported = this._cssPropertySupportsValue(propertyName, value);
        }
      }

      // This block should only be used for CSS properties.
      // options.cssPropertyName is only set if the parse call comes from a CSS
      // tool containing either a name and value or a string with valid name and
      // value combinations.
      if (options.cssPropertyName || (!options.cssPropertyName && nameValueSupported)) {
        matched = text.match(REGEX_ALL_COLORS);
        if (matched) {
          let match = matched[0];
          trimMatchFromStart(match);
          this._appendColor(match, options);
        }

        nameValueSupported = false;
      }

      if (!dirty) {
        // This test must always be last as it indicates use of an unknown
        // character that needs to be removed to prevent infinite loops.
        matched = text.match(REGEX_FIRST_WORD_OR_CHAR);
        if (matched) {
          let match = matched[0];
          trimMatchFromStart(match);
          this._appendTextNode(match);
          nameValueSupported = false;
        }
      }

      dirty = false;
    }

    return this._toDOM();
  },

  /**
   * Check if a CSS property supports a specific value.
   *
   * @param  {String} propertyName
   *         CSS Property name to check
   * @param  {String} propertyValue
   *         CSS Property value to check
   */
  _cssPropertySupportsValue: function(propertyName, propertyValue) {
    let autoCompleteValues = DOMUtils.getCSSValuesForProperty(propertyName);

    // Detect if propertyName supports colors by checking if papayawhip is a
    // valid value.
    if (autoCompleteValues.indexOf("papayawhip") !== -1) {
      return this._isColorValid(propertyValue);
    }

    // For the rest we can trust autocomplete value matches.
    return autoCompleteValues.indexOf(propertyValue) !== -1;
  },

  /**
   * Append a color to the output.
   *
   * @param  {String} color
   *         Color to append
   * @param  {Object} [options]
   *         Options object. For valid options and default values see
   *         _mergeOptions().
   */
  _appendColor: function(color, options={}) {
    if (options.colors && this._isColorValid(color)) {
      if (options.colorSwatchClass) {
        this._appendNode("span", {
          class: options.colorSwatchClass,
          style: "background-color:" + color
        });
      }
      if (options.defaultColorType) {
        color = new colorUtils.CssColor(color).toString();
      }
    }
    this._appendTextNode(color);
  },

  /**
   * Append a URL to the output.
   *
   * @param  {String} match
   *         Complete match that may include "url(xxx)""
   * @param  {String} url
   *         Actual URL
   * @param  {Number} line
   *         Line number from URL e.g. http://blah:42
   * @param  {Object} [options]
   *         Options object. For valid options and default values see
   *         _mergeOptions().
   */
  _appendURL: function(match, url, line, options={}) {
    this._appendTextNode(match);
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
    let node = doc.createElement(tagName);
    let attrs = Object.getOwnPropertyNames(attributes);

    for (let attr of attrs) {
      node.setAttribute(attr, attributes[attr]);
    }

    if (value) {
      let textNode = content.document.createTextNode(value);
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

    if (typeof lastItem !== "undefined" && lastItem.nodeName === "#text") {
      lastItem.nodeValue += text;
    } else {
      let win = Services.appShell.hiddenDOMWindow;
      let doc = win.document;
      let textNode = doc.createTextNode(text);
      this.parsed.push(textNode);
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
      frag.appendChild(item);
    }

    this.parsed.length = 0;
    return frag;
  },

  /**
   * Check that a string represents a valid volor.
   *
   * @param  {String} color
   *         Color to check
   */
  _isColorValid: function(color) {
    return new colorUtils.CssColor(color).valid;
  },

  /**
   * Merges options objects. Default values are set here.
   *
   * @param  {Object} overrides
   *         The option values to override e.g. _mergeOptions({colors: false})
   *
   *         Valid options are:
   *           - colors: true           // Allow processing of colors
   *           - defaultColorType: true // Convert colors to the default type
   *                                    // selected in the options panel.
   *           - colorSwatchClass: ""   // The class to use for color swatches.
   *           - cssPropertyName: ""    // Used by CSS tools. Passing in the
   *                                    // property name allows appropriate
   *                                    // processing of the property value.
   * @return {Object}
   *         Overridden options object
   */
  _mergeOptions: function(overrides) {
    let defaults = {
      colors: true,
      defaultColorType: true,
      colorSwatchClass: "",
      cssPropertyName: ""
    };

    for (let item in overrides) {
      defaults[item] = overrides[item];
    }
    return defaults;
  },
};
