/**
 * @fileoverview A collection of helper functions.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
"use strict";

const espree = require("espree");
const estraverse = require("estraverse");
const path = require("path");
const fs = require("fs");
const ini = require("ini-parser");

var gModules = null;
var gRootDir = null;
var directoryManifests = new Map();

const callExpressionDefinitions = [
  /^loader\.lazyGetter\(this, "(\w+)"/,
  /^loader\.lazyImporter\(this, "(\w+)"/,
  /^loader\.lazyServiceGetter\(this, "(\w+)"/,
  /^loader\.lazyRequireGetter\(this, "(\w+)"/,
  /^XPCOMUtils\.defineLazyGetter\(this, "(\w+)"/,
  /^XPCOMUtils\.defineLazyModuleGetter\(this, "(\w+)"/,
  /^XPCOMUtils\.defineLazyPreferenceGetter\(this, "(\w+)"/,
  /^XPCOMUtils\.defineLazyScriptGetter\(this, "(\w+)"/,
  /^XPCOMUtils\.defineLazyServiceGetter\(this, "(\w+)"/,
  /^XPCOMUtils\.defineConstant\(this, "(\w+)"/,
  /^DevToolsUtils\.defineLazyModuleGetter\(this, "(\w+)"/,
  /^DevToolsUtils\.defineLazyGetter\(this, "(\w+)"/,
  /^Object\.defineProperty\(this, "(\w+)"/,
  /^Reflect\.defineProperty\(this, "(\w+)"/,
  /^this\.__defineGetter__\("(\w+)"/
];

const imports = [
  /^(?:Cu|Components\.utils)\.import\(".*\/((.*?)\.jsm?)"(?:, this)?\)/
];

const workerImportFilenameMatch = /(.*\/)*(.*?\.jsm?)/;

module.exports = {
  get modulesGlobalData() {
    if (!gModules) {
      if (this.isMozillaCentralBased()) {
        gModules = require(path.join(this.rootDir, "tools", "lint", "eslint", "modules.json"));
      } else {
        gModules = require("./modules.json");
      }
    }

    return gModules;
  },

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
  getAST(sourceText) {
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
  getASTSource(node, context) {
    switch (node.type) {
      case "MemberExpression":
        if (node.computed) {
          let filename = context && context.getFilename();
          throw new Error(`getASTSource unsupported computed MemberExpression in ${filename}`);
        }
        return this.getASTSource(node.object) + "." +
          this.getASTSource(node.property);
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
      case "ArrayExpression":
        return "[" + node.elements.map(this.getASTSource, this).join(",") + "]";
      case "ArrowFunctionExpression":
        return "() => {}";
      case "AssignmentExpression":
        return this.getASTSource(node.left) + " = " +
          this.getASTSource(node.right);
      default:
        throw new Error("getASTSource unsupported node type: " + node.type);
    }
  },

  /**
   * This walks an AST in a manner similar to ESLint passing node events to the
   * listener. The listener is expected to be a simple function
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

    estraverse.traverse(ast, {
      enter(node, parent) {
        listener(node.type, node, parents);

        parents.push(node);
      },

      leave(node, parent) {
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
   *
   * @return {Array}
   *         An array of objects that contain details about the globals:
   *         - {String} name
   *                    The name of the global.
   *         - {Boolean} writable
   *                     If the global is writeable or not.
   */
  convertWorkerExpressionToGlobals(node, isGlobal, dirname) {
    var getGlobalsForFile = require("./globals").getGlobalsForFile;

    let globalModules = this.modulesGlobalData;

    let results = [];
    let expr = node.expression;

    if (node.expression.type === "CallExpression" &&
        expr.callee &&
        expr.callee.type === "Identifier" &&
        expr.callee.name === "importScripts") {
      for (var arg of expr.arguments) {
        var match = arg.value && arg.value.match(workerImportFilenameMatch);
        if (match) {
          if (!match[1]) {
            let filePath = path.resolve(dirname, match[2]);
            if (fs.existsSync(filePath)) {
              let additionalGlobals = getGlobalsForFile(filePath);
              results = results.concat(additionalGlobals);
            }
          } else if (match[2] in globalModules) {
            results = results.concat(globalModules[match[2]].map(name => {
              return { name, writable: true };
            }));
          }
        }
      }
    }

    return results;
  },

  /**
   * Attempts to convert an AssignmentExpression into a global variable
   * definition if it applies to `this` in the global scope.
   *
   * @param  {Object} node
   *         The AST node to convert.
   * @param  {boolean} isGlobal
   *         True if the current node is in the global scope.
   *
   * @return {Array}
   *         An array of objects that contain details about the globals:
   *         - {String} name
   *                    The name of the global.
   *         - {Boolean} writable
   *                     If the global is writeable or not.
   */
  convertThisAssignmentExpressionToGlobals(node, isGlobal) {
    if (isGlobal &&
        node.expression.left &&
        node.expression.left.object &&
        node.expression.left.object.type === "ThisExpression" &&
        node.expression.left.property &&
        node.expression.left.property.type === "Identifier") {
      return [{ name: node.expression.left.property.name, writable: true }];
    }
    return [];
  },

  /**
   * Attempts to convert an CallExpressions that look like module imports
   * into global variable definitions, using modules.json data if appropriate.
   *
   * @param  {Object} node
   *         The AST node to convert.
   * @param  {boolean} isGlobal
   *         True if the current node is in the global scope.
   *
   * @return {Array}
   *         An array of objects that contain details about the globals:
   *         - {String} name
   *                    The name of the global.
   *         - {Boolean} writable
   *                     If the global is writeable or not.
   */
  convertCallExpressionToGlobals(node, isGlobal) {
    let source;
    try {
      source = this.getASTSource(node);
    } catch (e) {
      return [];
    }

    for (let reg of imports) {
      let match = source.match(reg);
      if (match) {
        // The two argument form is only acceptable in the global scope
        if (node.expression.arguments.length > 1 && !isGlobal) {
          return [];
        }

        let globalModules = this.modulesGlobalData;

        if (match[1] in globalModules) {
          return globalModules[match[1]].map(name => ({ name, writable: true }));
        }

        return [{ name: match[2], writable: true }];
      }
    }

    // The definition matches below must be in the global scope for us to define
    // a global, so bail out early if we're not a global.
    if (!isGlobal) {
      return [];
    }

    for (let reg of callExpressionDefinitions) {
      let match = source.match(reg);
      if (match) {
        return [{ name: match[1], writable: true }];
      }
    }

    if (node.expression.callee.type == "MemberExpression" &&
        node.expression.callee.property.type == "Identifier" &&
        node.expression.callee.property.name == "defineLazyScriptGetter") {
      // The case where we have a single symbol as a string has already been
      // handled by the regexp, so we have an array of symbols here.
      return node.expression.arguments[1].elements.map(n => ({ name: n.value, writable: true }));
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
  addVarToScope(name, scope, writable) {
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
  addGlobals(globalVars, scope) {
    globalVars.forEach(v => this.addVarToScope(v.name, scope, v.writable));
  },

  /**
   * To allow espree to parse almost any JavaScript we need as many features as
   * possible turned on. This method returns that config.
   *
   * @return {Object}
   *         Espree compatible permissive config.
   */
  getPermissiveConfig() {
    return {
      range: true,
      loc: true,
      comment: true,
      attachComment: true,
      ecmaVersion: 8,
      sourceType: "script",
      ecmaFeatures: {
        experimentalObjectRestSpread: true,
        globalReturn: true
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
  getIsGlobalScope(ancestors) {
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
   *         e.g. helpers.getIsHeadFile(context)
   *
   * @return {Boolean}
   *         True or false
   */
  getIsHeadFile(scope) {
    var pathAndFilename = this.cleanUpPath(scope.getFilename());

    return /.*[\\/]head(_.+)?\.js$/.test(pathAndFilename);
  },

  /**
   * Gets the head files for a potential test file
   *
   * @param  {RuleContext} scope
   *         You should pass this from within a rule
   *         e.g. helpers.getIsHeadFile(context)
   *
   * @return {String[]}
   *         Paths to head files to load for the test
   */
  getTestHeadFiles(scope) {
    if (!this.getIsTest(scope)) {
      return [];
    }

    let filepath = this.cleanUpPath(scope.getFilename());
    let dir = path.dirname(filepath);

    let names =
      fs.readdirSync(dir)
        .filter(name => (name.startsWith("head") ||
                         name.startsWith("xpcshell-head")) && name.endsWith(".js"))
        .map(name => path.join(dir, name));
    return names;
  },

  /**
   * Gets all the test manifest data for a directory
   *
   * @param  {String} dir
   *         The directory
   *
   * @return {Array}
   *         An array of objects with file and manifest properties
   */
  getManifestsForDirectory(dir) {
    if (directoryManifests.has(dir)) {
      return directoryManifests.get(dir);
    }

    let manifests = [];

    let names = fs.readdirSync(dir);
    for (let name of names) {
      if (!name.endsWith(".ini")) {
        continue;
      }

      try {
        let manifest = ini.parse(fs.readFileSync(path.join(dir, name), "utf8"));

        manifests.push({
          file: path.join(dir, name),
          manifest
        });
      } catch (e) {
      }
    }

    directoryManifests.set(dir, manifests);
    return manifests;
  },

  /**
   * Gets the manifest file a test is listed in
   *
   * @param  {RuleContext} scope
   *         You should pass this from within a rule
   *         e.g. helpers.getIsHeadFile(context)
   *
   * @return {String}
   *         The path to the test manifest file
   */
  getTestManifest(scope) {
    let filepath = this.cleanUpPath(scope.getFilename());

    let dir = path.dirname(filepath);
    let filename = path.basename(filepath);

    for (let manifest of this.getManifestsForDirectory(dir)) {
      if (filename in manifest.manifest) {
        return manifest.file;
      }
    }

    return null;
  },

  /**
   * Check whether we are in a test of some kind.
   *
   * @param  {RuleContext} scope
   *         You should pass this from within a rule
   *         e.g. helpers.getIsTest(context)
   *
   * @return {Boolean}
   *         True or false
   */
  getIsTest(scope) {
    // Regardless of the manifest name being in a manifest means we're a test.
    let manifest = this.getTestManifest(scope);
    if (manifest) {
      return true;
    }

    return !!this.getTestType(scope);
  },

  /**
   * Gets the type of test or null if this isn't a test.
   *
   * @param  {RuleContext} scope
   *         You should pass this from within a rule
   *         e.g. helpers.getIsHeadFile(context)
   *
   * @return {String or null}
   *         Test type: xpcshell, browser, chrome, mochitest
   */
  getTestType(scope) {
    let testTypes = ["browser", "xpcshell", "chrome", "mochitest", "a11y"];
    let manifest = this.getTestManifest(scope);
    if (manifest) {
      let name = path.basename(manifest);
      for (let testType of testTypes) {
        if (name.startsWith(testType)) {
          return testType;
        }
      }
    }

    let filepath = this.cleanUpPath(scope.getFilename());
    let filename = path.basename(filepath);

    if (filename.startsWith("browser_")) {
      return "browser";
    }

    if (filename.startsWith("test_")) {
      let parent = path.basename(path.dirname(filepath));
      for (let testType of testTypes) {
        if (parent.startsWith(testType)) {
          return testType;
        }
      }

      // It likely is a test, we're just not sure what kind.
      return "unknown";
    }

    // Likely not a test
    return null;
  },

  getIsWorker(filePath) {
    let filename = path.basename(this.cleanUpPath(filePath)).toLowerCase();

    return filename.includes("worker");
  },

  /**
   * Gets the root directory of the repository by walking up directories from
   * this file until a .eslintignore file is found. If this fails, the same
   * procedure will be attempted from the current working dir.
   * @return {String} The absolute path of the repository directory
   */
  get rootDir() {
    if (!gRootDir) {
      function searchUpForIgnore(dirName, filename) {
        let parsed = path.parse(dirName);
        while (parsed.root !== dirName) {
          if (fs.existsSync(path.join(dirName, filename))) {
            return dirName;
          }
          // Move up a level
          dirName = parsed.dir;
          parsed = path.parse(dirName);
        }
        return null;
      }

      let possibleRoot = searchUpForIgnore(path.dirname(module.filename), ".eslintignore");
      if (!possibleRoot) {
        possibleRoot = searchUpForIgnore(path.resolve(), ".eslintignore");
      }
      if (!possibleRoot) {
        possibleRoot = searchUpForIgnore(path.resolve(), "package.json");
      }
      if (!possibleRoot) {
        // We've couldn't find a root from the module or CWD, so lets just go
        // for the CWD. We really don't want to throw if possible, as that
        // tends to give confusing results when used with ESLint.
        possibleRoot = process.cwd();
      }

      gRootDir = possibleRoot;
    }

    return gRootDir;
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
  getAbsoluteFilePath(context) {
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
    }
      // Case 1: executed form in a nested directory, e.g. from a text editor:
      //   fileName: a/b/c/d.js
      //   cwd: /path/to/mozilla/repo/a/b/c
    var dirName = path.dirname(fileName);
    return cwd.slice(0, cwd.length - dirName.length) + fileName;

  },

  /**
   * When ESLint is run from SublimeText, paths retrieved from
   * context.getFileName contain leading and trailing double-quote characters.
   * These characters need to be removed.
   */
  cleanUpPath(pathName) {
    return pathName.replace(/^"/, "").replace(/"$/, "");
  },

  get globalScriptsPath() {
    return path.join(this.rootDir, "browser",
                     "base", "content", "global-scripts.inc");
  },

  isMozillaCentralBased() {
    return fs.existsSync(this.globalScriptsPath);
  },

  getSavedEnvironmentItems(environment) {
    return require("./environments/saved-globals.json").environments[environment];
  }
};
