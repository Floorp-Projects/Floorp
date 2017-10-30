/**
 * @fileoverview Require use of Services.* rather than getService.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

"use strict";
const helpers = require("../helpers");
const fs = require("fs");
const path = require("path");

/**
 * An object that is used to help parse the AST of Services.jsm to extract
 * the services that it caches.
 *
 * It is specifically built around the current structure of the Services.jsm
 * file.
 */
var servicesASTParser = {
  identifiers: {},
  // These interfaces are difficult/not possible to get via processing.
  result: {
    "nsIPrefBranch": "prefs",
    "nsIPrefService": "prefs",
    "nsIXULRuntime": "appinfo",
    "nsIXULAppInfo": "appinfo",
    "nsIDirectoryService": "dirsvc",
    "nsIProperties": "dirsvc",
    "nsIFrameScriptLoader": "mm",
    "nsIProcessScriptLoader": "ppmm",
    "nsIIOService": "io",
    "nsIIOService2": "io",
    "nsISpeculativeConnect": "io",
    "nsICookieManager": "cookies"
  },

  /**
   * This looks for any global variable declarations that are being initialised
   * with objects, and records the assumed interface definitions.
   */
  VariableDeclaration(node, parents) {
    if (node.declarations.length === 1 &&
        node.declarations[0].id &&
        helpers.getIsGlobalScope(parents) &&
        node.declarations[0].init.type === "ObjectExpression") {
      let name = node.declarations[0].id.name;
      let interfaces = {};

      for (let property of node.declarations[0].init.properties) {
        interfaces[property.key.name] = property.value.elements[1].value;
      }

      this.identifiers[name] = interfaces;
    }
  },

  /**
   * This looks for any additions to the global variable declarations, and adds
   * them to the identifier tables created by the VariableDeclaration calls.
   */
  AssignmentExpression(node, parents) {
    if (node.left.type === "MemberExpression" &&
        node.right.type === "ArrayExpression" &&
        helpers.getIsGlobalScope(parents)) {
      let variableName = node.left.object.name;
      if (variableName in this.identifiers) {
        let servicesPropName = node.left.property.name;
        this.identifiers[variableName][servicesPropName] = node.right.elements[1].value;
      }
    }
  },

  /**
   * This looks for any XPCOMUtils.defineLazyServiceGetters calls, and looks
   * into the arguments they are called with to work out the interfaces that
   * Services.jsm is caching.
   */
  CallExpression(node) {
    if (node.callee.object &&
        node.callee.object.name === "XPCOMUtils" &&
        node.callee.property &&
        node.callee.property.name === "defineLazyServiceGetters" &&
        node.arguments.length >= 2) {
      // The second argument has the getters name.
      let gettersVarName = node.arguments[1].name;
      if (!(gettersVarName in this.identifiers)) {
        throw new Error(`Could not find definition for ${gettersVarName}`);
      }

      for (let name of Object.keys(this.identifiers[gettersVarName])) {
        this.result[this.identifiers[gettersVarName][name]] = name;
      }
    }
  }
};

function getInterfacesFromServicesFile() {
  let filePath = path.join(helpers.rootDir, "toolkit", "modules", "Services.jsm");

  let content = fs.readFileSync(filePath, "utf8");

  // Parse the content into an AST
  let ast = helpers.getAST(content);

  helpers.walkAST(ast, (type, node, parents) => {
    if (type in servicesASTParser) {
      servicesASTParser[type](node, parents);
    }
  });

  return servicesASTParser.result;
}

let getServicesInterfaceMap = helpers.isMozillaCentralBased() ?
  getInterfacesFromServicesFile() :
  helpers.getSavedRuleData("use-services.js");

// -----------------------------------------------------------------------------
// Rule Definition
// -----------------------------------------------------------------------------
module.exports = function(context) {
  // ---------------------------------------------------------------------------
  // Public
  //  --------------------------------------------------------------------------
  return {
    // ESLint assumes that items returned here are going to be a listener
    // for part of the rule. We want to export the servicesInterfaceArray for
    // the purposes of createExports, so we return it via a function which
    // makes everything happy.
    getServicesInterfaceMap() {
      return getServicesInterfaceMap;
    },

    CallExpression(node) {
      if (!node.callee ||
          !node.callee.property ||
          node.callee.property.type != "Identifier" ||
          node.callee.property.name != "getService" ||
          node.arguments.length != 1 ||
          !node.arguments[0].property ||
          node.arguments[0].property.type != "Identifier" ||
          !node.arguments[0].property.name ||
          !(node.arguments[0].property.name in getServicesInterfaceMap)) {
        return;
      }

      let serviceName = getServicesInterfaceMap[node.arguments[0].property.name];

      context.report(node,
                     `Use Services.${serviceName} rather than getService().`);
    }
  };
};
