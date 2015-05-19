/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * About the objects defined in this file:
 * - CssLogic contains style information about a view context. It provides
 *   access to 2 sets of objects: Css[Sheet|Rule|Selector] provide access to
 *   information that does not change when the selected element changes while
 *   Css[Property|Selector]Info provide information that is dependent on the
 *   selected element.
 *   Its key methods are highlight(), getPropertyInfo() and forEachSheet(), etc
 *   It also contains a number of static methods for l10n, naming, etc
 *
 * - CssSheet provides a more useful API to a DOM CSSSheet for our purposes,
 *   including shortSource and href.
 * - CssRule a more useful API to a nsIDOMCSSRule including access to the group
 *   of CssSelectors that the rule provides properties for
 * - CssSelector A single selector - i.e. not a selector group. In other words
 *   a CssSelector does not contain ','. This terminology is different from the
 *   standard DOM API, but more inline with the definition in the spec.
 *
 * - CssPropertyInfo contains style information for a single property for the
 *   highlighted element.
 * - CssSelectorInfo is a wrapper around CssSelector, which adds sorting with
 *   reference to the selected element.
 */

/**
 * Provide access to the style information in a page.
 * CssLogic uses the standard DOM API, and the Gecko inIDOMUtils API to access
 * styling information in the page, and present this to the user in a way that
 * helps them understand:
 * - why their expectations may not have been fulfilled
 * - how browsers process CSS
 * @constructor
 */

const { Cc, Ci, Cu } = require("chrome");
const Services = require("Services");
const DevToolsUtils = require("devtools/toolkit/DevToolsUtils");

// This should be ok because none of the functions that use this should be used
// on the worker thread, where Cu is not available.
if (Cu) {
  Cu.importGlobalProperties(['CSS']);
  Cu.import("resource://gre/modules/devtools/LayoutHelpers.jsm");
}

function CssLogic()
{
  // The cache of examined CSS properties.
  _propertyInfos: {};
}

exports.CssLogic = CssLogic;

/**
 * Special values for filter, in addition to an href these values can be used
 */
CssLogic.FILTER = {
  USER: "user", // show properties for all user style sheets.
  UA: "ua",    // USER, plus user-agent (i.e. browser) style sheets
};

/**
 * Known media values. To distinguish "all" stylesheets (above) from "all" media
 * The full list includes braille, embossed, handheld, print, projection,
 * speech, tty, and tv, but this is only a hack because these are not defined
 * in the DOM at all.
 * @see http://www.w3.org/TR/CSS21/media.html#media-types
 */
CssLogic.MEDIA = {
  ALL: "all",
  SCREEN: "screen",
};

/**
 * Each rule has a status, the bigger the number, the better placed it is to
 * provide styling information.
 *
 * These statuses are localized inside the styleinspector.properties string bundle.
 * @see csshtmltree.js RuleView._cacheStatusNames()
 */
CssLogic.STATUS = {
  BEST: 3,
  MATCHED: 2,
  PARENT_MATCH: 1,
  UNMATCHED: 0,
  UNKNOWN: -1,
};

