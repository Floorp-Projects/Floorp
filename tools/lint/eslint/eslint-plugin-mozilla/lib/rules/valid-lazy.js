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
  /^loader\.lazyServiceGetter\(lazy, "(\w+)"/,
  /^loader\.lazyRequireGetter\(lazy, "(\w+)"/,
  /^XPCOMUtils\.defineLazyGetter\(lazy, "(\w+)"/,
  /^Integration\.downloads\.defineESModuleGetter\(lazy, "(\w+)"/,
  /^ChromeUtils\.defineLazyGetter\(lazy, "(\w+)"/,
  /^ChromeUtils\.defineModuleGetter\(lazy, "(\w+)"/,
  /^XPCOMUtils\.defineLazyPreferenceGetter\(lazy, "(\w+)"/,
  /^XPCOMUtils\.defineLazyScriptGetter\(lazy, "(\w+)"/,
  /^XPCOMUtils\.defineLazyServiceGetter\(lazy, "(\w+)"/,
  /^XPCOMUtils\.defineConstant\(lazy, "(\w+)"/,
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
      url: "https://firefox-source-docs.mozilla.org/code-quality/lint/linters/eslint-plugin-mozilla/rules/valid-lazy.html",
    },
    messages: {
      duplicateSymbol: "Duplicate symbol {{name}} being added to lazy.",
      incorrectType: "Unexpected literal for property name {{name}}",
      unknownProperty: "Unknown lazy member property {{name}}",
      unusedProperty: "Unused lazy property {{name}}",
      topLevelAndUnconditional:
        "Lazy property {{name}} is used at top-level unconditionally. It should be non-lazy.",
    },
    schema: [],
    type: "problem",
  },

  create(context) {
    let lazyProperties = new Map();
    let unknownProperties = [];
    let isLazyExported = false;

    function getAncestorNodes(node) {
      const ancestors = [];
      node = node.parent;
      while (node) {
        ancestors.unshift(node);
        node = node.parent;
      }
      return ancestors;
    }

    // Returns true if lazy getter definitions in prevNode and currNode are
    // duplicate.
    // This returns false if prevNode and currNode have the same IfStatement as
    // ancestor and they're in different branches.
    function isDuplicate(prevNode, currNode) {
      const prevAncestors = getAncestorNodes(prevNode);
      const currAncestors = getAncestorNodes(currNode);

      for (
        let i = 0;
        i < prevAncestors.length && i < currAncestors.length;
        i++
      ) {
        const prev = prevAncestors[i];
        const curr = currAncestors[i];
        if (prev === curr && prev.type === "IfStatement") {
          if (prevAncestors[i + 1] !== currAncestors[i + 1]) {
            return false;
          }
        }
      }

      return true;
    }

    function addProp(callNode, propNode, name) {
      if (
        lazyProperties.has(name) &&
        isDuplicate(lazyProperties.get(name).callNode, callNode)
      ) {
        context.report({
          node: propNode,
          messageId: "duplicateSymbol",
          data: { name },
        });
        return;
      }
      lazyProperties.set(name, { used: false, callNode, propNode });
    }

    function setPropertiesFromArgument(callNode, arg) {
      if (arg.type === "ObjectExpression") {
        for (let propNode of arg.properties) {
          if (propNode.key.type == "Literal") {
            context.report({
              node: propNode,
              messageId: "incorrectType",
              data: { name: propNode.key.value },
            });
            continue;
          }
          addProp(callNode, propNode, propNode.key.name);
        }
      } else if (arg.type === "ArrayExpression") {
        for (let propNode of arg.elements) {
          if (propNode.type != "Literal") {
            continue;
          }
          addProp(callNode, propNode, propNode.value);
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
          setPropertiesFromArgument(node.init, node.init.arguments[0]);
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
            if (
              lazyProperties.has(match[1]) &&
              isDuplicate(lazyProperties.get(match[1]).callNode, node)
            ) {
              context.report({
                node,
                messageId: "duplicateSymbol",
                data: { name: match[1] },
              });
              return;
            }
            lazyProperties.set(match[1], {
              used: false,
              callNode: node,
              propNode: node,
            });
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
        if (
          helpers.getIsTopLevelAndUnconditionallyExecuted(
            helpers.getAncestors(context, node)
          )
        ) {
          context.report({
            node,
            messageId: "topLevelAndUnconditional",
            data: { name },
          });
        }
      },

      ExportNamedDeclaration(node) {
        for (const spec of node.specifiers) {
          if (spec.local.name === "lazy") {
            // If the lazy object is exported, do not check unused property.
            isLazyExported = true;
            break;
          }
        }
      },

      "Program:exit": function () {
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
        if (!isLazyExported) {
          for (let [name, property] of lazyProperties.entries()) {
            if (!property.used) {
              context.report({
                node: property.propNode,
                messageId: "unusedProperty",
                data: { name },
              });
            }
          }
        }
      },
    };
  },
};
