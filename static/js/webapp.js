/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/*global loop:true*/

var loop = loop || {};
loop.webapp = (function() {
  "use strict";

  function init() {
    console.log("Loop app started.");
  }

  return {
    init: init
  };
})();