CssLogic.prototype = {
  // Both setup by highlight().
  viewedElement: null,
  viewedDocument: null,

  // The cache of the known sheets.
  _sheets: null,

  // Have the sheets been cached?
  _sheetsCached: false,

  // The total number of rules, in all stylesheets, after filtering.
  _ruleCount: 0,

  // The computed styles for the viewedElement.
  _computedStyle: null,

  // Source filter. Only display properties coming from the given source
  _sourceFilter: CssLogic.FILTER.USER,

  // Used for tracking unique CssSheet/CssRule/CssSelector objects, in a run of
  // processMatchedSelectors().
  _passId: 0,

  // Used for tracking matched CssSelector objects.
  _matchId: 0,

  _matchedRules: null,
  _matchedSelectors: null,

  // Cached keyframes rules in all stylesheets
  _keyframesRules: null,

  /**
   * Reset various properties
   */
  reset: function CssLogic_reset()
  {
    this._propertyInfos = {};
    this._ruleCount = 0;
    this._sheetIndex = 0;
    this._sheets = {};
    this._sheetsCached = false;
    this._matchedRules = null;
    this._matchedSelectors = null;
    this._keyframesRules = [];
  },

  /**
   * Focus on a new element - remove the style caches.
   *
   * @param {nsIDOMElement} aViewedElement the element the user has highlighted
   * in the Inspector.
   */
  highlight: function CssLogic_highlight(aViewedElement)
  {
    if (!aViewedElement) {
      this.viewedElement = null;
      this.viewedDocument = null;
      this._computedStyle = null;
      this.reset();
      return;
    }

    if (aViewedElement === this.viewedElement) {
      return;
    }

    this.viewedElement = aViewedElement;

    let doc = this.viewedElement.ownerDocument;
    if (doc != this.viewedDocument) {
      // New document: clear/rebuild the cache.
      this.viewedDocument = doc;

      // Hunt down top level stylesheets, and cache them.
      this._cacheSheets();
    } else {
      // Clear cached data in the CssPropertyInfo objects.
      this._propertyInfos = {};
    }

    this._matchedRules = null;
    this._matchedSelectors = null;
    this._computedStyle = CssLogic.getComputedStyle(this.viewedElement);
  },

  /**
   * Get the values of all the computed CSS properties for the highlighted
   * element.
   * @returns {object} The computed CSS properties for a selected element
   */
  get computedStyle() {
    return this._computedStyle;
  },

  /**
   * Get the source filter.
   * @returns {string} The source filter being used.
   */
  get sourceFilter() {
    return this._sourceFilter;
  },

  /**
   * Source filter. Only display properties coming from the given source (web
   * address). Note that in order to avoid information overload we DO NOT show
   * unmatched system rules.
   * @see CssLogic.FILTER.*
   */
  set sourceFilter(aValue) {
    let oldValue = this._sourceFilter;
    this._sourceFilter = aValue;

    let ruleCount = 0;

    // Update the CssSheet objects.
    this.forEachSheet(function(aSheet) {
      aSheet._sheetAllowed = -1;
      if (aSheet.contentSheet && aSheet.sheetAllowed) {
        ruleCount += aSheet.ruleCount;
      }
    }, this);

    this._ruleCount = ruleCount;

    // Full update is needed because the this.processMatchedSelectors() method
    // skips UA stylesheets if the filter does not allow such sheets.
    let needFullUpdate = (oldValue == CssLogic.FILTER.UA ||
        aValue == CssLogic.FILTER.UA);

    if (needFullUpdate) {
      this._matchedRules = null;
      this._matchedSelectors = null;
      this._propertyInfos = {};
    } else {
      // Update the CssPropertyInfo objects.
      for each (let propertyInfo in this._propertyInfos) {
        propertyInfo.needRefilter = true;
      }
    }
  },

  /**
   * Return a CssPropertyInfo data structure for the currently viewed element
   * and the specified CSS property. If there is no currently viewed element we
   * return an empty object.
   *
   * @param {string} aProperty The CSS property to look for.
   * @return {CssPropertyInfo} a CssPropertyInfo structure for the given
   * property.
   */
  getPropertyInfo: function CssLogic_getPropertyInfo(aProperty)
  {
    if (!this.viewedElement) {
      return {};
    }

    let info = this._propertyInfos[aProperty];
    if (!info) {
      info = new CssPropertyInfo(this, aProperty);
      this._propertyInfos[aProperty] = info;
    }

    return info;
  },

  /**
   * Cache all the stylesheets in the inspected document
   * @private
   */
  _cacheSheets: function CssLogic_cacheSheets()
  {
    this._passId++;
    this.reset();

    // styleSheets isn't an array, but forEach can work on it anyway
    Array.prototype.forEach.call(this.viewedDocument.styleSheets,
        this._cacheSheet, this);

    this._sheetsCached = true;
  },

  /**
   * Cache a stylesheet if it falls within the requirements: if it's enabled,
   * and if the @media is allowed. This method also walks through the stylesheet
   * cssRules to find @imported rules, to cache the stylesheets of those rules
   * as well. In addition, the @keyframes rules in the stylesheet are cached.
   *
   * @private
   * @param {CSSStyleSheet} aDomSheet the CSSStyleSheet object to cache.
   */
  _cacheSheet: function CssLogic_cacheSheet(aDomSheet)
  {
    if (aDomSheet.disabled) {
      return;
    }

    // Only work with stylesheets that have their media allowed.
    if (!this.mediaMatches(aDomSheet)) {
      return;
    }

    // Cache the sheet.
    let cssSheet = this.getSheet(aDomSheet, this._sheetIndex++);
    if (cssSheet._passId != this._passId) {
      cssSheet._passId = this._passId;

      // Find import and keyframes rules.
      for (let aDomRule of aDomSheet.cssRules) {
        if (aDomRule.type == Ci.nsIDOMCSSRule.IMPORT_RULE && aDomRule.styleSheet &&
            this.mediaMatches(aDomRule)) {
          this._cacheSheet(aDomRule.styleSheet);
        } else if (aDomRule.type == Ci.nsIDOMCSSRule.KEYFRAMES_RULE) {
          this._keyframesRules.push(aDomRule);
        }
      }
    }
  },

  /**
   * Retrieve the list of stylesheets in the document.
   *
   * @return {array} the list of stylesheets in the document.
   */
  get sheets()
  {
    if (!this._sheetsCached) {
      this._cacheSheets();
    }

    let sheets = [];
    this.forEachSheet(function (aSheet) {
      if (aSheet.contentSheet) {
        sheets.push(aSheet);
      }
    }, this);

    return sheets;
  },

  /**
   * Retrieve the list of keyframes rules in the document.
   *
   * @ return {array} the list of keyframes rules in the document.
   */
  get keyframesRules()
  {
    if (!this._sheetsCached) {
      this._cacheSheets();
    }
    return this._keyframesRules;
  },

  /**
   * Retrieve a CssSheet object for a given a CSSStyleSheet object. If the
   * stylesheet is already cached, you get the existing CssSheet object,
   * otherwise the new CSSStyleSheet object is cached.
   *
   * @param {CSSStyleSheet} aDomSheet the CSSStyleSheet object you want.
   * @param {number} aIndex the index, within the document, of the stylesheet.
   *
   * @return {CssSheet} the CssSheet object for the given CSSStyleSheet object.
   */
  getSheet: function CL_getSheet(aDomSheet, aIndex)
  {
    let cacheId = "";

    if (aDomSheet.href) {
      cacheId = aDomSheet.href;
    } else if (aDomSheet.ownerNode && aDomSheet.ownerNode.ownerDocument) {
      cacheId = aDomSheet.ownerNode.ownerDocument.location;
    }

    let sheet = null;
    let sheetFound = false;

    if (cacheId in this._sheets) {
      for (let i = 0, numSheets = this._sheets[cacheId].length; i < numSheets; i++) {
        sheet = this._sheets[cacheId][i];
        if (sheet.domSheet === aDomSheet) {
          if (aIndex != -1) {
            sheet.index = aIndex;
          }
          sheetFound = true;
          break;
        }
      }
    }

    if (!sheetFound) {
      if (!(cacheId in this._sheets)) {
        this._sheets[cacheId] = [];
      }

      sheet = new CssSheet(this, aDomSheet, aIndex);
      if (sheet.sheetAllowed && sheet.contentSheet) {
        this._ruleCount += sheet.ruleCount;
      }

      this._sheets[cacheId].push(sheet);
    }

    return sheet;
  },

  /**
   * Process each cached stylesheet in the document using your callback.
   *
   * @param {function} aCallback the function you want executed for each of the
   * CssSheet objects cached.
   * @param {object} aScope the scope you want for the callback function. aScope
   * will be the this object when aCallback executes.
   */
  forEachSheet: function CssLogic_forEachSheet(aCallback, aScope)
  {
    for each (let sheets in this._sheets) {
      for (let i = 0; i < sheets.length; i ++) {
        // We take this as an opportunity to clean dead sheets
        try {
          let sheet = sheets[i];
          sheet.domSheet; // If accessing domSheet raises an exception, then the
          // style sheet is a dead object
          aCallback.call(aScope, sheet, i, sheets);
        } catch (e) {
          sheets.splice(i, 1);
          i --;
        }
      }
    }
  },

  /**
   * Process *some* cached stylesheets in the document using your callback. The
   * callback function should return true in order to halt processing.
   *
   * @param {function} aCallback the function you want executed for some of the
   * CssSheet objects cached.
   * @param {object} aScope the scope you want for the callback function. aScope
   * will be the this object when aCallback executes.
   * @return {Boolean} true if aCallback returns true during any iteration,
   * otherwise false is returned.
   */
  forSomeSheets: function CssLogic_forSomeSheets(aCallback, aScope)
  {
    for each (let sheets in this._sheets) {
      if (sheets.some(aCallback, aScope)) {
        return true;
      }
    }
    return false;
  },

  /**
   * Get the number nsIDOMCSSRule objects in the document, counted from all of
   * the stylesheets. System sheets are excluded. If a filter is active, this
   * tells only the number of nsIDOMCSSRule objects inside the selected
   * CSSStyleSheet.
   *
   * WARNING: This only provides an estimate of the rule count, and the results
   * could change at a later date. Todo remove this
   *
   * @return {number} the number of nsIDOMCSSRule (all rules).
   */
  get ruleCount()
  {
    if (!this._sheetsCached) {
      this._cacheSheets();
    }

    return this._ruleCount;
  },

  /**
   * Process the CssSelector objects that match the highlighted element and its
   * parent elements. aScope.aCallback() is executed for each CssSelector
   * object, being passed the CssSelector object and the match status.
   *
   * This method also includes all of the element.style properties, for each
   * highlighted element parent and for the highlighted element itself.
   *
   * Note that the matched selectors are cached, such that next time your
   * callback is invoked for the cached list of CssSelector objects.
   *
   * @param {function} aCallback the function you want to execute for each of
   * the matched selectors.
   * @param {object} aScope the scope you want for the callback function. aScope
   * will be the this object when aCallback executes.
   */
  processMatchedSelectors: function CL_processMatchedSelectors(aCallback, aScope)
  {
    if (this._matchedSelectors) {
      if (aCallback) {
        this._passId++;
        this._matchedSelectors.forEach(function(aValue) {
          aCallback.call(aScope, aValue[0], aValue[1]);
          aValue[0].cssRule._passId = this._passId;
        }, this);
      }
      return;
    }

    if (!this._matchedRules) {
      this._buildMatchedRules();
    }

    this._matchedSelectors = [];
    this._passId++;

    for (let i = 0; i < this._matchedRules.length; i++) {
      let rule = this._matchedRules[i][0];
      let status = this._matchedRules[i][1];

      rule.selectors.forEach(function (aSelector) {
        if (aSelector._matchId !== this._matchId &&
           (aSelector.elementStyle ||
            this.selectorMatchesElement(rule.domRule, aSelector.selectorIndex))) {

          aSelector._matchId = this._matchId;
          this._matchedSelectors.push([ aSelector, status ]);
          if (aCallback) {
            aCallback.call(aScope, aSelector, status);
          }
        }
      }, this);

      rule._passId = this._passId;
    }
  },

  /**
   * Check if the given selector matches the highlighted element or any of its
   * parents.
   *
   * @private
   * @param {DOMRule} domRule
   *        The DOM Rule containing the selector.
   * @param {Number} idx
   *        The index of the selector within the DOMRule.
   * @return {boolean}
   *         true if the given selector matches the highlighted element or any
   *         of its parents, otherwise false is returned.
   */
  selectorMatchesElement: function CL_selectorMatchesElement2(domRule, idx)
  {
    let element = this.viewedElement;
    do {
      if (domUtils.selectorMatchesElement(element, domRule, idx)) {
        return true;
      }
    } while ((element = element.parentNode) &&
             element.nodeType === Ci.nsIDOMNode.ELEMENT_NODE);

    return false;
  },

  /**
   * Check if the highlighted element or it's parents have matched selectors.
   *
   * @param {array} aProperties The list of properties you want to check if they
   * have matched selectors or not.
   * @return {object} An object that tells for each property if it has matched
   * selectors or not. Object keys are property names and values are booleans.
   */
  hasMatchedSelectors: function CL_hasMatchedSelectors(aProperties)
  {
    if (!this._matchedRules) {
      this._buildMatchedRules();
    }

    let result = {};

    this._matchedRules.some(function(aValue) {
      let rule = aValue[0];
      let status = aValue[1];
      aProperties = aProperties.filter((aProperty) => {
        // We just need to find if a rule has this property while it matches
        // the viewedElement (or its parents).
        if (rule.getPropertyValue(aProperty) &&
            (status == CssLogic.STATUS.MATCHED ||
             (status == CssLogic.STATUS.PARENT_MATCH &&
              domUtils.isInheritedProperty(aProperty)))) {
          result[aProperty] = true;
          return false;
        }
        return true; // Keep the property for the next rule.
      });
      return aProperties.length == 0;
    }, this);

    return result;
  },

  /**
   * Build the array of matched rules for the currently highlighted element.
   * The array will hold rules that match the viewedElement and its parents.
   *
   * @private
   */
  _buildMatchedRules: function CL__buildMatchedRules()
  {
    let domRules;
    let element = this.viewedElement;
    let filter = this.sourceFilter;
    let sheetIndex = 0;

    this._matchId++;
    this._passId++;
    this._matchedRules = [];

    if (!element) {
      return;
    }

    do {
      let status = this.viewedElement === element ?
                   CssLogic.STATUS.MATCHED : CssLogic.STATUS.PARENT_MATCH;

      try {
        // Handle finding rules on pseudo by reading style rules
        // on the parent node with proper pseudo arg to getCSSStyleRules.
        let {bindingElement, pseudo} = CssLogic.getBindingElementAndPseudo(element);
        domRules = domUtils.getCSSStyleRules(bindingElement, pseudo);
      } catch (ex) {
        Services.console.
          logStringMessage("CL__buildMatchedRules error: " + ex);
        continue;
      }

      // getCSSStyleRules can return null with a shadow DOM element.
      let numDomRules = domRules ? domRules.Count() : 0;
      for (let i = 0; i < numDomRules; i++) {
        let domRule = domRules.GetElementAt(i);
        if (domRule.type !== Ci.nsIDOMCSSRule.STYLE_RULE) {
          continue;
        }

        let sheet = this.getSheet(domRule.parentStyleSheet, -1);
        if (sheet._passId !== this._passId) {
          sheet.index = sheetIndex++;
          sheet._passId = this._passId;
        }

        if (filter === CssLogic.FILTER.USER && !sheet.contentSheet) {
          continue;
        }

        let rule = sheet.getRule(domRule);
        if (rule._passId === this._passId) {
          continue;
        }

        rule._matchId = this._matchId;
        rule._passId = this._passId;
        this._matchedRules.push([rule, status]);
      }

      // Add element.style information.
      if (element.style && element.style.length > 0) {
        let rule = new CssRule(null, { style: element.style }, element);
        rule._matchId = this._matchId;
        rule._passId = this._passId;
        this._matchedRules.push([rule, status]);
      }
    } while ((element = element.parentNode) &&
              element.nodeType === Ci.nsIDOMNode.ELEMENT_NODE);
  },

  /**
   * Tells if the given DOM CSS object matches the current view media.
   *
   * @param {object} aDomObject The DOM CSS object to check.
   * @return {boolean} True if the DOM CSS object matches the current view
   * media, or false otherwise.
   */
  mediaMatches: function CL_mediaMatches(aDomObject)
  {
    let mediaText = aDomObject.media.mediaText;
    return !mediaText || this.viewedDocument.defaultView.
                         matchMedia(mediaText).matches;
  },
};

