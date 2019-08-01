/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* globals AVAILABLE_INJECTIONS, AVAILABLE_UA_OVERRIDES, AboutCompatBroker,
           Injections, UAOverrides */

const injections = new Injections(AVAILABLE_INJECTIONS);
const uaOverrides = new UAOverrides(AVAILABLE_UA_OVERRIDES);

const aboutCompatBroker = new AboutCompatBroker({
  injections,
  uaOverrides,
});

aboutCompatBroker.bootup();
injections.bootup();
uaOverrides.bootup();
