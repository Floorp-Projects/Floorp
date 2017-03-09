/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Whether or not Marionette is enabled.
pref("marionette.enabled", false);

// Port to start Marionette server on.
pref("marionette.port", 2828);

// Forces client connections to come from a loopback device.
pref("marionette.forcelocal", true);

// Marionette logging verbosity.  Allowed values are "fatal", "error",
// "warn", "info", "config", "debug", and "trace".
pref("marionette.log.level", "info");