/**
 * If the element has an id, return '#id'. Otherwise return 'tagname[n]' where
 * n is the index of this element in its siblings.
 * <p>A technically more 'correct' output from the no-id case might be:
 * 'tagname:nth-of-type(n)' however this is unlikely to be more understood
 * and it is longer.
 *
 * @param {nsIDOMElement} aElement the element for which you want the short name.
 * @return {string} the string to be displayed for aElement.
 */
CssLogic.getShortName = function CssLogic_getShortName(aElement)
{
  if (!aElement) {
    return "null";
  }
  if (aElement.id) {
    return "#" + aElement.id;
  }
  let priorSiblings = 0;
  let temp = aElement;
  while ((temp = temp.previousElementSibling)) {
    priorSiblings++;
  }
  return aElement.tagName + "[" + priorSiblings + "]";
};

/**
 * Get an array of short names from the given element to document.body.
 *
 * @param {nsIDOMElement} aElement the element for which you want the array of
 * short names.
 * @return {array} The array of elements.
 * <p>Each element is an object of the form:
 * <ul>
 * <li>{ display: "what to display for the given (parent) element",
 * <li>  element: referenceToTheElement }
 * </ul>
 */
CssLogic.getShortNamePath = function CssLogic_getShortNamePath(aElement)
{
  let doc = aElement.ownerDocument;
  let reply = [];

  if (!aElement) {
    return reply;
  }

  // We want to exclude nodes high up the tree (body/html) unless the user
  // has selected that node, in which case we need to report something.
  do {
    reply.unshift({
      display: CssLogic.getShortName(aElement),
      element: aElement
    });
    aElement = aElement.parentNode;
  } while (aElement && aElement != doc.body && aElement != doc.head && aElement != doc);

  return reply;
};

/**
 * Get a string list of selectors for a given DOMRule.
 *
 * @param {DOMRule} aDOMRule
 *        The DOMRule to parse.
 * @return {Array}
 *         An array of string selectors.
 */
CssLogic.getSelectors = function CssLogic_getSelectors(aDOMRule)
{
  let selectors = [];

  let len = domUtils.getSelectorCount(aDOMRule);
  for (let i = 0; i < len; i++) {
    let text = domUtils.getSelectorText(aDOMRule, i);
    selectors.push(text);
  }
  return selectors;
}

/**
 * Given a node, check to see if it is a ::before or ::after element.
 * If so, return the node that is accessible from within the document
 * (the parent of the anonymous node), along with which pseudo element
 * it was.  Otherwise, return the node itself.
 *
 * @returns {Object}
 *            - {DOMNode} node The non-anonymous node
 *            - {string} pseudo One of ':before', ':after', or null.
 */
