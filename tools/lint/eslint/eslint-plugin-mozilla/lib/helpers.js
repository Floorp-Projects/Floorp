/**
 * @fileoverview A collection of helper functions.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
"use strict";

const parser = require("@babel/eslint-parser");
const { analyze } = require("eslint-scope");
const { KEYS: defaultVisitorKeys } = require("eslint-visitor-keys");
const estraverse = require("estraverse");
const path = require("path");
const fs = require("fs");
const ini = require("multi-ini");
const toml = require("toml-eslint-parser");
const recommendedConfig = require("./configs/recommended");

var gRootDir = null;
var directoryManifests = new Map();

let xpidlData;

module.exports = {
  get iniParser() {
    if (!this._iniParser) {
      this._iniParser = new ini.Parser();
    }
    return this._iniParser;
  },

  get servicesData() {
    return require("./services.json");
  },

  /**
   * Obtains xpidl data from the object directory specified in the
   * environment.
   *
   * @returns {Map<string, object>}
   *   A map of interface names to the interface details.
   */
  get xpidlData() {
    let xpidlDir;

    if (process.env.TASK_ID && !process.env.MOZ_XPT_ARTIFACTS_DIR) {
      throw new Error(
        "MOZ_XPT_ARTIFACTS_DIR must be set for this rule in automation"
      );
    }
    xpidlDir = process.env.MOZ_XPT_ARTIFACTS_DIR;

    if (!xpidlDir && process.env.MOZ_OBJDIR) {
      xpidlDir = `${process.env.MOZ_OBJDIR}/dist/xpt_artifacts/`;
      if (!fs.existsSync(xpidlDir)) {
        xpidlDir = `${process.env.MOZ_OBJDIR}/config/makefiles/xpidl/`;
      }
    }
    if (!xpidlDir) {
      throw new Error(
        "MOZ_OBJDIR must be defined in the environment for this rule, i.e. MOZ_OBJDIR=objdir-ff ./mach ..."
      );
    }
    if (xpidlData) {
      return xpidlData;
    }
    let files = fs.readdirSync(`${xpidlDir}`);
    // `Makefile` is an expected file in the directory.
    if (files.length <= 1) {
      throw new Error("Missing xpidl data files, maybe you need to build?");
    }
    xpidlData = new Map();
    for (let file of files) {
      if (!file.endsWith(".xpt")) {
        continue;
      }
      let data = JSON.parse(fs.readFileSync(path.join(`${xpidlDir}`, file)));
      for (let details of data) {
        xpidlData.set(details.name, details);
      }
    }
    return xpidlData;
  },

  /**
   * Gets the abstract syntax tree (AST) of the JavaScript source code contained
   * in sourceText. This matches the results for an eslint parser, see
   * https://eslint.org/docs/developer-guide/working-with-custom-parsers.
   *
   * @param  {String} sourceText
   *         Text containing valid JavaScript.
   * @param  {Object} astOptions
   *         Extra configuration to pass to the espree parser, these will override
   *         the configuration from getPermissiveConfig().
   * @param  {Object} configOptions
   *         Extra options for getPermissiveConfig().
   *
   * @return {Object}
   *         Returns an object containing `ast`, `scopeManager` and
   *         `visitorKeys`
   */
  parseCode(sourceText, astOptions = {}, configOptions = {}) {
    // Use a permissive config file to allow parsing of anything that Espree
    // can parse.
    let config = { ...this.getPermissiveConfig(configOptions), ...astOptions };

    let parseResult =
      "parseForESLint" in parser
        ? parser.parseForESLint(sourceText, config)
        : { ast: parser.parse(sourceText, config) };

    let visitorKeys = parseResult.visitorKeys || defaultVisitorKeys;
    visitorKeys.ExperimentalRestProperty = visitorKeys.RestElement;
    visitorKeys.ExperimentalSpreadProperty = visitorKeys.SpreadElement;

    return {
      ast: parseResult.ast,
      scopeManager: parseResult.scopeManager || analyze(parseResult.ast),
      visitorKeys,
    };
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
          throw new Error(
            `getASTSource unsupported computed MemberExpression in ${filename}`
          );
        }
        return (
          this.getASTSource(node.object) +
          "." +
          this.getASTSource(node.property)
        );
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
        return (
          this.getASTSource(node.left) + " = " + this.getASTSource(node.right)
        );
      case "BinaryExpression":
        return (
          this.getASTSource(node.left) +
          " " +
          node.operator +
          " " +
          this.getASTSource(node.right)
        );
      case "UnaryExpression":
        return node.operator + " " + this.getASTSource(node.argument);
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
   * @param  {Array} visitorKeys
   *         The visitor keys to use for the AST.
   * @param  {Function} listener
   *         A callback function to call for the nodes. Passed three arguments,
   *         event type, node and an array of parent nodes for the current node.
   */
  walkAST(ast, visitorKeys, listener) {
    let parents = [];

    estraverse.traverse(ast, {
      enter(node, parent) {
        listener(node.type, node, parents);

        parents.push(node);
      },

      leave(node, parent) {
        if (!parents.length) {
          throw new Error("Left more nodes than entered.");
        }
        parents.pop();
      },

      keys: visitorKeys,
    });
    if (parents.length) {
      throw new Error("Entered more nodes than left.");
    }
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
   * @param {Object} [node]
   *        The AST node that defined the globals.
   */
  addVarToScope(name, scope, writable, node) {
    scope.__defineGeneric(name, scope.set, scope.variables, null, null);

    let variable = scope.set.get(name);
    variable.eslintExplicitGlobal = false;
    variable.writeable = writable;
    if (node) {
      variable.defs.push({
        type: "Variable",
        node,
        name: { name, parent: node.parent },
      });
      variable.identifiers.push(node);
    }

    // Walk to the global scope which holds all undeclared variables.
    while (scope.type != "global") {
      scope = scope.upper;
    }

    // "through" contains all references with no found definition.
    scope.through = scope.through.filter(function (reference) {
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
   * @param {Object} [node]
   *        The AST node that defined the globals.
   */
  addGlobals(globalVars, scope, node) {
    globalVars.forEach(v =>
      this.addVarToScope(v.name, scope, v.writable, v.explicit && node)
    );
  },

  /**
   * To allow espree to parse almost any JavaScript we need as many features as
   * possible turned on. This method returns that config.
   *
   * @param {Object} options
   *        {
   *          useBabel: {boolean} whether to set babelOptions.
   *        }
   * @return {Object}
   *         Espree compatible permissive config.
   */
  getPermissiveConfig({ useBabel = true } = {}) {
    const config = {
      range: true,
      requireConfigFile: false,
      babelOptions: {
        // configFile: path.join(gRootDir, ".babel-eslint.rc.js"),
        // parserOpts: {
        //   plugins: [
        //     "@babel/plugin-proposal-class-static-block",
        //     "@babel/plugin-syntax-class-properties",
        //     "@babel/plugin-syntax-jsx",
        //   ],
        // },
      },
      loc: true,
      comment: true,
      attachComment: true,
      ecmaVersion: this.getECMAVersion(),
      sourceType: "script",
    };

    if (useBabel && this.isMozillaCentralBased()) {
      config.babelOptions.configFile = path.join(
        gRootDir,
        ".babel-eslint.rc.js"
      );
    }
    return config;
  },

  /**
   * Returns the ECMA version of the recommended config.
   *
   * @return {Number} The ECMA version of the recommended config.
   */
  getECMAVersion() {
    return recommendedConfig.parserOptions.ecmaVersion;
  },

  /**
   * Check whether it's inside top-level script.
   *
   * @param {Array} ancestors
   *        The parents of the current node.
   *
   * @return {Boolean}
   *         True or false
   */
  getIsTopLevelScript(ancestors) {
    for (let parent of ancestors) {
      switch (parent.type) {
        case "ArrowFunctionExpression":
        case "FunctionDeclaration":
        case "FunctionExpression":
        case "PropertyDefinition":
        case "StaticBlock":
          return false;
      }
    }
    return true;
  },

  isTopLevel(ancestors) {
    for (let parent of ancestors) {
      switch (parent.type) {
        case "ArrowFunctionExpression":
        case "FunctionDeclaration":
        case "FunctionExpression":
        case "PropertyDefinition":
        case "StaticBlock":
        case "BlockStatement":
          return false;
      }
    }
    return true;
  },

  /**
   * Check whether `this` expression points the global this.
   *
   * @param {Array} ancestors
   *        The parents of the current node.
   *
   * @return {Boolean}
   *         True or false
   */
  getIsGlobalThis(ancestors) {
    for (let parent of ancestors) {
      switch (parent.type) {
        case "FunctionDeclaration":
        case "FunctionExpression":
        case "PropertyDefinition":
        case "StaticBlock":
          return false;
      }
    }
    return true;
  },

  /**
   * Check whether the node is evaluated at top-level script unconditionally.
   *
   * @param {Array} ancestors
   *        The parents of the current node.
   *
   * @return {Boolean}
   *         True or false
   */
  getIsTopLevelAndUnconditionallyExecuted(ancestors) {
    for (let parent of ancestors) {
      switch (parent.type) {
        // Control flow
        case "IfStatement":
        case "SwitchStatement":
        case "TryStatement":
        case "WhileStatement":
        case "DoWhileStatement":
        case "ForStatement":
        case "ForInStatement":
        case "ForOfStatement":
          return false;

        // Function
        case "FunctionDeclaration":
        case "FunctionExpression":
        case "ArrowFunctionExpression":
        case "ClassBody":
          return false;

        // Branch
        case "LogicalExpression":
        case "ConditionalExpression":
        case "ChainExpression":
          return false;

        case "AssignmentExpression":
          switch (parent.operator) {
            // Branch
            case "||=":
            case "&&=":
            case "??=":
              return false;
          }
          break;

        // Implicit branch (default value)
        case "ObjectPattern":
        case "ArrayPattern":
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

    let names = fs
      .readdirSync(dir)
      .filter(
        name =>
          (name.startsWith("head") || name.startsWith("xpcshell-head")) &&
          name.endsWith(".js")
      )
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
    let names = [];
    try {
      names = fs.readdirSync(dir);
    } catch (err) {
      // Ignore directory not found, it might be faked by a test
      if (err.code !== "ENOENT") {
        throw err;
      }
    }

    for (let name of names) {
      if (name.endsWith(".ini")) {
        try {
          let manifest = this.iniParser.parse(
            fs.readFileSync(path.join(dir, name), "utf8").split("\n")
          );
          manifests.push({
            file: path.join(dir, name),
            manifest,
          });
        } catch (e) {}
      } else if (name.endsWith(".toml")) {
        try {
          const ast = toml.parseTOML(
            fs.readFileSync(path.join(dir, name), "utf8")
          );
          var manifest = {};
          ast.body.forEach(top => {
            if (top.type == "TOMLTopLevelTable") {
              top.body.forEach(obj => {
                if (obj.type == "TOMLTable") {
                  manifest[obj.resolvedKey] = {};
                }
              });
            }
          });
          manifests.push({
            file: path.join(dir, name),
            manifest,
          });
        } catch (e) {
          console.log(
            "TOML ERROR: " +
              e.message +
              " @line: " +
              e.lineNumber +
              ", column: " +
              e.column
          );
        }
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

  /*
   * Check if this is an .sjs file.
   */
  getIsSjs(scope) {
    let filepath = this.cleanUpPath(scope.getFilename());

    return path.extname(filepath) == ".sjs";
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

      let possibleRoot = searchUpForIgnore(
        path.dirname(module.filename),
        ".eslintignore"
      );
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

  get globalScriptPaths() {
    return [
      path.join(this.rootDir, "browser", "base", "content", "browser.xhtml"),
      path.join(
        this.rootDir,
        "browser",
        "base",
        "content",
        "global-scripts.inc"
      ),
    ];
  },

  isMozillaCentralBased() {
    return fs.existsSync(this.globalScriptPaths[0]);
  },

  getSavedEnvironmentItems(environment) {
    return require("./environments/saved-globals.json").environments[
      environment
    ];
  },

  getSavedRuleData(rule) {
    return require("./rules/saved-rules-data.json").rulesData[rule];
  },

  getBuildEnvironment() {
    var { execFileSync } = require("child_process");
    var output = execFileSync(
      path.join(this.rootDir, "mach"),
      ["environment", "--format=json"],
      { silent: true }
    );
    return JSON.parse(output);
  },

  /**
   * Extract the path of require (and require-like) helpers used in DevTools.
   */
  getDevToolsRequirePath(node) {
    if (
      node.callee.type == "Identifier" &&
      node.callee.name == "require" &&
      node.arguments.length == 1 &&
      node.arguments[0].type == "Literal"
    ) {
      return node.arguments[0].value;
    } else if (
      node.callee.type == "MemberExpression" &&
      node.callee.property.type == "Identifier" &&
      node.callee.property.name == "lazyRequireGetter" &&
      node.arguments.length >= 3 &&
      node.arguments[2].type == "Literal"
    ) {
      return node.arguments[2].value;
    }
    return null;
  },

  /**
   * Returns property name from MemberExpression. Also accepts Identifier for consistency.
   * @param {import("estree").MemberExpression | import("estree").Identifier} node
   * @returns {string | null}
   *
   * @example `foo` gives "foo"
   * @example `foo.bar` gives "bar"
   * @example `foo.bar.baz` gives "baz"
   */
  maybeGetMemberPropertyName(node) {
    if (node.type === "MemberExpression") {
      return node.property.name;
    }
    if (node.type === "Identifier") {
      return node.name;
    }
    return null;
  },
};
