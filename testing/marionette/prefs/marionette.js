/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global pref */

// Marionette is the remote protocol that lets OOP programs
// communicate with, instrument, and control Gecko.

// Starts and stops the Marionette server.
pref("marionette.enabled", false);

// Delay server startup until a modal dialogue has been clicked to
// allow time for user to set breakpoints in the Browser Toolbox.
pref("marionette.debugging.clicktostart", false);

// Verbosity of Marionette logger repository.
//
// Available levels are, in descending order of severity,
// "trace", "debug", "config", "info", "warn", "error", and "fatal".
// The value is treated case-insensitively.
pref("marionette.log.level", "info");

// Certain log messages that are known to be long are truncated
// before they are dumped to stdout.  The `marionette.log.truncate`
// preference indicates that the values should not be truncated.
pref("marionette.log.truncate", true);

// Port to start Marionette server on.
pref("marionette.port", 2828);

// Sets recommended automation preferences when Marionette is started.
pref("marionette.prefs.recommended", true);

// Whether content scripts can be safely reused.
//
// Deprecated and scheduled for removal
// with https://bugzil.la/marionette-window-tracking
pref("marionette.contentListener", false);
