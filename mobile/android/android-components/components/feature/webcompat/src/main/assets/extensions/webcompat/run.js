/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* globals AboutCompatBroker, AVAILABLE_INJECTIONS, AVAILABLE_SHIMS,
           AVAILABLE_PIP_OVERRIDES, AVAILABLE_UA_OVERRIDES, CUSTOM_FUNCTIONS,
           Injections, Shims, UAOverrides */

let injections, shims, uaOverrides;

try {
  injections = new Injections(AVAILABLE_INJECTIONS, CUSTOM_FUNCTIONS);
  injections.bootup();
} catch (e) {
  console.error("Injections failed to start", e);
  injections = undefined;
}

try {
  uaOverrides = new UAOverrides(AVAILABLE_UA_OVERRIDES);
  uaOverrides.bootup();
} catch (e) {
  console.error("UA overrides failed to start", e);
  uaOverrides = undefined;
}

try {
  shims = new Shims(AVAILABLE_SHIMS);
} catch (e) {
  console.error("Shims failed to start", e);
  shims = undefined;
}

try {
  const aboutCompatBroker = new AboutCompatBroker({
    injections,
    shims,
    uaOverrides,
  });
  aboutCompatBroker.bootup();
} catch (e) {
  console.error("about:compat broker failed to start", e);
}