CssLogic.getBindingElementAndPseudo = function(node)
{
  let bindingElement = node;
  let pseudo = null;
  if (node.nodeName == "_moz_generated_content_before") {
    bindingElement = node.parentNode;
    pseudo = ":before";
  } else if (node.nodeName == "_moz_generated_content_after") {
    bindingElement = node.parentNode;
    pseudo = ":after";
  }
  return {
    bindingElement: bindingElement,
    pseudo: pseudo
  };
};


/**
 * Get the computed style on a node.  Automatically handles reading
 * computed styles on a ::before/::after element by reading on the
 * parent node with the proper pseudo argument.
 *
 * @param {Node}
 * @returns {CSSStyleDeclaration}
 */
CssLogic.getComputedStyle = function(node)
{
  if (!node ||
      Cu.isDeadWrapper(node) ||
      node.nodeType !== Ci.nsIDOMNode.ELEMENT_NODE ||
      !node.ownerDocument ||
      !node.ownerDocument.defaultView) {
    return null;
  }

  let {bindingElement, pseudo} = CssLogic.getBindingElementAndPseudo(node);
  return node.ownerDocument.defaultView.getComputedStyle(bindingElement, pseudo);
};

/**
 * Memonized lookup of a l10n string from a string bundle.
 * @param {string} aName The key to lookup.
 * @returns A localized version of the given key.
 */
CssLogic.l10n = function(aName) CssLogic._strings.GetStringFromName(aName);

DevToolsUtils.defineLazyGetter(CssLogic, "_strings", function() Services.strings
             .createBundle("chrome://global/locale/devtools/styleinspector.properties"));

/**
 * Is the given property sheet a content stylesheet?
 *
 * @param {CSSStyleSheet} aSheet a stylesheet
 * @return {boolean} true if the given stylesheet is a content stylesheet,
 * false otherwise.
 */
CssLogic.isContentStylesheet = function CssLogic_isContentStylesheet(aSheet)
{
  // All sheets with owner nodes have been included by content.
  if (aSheet.ownerNode) {
    return true;
  }

  // If the sheet has a CSSImportRule we need to check the parent stylesheet.
  if (aSheet.ownerRule instanceof Ci.nsIDOMCSSImportRule) {
    return CssLogic.isContentStylesheet(aSheet.parentStyleSheet);
  }

  return false;
};

/**
 * Get a source for a stylesheet, taking into account embedded stylesheets
 * for which we need to use document.defaultView.location.href rather than
 * sheet.href
 *
 * @param {CSSStyleSheet} aSheet the DOM object for the style sheet.
 * @return {string} the address of the stylesheet.
 */
CssLogic.href = function CssLogic_href(aSheet)
{
  let href = aSheet.href;
  if (!href) {
    href = aSheet.ownerNode.ownerDocument.location;
  }

  return href;
};

/**
 * Return a shortened version of a style sheet's source.
 *
 * @param {CSSStyleSheet} aSheet the DOM object for the style sheet.
 */
CssLogic.shortSource = function CssLogic_shortSource(aSheet)
{
  // Use a string like "inline" if there is no source href
  if (!aSheet || !aSheet.href) {
    return CssLogic.l10n("rule.sourceInline");
  }

  // We try, in turn, the filename, filePath, query string, whole thing
  let url = {};
  try {
    url = Services.io.newURI(aSheet.href, null, null);
    url = url.QueryInterface(Ci.nsIURL);
  } catch (ex) {
    // Some UA-provided stylesheets are not valid URLs.
  }

  if (url.fileName) {
    return url.fileName;
  }

  if (url.filePath) {
    return url.filePath;
  }

  if (url.query) {
    return url.query;
  }

  let dataUrl = aSheet.href.match(/^(data:[^,]*),/);
  return dataUrl ? dataUrl[1] : aSheet.href;
}

/**
 * Find the position of [element] in [nodeList].
 * @returns an index of the match, or -1 if there is no match
 */
function positionInNodeList(element, nodeList) {
  for (var i = 0; i < nodeList.length; i++) {
    if (element === nodeList[i]) {
      return i;
    }
  }
  return -1;
}

/**
 * Find a unique CSS selector for a given element
 * @returns a string such that ele.ownerDocument.querySelector(reply) === ele
 * and ele.ownerDocument.querySelectorAll(reply).length === 1
 */
CssLogic.findCssSelector = function CssLogic_findCssSelector(ele) {
  ele = LayoutHelpers.getRootBindingParent(ele);
  var document = ele.ownerDocument;
  if (!document || !document.contains(ele)) {
    throw new Error('findCssSelector received element not inside document');
  }

  // document.querySelectorAll("#id") returns multiple if elements share an ID
  if (ele.id && document.querySelectorAll('#' + CSS.escape(ele.id)).length === 1) {
    return '#' + CSS.escape(ele.id);
  }

  // Inherently unique by tag name
  var tagName = ele.localName;
  if (tagName === 'html') {
    return 'html';
  }
  if (tagName === 'head') {
    return 'head';
  }
  if (tagName === 'body') {
    return 'body';
  }

  // We might be able to find a unique class name
  var selector, index, matches;
  if (ele.classList.length > 0) {
    for (var i = 0; i < ele.classList.length; i++) {
      // Is this className unique by itself?
      selector = '.' + CSS.escape(ele.classList.item(i));
      matches = document.querySelectorAll(selector);
      if (matches.length === 1) {
        return selector;
      }
      // Maybe it's unique with a tag name?
      selector = tagName + selector;
      matches = document.querySelectorAll(selector);
      if (matches.length === 1) {
        return selector;
      }
      // Maybe it's unique using a tag name and nth-child
      index = positionInNodeList(ele, ele.parentNode.children) + 1;
      selector = selector + ':nth-child(' + index + ')';
      matches = document.querySelectorAll(selector);
      if (matches.length === 1) {
        return selector;
      }
    }
  }

  // Not unique enough yet.  As long as it's not a child of the document,
  // continue recursing up until it is unique enough.
  if (ele.parentNode !== document) {
    index = positionInNodeList(ele, ele.parentNode.children) + 1;
    selector = CssLogic_findCssSelector(ele.parentNode) + ' > ' +
            tagName + ':nth-child(' + index + ')';
  }

  return selector;
};

const TAB_CHARS = "\t";

/**
 * Prettify minified CSS text.
 * This prettifies CSS code where there is no indentation in usual places while
 * keeping original indentation as-is elsewhere.
 * @param string text The CSS source to prettify.
 * @return string Prettified CSS source
 */
