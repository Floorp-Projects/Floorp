/**
 * @fileoverview functions for scanning an AST for globals including
 *               traversing referenced scripts.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

"use strict";

const path = require("path");
const fs = require("fs");
const helpers = require("./helpers");
const htmlparser = require("htmlparser2");
const testharnessEnvironment = require("./environments/testharness.js");

const callExpressionDefinitions = [
  /^loader\.lazyGetter\((?:globalThis|this), "(\w+)"/,
  /^loader\.lazyServiceGetter\((?:globalThis|this), "(\w+)"/,
  /^loader\.lazyRequireGetter\((?:globalThis|this), "(\w+)"/,
  /^XPCOMUtils\.defineLazyGetter\((?:globalThis|this), "(\w+)"/,
  /^ChromeUtils\.defineLazyGetter\((?:globalThis|this), "(\w+)"/,
  /^ChromeUtils\.defineModuleGetter\((?:globalThis|this), "(\w+)"/,
  /^XPCOMUtils\.defineLazyPreferenceGetter\((?:globalThis|this), "(\w+)"/,
  /^XPCOMUtils\.defineLazyScriptGetter\((?:globalThis|this), "(\w+)"/,
  /^XPCOMUtils\.defineLazyServiceGetter\((?:globalThis|this), "(\w+)"/,
  /^XPCOMUtils\.defineConstant\((?:globalThis|this), "(\w+)"/,
  /^DevToolsUtils\.defineLazyGetter\((?:globalThis|this), "(\w+)"/,
  /^Object\.defineProperty\((?:globalThis|this), "(\w+)"/,
  /^Reflect\.defineProperty\((?:globalThis|this), "(\w+)"/,
  /^this\.__defineGetter__\("(\w+)"/,
];

const callExpressionMultiDefinitions = [
  "XPCOMUtils.defineLazyGlobalGetters(this,",
  "XPCOMUtils.defineLazyGlobalGetters(globalThis,",
  "XPCOMUtils.defineLazyModuleGetters(this,",
  "XPCOMUtils.defineLazyModuleGetters(globalThis,",
  "XPCOMUtils.defineLazyServiceGetters(this,",
  "XPCOMUtils.defineLazyServiceGetters(globalThis,",
  "ChromeUtils.defineESModuleGetters(this,",
  "ChromeUtils.defineESModuleGetters(globalThis,",
  "loader.lazyRequireGetter(this,",
  "loader.lazyRequireGetter(globalThis,",
];

const subScriptMatches = [
  /Services\.scriptloader\.loadSubScript\("(.*?)", this\)/,
];

const workerImportFilenameMatch = /(.*\/)*((.*?)\.jsm?)/;

/**
 * Parses a list of "name:boolean_value" or/and "name" options divided by comma
 * or whitespace.
 *
 * This function was copied from eslint.js
 *
 * @param {string} string The string to parse.
 * @param {Comment} comment The comment node which has the string.
 * @returns {Object} Result map object of names and boolean values
 */
function parseBooleanConfig(string, comment) {
  let items = {};

  // Collapse whitespace around : to make parsing easier
  string = string.replace(/\s*:\s*/g, ":");
  // Collapse whitespace around ,
  string = string.replace(/\s*,\s*/g, ",");

  string.split(/\s|,+/).forEach(function (name) {
    if (!name) {
      return;
    }

    let pos = name.indexOf(":");
    let value;
    if (pos !== -1) {
      value = name.substring(pos + 1, name.length);
      name = name.substring(0, pos);
    }

    items[name] = {
      value: value === "true",
      comment,
    };
  });

  return items;
}

/**
 * Global discovery can require parsing many files. This map of
 * {String} => {Object} caches what globals were discovered for a file path.
 */
const globalCache = new Map();

/**
 * Global discovery can occasionally meet circular dependencies due to the way
 * js files are included via html/xhtml files etc. This set is used to avoid
 * getting into loops whilst the discovery is in progress.
 */
var globalDiscoveryInProgressForFiles = new Set();

/**
 * When looking for globals in HTML files, it can be common to have more than
 * one script tag with inline javascript. These will normally be called together,
 * so we store the globals for just the last HTML file processed.
 */
var lastHTMLGlobals = {};

