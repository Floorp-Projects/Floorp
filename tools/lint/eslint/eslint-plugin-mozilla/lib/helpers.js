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

var modules = null;

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
  /^this\.(\w+) =/
];

var imports = [
  /^(?:Cu|Components\.utils)\.import\(".*\/((.*?)\.jsm?)"(?:, this)?\)/,
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
      case "ArrowFunctionExpression":
        return "() => {}";
      case "AssignmentExpression":
        return this.getASTSource(node.left) + " = " + this.getASTSource(node.right);
      default:
        throw new Error("getASTSource unsupported node type: " + node.type);
    }
  },

  /**
   * This walks an AST in a manner similar to ESLint passing node and comment
   * events to the listener. The listener is expected to be a simple function
   * which accepts node type, node and parents arguments.
   *
   * @param  {Object} ast
   *         The AST to walk.
   * @param  {Function} listener
   *         A callback function to call for the nodes. Passed three arguments,
   *         event type, node and an array of parent nodes for the current node.
   */
  walkAST(ast, listener) {
    let parents = [];

    let seenComments = new Set();
    function sendCommentEvents(comments) {
      if (!comments) {
        return;
      }

      for (let comment of comments) {
        if (seenComments.has(comment)) {
          return;
        }
        seenComments.add(comment);

        listener(comment.type + "Comment", comment, parents);
      }
    }

    estraverse.traverse(ast, {
      enter(node, parent) {
        // Comments are held in node.comments for empty programs
        let leadingComments = node.leadingComments;
        if (node.type === "Program" && node.body.length == 0) {
          leadingComments = node.comments;
        }

        sendCommentEvents(leadingComments);
        listener(node.type, node, parents);
        sendCommentEvents(node.trailingComments);

        parents.push(node);
      },

      leave(node, parent) {
        // TODO send comment exit events
        listener(node.type + ":exit", node, parents);

        if (parents.length == 0) {
          throw new Error("Left more nodes than entered.");
        }
        parents.pop();
      }
    });
    if (parents.length) {
      throw new Error("Entered more nodes than left.");
    }
  },

  /**
   * Attempts to convert an ExpressionStatement to likely global variable
   * definitions.
   *
   * @param  {Object} node
   *         The AST node to convert.
   * @param  {boolean} isGlobal
   *         True if the current node is in the global scope.
   * @param  {String} repository
   *         The root of the repository.
   *
   * @return {Array}
   *         An array of variable names defined.
   */
  convertExpressionToGlobals: function(node, isGlobal, repository) {
    if (!modules) {
      modules = require(path.join(repository, "tools", "lint", "eslint", "modules.json"));
    }

    try {
      var source = this.getASTSource(node);
    }
    catch (e) {
      return [];
    }

    for (var reg of definitions) {
      var match = source.match(reg);
      if (match) {
        // Must be in the global scope
        if (!isGlobal) {
          return [];
        }

        return [match[1]];
      }
    }

    for (reg of imports) {
      var match = source.match(reg);
      if (match) {
        // The two argument form is only acceptable in the global scope
        if (node.expression.arguments.length > 1 && !isGlobal) {
          return [];
        }

        if (match[1] in modules) {
          return modules[match[1]];
        }

        return [match[2]];
      }
    }

    return [];
  },

  /**
   * Add a variable to the current scope.
   * HACK: This relies on eslint internals so it could break at any time.
   *
   * @param {String} name
   *        The variable name to add to the scope.
   * @param {ASTScope} scope
   *        The scope to add to.
   * @param {boolean} writable
   *        Whether the global can be overwritten.
   */
  addVarToScope: function(name, scope, writable) {
    scope.__defineGeneric(name, scope.set, scope.variables, null, null);

    let variable = scope.set.get(name);
    variable.eslintExplicitGlobal = false;
    variable.writeable = writable;

    // Walk to the global scope which holds all undeclared variables.
    while (scope.type != "global") {
      scope = scope.upper;
    }

    // "through" contains all references with no found definition.
    scope.through = scope.through.filter(function(reference) {
      if (reference.identifier.name != name) {
        return true;
      }

      // Links the variable and the reference.
      // And this reference is removed from `Scope#through`.
      reference.resolved = variable;
      variable.references.push(reference);
      return false;
    });
  },

  /**
   * Adds a set of globals to a scope.
   *
   * @param {Array} globalVars
   *        An array of global variable names.
   * @param {ASTScope} scope
   *        The scope.
   */
  addGlobals: function(globalVars, scope) {
    globalVars.forEach(v => this.addVarToScope(v.name, scope, v.writable));
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
      comment: true,
      attachComment: true,
      ecmaVersion: 8,
      sourceType: "script",
      ecmaFeatures: {
        experimentalObjectRestSpread: true,
        globalReturn: true,
      }
    };
  },

  /**
   * Check whether the context is the global scope.
   *
   * @param {Array} ancestors
   *        The parents of the current node.
   *
   * @return {Boolean}
   *         True or false
   */
  getIsGlobalScope: function(ancestors) {
    for (let parent of ancestors) {
      if (parent.type == "FunctionExpression" ||
          parent.type == "FunctionDeclaration") {
        return false;
      }
    }
    return true;
  },

  /**
   * Check whether we might be in a test head file.
   *
   * @param  {RuleContext} scope
   *         You should pass this from within a rule
   *         e.g. helpers.getIsHeadFile(this)
   *
   * @return {Boolean}
   *         True or false
   */
  getIsHeadFile: function(scope) {
    var pathAndFilename = this.cleanUpPath(scope.getFilename());

    return /.*[\\/]head(_.+)?\.js$/.test(pathAndFilename);
  },

  /**
   * Check whether we might be in an xpcshell test.
   *
   * @param  {RuleContext} scope
   *         You should pass this from within a rule
   *         e.g. helpers.getIsXpcshellTest(this)
   *
   * @return {Boolean}
   *         True or false
   */
  getIsXpcshellTest: function(scope) {
    var pathAndFilename = this.cleanUpPath(scope.getFilename());

    return /.*[\\/]test_.+\.js$/.test(pathAndFilename);
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
    var pathAndFilename = this.cleanUpPath(scope.getFilename());

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
    if (this.getIsXpcshellTest(scope)) {
      return true;
    }

    return this.getIsBrowserMochitest(scope);
  },

  /**
   * Gets the root directory of the repository by walking up directories until
   * a .eslintignore file is found.
   * @param {String} fileName
   *        The absolute path of a file in the repository
   *
   * @return {String} The absolute path of the repository directory
   */
  getRootDir: function(fileName) {
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
    var fileName = this.cleanUpPath(context.getFilename());
    var cwd = process.cwd();

    if (path.isAbsolute(fileName)) {
      // Case 2: executed from the repo's root with mach:
      //   fileName: /path/to/mozilla/repo/a/b/c/d.js
      //   cwd: /path/to/mozilla/repo
      return fileName;
    } else if (path.basename(fileName) == fileName) {
      // Case 1b: executed from a nested directory, fileName is the base name
      // without any path info (happens in Atom with linter-eslint)
      return path.join(cwd, fileName);
    } else {
      // Case 1: executed form in a nested directory, e.g. from a text editor:
      //   fileName: a/b/c/d.js
      //   cwd: /path/to/mozilla/repo/a/b/c
      var dirName = path.dirname(fileName);
      return cwd.slice(0, cwd.length - dirName.length) + fileName;
    }
  },

  /**
   * When ESLint is run from SublimeText, paths retrieved from
   * context.getFileName contain leading and trailing double-quote characters.
   * These characters need to be removed.
   */
  cleanUpPath: function(path) {
    return path.replace(/^"/, "").replace(/"$/, "");
  }
};