CssLogic.prettifyCSS = function(text, ruleCount) {
  if (CssLogic.LINE_SEPARATOR == null) {
    let os = Cc["@mozilla.org/xre/app-info;1"].getService(Ci.nsIXULRuntime).OS;
    CssLogic.LINE_SEPARATOR = (os === "WINNT" ? "\r\n" : "\n");
  }

  // remove initial and terminating HTML comments and surrounding whitespace
  text = text.replace(/(?:^\s*<!--[\r\n]*)|(?:\s*-->\s*$)/g, "");

  // don't attempt to prettify if there's more than one line per rule.
  let lineCount = text.split("\n").length - 1;
  if (ruleCount !== null && lineCount >= ruleCount) {
    return text;
  }

  // We reformat the text using a simple state machine.  The
  // reformatting preserves most of the input text, changing only
  // whitespace.  The rules are:
  //
  // * After a "{" or ";" symbol, ensure there is a newline and
  //   indentation before the next non-comment, non-whitespace token.
  // * Additionally after a "{" symbol, increase the indentation.
  // * A "}" symbol ensures there is a preceding newline, and
  //   decreases the indentation level.
  // * Ensure there is whitespace before a "{".
  //
  // This approach can be confused sometimes, but should do ok on a
  // minified file.
  let indent = "";
  let indentLevel = 0;
  let tokens = domUtils.getCSSLexer(text);
  let result = "";
  let pushbackToken = undefined;

  // A helper function that reads tokens, looking for the next
  // non-comment, non-whitespace token.  Comment and whitespace tokens
  // are appended to |result|.  If this encounters EOF, it returns
  // null.  Otherwise it returns the last whitespace token that was
  // seen.  This function also updates |pushbackToken|.
  let readUntilSignificantToken = () => {
    while (true) {
      let token = tokens.nextToken();
      if (!token || token.tokenType !== "whitespace") {
        pushbackToken = token;
        return token;
      }
      // Saw whitespace.  Before committing to it, check the next
      // token.
      let nextToken = tokens.nextToken();
      if (!nextToken || nextToken.tokenType !== "comment") {
        pushbackToken = nextToken;
        return token;
      }
      // Saw whitespace + comment.  Update the result and continue.
      result = result + text.substring(token.startOffset, nextToken.endOffset);
    }
  };

  // State variables for readUntilNewlineNeeded.
  //
  // Starting index of the accumulated tokens.
  let startIndex;
  // Ending index of the accumulated tokens.
  let endIndex;
  // True if any non-whitespace token was seen.
  let anyNonWS;
  // True if the terminating token is "}".
  let isCloseBrace;
  // True if the token just before the terminating token was
  // whitespace.
  let lastWasWS;

  // A helper function that reads tokens until there is a reason to
  // insert a newline.  This updates the state variables as needed.
  // If this encounters EOF, it returns null.  Otherwise it returns
  // the final token read.  Note that if the returned token is "{",
  // then it will not be included in the computed start/end token
  // range.  This is used to handle whitespace insertion before a "{".
  let readUntilNewlineNeeded = () => {
    let token;
    while (true) {
      if (pushbackToken) {
        token = pushbackToken;
        pushbackToken = undefined;
      } else {
        token = tokens.nextToken();
      }
      if (!token) {
        endIndex = text.length;
        break;
      }

      // A "}" symbol must be inserted later, to deal with indentation
      // and newline.
      if (token.tokenType === "symbol" && token.text === "}") {
        isCloseBrace = true;
        break;
      } else if (token.tokenType === "symbol" && token.text === "{") {
        break;
      }

      if (token.tokenType !== "whitespace") {
        anyNonWS = true;
      }

      if (startIndex === undefined) {
        startIndex = token.startOffset;
      }
      endIndex = token.endOffset;

      if (token.tokenType === "symbol" && token.text === ';') {
        break;
      }

      lastWasWS = token.tokenType === "whitespace";
    }
    return token;
  };

  while (true) {
    // Set the initial state.
    startIndex = undefined;
    endIndex = undefined;
    anyNonWS = false;
    isCloseBrace = false;
    lastWasWS = false;

    // Read tokens until we see a reason to insert a newline.
    let token = readUntilNewlineNeeded();

    // Append any saved up text to the result, applying indentation.
    if (startIndex !== undefined) {
      if (isCloseBrace && !anyNonWS) {
        // If we saw only whitespace followed by a "}", then we don't
        // need anything here.
      } else {
        result = result + indent + text.substring(startIndex, endIndex);
        if (isCloseBrace)
          result += CssLogic.LINE_SEPARATOR;
      }
    }

    if (isCloseBrace) {
      indent = TAB_CHARS.repeat(--indentLevel);
      result = result + indent + '}';
    }

    if (!token) {
      break;
    }

    if (token.tokenType === "symbol" && token.text === '{') {
      if (!lastWasWS) {
        result += ' ';
      }
      result += '{';
      indent = TAB_CHARS.repeat(++indentLevel);
    }

    // Now it is time to insert a newline.  However first we want to
    // deal with any trailing comments.
    token = readUntilSignificantToken();

    // "Early" bail-out if the text does not appear to be minified.
    // Here we ignore the case where whitespace appears at the end of
    // the text.
    if (pushbackToken && token && token.tokenType === "whitespace" &&
        /\n/g.test(text.substring(token.startOffset, token.endOffset))) {
      return text;
    }

    // Finally time for that newline.
    result = result + CssLogic.LINE_SEPARATOR;

    // Maybe we hit EOF.
    if (!pushbackToken) {
      break;
    }
  }

  return result;
};

/**
 * A safe way to access cached bits of information about a stylesheet.
 *
 * @constructor
 * @param {CssLogic} aCssLogic pointer to the CssLogic instance working with
 * this CssSheet object.
 * @param {CSSStyleSheet} aDomSheet reference to a DOM CSSStyleSheet object.
 * @param {number} aIndex tells the index/position of the stylesheet within the
 * main document.
 */
function CssSheet(aCssLogic, aDomSheet, aIndex)
{
  this._cssLogic = aCssLogic;
  this.domSheet = aDomSheet;
  this.index = this.contentSheet ? aIndex : -100 * aIndex;

  // Cache of the sheets href. Cached by the getter.
  this._href = null;
  // Short version of href for use in select boxes etc. Cached by getter.
  this._shortSource = null;

  // null for uncached.
  this._sheetAllowed = null;

  // Cached CssRules from the given stylesheet.
  this._rules = {};

  this._ruleCount = -1;
}

