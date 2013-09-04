/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {Cc, Ci} = require("chrome");
const protocol = require("devtools/server/protocol");
const {Arg, Option, method, RetVal, types} = protocol;
const events = require("sdk/event/core");
const object = require("sdk/util/object");
const { Class } = require("sdk/core/heritage");

loader.lazyGetter(this, "CssLogic", () => require("devtools/styleinspector/css-logic").CssLogic);
loader.lazyGetter(this, "DOMUtils", () => Cc["@mozilla.org/inspector/dom-utils;1"].getService(Ci.inIDOMUtils));

// The PageStyle actor flattens the DOM CSS objects a little bit, merging
// Rules and their Styles into one actor.  For elements (which have a style
// but no associated rule) we fake a rule with the following style id.
const ELEMENT_STYLE = 100;
exports.ELEMENT_STYLE = ELEMENT_STYLE;

const PSEUDO_ELEMENTS = [":first-line", ":first-letter", ":before", ":after", ":-moz-selection"];
exports.PSEUDO_ELEMENTS = PSEUDO_ELEMENTS;

// Predeclare the domnode actor type for use in requests.
types.addActorType("domnode");

/**
 * DOM Nodes returned by the style actor will be owned by the DOM walker
 * for the connection.
  */
types.addLifetime("walker", "walker");

/**
 * When asking for the styles applied to a node, we return a list of
 * appliedstyle json objects that lists the rules that apply to the node
 * and which element they were inherited from (if any).
 */
types.addDictType("appliedstyle", {
  rule: "domstylerule#actorid",
  inherited: "nullable:domnode#actorid"
});

types.addDictType("matchedselector", {
  rule: "domstylerule#actorid",
  selector: "string",
  value: "string",
  status: "number"
});

/**
 * The PageStyle actor lets the client look at the styles on a page, as
 * they are applied to a given node.
 */
