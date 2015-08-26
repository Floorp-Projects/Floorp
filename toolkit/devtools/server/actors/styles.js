/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* globals CssLogic, DOMUtils, CSS */

"use strict";

const {Cc, Ci, Cu} = require("chrome");
const promise = require("promise");
const protocol = require("devtools/server/protocol");
const {Arg, Option, method, RetVal, types} = protocol;
const events = require("sdk/event/core");
const {Class} = require("sdk/core/heritage");
const {LongStringActor} = require("devtools/server/actors/string");
const {PSEUDO_ELEMENT_SET} = require("devtools/styleinspector/css-logic");

// This will add the "stylesheet" actor type for protocol.js to recognize
require("devtools/server/actors/stylesheets");

loader.lazyGetter(this, "CssLogic", () => {
  return require("devtools/styleinspector/css-logic").CssLogic;
});
loader.lazyGetter(this, "DOMUtils", () => {
  return Cc["@mozilla.org/inspector/dom-utils;1"].getService(Ci.inIDOMUtils);
});

// The PageStyle actor flattens the DOM CSS objects a little bit, merging
// Rules and their Styles into one actor.  For elements (which have a style
// but no associated rule) we fake a rule with the following style id.
const ELEMENT_STYLE = 100;
exports.ELEMENT_STYLE = ELEMENT_STYLE;

// Not included since these are uneditable by the user.
// See https://hg.mozilla.org/mozilla-central/file/696a4ad5d011/layout/style/nsCSSPseudoElementList.h#l74
PSEUDO_ELEMENT_SET.delete(":-moz-meter-bar");
PSEUDO_ELEMENT_SET.delete(":-moz-list-bullet");
PSEUDO_ELEMENT_SET.delete(":-moz-list-number");
PSEUDO_ELEMENT_SET.delete(":-moz-focus-inner");
PSEUDO_ELEMENT_SET.delete(":-moz-focus-outer");
PSEUDO_ELEMENT_SET.delete(":-moz-math-anonymous");
PSEUDO_ELEMENT_SET.delete(":-moz-math-stretchy");

const PSEUDO_ELEMENTS = Array.from(PSEUDO_ELEMENT_SET);

exports.PSEUDO_ELEMENTS = PSEUDO_ELEMENTS;

// When gathering rules to read for pseudo elements, we will skip
// :before and :after, which are handled as a special case.
const PSEUDO_ELEMENTS_TO_READ = PSEUDO_ELEMENTS.filter(pseudo => {
  return pseudo !== ":before" && pseudo !== ":after";
});

const XHTML_NS = "http://www.w3.org/1999/xhtml";
const FONT_PREVIEW_TEXT = "Abc";
const FONT_PREVIEW_FONT_SIZE = 40;
const FONT_PREVIEW_FILLSTYLE = "black";
const NORMAL_FONT_WEIGHT = 400;
const BOLD_FONT_WEIGHT = 700;
// Offset (in px) to avoid cutting off text edges of italic fonts.
const FONT_PREVIEW_OFFSET = 4;

// Predeclare the domnode actor type for use in requests.
types.addActorType("domnode");

// Predeclare the domstylerule actor type
types.addActorType("domstylerule");

/**
 * DOM Nodes returned by the style actor will be owned by the DOM walker
 * for the connection.
  */
types.addLifetime("walker", "walker");

/**
 * When asking for the styles applied to a node, we return a list of
 * appliedstyle json objects that lists the rules that apply to the node
 * and which element they were inherited from (if any).
 *
 * Note appliedstyle only sends the list of actorIDs and is not a valid return
 * value on its own. appliedstyle should be returned with the actual list of
 * StyleRuleActor and StyleSheetActor. See appliedStylesReturn.
 */
types.addDictType("appliedstyle", {
  rule: "domstylerule#actorid",
  inherited: "nullable:domnode#actorid",
  keyframes: "nullable:domstylerule#actorid"
});

types.addDictType("matchedselector", {
  rule: "domstylerule#actorid",
  selector: "string",
  value: "string",
  status: "number"
});

types.addDictType("appliedStylesReturn", {
  entries: "array:appliedstyle",
  rules: "array:domstylerule",
  sheets: "array:stylesheet"
});

types.addDictType("modifiedStylesReturn", {
  isMatching: RetVal("boolean"),
  ruleProps: RetVal("nullable:appliedStylesReturn")
});

types.addDictType("fontpreview", {
  data: "nullable:longstring",
  size: "json"
});

types.addDictType("fontface", {
  name: "string",
  CSSFamilyName: "string",
  rule: "nullable:domstylerule",
  srcIndex: "number",
  URI: "string",
  format: "string",
  preview: "nullable:fontpreview",
  localName: "string",
  metadata: "string"
});

/**
 * The PageStyle actor lets the client look at the styles on a page, as
 * they are applied to a given node.
 */
