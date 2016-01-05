/**
 * @fileoverview A collection of helper functions.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
"use strict";

var escope = require("escope");
var espree = require("espree");
var estraverse = require("estraverse");
var path = require("path");
var fs = require("fs");

var definitions = [
  /^loader\.lazyGetter\(this, "(\w+)"/,
  /^loader\.lazyImporter\(this, "(\w+)"/,
  /^loader\.lazyServiceGetter\(this, "(\w+)"/,
  /^loader\.lazyRequireGetter\(this, "(\w+)"/,
  /^XPCOMUtils\.defineLazyGetter\(this, "(\w+)"/,
  /^XPCOMUtils\.defineLazyModuleGetter\(this, "(\w+)"/,
  /^XPCOMUtils\.defineLazyServiceGetter\(this, "(\w+)"/,
  /^XPCOMUtils\.defineConstant\(this, "(\w+)"/,
  /^DevToolsUtils\.defineLazyModuleGetter\(this, "(\w+)"/,
  /^DevToolsUtils\.defineLazyGetter\(this, "(\w+)"/,
  /^Object\.defineProperty\(this, "(\w+)"/,
  /^Reflect\.defineProperty\(this, "(\w+)"/,
  /^this\.__defineGetter__\("(\w+)"/,
];

var imports = [
  /^(?:Cu|Components\.utils)\.import\(".*\/(.*?)\.jsm?"(?:, this)?\)/,
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
   * A simplistic conversion of some AST nodes to a standard string form.
   *
   * @param  {Object} node
   *         The AST node to convert.
   *
   * @return {String}
   *         The JS source for the node.
   */
  getASTSource: function(node) {
    switch (node.type) {
      case "MemberExpression":
        if (node.computed)
          throw new Error("getASTSource unsupported computed MemberExpression");
        return this.getASTSource(node.object) + "." + this.getASTSource(node.property);
      case "ThisExpression":
        return "this";
      case "Identifier":
        return node.name;
      case "Literal":
        return JSON.stringify(node.value);
      case "CallExpression":
        var args = node.arguments.map(a => this.getASTSource(a)).join(", ");
        return this.getASTSource(node.callee) + "(" + args + ")";
      case "ObjectExpression":
        return "{}";
      case "ExpressionStatement":
        return this.getASTSource(node.expression) + ";";
      case "FunctionExpression":
        return "function() {}";
      default:
        throw new Error("getASTSource unsupported node type: " + node.type);
    }
  },

  /**
   * Attempts to convert an ExpressionStatement to a likely global variable
   * definition.
   *
   * @param  {Object} node
   *         The AST node to convert.
   *
   * @return {String or null}
   *         The variable name defined.
   */
  convertExpressionToGlobal: function(node, isGlobal) {
    try {
      var source = this.getASTSource(node);
    }
    catch (e) {
      return null;
    }

    for (var reg of definitions) {
      var match = source.match(reg);
      if (match) {
        // Must be in the global scope
        if (!isGlobal) {
          return null;
        }

        return match[1];
      }
    }

    for (reg of imports) {
      var match = source.match(reg);
      if (match) {
        // The two argument form is only acceptable in the global scope
        if (node.expression.arguments.length > 1 && !isGlobal) {
          return null;
        }

        return match[1];
      }
    }

    return null;
  },

  /**
   * Walks over an AST and calls a callback for every ExpressionStatement found.
   *
   * @param  {Object} ast
   *         The AST to traverse.
   *
   * @return {Function}
   *         The callback to call for each ExpressionStatement.
   */
  expressionTraverse: function(ast, callback) {
    var helpers = this;
    var parents = new Map();

    // Walk the parents of a node to see if any are functions
    function isGlobal(node) {
      var parent = parents.get(node);
      while (parent) {
        if (parent.type == "FunctionExpression" ||
            parent.type == "FunctionDeclaration") {
          return false;
        }
        parent = parents.get(parent);
      }
      return true;
    }

    estraverse.traverse(ast, {
      enter: function(node, parent) {
        parents.set(node, parent);

        if (node.type == "ExpressionStatement") {
          callback(node, isGlobal(node));
        }
      }
    });
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

    var helpers = this;
    this.expressionTraverse(ast, function(node, isGlobal) {
      var name = helpers.convertExpressionToGlobal(node, isGlobal);

      if (name) {
        result.push(name);
      }
    });

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

  // Caches globals found in a file so we only have to parse a file once.
  globalCache: new Map(),

  /**
   * Finds all the globals defined in a given file.
   *
   * @param {String} fileName
   *        The file to parse for globals.
   */
  getGlobalsForFile: function(fileName) {
    // If the file can't be found, let the error go up to the caller so it can
    // be logged as an error in the current file.
    var content = fs.readFileSync(fileName, "utf8");

    if (this.globalCache.has(fileName)) {
      return this.globalCache.get(fileName);
    }

    // Parse the content and get the globals from the ast.
    var ast = this.getAST(content);
    var globalVars = this.getGlobals(ast);
    this.globalCache.set(fileName, globalVars);

    return globalVars;
  },

  /**
   * Adds a set of globals to a context.
   *
   * @param {Array} globalVars
   *        An array of global variable names.
   * @param {ASTContext} context
   *        The current context.
   */
  addGlobals: function(globalVars, context) {
    for (var i = 0; i < globalVars.length; i++) {
      var varName = globalVars[i];
      this.addVarToScope(varName, context);
    }
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
  },

  /**
   * Check whether we are in a test of some kind.
   *
   * @param  {RuleContext} scope
   *         You should pass this from within a rule
   *         e.g. helpers.getIsTest(this)
   *
   * @return {Boolean}
   *         True or false
   */
  getIsTest: function(scope) {
    var pathAndFilename = scope.getFilename();

    if (/.*[\\/]test_.+\.js$/.test(pathAndFilename)) {
      return true;
    }

    return this.getIsBrowserMochitest(scope);
  },

  /**
   * Gets the root directory of the repository by walking up directories until
   * a .eslintignore file is found.
   * @param {ASTContext} context
   *        The current context.
   *
   * @return {String} The absolute path of the repository directory
   */
  getRootDir: function(context) {
    var fileName = this.getAbsoluteFilePath(context);
    var dirName = path.dirname(fileName);

    while (dirName && !fs.existsSync(path.join(dirName, ".eslintignore"))) {
      dirName = path.dirname(dirName);
    }

    if (!dirName) {
      throw new Error("Unable to find root of repository");
    }

    return dirName;
  },

  /**
   * ESLint may be executed from various places: from mach, at the root of the
   * repository, or from a directory in the repository when, for instance,
   * executed by a text editor's plugin.
   * The value returned by context.getFileName() varies because of this.
   * This helper function makes sure to return an absolute file path for the
   * current context, by looking at process.cwd().
   * @param {Context} context
   * @return {String} The absolute path
   */
  getAbsoluteFilePath: function(context) {
    var fileName = context.getFilename();
    var cwd = process.cwd();

    if (path.isAbsolute(fileName)) {
      // Case 2: executed from the repo's root with mach:
      //   fileName: /path/to/mozilla/repo/a/b/c/d.js
      //   cwd: /path/to/mozilla/repo
      return fileName;
    } else {
      // Case 1: executed form in a nested directory, e.g. from a text editor:
      //   fileName: a/b/c/d.js
      //   cwd: /path/to/mozilla/repo/a/b/c
      var dirName = path.dirname(fileName);
      return cwd.slice(0, cwd.length - dirName.length) + fileName;
    }
  }
};
