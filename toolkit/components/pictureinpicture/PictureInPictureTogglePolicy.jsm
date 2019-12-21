/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["TOGGLE_POLICIES", "TOGGLE_POLICY_STRINGS"];

// These are the possible toggle positions along the right side of
// a qualified video element.
this.TOGGLE_POLICIES = {
  DEFAULT: 1,
  HIDDEN: 2,
  TOP: 3,
  ONE_QUARTER: 4,
  THREE_QUARTERS: 5,
  BOTTOM: 6,
};

// These strings are used in the videocontrols.css stylesheet as
// toggle policy attribute values for setting rules on the position
// of the toggle.
this.TOGGLE_POLICY_STRINGS = {
  [TOGGLE_POLICIES.DEFAULT]: "default",
  [TOGGLE_POLICIES.HIDDEN]: "hidden",
  [TOGGLE_POLICIES.TOP]: "top",
  [TOGGLE_POLICIES.ONE_QUARTER]: "one-quarter",
  [TOGGLE_POLICIES.THREE_QUARTERS]: "three-quarters",
  [TOGGLE_POLICIES.BOTTOM]: "bottom",
};