let PageStyleActor = protocol.ActorClass({
  typeName: "pagestyle",

  /**
   * Create a PageStyleActor.
   *
   * @param inspector
   *    The InspectorActor that owns this PageStyleActor.
   *
   * @constructor
   */
  initialize: function(inspector) {
    protocol.Actor.prototype.initialize.call(this, null);
    this.inspector = inspector;
    if (!this.inspector.walker) {
      throw Error("The inspector's WalkerActor must be created before " +
                   "creating a PageStyleActor.");
    }
    this.walker = inspector.walker;
    this.cssLogic = new CssLogic();

    // Stores the association of DOM objects -> actors
    this.refMap = new Map();

    this.onFrameUnload = this.onFrameUnload.bind(this);
    events.on(this.inspector.tabActor, "will-navigate", this.onFrameUnload);
  },

  get conn() {
    return this.inspector.conn;
  },

  form: function(detail) {
    if (detail === "actorid") {
      return this.actorID;
    }

    return {
      actor: this.actorID,
      traits: {
        // Whether the actor has had bug 1103993 fixed, which means that the
        // getApplied method calls cssLogic.highlight(node) to recreate the
        // style cache. Clients requesting getApplied from actors that have not
        // been fixed must make sure cssLogic.highlight(node) was called before.
        getAppliedCreatesStyleCache: true
      }
    };
  },

  /**
   * Return or create a StyleRuleActor for the given item.
   * @param item Either a CSSStyleRule or a DOM element.
   */
  _styleRef: function(item) {
    if (this.refMap.has(item)) {
      return this.refMap.get(item);
    }
    let actor = StyleRuleActor(this, item);
    this.manage(actor);
    this.refMap.set(item, actor);

    return actor;
  },

  /**
   * Return or create a StyleSheetActor for the given nsIDOMCSSStyleSheet.
   * @param  {DOMStyleSheet} sheet
   *         The style sheet to create an actor for.
   * @return {StyleSheetActor}
   *         The actor for this style sheet
   */
  _sheetRef: function(sheet) {
    let tabActor = this.inspector.tabActor;
    let actor = tabActor.createStyleSheetActor(sheet);
    return actor;
  },

  /**
   * Get the computed style for a node.
   *
   * @param NodeActor node
   * @param object options
   *   `filter`: A string filter that affects the "matched" handling.
   *     'user': Include properties from user style sheets.
   *     'ua': Include properties from user and user-agent sheets.
   *     Default value is 'ua'
   *   `markMatched`: true if you want the 'matched' property to be added
   *     when a computed property has been modified by a style included
   *     by `filter`.
   *   `onlyMatched`: true if unmatched properties shouldn't be included.
   *
   * @returns a JSON blob with the following form:
   *   {
   *     "property-name": {
   *       value: "property-value",
   *       priority: "!important" <optional>
   *       matched: <true if there are matched selectors for this value>
   *     },
   *     ...
   *   }
   */
  getComputed: method(function(node, options) {
    let ret = Object.create(null);

    this.cssLogic.sourceFilter = options.filter || CssLogic.FILTER.UA;
    this.cssLogic.highlight(node.rawNode);
    let computed = this.cssLogic.computedStyle || [];

    Array.prototype.forEach.call(computed, name => {
      ret[name] = {
        value: computed.getPropertyValue(name),
        priority: computed.getPropertyPriority(name) || undefined
      };
    });

    if (options.markMatched || options.onlyMatched) {
      let matched = this.cssLogic.hasMatchedSelectors(Object.keys(ret));
      for (let key in ret) {
        if (matched[key]) {
          ret[key].matched = options.markMatched ? true : undefined;
        } else if (options.onlyMatched) {
          delete ret[key];
        }
      }
    }

    return ret;
  }, {
    request: {
      node: Arg(0, "domnode"),
      markMatched: Option(1, "boolean"),
      onlyMatched: Option(1, "boolean"),
      filter: Option(1, "string"),
    },
    response: {
      computed: RetVal("json")
    }
  }),

  /**
   * Get all the fonts from a page.
   *
   * @param object options
   *   `includePreviews`: Whether to also return image previews of the fonts.
   *   `previewText`: The text to display in the previews.
   *   `previewFontSize`: The font size of the text in the previews.
   *
   * @returns object
   *   object with 'fontFaces', a list of fonts that apply to this node.
   */
  getAllUsedFontFaces: method(function(options) {
    let windows = this.inspector.tabActor.windows;
    let fontsList = [];
    for (let win of windows) {
      fontsList = [...fontsList,
                   ...this.getUsedFontFaces(win.document.body, options)];
    }
    return fontsList;
  }, {
    request: {
      includePreviews: Option(0, "boolean"),
      previewText: Option(0, "string"),
      previewFontSize: Option(0, "string"),
      previewFillStyle: Option(0, "string")
    },
    response: {
      fontFaces: RetVal("array:fontface")
    }
  }),

  /**
   * Get the font faces used in an element.
   *
   * @param NodeActor node / actual DOM node
   *    The node to get fonts from.
   * @param object options
   *   `includePreviews`: Whether to also return image previews of the fonts.
   *   `previewText`: The text to display in the previews.
   *   `previewFontSize`: The font size of the text in the previews.
   *
   * @returns object
   *   object with 'fontFaces', a list of fonts that apply to this node.
   */
  getUsedFontFaces: method(function(node, options) {
    // node.rawNode is defined for NodeActor objects
    let actualNode = node.rawNode || node;
    let contentDocument = actualNode.ownerDocument;
    // We don't get fonts for a node, but for a range
    let rng = contentDocument.createRange();
    rng.selectNodeContents(actualNode);
    let fonts = DOMUtils.getUsedFontFaces(rng);
    let fontsArray = [];

    for (let i = 0; i < fonts.length; i++) {
      let font = fonts.item(i);
      let fontFace = {
        name: font.name,
        CSSFamilyName: font.CSSFamilyName,
        srcIndex: font.srcIndex,
        URI: font.URI,
        format: font.format,
        localName: font.localName,
        metadata: font.metadata
      };

      // If this font comes from a @font-face rule
      if (font.rule) {
        fontFace.rule = StyleRuleActor(this, font.rule);
        fontFace.ruleText = font.rule.cssText;
      }

      // Get the weight and style of this font for the preview and sort order
      let weight = NORMAL_FONT_WEIGHT, style = "";
      if (font.rule) {
        weight = font.rule.style.getPropertyValue("font-weight")
                 || NORMAL_FONT_WEIGHT;
        if (weight == "bold") {
          weight = BOLD_FONT_WEIGHT;
        } else if (weight == "normal") {
          weight = NORMAL_FONT_WEIGHT;
        }
        style = font.rule.style.getPropertyValue("font-style") || "";
      }
      fontFace.weight = weight;
      fontFace.style = style;

      if (options.includePreviews) {
        let opts = {
          previewText: options.previewText,
          previewFontSize: options.previewFontSize,
          fontStyle: weight + " " + style,
          fillStyle: options.previewFillStyle
        };
        let { dataURL, size } = getFontPreviewData(font.CSSFamilyName,
                                                   contentDocument, opts);
        fontFace.preview = {
          data: LongStringActor(this.conn, dataURL),
          size: size
        };
      }
      fontsArray.push(fontFace);
    }

    // @font-face fonts at the top, then alphabetically, then by weight
    fontsArray.sort(function(a, b) {
      return a.weight > b.weight ? 1 : -1;
    });
    fontsArray.sort(function(a, b) {
      if (a.CSSFamilyName == b.CSSFamilyName) {
        return 0;
      }
      return a.CSSFamilyName > b.CSSFamilyName ? 1 : -1;
    });
    fontsArray.sort(function(a, b) {
      if ((a.rule && b.rule) || (!a.rule && !b.rule)) {
        return 0;
      }
      return !a.rule && b.rule ? 1 : -1;
    });

    return fontsArray;
  }, {
    request: {
      node: Arg(0, "domnode"),
      includePreviews: Option(1, "boolean"),
      previewText: Option(1, "string"),
      previewFontSize: Option(1, "string"),
      previewFillStyle: Option(1, "string")
    },
    response: {
      fontFaces: RetVal("array:fontface")
    }
  }),

  /**
   * Get a list of selectors that match a given property for a node.
   *
   * @param NodeActor node
   * @param string property
   * @param object options
   *   `filter`: A string filter that affects the "matched" handling.
   *     'user': Include properties from user style sheets.
   *     'ua': Include properties from user and user-agent sheets.
   *     Default value is 'ua'
   *
   * @returns a JSON object with the following form:
   *   {
   *     // An ordered list of rules that apply
   *     matched: [{
   *       rule: <rule actorid>,
   *       sourceText: <string>, // The source of the selector, relative
   *                             // to the node in question.
   *       selector: <string>, // the selector ID that matched
   *       value: <string>, // the value of the property
   *       status: <int>,
   *         // The status of the match - high numbers are better placed
   *         // to provide styling information:
   *         // 3: Best match, was used.
   *         // 2: Matched, but was overridden.
   *         // 1: Rule from a parent matched.
   *         // 0: Unmatched (never returned in this API)
   *     }, ...],
   *
   *     // The full form of any domrule referenced.
   *     rules: [ <domrule>, ... ], // The full form of any domrule referenced
   *
   *     // The full form of any sheets referenced.
   *     sheets: [ <domsheet>, ... ]
   *  }
   */
  getMatchedSelectors: method(function(node, property, options) {
    this.cssLogic.sourceFilter = options.filter || CssLogic.FILTER.UA;
    this.cssLogic.highlight(node.rawNode);

    let rules = new Set();
    let sheets = new Set();

    let matched = [];
    let propInfo = this.cssLogic.getPropertyInfo(property);
    for (let selectorInfo of propInfo.matchedSelectors) {
      let cssRule = selectorInfo.selector.cssRule;
      let domRule = cssRule.sourceElement || cssRule.domRule;

      let rule = this._styleRef(domRule);
      rules.add(rule);

      matched.push({
        rule: rule,
        sourceText: this.getSelectorSource(selectorInfo, node.rawNode),
        selector: selectorInfo.selector.text,
        name: selectorInfo.property,
        value: selectorInfo.value,
        status: selectorInfo.status
      });
    }

    this.expandSets(rules, sheets);

    return {
      matched: matched,
      rules: [...rules],
      sheets: [...sheets]
    };
  }, {
    request: {
      node: Arg(0, "domnode"),
      property: Arg(1, "string"),
      filter: Option(2, "string")
    },
    response: RetVal(types.addDictType("matchedselectorresponse", {
      rules: "array:domstylerule",
      sheets: "array:stylesheet",
      matched: "array:matchedselector"
    }))
  }),

  // Get a selector source for a CssSelectorInfo relative to a given
  // node.
  getSelectorSource: function(selectorInfo, relativeTo) {
    let result = selectorInfo.selector.text;
    if (selectorInfo.elementStyle) {
      let source = selectorInfo.sourceElement;
      if (source === relativeTo) {
        result = "this";
      } else {
        result = CssLogic.getShortName(source);
      }
      result += ".style";
    }
    return result;
  },

  /**
   * Get the set of styles that apply to a given node.
   * @param NodeActor node
   * @param object options
   *   `filter`: A string filter that affects the "matched" handling.
   *     'user': Include properties from user style sheets.
   *     'ua': Include properties from user and user-agent sheets.
   *     Default value is 'ua'
   *   `inherited`: Include styles inherited from parent nodes.
   *   `matchedSelectors`: Include an array of specific selectors that
   *     caused this rule to match its node.
   */
  getApplied: method(function(node, options) {
    if (!node) {
      return {entries: [], rules: [], sheets: []};
    }

    this.cssLogic.highlight(node.rawNode);
    let entries = [];
    entries = entries.concat(this._getAllElementRules(node, undefined, options));
    return this.getAppliedProps(node, entries, options);
  }, {
    request: {
      node: Arg(0, "domnode"),
      inherited: Option(1, "boolean"),
      matchedSelectors: Option(1, "boolean"),
      filter: Option(1, "string")
    },
    response: RetVal("appliedStylesReturn")
  }),

  _hasInheritedProps: function(style) {
    return Array.prototype.some.call(style, prop => {
      return DOMUtils.isInheritedProperty(prop);
    });
  },

  /**
   * Helper function for getApplied, gets all the rules from a given
   * element. See getApplied for documentation on parameters.
   * @param NodeActor node
   * @param bool inherited
   * @param object options

   * @return Array The rules for a given element. Each item in the
   *               array has the following signature:
   *                - rule RuleActor
   *                - isSystem Boolean
   *                - inherited Boolean
   *                - pseudoElement String
   */
  _getAllElementRules: function(node, inherited, options) {
    let {bindingElement, pseudo} = CssLogic.getBindingElementAndPseudo(node.rawNode);
    let rules = [];

    if (!bindingElement || !bindingElement.style) {
      return rules;
    }

    let elementStyle = this._styleRef(bindingElement);
    let showElementStyles = !inherited && !pseudo;
    let showInheritedStyles = inherited &&
                              this._hasInheritedProps(bindingElement.style);

    let rule = {
      rule: elementStyle,
      pseudoElement: null,
      isSystem: false,
      inherited: false
    };

    // First any inline styles
    if (showElementStyles) {
      rules.push(rule);
    }

    // Now any inherited styles
    if (showInheritedStyles) {
      rule.inherited = inherited;
      rules.push(rule);
    }

    // Add normal rules.  Typically this is passing in the node passed into the
    // function, unless if that node was ::before/::after.  In which case,
    // it will pass in the parentNode along with "::before"/"::after".
    this._getElementRules(bindingElement, pseudo, inherited, options).forEach(rule => {
      // The only case when there would be a pseudo here is ::before/::after,
      // and in this case we want to tell the view that it belongs to the
      // element (which is a _moz_generated_content native anonymous element).
      rule.pseudoElement = null;
      rules.push(rule);
    });

    // Now any pseudos (except for ::before / ::after, which was handled as
    // a 'normal rule' above.
    if (showElementStyles) {
      for (let pseudo of PSEUDO_ELEMENTS_TO_READ) {
        this._getElementRules(bindingElement, pseudo, inherited, options).forEach(rule => {
          rules.push(rule);
        });
      }
    }

    return rules;
  },

  /**
   * Helper function for _getAllElementRules, returns the rules from a given
   * element. See getApplied for documentation on parameters.
   * @param DOMNode node
   * @param string pseudo
   * @param DOMNode inherited
   * @param object options
   *
   * @returns Array
   */
  _getElementRules: function(node, pseudo, inherited, options) {
    let domRules = DOMUtils.getCSSStyleRules(node, pseudo);
    if (!domRules) {
      return [];
    }

    let rules = [];

    // getCSSStyleRules returns ordered from least-specific to
    // most-specific.
    for (let i = domRules.Count() - 1; i >= 0; i--) {
      let domRule = domRules.GetElementAt(i);

      let isSystem = !CssLogic.isContentStylesheet(domRule.parentStyleSheet);

      if (isSystem && options.filter != CssLogic.FILTER.UA) {
        continue;
      }

      if (inherited) {
        // Don't include inherited rules if none of its properties
        // are inheritable.
        let hasInherited = [...domRule.style].some(
          prop => DOMUtils.isInheritedProperty(prop)
        );
        if (!hasInherited) {
          continue;
        }
      }

      let ruleActor = this._styleRef(domRule);
      rules.push({
        rule: ruleActor,
        inherited: inherited,
        isSystem: isSystem,
        pseudoElement: pseudo
      });
    }
    return rules;
  },

  /**
   * Helper function for getApplied that fetches a set of style properties that
   * apply to the given node and associated rules
   * @param NodeActor node
   * @param object options
   *   `filter`: A string filter that affects the "matched" handling.
   *     'user': Include properties from user style sheets.
   *     'ua': Include properties from user and user-agent sheets.
   *     Default value is 'ua'
   *   `inherited`: Include styles inherited from parent nodes.
   *   `matchedSeletors`: Include an array of specific selectors that
   *     caused this rule to match its node.
   * @param array entries
   *   List of appliedstyle objects that lists the rules that apply to the
   *   node. If adding a new rule to the stylesheet, only the new rule entry
   *   is provided and only the style properties that apply to the new
   *   rule is fetched.
   * @returns Object containing the list of rule entries, rule actors and
   *   stylesheet actors that applies to the given node and its associated
   *   rules.
   */
  getAppliedProps: function(node, entries, options) {
    if (options.inherited) {
      let parent = this.walker.parentNode(node);
      while (parent && parent.rawNode.nodeType != Ci.nsIDOMNode.DOCUMENT_NODE) {
        entries = entries.concat(this._getAllElementRules(parent, parent, options));
        parent = this.walker.parentNode(parent);
      }
    }

    if (options.matchedSelectors) {
      for (let entry of entries) {
        if (entry.rule.type === ELEMENT_STYLE) {
          continue;
        }

        let domRule = entry.rule.rawRule;
        let selectors = CssLogic.getSelectors(domRule);
        let element = entry.inherited ? entry.inherited.rawNode : node.rawNode;

        let {bindingElement, pseudo} = CssLogic.getBindingElementAndPseudo(element);
        entry.matchedSelectors = [];
        for (let i = 0; i < selectors.length; i++) {
          if (DOMUtils.selectorMatchesElement(bindingElement, domRule, i, pseudo)) {
            entry.matchedSelectors.push(selectors[i]);
          }
        }
      }
    }

    // Add all the keyframes rule associated with the element
    let computedStyle = this.cssLogic.computedStyle;
    if (computedStyle) {
      let animationNames = computedStyle.animationName.split(",");
      animationNames = animationNames.map(name => name.trim());

      if (animationNames) {
        // Traverse through all the available keyframes rule and add
        // the keyframes rule that matches the computed animation name
        for (let keyframesRule of this.cssLogic.keyframesRules) {
          if (animationNames.indexOf(keyframesRule.name) > -1) {
            for (let rule of keyframesRule.cssRules) {
              entries.push({
                rule: this._styleRef(rule),
                keyframes: this._styleRef(keyframesRule)
              });
            }
          }
        }
      }
    }

    let rules = new Set();
    let sheets = new Set();
    entries.forEach(entry => rules.add(entry.rule));
    this.expandSets(rules, sheets);

    return {
      entries: entries,
      rules: [...rules],
      sheets: [...sheets]
    };
  },

  /**
   * Expand Sets of rules and sheets to include all parent rules and sheets.
   */
  expandSets: function(ruleSet, sheetSet) {
    // Sets include new items in their iteration
    for (let rule of ruleSet) {
      if (rule.rawRule.parentRule) {
        let parent = this._styleRef(rule.rawRule.parentRule);
        if (!ruleSet.has(parent)) {
          ruleSet.add(parent);
        }
      }
      if (rule.rawRule.parentStyleSheet) {
        let parent = this._sheetRef(rule.rawRule.parentStyleSheet);
        if (!sheetSet.has(parent)) {
          sheetSet.add(parent);
        }
      }
    }

    for (let sheet of sheetSet) {
      if (sheet.rawSheet.parentStyleSheet) {
        let parent = this._sheetRef(sheet.rawSheet.parentStyleSheet);
        if (!sheetSet.has(parent)) {
          sheetSet.add(parent);
        }
      }
    }
  },

  /**
   * Get layout-related information about a node.
   * This method returns an object with properties giving information about
   * the node's margin, border, padding and content region sizes, as well
   * as information about the type of box, its position, z-index, etc...
   * @param {NodeActor} node
   * @param {Object} options The only available option is autoMargins.
   * If set to true, the element's margins will receive an extra check to see
   * whether they are set to "auto" (knowing that the computed-style in this
   * case would return "0px").
   * The returned object will contain an extra property (autoMargins) listing
   * all margins that are set to auto, e.g. {top: "auto", left: "auto"}.
   * @return {Object}
   */
  getLayout: method(function(node, options) {
    this.cssLogic.highlight(node.rawNode);

    let layout = {};

    // First, we update the first part of the layout view, with
    // the size of the element.

    let clientRect = node.rawNode.getBoundingClientRect();
    layout.width = parseFloat(clientRect.width.toPrecision(6));
    layout.height = parseFloat(clientRect.height.toPrecision(6));

    // We compute and update the values of margins & co.
    let style = CssLogic.getComputedStyle(node.rawNode);
    for (let prop of [
      "position",
      "margin-top",
      "margin-right",
      "margin-bottom",
      "margin-left",
      "padding-top",
      "padding-right",
      "padding-bottom",
      "padding-left",
      "border-top-width",
      "border-right-width",
      "border-bottom-width",
      "border-left-width",
      "z-index",
      "box-sizing",
      "display"
    ]) {
      layout[prop] = style.getPropertyValue(prop);
    }

    if (options.autoMargins) {
      layout.autoMargins = this.processMargins(this.cssLogic);
    }

    for (let i in this.map) {
      let property = this.map[i].property;
      this.map[i].value = parseFloat(style.getPropertyValue(property));
    }

    return layout;
  }, {
    request: {
      node: Arg(0, "domnode"),
      autoMargins: Option(1, "boolean")
    },
    response: RetVal("json")
  }),

  /**
   * Find 'auto' margin properties.
   */
  processMargins: function(cssLogic) {
    let margins = {};

    for (let prop of ["top", "bottom", "left", "right"]) {
      let info = cssLogic.getPropertyInfo("margin-" + prop);
      let selectors = info.matchedSelectors;
      if (selectors && selectors.length > 0 && selectors[0].value == "auto") {
        margins[prop] = "auto";
      }
    }

    return margins;
  },

  /**
   * On page navigation, tidy up remaining objects.
   */
  onFrameUnload: function() {
    this._styleElement = null;
  },

  /**
   * Helper function to addNewRule to construct a new style tag in the document.
   * @returns DOMElement of the style tag
   */
  get styleElement() {
    if (!this._styleElement) {
      let document = this.inspector.window.document;
      let style = document.createElementNS(XHTML_NS, "style");
      style.setAttribute("type", "text/css");
      document.documentElement.appendChild(style);
      this._styleElement = style;
    }

    return this._styleElement;
  },

  /**
   * Helper function for adding a new rule and getting its applied style
   * properties
   * @param NodeActor node
   * @param CSSStyleRUle rule
   * @returns Object containing its applied style properties
   */
  getNewAppliedProps: function(node, rule) {
    let ruleActor = this._styleRef(rule);
    return this.getAppliedProps(node, [{ rule: ruleActor }],
      { matchedSelectors: true });
  },

  /**
   * Adds a new rule, and returns the new StyleRuleActor.
   * @param NodeActor node
   * @param [string] pseudoClasses The list of pseudo classes to append to the
   * new selector.
   * @returns StyleRuleActor of the new rule
   */
  addNewRule: method(function(node, pseudoClasses) {
    let style = this.styleElement;
    let sheet = style.sheet;
    let cssRules = sheet.cssRules;
    let rawNode = node.rawNode;

    let selector;
    if (rawNode.id) {
      selector = "#" + CSS.escape(rawNode.id);
    } else if (rawNode.className) {
      selector = "." + [...rawNode.classList].map(c => CSS.escape(c)).join(".");
    } else {
      selector = rawNode.tagName.toLowerCase();
    }

    if (pseudoClasses && pseudoClasses.length > 0) {
      selector += pseudoClasses.join("");
    }

    let index = sheet.insertRule(selector + " {}", cssRules.length);
    return this.getNewAppliedProps(node, cssRules.item(index));
  }, {
    request: {
      node: Arg(0, "domnode"),
      pseudoClasses: Arg(1, "nullable:array:string")
    },
    response: RetVal("appliedStylesReturn")
  }),
});
exports.PageStyleActor = PageStyleActor;