var PageStyleActor = protocol.ActorClass({
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
    this.cssLogic = new CssLogic;

    // Stores the association of DOM objects -> actors
    this.refMap = new Map;
  },

  get conn() this.inspector.conn,

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
   * Return or create a StyleSheetActor for the given
   * nsIDOMCSSStyleSheet
   */
  _sheetRef: function(sheet) {
    if (this.refMap.has(sheet)) {
      return this.refMap.get(sheet);
    }
    let actor = StyleSheetActor(this, sheet);
    this.manage(actor);
    this.refMap.set(sheet, actor);

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
    let win = node.rawNode.ownerDocument.defaultView;
    let ret = Object.create(null);

    this.cssLogic.sourceFilter = options.filter || CssLogic.FILTER.UA;
    this.cssLogic.highlight(node.rawNode);
    let computed = this.cssLogic._computedStyle;

    Array.prototype.forEach.call(computed, name => {
      let matched = undefined;
      ret[name] = {
        value: computed.getPropertyValue(name),
        priority: computed.getPropertyPriority(name) || undefined
      };
    });

    if (options.markMatched || options.onlyMatched) {
      let matched = this.cssLogic.hasMatchedSelectors(Object.keys(ret));
      for (let key in ret) {
        if (matched[key]) {
          ret[key].matched = options.markMatched ? true : undefined
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

    let walker = node.parent();

    let rules = new Set;
    let sheets = new Set;

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
        value: selectorInfo.value,
        status: selectorInfo.status
      });
    }

    this.expandSets(rules, sheets);

    return {
      matched: matched,
      rules: [...rules],
      sheets: [...sheets],
    }
  }, {
    request: {
      node: Arg(0, "domnode"),
      property: Arg(1, "string"),
      filter: Option(2, "string")
    },
    response: RetVal(types.addDictType("matchedselectorresponse", {
      rules: "array:domstylerule",
      sheets: "array:domsheet",
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
      result += ".style"
    }
    return result;
  },

  /**
   * Get the set of styles that apply to a given node.
   * @param NodeActor node
   * @param string property
   * @param object options
   *   `filter`: A string filter that affects the "matched" handling.
   *     'user': Include properties from user style sheets.
   *     'ua': Include properties from user and user-agent sheets.
   *     Default value is 'ua'
   *   `inherited`: Include styles inherited from parent nodes.
   *   `matchedSeletors`: Include an array of specific selectors that
   *     caused this rule to match its node.
   */
  getApplied: method(function(node, options) {
    let entries = [];

    this.addElementRules(node.rawNode, undefined, options, entries);

    if (options.inherited) {
      let parent = this.walker.parentNode(node);
      while (parent && parent.rawNode.nodeType != Ci.nsIDOMNode.DOCUMENT_NODE) {
        this.addElementRules(parent.rawNode, parent, options, entries);
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
        entry.matchedSelectors = [];
        for (let i = 0; i < selectors.length; i++) {
          if (DOMUtils.selectorMatchesElement(element, domRule, i)) {
            entry.matchedSelectors.push(selectors[i]);
          }
        }

      }
    }

    let rules = new Set;
    let sheets = new Set;
    entries.forEach(entry => rules.add(entry.rule));
    this.expandSets(rules, sheets);

    return {
      entries: entries,
      rules: [...rules],
      sheets: [...sheets]
    }
  }, {
    request: {
      node: Arg(0, "domnode"),
      inherited: Option(1, "boolean"),
      matchedSelectors: Option(1, "boolean"),
      filter: Option(1, "string")
    },
    response: RetVal(types.addDictType("appliedStylesReturn", {
      entries: "array:appliedstyle",
      rules: "array:domstylerule",
      sheets: "array:domsheet"
    }))
  }),

  _hasInheritedProps: function(style) {
    return Array.prototype.some.call(style, prop => {
      return DOMUtils.isInheritedProperty(prop);
    });
  },

  /**
   * Helper function for getApplied, adds all the rules from a given
   * element.
   */
  addElementRules: function(element, inherited, options, rules)
  {
    let elementStyle = this._styleRef(element);

    if (!inherited || this._hasInheritedProps(element.style)) {
      rules.push({
        rule: elementStyle,
        inherited: inherited,
      });
    }

    let pseudoElements = inherited ? [null] : [null, ...PSEUDO_ELEMENTS];
    for (let pseudo of pseudoElements) {

      // Get the styles that apply to the element.
      let domRules = DOMUtils.getCSSStyleRules(element, pseudo);

      if (!domRules) {
        continue;
      }

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
          let hasInherited = Array.prototype.some.call(domRule.style, prop => {
            return DOMUtils.isInheritedProperty(prop);
          });
          if (!hasInherited) {
            continue;
          }
        }

        let ruleActor = this._styleRef(domRule);
        rules.push({
          rule: ruleActor,
          inherited: inherited,
          pseudoElement: pseudo
        });
      }

    }
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

  getLayout: method(function(node, options) {
    this.cssLogic.highlight(node.rawNode);

    let layout = {};

    // First, we update the first part of the layout view, with
    // the size of the element.

    let clientRect = node.rawNode.getBoundingClientRect();
    layout.width = Math.round(clientRect.width);
    layout.height = Math.round(clientRect.height);

    // We compute and update the values of margins & co.
    let style = node.rawNode.ownerDocument.defaultView.getComputedStyle(node.rawNode);
    for (let prop of [
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
      "border-left-width"
    ]) {
      layout[prop] = style.getPropertyValue(prop);
    }

    if (options.autoMargins) {
      layout.autoMargins = this.processMargins(this.cssLogic);
    }

    for (let i in this.map) {
      let property = this.map[i].property;
      this.map[i].value = parseInt(style.getPropertyValue(property));
    }


    if (options.margins) {
      layout.margins = this.processMargins(cssLogic);
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

});
exports.PageStyleActor = PageStyleActor;

/**
 * Front object for the PageStyleActor
 */
var PageStyleFront = protocol.FrontClass(PageStyleActor, {
  initialize: function(conn, form, ctx, detail) {
    protocol.Front.prototype.initialize.call(this, conn, form, ctx, detail);
    this.inspector = this.parent();
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

  getApplied: protocol.custom(function(node, options={}) {
    return this._getApplied(node, options).then(ret => {
      return ret.entries;
    });
  }, {
    impl: "_getApplied"
  })
});

/**
 * Actor representing an nsIDOMCSSStyleSheet.
 */
var StyleSheetActor = protocol.ActorClass({
  typeName: "domsheet",

  initialize: function(pageStyle, sheet) {
    protocol.Front.prototype.initialize.call(this);
    this.pageStyle = pageStyle;
    this.rawSheet = sheet;
  },

  get conn() this.pageStyle.conn,

  form: function(detail) {
    if (detail === "actorid") {
      return this.actorID;
    }

    return {
      actor: this.actorID,

      // href stores the uri of the sheet
      href: this.rawSheet.href,

      // nodeHref stores the URI of the document that
      // included the sheet.
      nodeHref: this.rawSheet.ownerNode ? this.rawSheet.ownerNode.ownerDocument.location.href : undefined,

      system: !CssLogic.isContentStylesheet(this.rawSheet),
      disabled: this.rawSheet.disabled ? true : undefined
    }
  }
});

/**
 * Front for the StyleSheetActor.
 */
var StyleSheetFront = protocol.FrontClass(StyleSheetActor, {
  initialize: function(conn, form, ctx, detail) {
    protocol.Front.prototype.initialize.call(this, conn, form, ctx, detail);
  },

  form: function(form, detail) {
    if (detail === "actorid") {
      this.actorID = form;
      return;
    }
    this.actorID = form.actorID;
    this._form = form;
  },

  get href() this._form.href,
  get nodeHref() this._form.nodeHref,
  get disabled() !!this._form.disabled,
  get isSystem() this._form.system
});


// Predeclare the domstylerule actor type
types.addActorType("domstylerule");

/**
 * An actor that represents a CSS style object on the protocol.
 *
 * We slightly flatten the CSSOM for this actor, it represents
 * both the CSSRule and CSSStyle objects in one actor.  For nodes
 * (which have a CSSStyle but no CSSRule) we create a StyleRuleActor
 * with a special rule type (100).
 */
var StyleRuleActor = protocol.ActorClass({
  typeName: "domstylerule",
  initialize: function(pageStyle, item) {
    protocol.Actor.prototype.initialize.call(this, null);
    this.pageStyle = pageStyle;
    this.rawStyle = item.style;

    if (item instanceof (Ci.nsIDOMCSSRule)) {
      this.type = item.type;
      this.rawRule = item;
      if (this.rawRule instanceof Ci.nsIDOMCSSStyleRule && this.rawRule.parentStyleSheet) {
        this.line = DOMUtils.getRuleLine(this.rawRule);
      }
    } else {
      // Fake a rule
      this.type = ELEMENT_STYLE;
      this.rawNode = item;
      this.rawRule = {
        style: item.style,
        toString: function() "[element rule " + this.style + "]"
      }
    }
  },

  get conn() this.pageStyle.conn,

  // Objects returned by this actor are owned by the PageStyleActor
  // to which this rule belongs.
  get marshallPool() this.pageStyle,

  toString: function() "[StyleRuleActor for " + this.rawRule + "]",

  form: function(detail) {
    if (detail === "actorid") {
      return this.actorID;
    }

    let form = {
      actor: this.actorID,
      type: this.type,
      line: this.line || undefined,
    };

    if (this.rawRule.parentRule) {
      form.parentRule = this.pageStyle._styleRef(this.rawRule.parentRule).actorID;
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
        form.href = this.rawNode.ownerDocument.location.href;
        form.cssText = this.rawStyle.cssText || "";
        break;
      case Ci.nsIDOMCSSRule.CHARSET_RULE:
        form.encoding = this.rawRule.encoding;
        break;
      case Ci.nsIDOMCSSRule.IMPORT_RULE:
        form.href = this.rawRule.href;
        break;
      case Ci.nsIDOMCSSRule.MEDIA_RULE:
        form.media = [];
        for (let i = 0, n = this.rawRule.media.length; i < n; i++) {
          form.media.push(this.rawRule.media.item(i));
        }
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
    for (let mod of modifications) {
      if (mod.type === "set") {
        this.rawStyle.setProperty(mod.name, mod.value, mod.priority || "");
      } else if (mod.type === "remove") {
        this.rawStyle.removeProperty(mod.name);
      }
    }
    return this;
  }, {
    request: { modifications: Arg(0, "array:json") },
    response: { rule: RetVal("domstylerule") }
  })
});

/**
 * Front for the StyleRule actor.
 */
var StyleRuleFront = protocol.FrontClass(StyleRuleActor, {
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

  get type() this._form.type,
  get line() this._form.line || -1,
  get cssText() {
    return this._form.cssText;
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
    return sheet.href || sheet.nodeHref;
  },

  // Only used for testing, please keep it that way.
  _rawStyle: function() {
    if (!this.conn._transport._serverConnection) {
      console.warn("Tried to use rawNode on a remote connection.");
      return null;
    }
    let actor = this.conn._transport._serverConnection.getActor(this.actorID);
    if (!actor) {
      return null;
    }
    return actor.rawStyle;
  }
});

/**
 * Convenience API for building a list of attribute modifications
 * for the `modifyAttributes` request.
 */
var RuleModificationList = Class({
  initialize: function(rule) {
    this.rule = rule;
    this.modifications = [];
  },

  apply: function() {
    return this.rule.modifyProperties(this.modifications);
  },
  setProperty: function(name, value, priority) {
    this.modifications.push({
      type: "set",
      name: name,
      value: value,
      priority: priority
    });
  },
  removeProperty: function(name) {
    this.modifications.push({
      type: "remove",
      name: name
    });
  }
});

