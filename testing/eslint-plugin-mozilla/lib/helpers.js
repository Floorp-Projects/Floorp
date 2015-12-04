/**
 * @fileoverview A collection of helper functions.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
"use strict";

var escope = require("escope");
var espree = require("espree");

var regexes = [
  /^(?:Cu|Components\.utils)\.import\(".*\/(.*?)\.jsm?"\);?$/,
  /^loader\.lazyImporter\(\w+, "(\w+)"/,
  /^loader\.lazyRequireGetter\(\w+, "(\w+)"/,
  /^loader\.lazyServiceGetter\(\w+, "(\w+)"/,
  /^XPCOMUtils\.defineLazyModuleGetter\(\w+, "(\w+)"/,
  /^loader\.lazyGetter\(\w+, "(\w+)"/,
  /^XPCOMUtils\.defineLazyGetter\(\w+, "(\w+)"/
];

module.exports = {
  /**
   * Gets the abstract syntax tree (AST) of the JavaScript source code contained
   * in sourceText.
   *
   * @param  {String} sourceText
   *         Text containing valid JavaScript.
   *
   * @return {Object}
   *         The resulting AST.
   */
  getAST: function(sourceText) {
    // Use a permissive config file to allow parsing of anything that Espree
    // can parse.
    var config = this.getPermissiveConfig();

    return espree.parse(sourceText, config);
  },

  /**
   * Gets the source text of an AST node.
   *
   * @param  {ASTNode} node
   *         The AST node representing the source text.
   * @param  {ASTContext} context
   *         The current context.
   *
   * @return {String}
   *         The source text representing the AST node.
   */
  getSource: function(node, context) {
    return context.getSource(node).replace(/[\r\n]+\s*/g, " ")
                                  .replace(/\s*=\s*/g, " = ")
                                  .replace(/\s+\./g, ".")
                                  .replace(/,\s+/g, ", ")
                                  .replace(/;\n(\d+)/g, ";$1")
                                  .replace(/\s+/g, " ");
  },

  /**
   * Gets the variable name from an import source
   * e.g. Cu.import("path/to/someName") will return "someName."
   *
   * Some valid input strings:
   *  - Cu.import("resource://devtools/client/shared/widgets/ViewHelpers.jsm");
   *  - loader.lazyImporter(this, "name1");
   *  - loader.lazyRequireGetter(this, "name2"
   *  - loader.lazyServiceGetter(this, "name3"
   *  - XPCOMUtils.defineLazyModuleGetter(this, "setNamedTimeout", ...)
   *  - loader.lazyGetter(this, "toolboxStrings"
   *  - XPCOMUtils.defineLazyGetter(this, "clipboardHelper"
   *
   * @param  {String} source
   *         The source representing an import statement.
   *
   * @return {String}
   *         The variable name imported.
   */
  getVarNameFromImportSource: function(source) {
    for (var i = 0; i < regexes.length; i++) {
      var regex = regexes[i];
      var matches = source.match(regex);

      if (matches) {
        var name = matches[1];

        return name;
      }
    }
  },

  /**
   * Get an array of globals from an AST.
   *
   * @param  {Object} ast
   *         The AST for which the globals are to be returned.
   *
   * @return {Array}
   *         An array of variable names.
   */
  getGlobals: function(ast) {
    var scopeManager = escope.analyze(ast);
    var globalScope = scopeManager.acquire(ast);
    var result = [];

    for (var variable in globalScope.variables) {
      var name = globalScope.variables[variable].name;
      result.push(name);
    }

    return result;
  },

  /**
   * Add a variable to the current scope.
   * HACK: This relies on eslint internals so it could break at any time.
   *
   * @param {String} name
   *        The variable name to add to the current scope.
   * @param {ASTContext} context
   *        The current context.
   */
  addVarToScope: function(name, context) {
    var scope = context.getScope();
    var variables = scope.variables;
    var variable = new escope.Variable(name, scope);

    variable.eslintExplicitGlobal = false;
    variable.writeable = true;
    variables.push(variable);

    // Since eslint 1.10.3, scope variables are now duplicated in the scope.set
    // map, so we need to store them there too if it exists.
    // See https://groups.google.com/forum/#!msg/eslint/Y4_oHMWwP-o/5S57U8jXd8kJ
    if (scope.set) {
      scope.set.set(name, variable);
    }
  },

  /**
   * Get the single line text represented by a particular AST node.
   *
   * @param  {ASTNode} node
   *         The AST node representing the source text.
   * @param  {String} text
   *         The text representing the AST node.
   *
   * @return {String}
   *         A single line version of the string represented by node.
   */
  getTextForNode: function(node, text) {
    var source = text.substr(node.range[0], node.range[1] - node.range[0]);

    return source.replace(/[\r\n]+\s*/g, "")
                 .replace(/\s*=\s*/g, " = ")
                 .replace(/\s+\./g, ".")
                 .replace(/,\s+/g, ", ")
                 .replace(/;\n(\d+)/g, ";$1");
  },

  /**
   * To allow espree to parse almost any JavaScript we need as many features as
   * possible turned on. This method returns that config.
   *
   * @return {Object}
   *         Espree compatible permissive config.
   */
  getPermissiveConfig: function() {
    return {
      range: true,
      loc: true,
      tolerant: true,
      ecmaFeatures: {
        arrowFunctions: true,
        binaryLiterals: true,
        blockBindings: true,
        classes: true,
        defaultParams: true,
        destructuring: true,
        forOf: true,
        generators: true,
        globalReturn: true,
        modules: true,
        objectLiteralComputedProperties: true,
        objectLiteralDuplicateProperties: true,
        objectLiteralShorthandMethods: true,
        objectLiteralShorthandProperties: true,
        octalLiterals: true,
        regexUFlag: true,
        regexYFlag: true,
        restParams: true,
        spread: true,
        superInFunctions: true,
        templateStrings: true,
        unicodeCodePointEscapes: true,
      }
    };
  },

  /**
   * Check whether the context is the global scope.
   *
   * @param {ASTContext} context
   *        The current context.
   *
   * @return {Boolean}
   *         True or false
   */
  getIsGlobalScope: function(context) {
    var ancestors = context.getAncestors();
    var parent = ancestors.pop();

    if (parent.type == "ExpressionStatement") {
      parent = ancestors.pop();
    }

    return parent.type == "Program";
  },

  /**
   * Check whether we are in a browser mochitest.
   *
   * @param  {RuleContext} scope
   *         You should pass this from within a rule
   *         e.g. helpers.getIsBrowserMochitest(this)
   *
   * @return {Boolean}
   *         True or false
   */
  getIsBrowserMochitest: function(scope) {
    var pathAndFilename = scope.getFilename();

    return /.*[\\/]browser_.+\.js$/.test(pathAndFilename);
  }
};