/**
 * Front object for the PageStyleActor
 */
let PageStyleFront = protocol.FrontClass(PageStyleActor, {
  initialize: function(conn, form, ctx, detail) {
    protocol.Front.prototype.initialize.call(this, conn, form, ctx, detail);
    this.inspector = this.parent();
  },

  form: function(form, detail) {
    if (detail === "actorid") {
      this.actorID = form;
      return;
    }
    this._form = form;
  },

  destroy: function() {
    protocol.Front.prototype.destroy.call(this);
  },

  get walker() {
    return this.inspector.walker;
  },

  getMatchedSelectors: protocol.custom(function(node, property, options) {
    return this._getMatchedSelectors(node, property, options).then(ret => {
      return ret.matched;
    });
  }, {
    impl: "_getMatchedSelectors"
  }),

  getApplied: protocol.custom(Task.async(function*(node, options={}) {
    // If the getApplied method doesn't recreate the style cache itself, this
    // means a call to cssLogic.highlight is required before trying to access
    // the applied rules. Issue a request to getLayout if this is the case.
    // See https://bugzilla.mozilla.org/show_bug.cgi?id=1103993#c16.
    if (!this._form.traits || !this._form.traits.getAppliedCreatesStyleCache) {
      yield this.getLayout(node);
    }
    let ret = yield this._getApplied(node, options);
    return ret.entries;
  }), {
    impl: "_getApplied"
  }),

  addNewRule: protocol.custom(function(node, pseudoClasses) {
    return this._addNewRule(node, pseudoClasses).then(ret => {
      return ret.entries[0];
    });
  }, {
    impl: "_addNewRule"
  })
});