CssSheet.prototype = {
  _passId: null,
  _contentSheet: null,
  _mediaMatches: null,

  /**
   * Tells if the stylesheet is provided by the browser or not.
   *
   * @return {boolean} false if this is a browser-provided stylesheet, or true
   * otherwise.
   */
  get contentSheet()
  {
    if (this._contentSheet === null) {
      this._contentSheet = CssLogic.isContentStylesheet(this.domSheet);
    }
    return this._contentSheet;
  },

  /**
   * Tells if the stylesheet is disabled or not.
   * @return {boolean} true if this stylesheet is disabled, or false otherwise.
   */
  get disabled()
  {
    return this.domSheet.disabled;
  },

  /**
   * Tells if the stylesheet matches the current browser view media.
   * @return {boolean} true if this stylesheet matches the current browser view
   * media, or false otherwise.
   */
  get mediaMatches()
  {
    if (this._mediaMatches === null) {
      this._mediaMatches = this._cssLogic.mediaMatches(this.domSheet);
    }
    return this._mediaMatches;
  },

  /**
   * Get a source for a stylesheet, using CssLogic.href
   *
   * @return {string} the address of the stylesheet.
   */
  get href()
  {
    if (this._href) {
      return this._href;
    }

    this._href = CssLogic.href(this.domSheet);
    return this._href;
  },

  /**
   * Create a shorthand version of the href of a stylesheet.
   *
   * @return {string} the shorthand source of the stylesheet.
   */
  get shortSource()
  {
    if (this._shortSource) {
      return this._shortSource;
    }

    this._shortSource = CssLogic.shortSource(this.domSheet);
    return this._shortSource;
  },

  /**
   * Tells if the sheet is allowed or not by the current CssLogic.sourceFilter.
   *
   * @return {boolean} true if the stylesheet is allowed by the sourceFilter, or
   * false otherwise.
   */
  get sheetAllowed()
  {
    if (this._sheetAllowed !== null) {
      return this._sheetAllowed;
    }

    this._sheetAllowed = true;

    let filter = this._cssLogic.sourceFilter;
    if (filter === CssLogic.FILTER.USER && !this.contentSheet) {
      this._sheetAllowed = false;
    }
    if (filter !== CssLogic.FILTER.USER && filter !== CssLogic.FILTER.UA) {
      this._sheetAllowed = (filter === this.href);
    }

    return this._sheetAllowed;
  },

  /**
   * Retrieve the number of rules in this stylesheet.
   *
   * @return {number} the number of nsIDOMCSSRule objects in this stylesheet.
   */
  get ruleCount()
  {
    return this._ruleCount > -1 ?
      this._ruleCount :
      this.domSheet.cssRules.length;
  },

  /**
   * Retrieve a CssRule object for the given CSSStyleRule. The CssRule object is
   * cached, such that subsequent retrievals return the same CssRule object for
   * the same CSSStyleRule object.
   *
   * @param {CSSStyleRule} aDomRule the CSSStyleRule object for which you want a
   * CssRule object.
   * @return {CssRule} the cached CssRule object for the given CSSStyleRule
   * object.
   */
  getRule: function CssSheet_getRule(aDomRule)
  {
    let cacheId = aDomRule.type + aDomRule.selectorText;

    let rule = null;
    let ruleFound = false;

    if (cacheId in this._rules) {
      for (let i = 0, rulesLen = this._rules[cacheId].length; i < rulesLen; i++) {
        rule = this._rules[cacheId][i];
        if (rule.domRule === aDomRule) {
          ruleFound = true;
          break;
        }
      }
    }

    if (!ruleFound) {
      if (!(cacheId in this._rules)) {
        this._rules[cacheId] = [];
      }

      rule = new CssRule(this, aDomRule);
      this._rules[cacheId].push(rule);
    }

    return rule;
  },

  /**
   * Process each rule in this stylesheet using your callback function. Your
   * function receives one argument: the CssRule object for each CSSStyleRule
   * inside the stylesheet.
   *
   * Note that this method also iterates through @media rules inside the
   * stylesheet.
   *
   * @param {function} aCallback the function you want to execute for each of
   * the style rules.
   * @param {object} aScope the scope you want for the callback function. aScope
   * will be the this object when aCallback executes.
   */
  forEachRule: function CssSheet_forEachRule(aCallback, aScope)
  {
    let ruleCount = 0;
    let domRules = this.domSheet.cssRules;

    function _iterator(aDomRule) {
      if (aDomRule.type == Ci.nsIDOMCSSRule.STYLE_RULE) {
        aCallback.call(aScope, this.getRule(aDomRule));
        ruleCount++;
      } else if (aDomRule.type == Ci.nsIDOMCSSRule.MEDIA_RULE &&
          aDomRule.cssRules && this._cssLogic.mediaMatches(aDomRule)) {
        Array.prototype.forEach.call(aDomRule.cssRules, _iterator, this);
      }
    }

    Array.prototype.forEach.call(domRules, _iterator, this);

    this._ruleCount = ruleCount;
  },

  /**
   * Process *some* rules in this stylesheet using your callback function. Your
   * function receives one argument: the CssRule object for each CSSStyleRule
   * inside the stylesheet. In order to stop processing the callback function
   * needs to return a value.
   *
   * Note that this method also iterates through @media rules inside the
   * stylesheet.
   *
   * @param {function} aCallback the function you want to execute for each of
   * the style rules.
   * @param {object} aScope the scope you want for the callback function. aScope
   * will be the this object when aCallback executes.
   * @return {Boolean} true if aCallback returns true during any iteration,
   * otherwise false is returned.
   */
  forSomeRules: function CssSheet_forSomeRules(aCallback, aScope)
  {
    let domRules = this.domSheet.cssRules;
    function _iterator(aDomRule) {
      if (aDomRule.type == Ci.nsIDOMCSSRule.STYLE_RULE) {
        return aCallback.call(aScope, this.getRule(aDomRule));
      } else if (aDomRule.type == Ci.nsIDOMCSSRule.MEDIA_RULE &&
          aDomRule.cssRules && this._cssLogic.mediaMatches(aDomRule)) {
        return Array.prototype.some.call(aDomRule.cssRules, _iterator, this);
      }
      return false;
    }
    return Array.prototype.some.call(domRules, _iterator, this);
  },

  toString: function CssSheet_toString()
  {
    return "CssSheet[" + this.shortSource + "]";
  }
};

/**
 * Information about a single CSSStyleRule.
 *
 * @param {CSSSheet|null} aCssSheet the CssSheet object of the stylesheet that
 * holds the CSSStyleRule. If the rule comes from element.style, set this
 * argument to null.
 * @param {CSSStyleRule|object} aDomRule the DOM CSSStyleRule for which you want
 * to cache data. If the rule comes from element.style, then provide
 * an object of the form: {style: element.style}.
 * @param {Element} [aElement] If the rule comes from element.style, then this
 * argument must point to the element.
 * @constructor
 */
function CssRule(aCssSheet, aDomRule, aElement)
{
  this._cssSheet = aCssSheet;
  this.domRule = aDomRule;

  let parentRule = aDomRule.parentRule;
  if (parentRule && parentRule.type == Ci.nsIDOMCSSRule.MEDIA_RULE) {
    this.mediaText = parentRule.media.mediaText;
  }

  if (this._cssSheet) {
    // parse domRule.selectorText on call to this.selectors
    this._selectors = null;
    this.line = domUtils.getRuleLine(this.domRule);
    this.source = this._cssSheet.shortSource + ":" + this.line;
    if (this.mediaText) {
      this.source += " @media " + this.mediaText;
    }
    this.href = this._cssSheet.href;
    this.contentRule = this._cssSheet.contentSheet;
  } else if (aElement) {
    this._selectors = [ new CssSelector(this, "@element.style", 0) ];
    this.line = -1;
    this.source = CssLogic.l10n("rule.sourceElement");
    this.href = "#";
    this.contentRule = true;
    this.sourceElement = aElement;
  }
}

CssRule.prototype = {
  _passId: null,

  mediaText: "",

  get isMediaRule()
  {
    return !!this.mediaText;
  },

  /**
   * Check if the parent stylesheet is allowed by the CssLogic.sourceFilter.
   *
   * @return {boolean} true if the parent stylesheet is allowed by the current
   * sourceFilter, or false otherwise.
   */
  get sheetAllowed()
  {
    return this._cssSheet ? this._cssSheet.sheetAllowed : true;
  },

  /**
   * Retrieve the parent stylesheet index/position in the viewed document.
   *
   * @return {number} the parent stylesheet index/position in the viewed
   * document.
   */
  get sheetIndex()
  {
    return this._cssSheet ? this._cssSheet.index : 0;
  },

  /**
   * Retrieve the style property value from the current CSSStyleRule.
   *
   * @param {string} aProperty the CSS property name for which you want the
   * value.
   * @return {string} the property value.
   */
  getPropertyValue: function(aProperty)
  {
    return this.domRule.style.getPropertyValue(aProperty);
  },

  /**
   * Retrieve the style property priority from the current CSSStyleRule.
   *
   * @param {string} aProperty the CSS property name for which you want the
   * priority.
   * @return {string} the property priority.
   */
  getPropertyPriority: function(aProperty)
  {
    return this.domRule.style.getPropertyPriority(aProperty);
  },

  /**
   * Retrieve the list of CssSelector objects for each of the parsed selectors
   * of the current CSSStyleRule.
   *
   * @return {array} the array hold the CssSelector objects.
   */
  get selectors()
  {
    if (this._selectors) {
      return this._selectors;
    }

    // Parse the CSSStyleRule.selectorText string.
    this._selectors = [];

    if (!this.domRule.selectorText) {
      return this._selectors;
    }

    let selectors = CssLogic.getSelectors(this.domRule);

    for (let i = 0, len = selectors.length; i < len; i++) {
      this._selectors.push(new CssSelector(this, selectors[i], i));
    }

    return this._selectors;
  },

  toString: function CssRule_toString()
  {
    return "[CssRule " + this.domRule.selectorText + "]";
  },
};