/**
 * Attempts to convert an CallExpressions that look like module imports
 * into global variable definitions.
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
function convertCallExpressionToGlobals(node, isGlobal) {
  let express = node.expression;
  if (
    express.type === "CallExpression" &&
    express.callee.type === "MemberExpression" &&
    express.callee.object &&
    express.callee.object.type === "Identifier" &&
    express.arguments.length === 1 &&
    express.arguments[0].type === "ArrayExpression" &&
    express.callee.property.type === "Identifier" &&
    express.callee.property.name === "importGlobalProperties"
  ) {
    return express.arguments[0].elements.map(literal => {
      return {
        explicit: true,
        name: literal.value,
        writable: false,
      };
    });
  }

  let source;
  try {
    source = helpers.getASTSource(node);
  } catch (e) {
    return [];
  }

  // The definition matches below must be in the global scope for us to define
  // a global, so bail out early if we're not a global.
  if (!isGlobal) {
    return [];
  }

  for (let reg of subScriptMatches) {
    let match = source.match(reg);
    if (match) {
      return getGlobalsForScript(match[1], "script").map(g => {
        // We don't want any loadSubScript globals to be explicit, as this
        // could trigger no-unused-vars when importing multiple variables
        // from a script and not using all of them.
        g.explicit = false;
        return g;
      });
    }
  }

  for (let reg of callExpressionDefinitions) {
    let match = source.match(reg);
    if (match) {
      return [{ name: match[1], writable: true, explicit: true }];
    }
  }

  if (
    callExpressionMultiDefinitions.some(expr => source.startsWith(expr)) &&
    node.expression.arguments[1]
  ) {
    let arg = node.expression.arguments[1];
    if (arg.type === "ObjectExpression") {
      return arg.properties
        .map(p => ({
          name: p.type === "Property" && p.key.name,
          writable: true,
          explicit: true,
        }))
        .filter(g => g.name);
    }
    if (arg.type === "ArrayExpression") {
      return arg.elements
        .map(p => ({
          name: p.type === "Literal" && p.value,
          writable: true,
          explicit: true,
        }))
        .filter(g => typeof g.name == "string");
    }
  }

  if (
    node.expression.callee.type == "MemberExpression" &&
    node.expression.callee.property.type == "Identifier" &&
    node.expression.callee.property.name == "defineLazyScriptGetter"
  ) {
    // The case where we have a single symbol as a string has already been
    // handled by the regexp, so we have an array of symbols here.
    return node.expression.arguments[1].elements.map(n => ({
      name: n.value,
      writable: true,
      explicit: true,
    }));
  }

  return [];
}

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
function convertThisAssignmentExpressionToGlobals(node, isGlobal) {
  if (
    isGlobal &&
    node.expression.left &&
    node.expression.left.object &&
    node.expression.left.object.type === "ThisExpression" &&
    node.expression.left.property &&
    node.expression.left.property.type === "Identifier"
  ) {
    return [{ name: node.expression.left.property.name, writable: true }];
  }
  return [];
}

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
function convertWorkerExpressionToGlobals(node, isGlobal, dirname) {
  let results = [];
  let expr = node.expression;

  if (
    node.expression.type === "CallExpression" &&
    expr.callee &&
    expr.callee.type === "Identifier" &&
    expr.callee.name === "importScripts"
  ) {
    for (var arg of expr.arguments) {
      var match = arg.value && arg.value.match(workerImportFilenameMatch);
      if (match) {
        if (!match[1]) {
          let filePath = path.resolve(dirname, match[2]);
          if (fs.existsSync(filePath)) {
            let additionalGlobals = module.exports.getGlobalsForFile(filePath);
            results = results.concat(additionalGlobals);
          }
        }
        // Import with relative/absolute path should explicitly use
        // `import-globals-from` comment.
      }
    }
  }

  return results;
}

/**
 * Attempts to load the globals for a given script.
 *
 * @param {string} src
 *   The source path or url of the script to look for.
 * @param {string} type
 *   The type of the current file (script/module).
 * @param {string} [dir]
 *   The directory of the current file.
 * @returns {object[]}
 *   An array of objects with details of the globals in them.
 */
function getGlobalsForScript(src, type, dir) {
  let scriptName;
  if (src.includes("http:")) {
    // We don't handle this currently as the paths are complex to match.
  } else if (src.startsWith("chrome://mochikit/content/")) {
    // Various ways referencing test files.
    src = src.replace("chrome://mochikit/content/", "/");
    scriptName = path.join(helpers.rootDir, "testing", "mochitest", src);
  } else if (src.startsWith("chrome://mochitests/content/browser")) {
    src = src.replace("chrome://mochitests/content/browser", "");
    scriptName = path.join(helpers.rootDir, src);
  } else if (src.includes("SimpleTest")) {
    // This is another way of referencing test files...
    scriptName = path.join(helpers.rootDir, "testing", "mochitest", src);
  } else if (src.startsWith("/tests/")) {
    scriptName = path.join(helpers.rootDir, src.substring(7));
  } else if (src.startsWith("/resources/testharness.js")) {
    return Object.keys(testharnessEnvironment.globals).map(name => ({
      name,
      writable: true,
    }));
  } else if (dir) {
    // Fallback to hoping this is a relative path.
    scriptName = path.join(dir, src);
  }
  if (scriptName && fs.existsSync(scriptName)) {
    return module.exports.getGlobalsForFile(scriptName, {
      ecmaVersion: helpers.getECMAVersion(),
      sourceType: type,
    });
  }
  return [];
}