/**
 * An actor that represents a CSS style object on the protocol.
 *
 * We slightly flatten the CSSOM for this actor, it represents
 * both the CSSRule and CSSStyle objects in one actor.  For nodes
 * (which have a CSSStyle but no CSSRule) we create a StyleRuleActor
 * with a special rule type (100).
 */
let StyleRuleActor = protocol.ActorClass({
  typeName: "domstylerule",
  initialize: function(pageStyle, item) {
    protocol.Actor.prototype.initialize.call(this, null);
    this.pageStyle = pageStyle;
    this.rawStyle = item.style;

    if (item instanceof (Ci.nsIDOMCSSRule)) {
      this.type = item.type;
      this.rawRule = item;
      if ((this.rawRule instanceof Ci.nsIDOMCSSStyleRule ||
           this.rawRule instanceof Ci.nsIDOMMozCSSKeyframeRule) &&
           this.rawRule.parentStyleSheet) {
        this.line = DOMUtils.getRuleLine(this.rawRule);
        this.column = DOMUtils.getRuleColumn(this.rawRule);
      }
    } else {
      // Fake a rule
      this.type = ELEMENT_STYLE;
      this.rawNode = item;
      this.rawRule = {
        style: item.style,
        toString: function() {
          return "[element rule " + this.style + "]";
        }
      };
    }
  },

  get conn() {
    return this.pageStyle.conn;
  },

  // Objects returned by this actor are owned by the PageStyleActor
  // to which this rule belongs.
  get marshallPool() {
    return this.pageStyle;
  },

  getDocument: function(sheet) {
    let document;

    if (sheet.ownerNode instanceof Ci.nsIDOMHTMLDocument) {
      document = sheet.ownerNode;
    } else {
      document = sheet.ownerNode.ownerDocument;
    }

    return document;
  },

  toString: function() {
    return "[StyleRuleActor for " + this.rawRule + "]"
  },

  form: function(detail) {
    if (detail === "actorid") {
      return this.actorID;
    }

    let form = {
      actor: this.actorID,
      type: this.type,
      line: this.line || undefined,
      column: this.column,
      traits: {
        // Whether the style rule actor implements the modifySelector2 method
        // that allows for unmatched rule to be added
        modifySelectorUnmatched: true,
      }
    };

    if (this.rawRule.parentRule) {
      form.parentRule = this.pageStyle._styleRef(this.rawRule.parentRule).actorID;

      // CSS rules that we call media rules are STYLE_RULES that are children
      // of MEDIA_RULEs. We need to check the parentRule to check if a rule is
      // a media rule so we do this here instead of in the switch statement
      // below.
      if (this.rawRule.parentRule.type === Ci.nsIDOMCSSRule.MEDIA_RULE) {
        form.media = [];
        for (let i = 0, n = this.rawRule.parentRule.media.length; i < n; i++) {
          form.media.push(this.rawRule.parentRule.media.item(i));
        }
      }
    }
    if (this.rawRule.parentStyleSheet) {
      form.parentStyleSheet = this.pageStyle._sheetRef(this.rawRule.parentStyleSheet).actorID;
    }

    switch (this.type) {
      case Ci.nsIDOMCSSRule.STYLE_RULE:
        form.selectors = CssLogic.getSelectors(this.rawRule);
        form.cssText = this.rawStyle.cssText || "";
        break;
      case ELEMENT_STYLE:
        // Elements don't have a parent stylesheet, and therefore
        // don't have an associated URI.  Provide a URI for
        // those.
        let doc = this.rawNode.ownerDocument;
        form.href = doc.location ? doc.location.href : "";
        form.cssText = this.rawStyle.cssText || "";
        break;
      case Ci.nsIDOMCSSRule.CHARSET_RULE:
        form.encoding = this.rawRule.encoding;
        break;
      case Ci.nsIDOMCSSRule.IMPORT_RULE:
        form.href = this.rawRule.href;
        break;
      case Ci.nsIDOMCSSRule.KEYFRAMES_RULE:
        form.cssText = this.rawRule.cssText;
        form.name = this.rawRule.name;
        break;
      case Ci.nsIDOMCSSRule.KEYFRAME_RULE:
        form.cssText = this.rawStyle.cssText || "";
        form.keyText = this.rawRule.keyText || "";
        break;
    }

    return form;
  },

  /**
   * Modify a rule's properties.  Passed an array of modifications:
   * {
   *   type: "set",
   *   name: <string>,
   *   value: <string>,
   *   priority: <optional string>
   * }
   *  or
   * {
   *   type: "remove",
   *   name: <string>,
   * }
   *
   * @returns the rule with updated properties
   */
  modifyProperties: method(function(modifications) {
    // Use a fresh element for each call to this function to prevent side
    // effects that pop up based on property values that were already set on the
    // element.

    let document;
    if (this.rawNode) {
      document = this.rawNode.ownerDocument;
    } else {
      let parentStyleSheet = this.rawRule.parentStyleSheet;
      while (parentStyleSheet.ownerRule &&
          parentStyleSheet.ownerRule instanceof Ci.nsIDOMCSSImportRule) {
        parentStyleSheet = parentStyleSheet.ownerRule.parentStyleSheet;
      }

      document = this.getDocument(parentStyleSheet);
    }

    let tempElement = document.createElementNS(XHTML_NS, "div");

    for (let mod of modifications) {
      if (mod.type === "set") {
        tempElement.style.setProperty(mod.name, mod.value, mod.priority || "");
        this.rawStyle.setProperty(mod.name,
          tempElement.style.getPropertyValue(mod.name), mod.priority || "");
      } else if (mod.type === "remove") {
        this.rawStyle.removeProperty(mod.name);
      }
    }

    return this;
  }, {
    request: { modifications: Arg(0, "array:json") },
    response: { rule: RetVal("domstylerule") }
  }),

  /**
   * Helper function for modifySelector and modifySelector2, inserts the new
   * rule with the new selector into the parent style sheet and removes the
   * current rule. Returns the newly inserted css rule or null if the rule is
   * unsuccessfully inserted to the parent style sheet.
   *
   * @param string value
   *        The new selector value
   *
   * @returns CSSRule
   *        The new CSS rule added
   */
  _addNewSelector: function(value) {
    let rule = this.rawRule;
    let parentStyleSheet = rule.parentStyleSheet;
    let cssRules = parentStyleSheet.cssRules;
    let cssText = rule.cssText;
    let selectorText = rule.selectorText;

    for (let i = 0; i < cssRules.length; i++) {
      if (rule === cssRules.item(i)) {
        try {
          // Inserts the new style rule into the current style sheet and
          // delete the current rule
          let ruleText = cssText.slice(selectorText.length).trim();
          parentStyleSheet.insertRule(value + " " + ruleText, i);
          parentStyleSheet.deleteRule(i + 1);
          return cssRules.item(i);
        } catch(e) {
          // The selector could be invalid, or the rule could fail to insert.
          // If that happens, the method returns null.
        }

        break;
      }
    }

    return null;
  },

  /**
   * Modify the current rule's selector by inserting a new rule with the new
   * selector value and removing the current rule.
   *
   * Note this method was kept for backward compatibility, but unmatched rules
   * support was added in FF41.
   *
   * @param string value
   *        The new selector value
   * @returns boolean
   *        Returns a boolean if the selector in the stylesheet was modified,
   *        and false otherwise
   */
  modifySelector: method(function(value) {
    if (this.type === ELEMENT_STYLE) {
      return false;
    }

    let document = this.getDocument(this.rawRule.parentStyleSheet);
    // Extract the selector, and pseudo elements and classes
    let [selector, pseudoProp] = value.split(/(:{1,2}.+$)/);
    let selectorElement;

    try {
      selectorElement = document.querySelector(selector);
    } catch (e) {
      return false;
    }

    // Check if the selector is valid and not the same as the original
    // selector
    if (selectorElement && this.rawRule.selectorText !== value) {
      this._addNewSelector(value);
      return true;
    }
    return false;
  }, {
    request: { selector: Arg(0, "string") },
    response: { isModified: RetVal("boolean") },
  }),

  /**
   * Modify the current rule's selector by inserting a new rule with the new
   * selector value and removing the current rule.
   *
   * In contrast with the modifySelector method which was used before FF41,
   * this method also returns information about the new rule and applied style
   * so that consumers can immediately display the new rule, whether or not the
   * selector matches the current element without having to refresh the whole
   * list.
   *
   * @param DOMNode node
   *        The current selected element
   * @param string value
   *        The new selector value
   * @returns Object
   *        Returns an object that contains the applied style properties of the
   *        new rule and a boolean indicating whether or not the new selector
   *        matches the current selected element
   */
  modifySelector2: method(function(node, value) {
    let isMatching = false;
    let ruleProps = null;

    if (this.type === ELEMENT_STYLE ||
        this.rawRule.selectorText === value) {
      return { ruleProps, isMatching: true };
    }

    let newCssRule = this._addNewSelector(value);
    if (newCssRule) {
      ruleProps = this.pageStyle.getNewAppliedProps(node, newCssRule);
    }

    // Determine if the new selector value matches the current selected element
    try {
      isMatching = node.rawNode.matches(value);
    } catch(e) {
      // This fails when value is an invalid selector.
    }

    return { ruleProps, isMatching };
  }, {
    request: {
      node: Arg(0, "domnode"),
      value: Arg(1, "string")
    },
    response: RetVal("modifiedStylesReturn")
  })
});

