/**
 * @fileoverview Check that there's a removeEventListener for each
 * addEventListener and an off for each on.
 * Note that for now, this rule is rather simple in that it only checks that
 * for each event name there is both an add and remove listener. It doesn't
 * check that these are called on the right objects or with the same callback.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

"use strict";

module.exports = {
  meta: {
    docs: {
      url: "https://firefox-source-docs.mozilla.org/code-quality/lint/linters/eslint-plugin-mozilla/rules/balanced-listeners.html",
    },
    messages: {
      noCorresponding:
        "No corresponding '{{functionName}}({{type}})' was found.",
    },
    schema: [],
    type: "problem",
  },

  create(context) {
    var DICTIONARY = {
      addEventListener: "removeEventListener",
      on: "off",
    };
    // Invert this dictionary to make it easy later.
    var INVERTED_DICTIONARY = {};
    for (var i in DICTIONARY) {
      INVERTED_DICTIONARY[DICTIONARY[i]] = i;
    }

    // Collect the add/remove listeners in these 2 arrays.
    var addedListeners = [];
    var removedListeners = [];

    function addAddedListener(node) {
      var capture = false;
      let options = node.arguments[2];
      if (options) {
        if (options.type == "ObjectExpression") {
          if (
            options.properties.some(
              p => p.key.name == "once" && p.value.value === true
            )
          ) {
            // No point in adding listeners using the 'once' option.
            return;
          }
          capture = options.properties.some(
            p => p.key.name == "capture" && p.value.value === true
          );
        } else {
          capture = options.value;
        }
      }
      addedListeners.push({
        functionName: node.callee.property.name,
        type: node.arguments[0].value,
        node: node.callee.property,
        useCapture: capture,
      });
    }

    function addRemovedListener(node) {
      var capture = false;
      let options = node.arguments[2];
      if (options) {
        if (options.type == "ObjectExpression") {
          capture = options.properties.some(
            p => p.key.name == "capture" && p.value.value === true
          );
        } else {
          capture = options.value;
        }
      }
      removedListeners.push({
        functionName: node.callee.property.name,
        type: node.arguments[0].value,
        useCapture: capture,
      });
    }

    function getUnbalancedListeners() {
      var unbalanced = [];

      for (var j = 0; j < addedListeners.length; j++) {
        if (!hasRemovedListener(addedListeners[j])) {
          unbalanced.push(addedListeners[j]);
        }
      }
      addedListeners = removedListeners = [];

      return unbalanced;
    }

    function hasRemovedListener(addedListener) {
      for (var k = 0; k < removedListeners.length; k++) {
        var listener = removedListeners[k];
        if (
          DICTIONARY[addedListener.functionName] === listener.functionName &&
          addedListener.type === listener.type &&
          addedListener.useCapture === listener.useCapture
        ) {
          return true;
        }
      }

      return false;
    }

    return {
      CallExpression(node) {
        if (node.arguments.length === 0) {
          return;
        }

        if (node.callee.type === "MemberExpression") {
          var listenerMethodName = node.callee.property.name;

          if (DICTIONARY.hasOwnProperty(listenerMethodName)) {
            addAddedListener(node);
          } else if (INVERTED_DICTIONARY.hasOwnProperty(listenerMethodName)) {
            addRemovedListener(node);
          }
        }
      },

      "Program:exit": function () {
        getUnbalancedListeners().forEach(function (listener) {
          context.report({
            node: listener.node,
            messageId: "noCorresponding",
            data: {
              functionName: DICTIONARY[listener.functionName],
              type: listener.type,
            },
          });
        });
      },
    };
  },
};