/**
 * The CSS selector class allows us to document the ranking of various CSS
 * selectors.
 *
 * @constructor
 * @param {CssRule} aCssRule the CssRule instance from where the selector comes.
 * @param {string} aSelector The selector that we wish to investigate.
 * @param {Number} aIndex The index of the selector within it's rule.
 */
function CssSelector(aCssRule, aSelector, aIndex)
{
  this.cssRule = aCssRule;
  this.text = aSelector;
  this.elementStyle = this.text == "@element.style";
  this._specificity = null;
  this.selectorIndex = aIndex;
}

exports.CssSelector = CssSelector;

CssSelector.prototype = {
  _matchId: null,

  /**
   * Retrieve the CssSelector source, which is the source of the CssSheet owning
   * the selector.
   *
   * @return {string} the selector source.
   */
  get source()
  {
    return this.cssRule.source;
  },

  /**
   * Retrieve the CssSelector source element, which is the source of the CssRule
   * owning the selector. This is only available when the CssSelector comes from
   * an element.style.
   *
   * @return {string} the source element selector.
   */
  get sourceElement()
  {
    return this.cssRule.sourceElement;
  },

  /**
   * Retrieve the address of the CssSelector. This points to the address of the
   * CssSheet owning this selector.
   *
   * @return {string} the address of the CssSelector.
   */
  get href()
  {
    return this.cssRule.href;
  },

  /**
   * Check if the selector comes from a browser-provided stylesheet.
   *
   * @return {boolean} true if the selector comes from a content-provided
   * stylesheet, or false otherwise.
   */
  get contentRule()
  {
    return this.cssRule.contentRule;
  },

  /**
   * Check if the parent stylesheet is allowed by the CssLogic.sourceFilter.
   *
   * @return {boolean} true if the parent stylesheet is allowed by the current
   * sourceFilter, or false otherwise.
   */
  get sheetAllowed()
  {
    return this.cssRule.sheetAllowed;
  },

  /**
   * Retrieve the parent stylesheet index/position in the viewed document.
   *
   * @return {number} the parent stylesheet index/position in the viewed
   * document.
   */
  get sheetIndex()
  {
    return this.cssRule.sheetIndex;
  },

  /**
   * Retrieve the line of the parent CSSStyleRule in the parent CSSStyleSheet.
   *
   * @return {number} the line of the parent CSSStyleRule in the parent
   * stylesheet.
   */
  get ruleLine()
  {
    return this.cssRule.line;
  },

  /**
   * Retrieve the pseudo-elements that we support. This list should match the
   * elements specified in layout/style/nsCSSPseudoElementList.h
   */
  get pseudoElements()
  {
    if (!CssSelector._pseudoElements) {
      let pseudos = CssSelector._pseudoElements = new Set();
      pseudos.add("after");
      pseudos.add("before");
      pseudos.add("first-letter");
      pseudos.add("first-line");
      pseudos.add("selection");
      pseudos.add("-moz-color-swatch");
      pseudos.add("-moz-focus-inner");
      pseudos.add("-moz-focus-outer");
      pseudos.add("-moz-list-bullet");
      pseudos.add("-moz-list-number");
      pseudos.add("-moz-math-anonymous");
      pseudos.add("-moz-math-stretchy");
      pseudos.add("-moz-progress-bar");
      pseudos.add("-moz-selection");
    }
    return CssSelector._pseudoElements;
  },

  /**
   * Retrieve specificity information for the current selector.
   *
   * @see http://www.w3.org/TR/css3-selectors/#specificity
   * @see http://www.w3.org/TR/CSS2/selector.html
   *
   * @return {Number} The selector's specificity.
   */
  get specificity()
  {
    if (this.elementStyle) {
      // We can't ask specificity from DOMUtils as element styles don't provide
      // CSSStyleRule interface DOMUtils expect. However, specificity of element
      // style is constant, 1,0,0,0 or 0x01000000, just return the constant
      // directly. @see http://www.w3.org/TR/CSS2/cascade.html#specificity
      return 0x01000000;
    }

    if (this._specificity) {
      return this._specificity;
    }

    this._specificity = domUtils.getSpecificity(this.cssRule.domRule,
                                                this.selectorIndex);

    return this._specificity;
  },

  toString: function CssSelector_toString()
  {
    return this.text;
  },
};

/**
 * A cache of information about the matched rules, selectors and values attached
 * to a CSS property, for the highlighted element.
 *
 * The heart of the CssPropertyInfo object is the _findMatchedSelectors()
 * method. This are invoked when the PropertyView tries to access the
 * .matchedSelectors array.
 * Results are cached, for later reuse.
 *
 * @param {CssLogic} aCssLogic Reference to the parent CssLogic instance
 * @param {string} aProperty The CSS property we are gathering information for
 * @constructor
 */
function CssPropertyInfo(aCssLogic, aProperty)
{
  this._cssLogic = aCssLogic;
  this.property = aProperty;
  this._value = "";

  // The number of matched rules holding the this.property style property.
  // Additionally, only rules that come from allowed stylesheets are counted.
  this._matchedRuleCount = 0;

  // An array holding CssSelectorInfo objects for each of the matched selectors
  // that are inside a CSS rule. Only rules that hold the this.property are
  // counted. This includes rules that come from filtered stylesheets (those
  // that have sheetAllowed = false).
  this._matchedSelectors = null;
}