/**
 * Front for the StyleRule actor.
 */
let StyleRuleFront = protocol.FrontClass(StyleRuleActor, {
  initialize: function(client, form, ctx, detail) {
    protocol.Front.prototype.initialize.call(this, client, form, ctx, detail);
  },

  destroy: function() {
    protocol.Front.prototype.destroy.call(this);
  },

  form: function(form, detail) {
    if (detail === "actorid") {
      this.actorID = form;
      return;
    }
    this.actorID = form.actor;
    this._form = form;
    if (this._mediaText) {
      this._mediaText = null;
    }
  },

  /**
   * Return a new RuleModificationList for this node.
   */
  startModifyingProperties: function() {
    return new RuleModificationList(this);
  },

  get type() {
    return this._form.type;
  },
  get line() {
    return this._form.line || -1;
  },
  get column() {
    return this._form.column || -1;
  },
  get cssText() {
    return this._form.cssText;
  },
  get keyText() {
    return this._form.keyText;
  },
  get name() {
    return this._form.name;
  },
  get selectors() {
    return this._form.selectors;
  },
  get media() {
    return this._form.media;
  },
  get mediaText() {
    if (!this._form.media) {
      return null;
    }
    if (this._mediaText) {
      return this._mediaText;
    }
    this._mediaText = this.media.join(", ");
    return this._mediaText;
  },

  get parentRule() {
    return this.conn.getActor(this._form.parentRule);
  },

  get parentStyleSheet() {
    return this.conn.getActor(this._form.parentStyleSheet);
  },

  get element() {
    return this.conn.getActor(this._form.element);
  },

  get href() {
    if (this._form.href) {
      return this._form.href;
    }
    let sheet = this.parentStyleSheet;
    return sheet ? sheet.href : "";
  },

  get nodeHref() {
    let sheet = this.parentStyleSheet;
    return sheet ? sheet.nodeHref : "";
  },

  get supportsModifySelectorUnmatched() {
    return this._form.traits && this._form.traits.modifySelectorUnmatched;
  },

  get location() {
    return {
      source: this.parentStyleSheet,
      href: this.href,
      line: this.line,
      column: this.column
    };
  },

  getOriginalLocation: function() {
    if (this._originalLocation) {
      return promise.resolve(this._originalLocation);
    }
    let parentSheet = this.parentStyleSheet;
    if (!parentSheet) {
      // This rule doesn't belong to a stylesheet so it is an inline style.
      // Inline styles do not have any mediaText so we can return early.
      return promise.resolve(this.location);
    }
    return parentSheet.getOriginalLocation(this.line, this.column)
      .then(({ fromSourceMap, source, line, column }) => {
        let location = {
          href: source,
          line: line,
          column: column,
          mediaText: this.mediaText
        };
        if (fromSourceMap === false) {
          location.source = this.parentStyleSheet;
        }
        if (!source) {
          location.href = this.href;
        }
        this._originalLocation = location;
        return location;
      });
  },

  modifySelector: protocol.custom(Task.async(function*(node, value) {
    let response;
    if (this.supportsModifySelectorUnmatched) {
      // If the debugee supports adding unmatched rules (post FF41)
      response = yield this.modifySelector2(node, value);
    } else {
      response = yield this._modifySelector(value);
    }

    if (response.ruleProps) {
      response.ruleProps = response.ruleProps.entries[0];
    }
    return response;
  }), {
    impl: "_modifySelector"
  })
});

