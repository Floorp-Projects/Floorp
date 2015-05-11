/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {Cc, Ci, Cu} = require("chrome");
const {colorUtils} = require("devtools/css-color");
const {Services} = Cu.import("resource://gre/modules/Services.jsm", {});

const HTML_NS = "http://www.w3.org/1999/xhtml";

const MAX_ITERATIONS = 100;
const REGEX_QUOTES = /^".*?"|^".*|^'.*?'|^'.*/;
const REGEX_WHITESPACE = /^\s+/;
const REGEX_FIRST_WORD_OR_CHAR = /^\w+|^./;
const REGEX_CUBIC_BEZIER = /^linear|^ease-in-out|^ease-in|^ease-out|^ease|^cubic-bezier\(([0-9.\- ]+,){3}[0-9.\- ]+\)/;

// CSS variable names are identifiers which the spec defines as follows:
//   In CSS, identifiers (including element names, classes, and IDs in
//   selectors) can contain only the characters [a-zA-Z0-9] and ISO 10646
//   characters U+00A0 and higher, plus the hyphen (-) and the underscore (_).
const REGEX_CSS_VAR = /^\bvar\(\s*--[-_a-zA-Z0-9\u00A0-\u10FFFF]+\s*\)/;

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
 */
function OutputParser() {
  this.parsed = [];
  this.colorSwatches = new WeakMap();
  this._onSwatchMouseDown = this._onSwatchMouseDown.bind(this);
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

    options.expectCubicBezier =
      safeCssPropertySupportsType(name, DOMUtils.TYPE_TIMING_FUNCTION);
    options.expectFilter = name === "filter";
    options.supportsColor =
      safeCssPropertySupportsType(name, DOMUtils.TYPE_COLOR) ||
      safeCssPropertySupportsType(name, DOMUtils.TYPE_GRADIENT);

    if (this._cssPropertySupportsValue(name, value)) {
      return this._parse(value, options);
    }
    this._appendTextNode(value);

    return this._toDOM();
  },

  /**
   * Matches the beginning of the provided string to a css background-image url
   * and return both the whole url(...) match and the url itself.
   * This isn't handled via a regular expression to make sure we can match urls
   * that contain parenthesis easily
   */
  _matchBackgroundUrl: function(text) {
    let startToken = "url(";
    if (text.indexOf(startToken) !== 0) {
      return null;
    }

    let uri = text.substring(startToken.length).trim();
    let quote = uri.substring(0, 1);
    if (quote === "'" || quote === '"') {
      uri = uri.substring(1, uri.search(new RegExp(quote + "\\s*\\)")));
    } else {
      uri = uri.substring(0, uri.indexOf(")"));
      quote = "";
    }
    let end = startToken + quote + uri;
    text = text.substring(0, text.indexOf(")", end.length) + 1);

    return [text, uri.trim()];
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
    let i = 0;

    while (text.length > 0) {
      let matched = null;

      // Prevent this loop from slowing down the browser with too
      // many nodes being appended into output. In practice it is very unlikely
      // that this will ever happen.
      i++;
      if (i > MAX_ITERATIONS) {
        this._appendTextNode(text);
        text = "";
        break;
      }

      if (options.expectFilter) {
        this._appendFilter(text, options);
        break;
      }

      matched = text.match(REGEX_QUOTES);
      if (matched) {
        let match = matched[0];

        text = this._trimMatchFromStart(text, match);
        this._appendTextNode(match);
        continue;
      }

      matched = text.match(REGEX_CSS_VAR);
      if (matched) {
        let match = matched[0];

        text = this._trimMatchFromStart(text, match);
        this._appendTextNode(match);
        continue;
      }

      matched = text.match(REGEX_WHITESPACE);
      if (matched) {
        let match = matched[0];

        text = this._trimMatchFromStart(text, match);
        this._appendTextNode(match);
        continue;
      }

      matched = this._matchBackgroundUrl(text);
      if (matched) {
        let [match, url] = matched;
        text = this._trimMatchFromStart(text, match);

        this._appendURL(match, url, options);
        continue;
      }

      if (options.expectCubicBezier) {
        matched = text.match(REGEX_CUBIC_BEZIER);
        if (matched) {
          let match = matched[0];
          text = this._trimMatchFromStart(text, match);

          this._appendCubicBezier(match, options);
          continue;
        }
      }

      if (options.supportsColor) {
        let dirty;

        [text, dirty] = this._appendColorOnMatch(text, options);

        if (dirty) {
          continue;
        }
      }

      // This test must always be last as it indicates use of an unknown
      // character that needs to be removed to prevent infinite loops.
      matched = text.match(REGEX_FIRST_WORD_OR_CHAR);
      if (matched) {
        let match = matched[0];

        text = this._trimMatchFromStart(text, match);
        this._appendTextNode(match);
      }
    }

    return this._toDOM();
  },

  /**
   * Convenience function to make the parser a little more readable.
   *
   * @param  {String} text
   *         Main text
   * @param  {String} match
   *         Text to remove from the beginning
   *
   * @return {String}
   *         The string passed as 'text' with 'match' stripped from the start.
   */
  _trimMatchFromStart: function(text, match) {
    return text.substr(match.length);
  },

  /**
   * Check if there is a color match and append it if it is valid.
   *
   * @param  {String} text
   *         Main text
   * @param  {Object} options
   *         Options object. For valid options and default values see
   *         _mergeOptions().
   *
   * @return {Array}
   *         An array containing the remaining text and a dirty flag. This array
   *         is designed for deconstruction using [text, dirty].
   */
  _appendColorOnMatch: function(text, options) {
    let dirty;
    let matched = text.match(REGEX_ALL_COLORS);

    if (matched) {
      let match = matched[0];
      if (this._appendColor(match, options)) {
        text = this._trimMatchFromStart(text, match);
        dirty = true;
      }
    } else {
      dirty = false;
    }

    return [text, dirty];
  },

  /**
   * Append a cubic-bezier timing function value to the output
   *
   * @param {String} bezier
   *        The cubic-bezier timing function
   * @param {Object} options
   *        Options object. For valid options and default values see
   *        _mergeOptions()
   */
  _appendCubicBezier: function(bezier, options) {
    let container = this._createNode("span", {
       "data-bezier": bezier
    });

    if (options.bezierSwatchClass) {
      let swatch = this._createNode("span", {
        class: options.bezierSwatchClass
      });
      container.appendChild(swatch);
    }

    let value = this._createNode("span", {
      class: options.bezierClass
    }, bezier);

    container.appendChild(value);
    this.parsed.push(container);
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
    return DOMUtils.cssPropertyIsValid(name, value);
  },

  /**
   * Tests if a given colorObject output by CssColor is valid for parsing.
   * Valid means it's really a color, not any of the CssColor SPECIAL_VALUES
   * except transparent
   */
  _isValidColor: function(colorObj) {
    return colorObj.valid &&
      (!colorObj.specialValue || colorObj.specialValue === "transparent");
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

    if (this._isValidColor(colorObj)) {
      let container = this._createNode("span", {
         "data-color": color
      });

      if (options.colorSwatchClass) {
        let swatch = this._createNode("span", {
          class: options.colorSwatchClass,
          style: "background-color:" + color
        });
        this.colorSwatches.set(swatch, colorObj);
        swatch.addEventListener("mousedown", this._onSwatchMouseDown, false);
        container.appendChild(swatch);
      }

      if (options.defaultColorType) {
        color = colorObj.toString();
        container.dataset.color = color;
      }

      let value = this._createNode("span", {
        class: options.colorClass
      }, color);

      container.appendChild(value);
      this.parsed.push(container);
      return true;
    }
    return false;
  },

  _appendFilter: function(filters, options={}) {
    let container = this._createNode("span", {
      "data-filters": filters
    });

    if (options.filterSwatchClass) {
      let swatch = this._createNode("span", {
        class: options.filterSwatchClass
      });
      container.appendChild(swatch);
    }

    let value = this._createNode("span", {
      class: options.filterClass
    }, filters);

    container.appendChild(value);
    this.parsed.push(container);
  },

  _onSwatchMouseDown: function(event) {
    // Prevent text selection in the case of shift-click or double-click.
    event.preventDefault();

    if (!event.shiftKey) {
      return;
    }

    let swatch = event.target;
    let color = this.colorSwatches.get(swatch);
    let val = color.nextColorUnit();

    swatch.nextElementSibling.textContent = val;
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
      this._appendTextNode("url(\"");

      let href = url;
      if (options.baseURI) {
        href = options.baseURI.resolve(url);
      }

      this._appendNode("a",  {
        target: "_blank",
        class: options.urlClass,
        href: href
      }, url);

      this._appendTextNode("\")");
    } else {
      this._appendTextNode("url(\"" + url + "\")");
    }
  },

  /**
   * Create a node.
   *
   * @param  {String} tagName
   *         Tag type e.g. "div"
   * @param  {Object} attributes
   *         e.g. {class: "someClass", style: "cursor:pointer"};
   * @param  {String} [value]
   *         If a value is included it will be appended as a text node inside
   *         the tag. This is useful e.g. for span tags.
   * @return {Node} Newly created Node.
   */
  _createNode: function(tagName, attributes, value="") {
    let win = Services.appShell.hiddenDOMWindow;
    let doc = win.document;
    let node = doc.createElementNS(HTML_NS, tagName);
    let attrs = Object.getOwnPropertyNames(attributes);

    for (let attr of attrs) {
      if (attributes[attr]) {
        node.setAttribute(attr, attributes[attr]);
      }
    }

    if (value) {
      let textNode = doc.createTextNode(value);
      node.appendChild(textNode);
    }

    return node;
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
    let node = this._createNode(tagName, attributes, value);
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
      this.parsed[this.parsed.length - 1] = lastItem + text;
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
   *           - colorClass: ""         // The class to use for the color value
   *                                    // that follows the swatch.
   *           - bezierSwatchClass: ""  // The class to use for bezier swatches.
   *           - bezierClass: ""        // The class to use for the bezier value
   *                                    // that follows the swatch.
   *           - supportsColor: false   // Does the CSS property support colors?
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
      colorClass: "",
      bezierSwatchClass: "",
      bezierClass: "",
      supportsColor: false,
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
  }
};

/**
 * A wrapper for DOMUtils.cssPropertySupportsType that ignores invalid
 * properties.
 *
 * @param {String} name The property name.
 * @param {number} type The type tested for support.
 * @return {Boolean} Whether the property supports the type.
 *        If the property is unknown, false is returned.
 */
function safeCssPropertySupportsType(name, type) {
  try {
    return DOMUtils.cssPropertySupportsType(name, type);
  } catch(e) {
    return false;
  }
}