/**
 * An object that returns found globals for given AST node types. Each prototype
 * property should be named for a node type and accepts a node parameter and a
 * parents parameter which is a list of the parent nodes of the current node.
 * Each returns an array of globals found.
 *
 * @param  {String} filePath
 *         The absolute path of the file being parsed.
 */
function GlobalsForNode(filePath, context) {
  this.path = filePath;
  this.context = context;

  if (this.path) {
    this.dirname = path.dirname(this.path);
  } else {
    this.dirname = null;
  }
}

GlobalsForNode.prototype = {
  Program(node) {
    let globals = [];
    for (let comment of node.comments) {
      if (comment.type !== "Block") {
        continue;
      }
      let value = comment.value.trim();
      value = value.replace(/\n/g, "");

      // We have to discover any globals that ESLint would have defined through
      // comment directives.
      let match = /^globals?\s+(.+)/.exec(value);
      if (match) {
        let values = parseBooleanConfig(match[1].trim(), node);
        for (let name of Object.keys(values)) {
          globals.push({
            name,
            writable: values[name].value,
          });
        }
        // We matched globals, so we won't match import-globals-from.
        continue;
      }

      match = /^import-globals-from\s+(.+)$/.exec(value);
      if (!match) {
        continue;
      }

      if (!this.dirname) {
        // If this is testing context without path, ignore import.
        return globals;
      }

      let filePath = match[1].trim();

      if (filePath.endsWith(".mjs")) {
        if (this.context) {
          this.context.report(
            comment,
            "import-globals-from does not support module files - use a direct import instead"
          );
        } else {
          // Fall back to throwing an error, as we do not have a context in all situations,
          // e.g. when loading the environment.
          throw new Error(
            "import-globals-from does not support module files - use a direct import instead"
          );
        }
        continue;
      }

      if (!path.isAbsolute(filePath)) {
        filePath = path.resolve(this.dirname, filePath);
      } else {
        filePath = path.join(helpers.rootDir, filePath);
      }
      globals = globals.concat(module.exports.getGlobalsForFile(filePath));
    }

    return globals;
  },

  ExpressionStatement(node, parents, globalScope) {
    let isGlobal = helpers.getIsGlobalThis(parents);
    let globals = [];

    // Note: We check the expression types here and only call the necessary
    // functions to aid performance.
    if (node.expression.type === "AssignmentExpression") {
      globals = convertThisAssignmentExpressionToGlobals(node, isGlobal);
    } else if (node.expression.type === "CallExpression") {
      globals = convertCallExpressionToGlobals(node, isGlobal);
    }

    // Here we assume that if importScripts is set in the global scope, then
    // this is a worker. It would be nice if eslint gave us a way of getting
    // the environment directly.
    //
    // If this is testing context without path, ignore import.
    if (globalScope && globalScope.set.get("importScripts") && this.dirname) {
      let workerDetails = convertWorkerExpressionToGlobals(
        node,
        isGlobal,
        this.dirname
      );
      globals = globals.concat(workerDetails);
    }

    return globals;
  },
};