/**
 * Convenience API for building a list of attribute modifications
 * for the `modifyProperties` request.  A RuleModificationList holds a
 * list of modifications that will be applied to a StyleRuleActor.
 * The modifications are processed in the order in which they are
 * added to the RuleModificationList.
 */
let RuleModificationList = Class({
  /**
   * Initialize a RuleModificationList.
   * @param {StyleRuleFront} rule the associated rule
   */
  initialize: function(rule) {
    this.rule = rule;
    this.modifications = [];
  },

  /**
   * Apply the modifications in this object to the associated rule.
   *
   * @return {Promise} A promise which will be resolved when the modifications
   *         are complete; @see StyleRuleActor.modifyProperties.
   */
  apply: function() {
    return this.rule.modifyProperties(this.modifications);
  },

  /**
   * Add a "set" entry to the modification list.
   *
   * @param {string} name the property's name
   * @param {string} value the property's value
   * @param {string} priority the property's priority, either the empty
   *                          string or "important"
   */
  setProperty: function(name, value, priority) {
    this.modifications.push({
      type: "set",
      name: name,
      value: value,
      priority: priority
    });
  },

  /**
   * Add a "remove" entry to the modification list.
   *
   * @param {string} name the name of the property to remove
   */
  removeProperty: function(name) {
    this.modifications.push({
      type: "remove",
      name: name
    });
  }
});

