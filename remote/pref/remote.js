/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Indicates whether the remote agent is enabled.
// If it is false, the remote agent will not be loaded.
pref("remote.enabled", false);

// Limits remote agent to listen on loopback devices,
// e.g. 127.0.0.1, localhost, and ::1.
pref("remote.force-local", true);

// Defines the verbosity of the internal logger.
//
// Available levels are, in descending order of severity,
// "Trace", "Debug", "Config", "Info", "Warn", "Error", and "Fatal".
// The value is treated case-sensitively.
pref("remote.log.level", "Info");