CssPropertyInfo.prototype = {
  /**
   * Retrieve the computed style value for the current property, for the
   * highlighted element.
   *
   * @return {string} the computed style value for the current property, for the
   * highlighted element.
   */
  get value()
  {
    if (!this._value && this._cssLogic.computedStyle) {
      try {
        this._value = this._cssLogic.computedStyle.getPropertyValue(this.property);
      } catch (ex) {
        Services.console.logStringMessage('Error reading computed style for ' +
          this.property);
        Services.console.logStringMessage(ex);
      }
    }
    return this._value;
  },

  /**
   * Retrieve the number of matched rules holding the this.property style
   * property. Only rules that come from allowed stylesheets are counted.
   *
   * @return {number} the number of matched rules.
   */
  get matchedRuleCount()
  {
    if (!this._matchedSelectors) {
      this._findMatchedSelectors();
    } else if (this.needRefilter) {
      this._refilterSelectors();
    }

    return this._matchedRuleCount;
  },

  /**
   * Retrieve the array holding CssSelectorInfo objects for each of the matched
   * selectors, from each of the matched rules. Only selectors coming from
   * allowed stylesheets are included in the array.
   *
   * @return {array} the list of CssSelectorInfo objects of selectors that match
   * the highlighted element and its parents.
   */
  get matchedSelectors()
  {
    if (!this._matchedSelectors) {
      this._findMatchedSelectors();
    } else if (this.needRefilter) {
      this._refilterSelectors();
    }

    return this._matchedSelectors;
  },

  /**
   * Find the selectors that match the highlighted element and its parents.
   * Uses CssLogic.processMatchedSelectors() to find the matched selectors,
   * passing in a reference to CssPropertyInfo._processMatchedSelector() to
   * create CssSelectorInfo objects, which we then sort
   * @private
   */
  _findMatchedSelectors: function CssPropertyInfo_findMatchedSelectors()
  {
    this._matchedSelectors = [];
    this._matchedRuleCount = 0;
    this.needRefilter = false;

    this._cssLogic.processMatchedSelectors(this._processMatchedSelector, this);

    // Sort the selectors by how well they match the given element.
    this._matchedSelectors.sort(function(aSelectorInfo1, aSelectorInfo2) {
      if (aSelectorInfo1.status > aSelectorInfo2.status) {
        return -1;
      } else if (aSelectorInfo2.status > aSelectorInfo1.status) {
        return 1;
      } else {
        return aSelectorInfo1.compareTo(aSelectorInfo2);
      }
    });

    // Now we know which of the matches is best, we can mark it BEST_MATCH.
    if (this._matchedSelectors.length > 0 &&
        this._matchedSelectors[0].status > CssLogic.STATUS.UNMATCHED) {
      this._matchedSelectors[0].status = CssLogic.STATUS.BEST;
    }
  },

  /**
   * Process a matched CssSelector object.
   *
   * @private
   * @param {CssSelector} aSelector the matched CssSelector object.
   * @param {CssLogic.STATUS} aStatus the CssSelector match status.
   */
  _processMatchedSelector: function CssPropertyInfo_processMatchedSelector(aSelector, aStatus)
  {
    let cssRule = aSelector.cssRule;
    let value = cssRule.getPropertyValue(this.property);
    if (value &&
        (aStatus == CssLogic.STATUS.MATCHED ||
         (aStatus == CssLogic.STATUS.PARENT_MATCH &&
          domUtils.isInheritedProperty(this.property)))) {
      let selectorInfo = new CssSelectorInfo(aSelector, this.property, value,
          aStatus);
      this._matchedSelectors.push(selectorInfo);
      if (this._cssLogic._passId !== cssRule._passId && cssRule.sheetAllowed) {
        this._matchedRuleCount++;
      }
    }
  },

  /**
   * Refilter the matched selectors array when the CssLogic.sourceFilter
   * changes. This allows for quick filter changes.
   * @private
   */
  _refilterSelectors: function CssPropertyInfo_refilterSelectors()
  {
    let passId = ++this._cssLogic._passId;
    let ruleCount = 0;

    let iterator = function(aSelectorInfo) {
      let cssRule = aSelectorInfo.selector.cssRule;
      if (cssRule._passId != passId) {
        if (cssRule.sheetAllowed) {
          ruleCount++;
        }
        cssRule._passId = passId;
      }
    };

    if (this._matchedSelectors) {
      this._matchedSelectors.forEach(iterator);
      this._matchedRuleCount = ruleCount;
    }

    this.needRefilter = false;
  },

  toString: function CssPropertyInfo_toString()
  {
    return "CssPropertyInfo[" + this.property + "]";
  },
};

/**
 * A class that holds information about a given CssSelector object.
 *
 * Instances of this class are given to CssHtmlTree in the array of matched
 * selectors. Each such object represents a displayable row in the PropertyView
 * objects. The information given by this object blends data coming from the
 * CssSheet, CssRule and from the CssSelector that own this object.
 *
 * @param {CssSelector} aSelector The CssSelector object for which to present information.
 * @param {string} aProperty The property for which information should be retrieved.
 * @param {string} aValue The property value from the CssRule that owns the selector.
 * @param {CssLogic.STATUS} aStatus The selector match status.
 * @constructor
 */
function CssSelectorInfo(aSelector, aProperty, aValue, aStatus)
{
  this.selector = aSelector;
  this.property = aProperty;
  this.status = aStatus;
  this.value = aValue;
  let priority = this.selector.cssRule.getPropertyPriority(this.property);
  this.important = (priority === "important");
}

CssSelectorInfo.prototype = {
  /**
   * Retrieve the CssSelector source, which is the source of the CssSheet owning
   * the selector.
   *
   * @return {string} the selector source.
   */
  get source()
  {
    return this.selector.source;
  },

  /**
   * Retrieve the CssSelector source element, which is the source of the CssRule
   * owning the selector. This is only available when the CssSelector comes from
   * an element.style.
   *
   * @return {string} the source element selector.
   */
  get sourceElement()
  {
    return this.selector.sourceElement;
  },

  /**
   * Retrieve the address of the CssSelector. This points to the address of the
   * CssSheet owning this selector.
   *
   * @return {string} the address of the CssSelector.
   */
  get href()
  {
    return this.selector.href;
  },

  /**
   * Check if the CssSelector comes from element.style or not.
   *
   * @return {boolean} true if the CssSelector comes from element.style, or
   * false otherwise.
   */
  get elementStyle()
  {
    return this.selector.elementStyle;
  },

  /**
   * Retrieve specificity information for the current selector.
   *
   * @return {object} an object holding specificity information for the current
   * selector.
   */
  get specificity()
  {
    return this.selector.specificity;
  },

  /**
   * Retrieve the parent stylesheet index/position in the viewed document.
   *
   * @return {number} the parent stylesheet index/position in the viewed
   * document.
   */
  get sheetIndex()
  {
    return this.selector.sheetIndex;
  },

  /**
   * Check if the parent stylesheet is allowed by the CssLogic.sourceFilter.
   *
   * @return {boolean} true if the parent stylesheet is allowed by the current
   * sourceFilter, or false otherwise.
   */
  get sheetAllowed()
  {
    return this.selector.sheetAllowed;
  },

  /**
   * Retrieve the line of the parent CSSStyleRule in the parent CSSStyleSheet.
   *
   * @return {number} the line of the parent CSSStyleRule in the parent
   * stylesheet.
   */
  get ruleLine()
  {
    return this.selector.ruleLine;
  },

  /**
   * Check if the selector comes from a browser-provided stylesheet.
   *
   * @return {boolean} true if the selector comes from a browser-provided
   * stylesheet, or false otherwise.
   */
  get contentRule()
  {
    return this.selector.contentRule;
  },

  /**
   * Compare the current CssSelectorInfo instance to another instance, based on
   * specificity information.
   *
   * @param {CssSelectorInfo} aThat The instance to compare ourselves against.
   * @return number -1, 0, 1 depending on how aThat compares with this.
   */
  compareTo: function CssSelectorInfo_compareTo(aThat)
  {
    if (!this.contentRule && aThat.contentRule) return 1;
    if (this.contentRule && !aThat.contentRule) return -1;

    if (this.elementStyle && !aThat.elementStyle) {
      if (!this.important && aThat.important) return 1;
      else return -1;
    }

    if (!this.elementStyle && aThat.elementStyle) {
      if (this.important && !aThat.important) return -1;
      else return 1;
    }

    if (this.important && !aThat.important) return -1;
    if (aThat.important && !this.important) return 1;

    if (this.specificity > aThat.specificity) return -1;
    if (aThat.specificity > this.specificity) return 1;

    if (this.sheetIndex > aThat.sheetIndex) return -1;
    if (aThat.sheetIndex > this.sheetIndex) return 1;

    if (this.ruleLine > aThat.ruleLine) return -1;
    if (aThat.ruleLine > this.ruleLine) return 1;

    return 0;
  },

  toString: function CssSelectorInfo_toString()
  {
    return this.selector + " -> " + this.value;
  },
};

DevToolsUtils.defineLazyGetter(this, "domUtils", function() {
  return Cc["@mozilla.org/inspector/dom-utils;1"].getService(Ci.inIDOMUtils);
});
