/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global pref */

// Marionette is the remote protocol that lets OOP programs communicate
// with, instrument, and control Gecko.
//
// It is included in Firefox, but not enabled by default unless the
// -marionette flag is passed.

// Port to start Marionette server on.
pref("marionette.port", 2828);

// Marionette logging verbosity.  Allowed values are "fatal", "error",
// "warn", "info", "config", "debug", and "trace".
pref("marionette.log.level", "info");

// Sets preferences recommended when using Firefox in automation with
// Marionette.
pref("marionette.prefs.recommended", true);