module.exports = {
  /**
   * Returns all globals for a given file. Recursively searches through
   * import-globals-from directives and also includes globals defined by
   * standard eslint directives.
   *
   * @param  {String} filePath
   *         The absolute path of the file to be parsed.
   * @param  {Object} astOptions
   *         Extra options to pass to the parser.
   * @return {Array}
   *         An array of objects that contain details about the globals:
   *         - {String} name
   *                    The name of the global.
   *         - {Boolean} writable
   *                     If the global is writeable or not.
   */
  getGlobalsForFile(filePath, astOptions = {}) {
    if (globalCache.has(filePath)) {
      return globalCache.get(filePath);
    }

    if (globalDiscoveryInProgressForFiles.has(filePath)) {
      // We're already processing this file, so return an empty set for now -
      // the initial processing will pick up on the globals for this file.
      return [];
    }
    globalDiscoveryInProgressForFiles.add(filePath);

    let content = fs.readFileSync(filePath, "utf8");

    // Parse the content into an AST
    let { ast, scopeManager, visitorKeys } = helpers.parseCode(
      content,
      astOptions
    );

    // Discover global declarations
    let globalScope = scopeManager.acquire(ast);

    let globals = Object.keys(globalScope.variables).map(v => ({
      name: globalScope.variables[v].name,
      writable: true,
    }));

    // Walk over the AST to find any of our custom globals
    let handler = new GlobalsForNode(filePath);

    helpers.walkAST(ast, visitorKeys, (type, node, parents) => {
      if (type in handler) {
        let newGlobals = handler[type](node, parents, globalScope);
        globals.push.apply(globals, newGlobals);
      }
    });

    globalCache.set(filePath, globals);

    globalDiscoveryInProgressForFiles.delete(filePath);
    return globals;
  },

  /**
   * Returns all globals for a code.
   * This is only for testing.
   *
   * @param  {String} code
   *         The JS code
   * @param  {Object} astOptions
   *         Extra options to pass to the parser.
   * @return {Array}
   *         An array of objects that contain details about the globals:
   *         - {String} name
   *                    The name of the global.
   *         - {Boolean} writable
   *                     If the global is writeable or not.
   */
  getGlobalsForCode(code, astOptions = {}) {
    // Parse the content into an AST
    let { ast, scopeManager, visitorKeys } = helpers.parseCode(
      code,
      astOptions,
      { useBabel: false }
    );

    // Discover global declarations
    let globalScope = scopeManager.acquire(ast);

    let globals = Object.keys(globalScope.variables).map(v => ({
      name: globalScope.variables[v].name,
      writable: true,
    }));

    // Walk over the AST to find any of our custom globals
    let handler = new GlobalsForNode(null);

    helpers.walkAST(ast, visitorKeys, (type, node, parents) => {
      if (type in handler) {
        let newGlobals = handler[type](node, parents, globalScope);
        globals.push.apply(globals, newGlobals);
      }
    });

    return globals;
  },

  /**
   * Returns all the globals for an html file that are defined by imported
   * scripts (i.e. <script src="foo.js">).
   *
   * This function will cache results for one html file only - we expect
   * this to be called sequentially for each chunk of a HTML file, rather
   * than chucks of different files in random order.
   *
   * @param  {String} filePath
   *         The absolute path of the file to be parsed.
   * @return {Array}
   *         An array of objects that contain details about the globals:
   *         - {String} name
   *                    The name of the global.
   *         - {Boolean} writable
   *                     If the global is writeable or not.
   */
  getImportedGlobalsForHTMLFile(filePath) {
    if (lastHTMLGlobals.filename === filePath) {
      return lastHTMLGlobals.globals;
    }

    let dir = path.dirname(filePath);
    let globals = [];

    let content = fs.readFileSync(filePath, "utf8");
    let scriptSrcs = [];

    // We use htmlparser as this ensures we find the script tags correctly.
    let parser = new htmlparser.Parser(
      {
        onopentag(name, attribs) {
          if (name === "script" && "src" in attribs) {
            scriptSrcs.push({
              src: attribs.src,
              type:
                "type" in attribs && attribs.type == "module"
                  ? "module"
                  : "script",
            });
          }
        },
      },
      {
        xmlMode: filePath.endsWith("xhtml"),
      }
    );

    parser.parseComplete(content);

    for (let script of scriptSrcs) {
      // Ensure that the script src isn't just "".
      if (!script.src) {
        continue;
      }
      globals.push(...getGlobalsForScript(script.src, script.type, dir));
    }

    lastHTMLGlobals.filePath = filePath;
    return (lastHTMLGlobals.globals = globals);
  },

  /**
   * Intended to be used as-is for an ESLint rule that parses for globals in
   * the current file and recurses through import-globals-from directives.
   *
   * @param  {Object} context
   *         The ESLint parsing context.
   */
  getESLintGlobalParser(context) {
    let globalScope;

    let parser = {
      Program(node) {
        globalScope = context.getScope();
      },
    };
    let filename = context.getFilename();

    let extraHTMLGlobals = [];
    if (filename.endsWith(".html") || filename.endsWith(".xhtml")) {
      extraHTMLGlobals = module.exports.getImportedGlobalsForHTMLFile(filename);
    }

    // Install thin wrappers around GlobalsForNode
    let handler = new GlobalsForNode(helpers.getAbsoluteFilePath(context));

    for (let type of Object.keys(GlobalsForNode.prototype)) {
      parser[type] = function (node) {
        if (type === "Program") {
          globalScope = context.getScope();
          helpers.addGlobals(extraHTMLGlobals, globalScope);
        }
        let globals = handler[type](node, context.getAncestors(), globalScope);
        helpers.addGlobals(
          globals,
          globalScope,
          node.type !== "Program" && node
        );
      };
    }

    return parser;
  },
};
