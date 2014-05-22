/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, you can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = ['getAttributes'];


var getAttributes = function (node) {
  var attributes = {};

  for (var i in node.attributes) {
    if (!isNaN(i)) {
      try {
        var attr = node.attributes[i];
        attributes[attr.name] = attr.value;
      }
      catch (e) {
      }
    }
  }

  return attributes;
}

