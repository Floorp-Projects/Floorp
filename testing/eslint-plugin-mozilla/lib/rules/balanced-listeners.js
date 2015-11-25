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

// -----------------------------------------------------------------------------
// Rule Definition
// -----------------------------------------------------------------------------

module.exports = function(context) {
  // ---------------------------------------------------------------------------
  // Helpers
  // ---------------------------------------------------------------------------

  var DICTIONARY = {
    "addEventListener": "removeEventListener",
    "on": "off"
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
    addedListeners.push({
      functionName: node.callee.property.name,
      type: node.arguments[0].value,
      node: node.callee.property,
      useCapture: node.arguments[2] ? node.arguments[2].value : null
    });
  }

  function addRemovedListener(node) {
    removedListeners.push({
      functionName: node.callee.property.name,
      type: node.arguments[0].value,
      useCapture: node.arguments[2] ? node.arguments[2].value : null
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
      if (DICTIONARY[addedListener.functionName] === listener.functionName &&
          addedListener.type === listener.type &&
          addedListener.useCapture === listener.useCapture) {
        return true;
      }
    }

    return false;
  }

  // ---------------------------------------------------------------------------
  // Public
  // ---------------------------------------------------------------------------

  return {
    CallExpression: function(node) {
      if (node.callee.type === "MemberExpression") {
        var listenerMethodName = node.callee.property.name;

        if (DICTIONARY.hasOwnProperty(listenerMethodName)) {
          addAddedListener(node);
        } else if (INVERTED_DICTIONARY.hasOwnProperty(listenerMethodName)) {
          addRemovedListener(node);
        }
      }
    },

    "Program:exit": function() {
      getUnbalancedListeners().forEach(function(listener) {
        context.report(listener.node,
          "No corresponding '{{functionName}}({{type}})' was found.",
          {
            functionName: DICTIONARY[listener.functionName],
            type: listener.type
          });
      });
    }
  };
};
