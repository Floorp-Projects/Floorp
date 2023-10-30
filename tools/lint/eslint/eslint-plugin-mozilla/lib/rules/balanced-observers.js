/**
 * @fileoverview Check that there's a Services.(prefs|obs).removeObserver for
 * each addObserver.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

"use strict";

module.exports = {
  meta: {
    docs: {
      url: "https://firefox-source-docs.mozilla.org/code-quality/lint/linters/eslint-plugin-mozilla/rules/balanced-observers.html",
    },
    messages: {
      noCorresponding:
        "No corresponding 'removeObserver(\"{{observable}}\")' was found.",
    },
    schema: [],
    type: "problem",
  },

  create(context) {
    var addedObservers = [];
    var removedObservers = [];

    function getObserverAPI(node) {
      const object = node.callee.object;
      if (
        object.type == "MemberExpression" &&
        object.property.type == "Identifier"
      ) {
        return object.property.name;
      }
      return null;
    }

    function isServicesObserver(api) {
      return api == "obs" || api == "prefs";
    }

    function getObservableName(node, api) {
      if (api === "obs") {
        return node.arguments[1].value;
      }
      return node.arguments[0].value;
    }

    function addAddedObserver(node) {
      const api = getObserverAPI(node);
      if (!isServicesObserver(api)) {
        return;
      }

      addedObservers.push({
        functionName: node.callee.property.name,
        observable: getObservableName(node, api),
        node: node.callee.property,
      });
    }

    function addRemovedObserver(node) {
      const api = getObserverAPI(node);
      if (!isServicesObserver(api)) {
        return;
      }

      removedObservers.push({
        functionName: node.callee.property.name,
        observable: getObservableName(node, api),
      });
    }

    function getUnbalancedObservers() {
      const unbalanced = addedObservers.filter(
        observer => !hasRemovedObserver(observer)
      );
      addedObservers = removedObservers = [];

      return unbalanced;
    }

    function hasRemovedObserver(addedObserver) {
      return removedObservers.some(
        observer => addedObserver.observable === observer.observable
      );
    }

    return {
      CallExpression(node) {
        if (node.arguments.length === 0) {
          return;
        }

        if (node.callee.type === "MemberExpression") {
          var methodName = node.callee.property.name;

          if (methodName === "addObserver") {
            addAddedObserver(node);
          } else if (methodName === "removeObserver") {
            addRemovedObserver(node);
          }
        }
      },

      "Program:exit": function () {
        getUnbalancedObservers().forEach(function (observer) {
          context.report({
            node: observer.node,
            messageId: "noCorresponding",
            data: {
              observable: observer.observable,
            },
          });
        });
      },
    };
  },
};
