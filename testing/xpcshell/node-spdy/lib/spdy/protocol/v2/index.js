/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var v2;

try {
  v2 = require('./protocol.node');
} catch (e) {
  v2 = require('./protocol.js');
}
module.exports = v2;

v2.Framer = require('./framer').Framer;