/**
 * Helper function for getting an image preview of the given font.
 *
 * @param font {string}
 *        Name of font to preview
 * @param doc {Document}
 *        Document to use to render font
 * @param options {object}
 *        Object with options 'previewText' and 'previewFontSize'
 *
 * @return dataUrl
 *         The data URI of the font preview image
 */
function getFontPreviewData(font, doc, options) {
  options = options || {};
  let previewText = options.previewText || FONT_PREVIEW_TEXT;
  let previewFontSize = options.previewFontSize || FONT_PREVIEW_FONT_SIZE;
  let fillStyle = options.fillStyle || FONT_PREVIEW_FILLSTYLE;
  let fontStyle = options.fontStyle || "";

  let canvas = doc.createElementNS(XHTML_NS, "canvas");
  let ctx = canvas.getContext("2d");
  let fontValue = fontStyle + " " + previewFontSize + "px " + font + ", serif";

  // Get the correct preview text measurements and set the canvas dimensions
  ctx.font = fontValue;
  ctx.fillStyle = fillStyle;
  let textWidth = ctx.measureText(previewText).width;

  canvas.width = textWidth * 2 + FONT_PREVIEW_OFFSET * 2;
  canvas.height = previewFontSize * 3;

  // we have to reset these after changing the canvas size
  ctx.font = fontValue;
  ctx.fillStyle = fillStyle;

  // Oversample the canvas for better text quality
  ctx.textBaseline = "top";
  ctx.scale(2, 2);
  ctx.fillText(previewText,
               FONT_PREVIEW_OFFSET,
               Math.round(previewFontSize / 3));

  let dataURL = canvas.toDataURL("image/png");

  return {
    dataURL: dataURL,
    size: textWidth + FONT_PREVIEW_OFFSET * 2
  };
}

exports.getFontPreviewData = getFontPreviewData;
