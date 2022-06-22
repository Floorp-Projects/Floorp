/**
 * @fileoverview Ensures that definitions and uses of properties on the
 * ``lazy`` object are valid.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

"use strict";
const helpers = require("../helpers");

const items = [
  "loader",
  "XPCOMUtils",
  "Integration",
  "ChromeUtils",
  "DevToolsUtils",
  "Object",
  "Reflect",
];

const callExpressionDefinitions = [
  /^loader\.lazyGetter\(lazy, "(\w+)"/,
  /^loader\.lazyImporter\(lazy, "(\w+)"/,
  /^loader\.lazyServiceGetter\(lazy, "(\w+)"/,
  /^loader\.lazyRequireGetter\(lazy, "(\w+)"/,
  /^XPCOMUtils\.defineLazyGetter\(lazy, "(\w+)"/,
  /^Integration\.downloads\.defineModuleGetter\(lazy, "(\w+)"/,
  /^XPCOMUtils\.defineLazyModuleGetter\(lazy, "(\w+)"/,
  /^ChromeUtils\.defineModuleGetter\(lazy, "(\w+)"/,
  /^XPCOMUtils\.defineLazyPreferenceGetter\(lazy, "(\w+)"/,
  /^XPCOMUtils\.defineLazyProxy\(lazy, "(\w+)"/,
  /^XPCOMUtils\.defineLazyScriptGetter\(lazy, "(\w+)"/,
  /^XPCOMUtils\.defineLazyServiceGetter\(lazy, "(\w+)"/,
  /^XPCOMUtils\.defineConstant\(lazy, "(\w+)"/,
  /^DevToolsUtils\.defineLazyModuleGetter\(lazy, "(\w+)"/,
  /^DevToolsUtils\.defineLazyGetter\(lazy, "(\w+)"/,
  /^Object\.defineProperty\(lazy, "(\w+)"/,
  /^Reflect\.defineProperty\(lazy, "(\w+)"/,
];

const callExpressionMultiDefinitions = [
  "ChromeUtils.defineESModuleGetters(lazy,",
  "XPCOMUtils.defineLazyModuleGetters(lazy,",
  "XPCOMUtils.defineLazyServiceGetters(lazy,",
  "Object.defineProperties(lazy,",
  "loader.lazyRequireGetter(lazy,",
];

module.exports = {
  meta: {
    docs: {
      url:
        "https://firefox-source-docs.mozilla.org/code-quality/lint/linters/eslint-plugin-mozilla/valid-lazy.html",
    },
    messages: {
      duplicateSymbol: "Duplicate symbol {{name}} being added to lazy.",
      incorrectType: "Unexpected literal for property name {{name}}",
      unknownProperty: "Unknown lazy member property {{name}}",
      unusedProperty: "Unused lazy property {{name}}",
    },
    type: "problem",
  },

  create(context) {
    let lazyProperties = new Map();
    let unknownProperties = [];

    function addProp(node, name) {
      if (lazyProperties.has(name)) {
        context.report({
          node,
          messageId: "duplicateSymbol",
          data: { name },
        });
        return;
      }
      lazyProperties.set(name, { used: false, node });
    }

    function setPropertiesFromArgument(node, arg) {
      if (arg.type === "ObjectExpression") {
        for (let prop of arg.properties) {
          if (prop.key.type == "Literal") {
            context.report({
              node,
              messageId: "incorrectType",
              data: { name: prop.key.value },
            });
            continue;
          }
          addProp(node, prop.key.name);
        }
      } else if (arg.type === "ArrayExpression") {
        for (let prop of arg.elements) {
          if (prop.type != "Literal") {
            continue;
          }
          addProp(node, prop.value);
        }
      }
    }

    return {
      VariableDeclarator(node) {
        if (
          node.id.type === "Identifier" &&
          node.id.name == "lazy" &&
          node.init.type == "CallExpression" &&
          node.init.callee.name == "createLazyLoaders"
        ) {
          setPropertiesFromArgument(node, node.init.arguments[0]);
        }
      },

      CallExpression(node) {
        if (
          node.callee.type != "MemberExpression" ||
          (node.callee.object.type == "MemberExpression" &&
            !items.includes(node.callee.object.object.name)) ||
          (node.callee.object.type != "MemberExpression" &&
            !items.includes(node.callee.object.name))
        ) {
          return;
        }

        let source;
        try {
          source = helpers.getASTSource(node);
        } catch (e) {
          return;
        }

        for (let reg of callExpressionDefinitions) {
          let match = source.match(reg);
          if (match) {
            if (lazyProperties.has(match[1])) {
              context.report({
                node,
                messageId: "duplicateSymbol",
                data: { name: match[1] },
              });
              return;
            }
            lazyProperties.set(match[1], { used: false, node });
            break;
          }
        }

        if (
          callExpressionMultiDefinitions.some(expr =>
            source.startsWith(expr)
          ) &&
          node.arguments[1]
        ) {
          setPropertiesFromArgument(node, node.arguments[1]);
        }
      },

      MemberExpression(node) {
        if (node.computed || node.object.type !== "Identifier") {
          return;
        }

        let name;
        if (node.object.name == "lazy") {
          name = node.property.name;
        } else {
          return;
        }
        let property = lazyProperties.get(name);
        if (!property) {
          // These will be reported on Program:exit - some definitions may
          // be after first use, so we need to wait until we've processed
          // the whole file before reporting.
          unknownProperties.push({ name, node });
        } else {
          property.used = true;
        }
      },

      "Program:exit": function() {
        for (let { name, node } of unknownProperties) {
          let property = lazyProperties.get(name);
          if (!property) {
            context.report({
              node,
              messageId: "unknownProperty",
              data: { name },
            });
          } else {
            property.used = true;
          }
        }
        for (let [name, property] of lazyProperties.entries()) {
          if (!property.used) {
            context.report({
              node: property.node,
              messageId: "unusedProperty",
              data: { name },
            });
          }
        }
      },
    };
  },
};
