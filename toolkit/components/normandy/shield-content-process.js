/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/**
 * Registers about: pages provided by Shield, and listens for a shutdown event
 * from the add-on before un-registering them.
 *
 * This file is loaded as a process script. It is executed once for each
 * process, including the parent one.
 */

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://normandy-content/AboutPages.jsm");

// generateNSGetFactory only knows how to register factory classes, with
// classID properties on their prototype, and a constructor that returns
// an instance. It can't handle singletons directly. So wrap the
// aboutStudies singleton in a trivial constructor function.
function AboutStudies() {
  return AboutStudies.prototype;
}
AboutStudies.prototype = AboutPages.aboutStudies;

var NSGetFactory = XPCOMUtils.generateNSGetFactory([AboutStudies]);
