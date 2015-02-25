/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// We run this code in a .jsm rather than here to avoid keeping the current
// compartment alive.
Components.utils.import("chrome://mochikit/content/ShutdownLeaksCollector.jsm");
