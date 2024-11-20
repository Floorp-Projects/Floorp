/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { createEffect, createSignal } from "solid-js";
import { zWorkspacesServicesConfigs } from "./utils/type.js";
import { getOldConfigs } from "./old-config.js";

/** enable/disable workspaces */
export const [enabled, setEnabled] = createSignal(
  Services.prefs.getBoolPref("floorp.browser.workspaces.enabled", true),
);
Services.prefs.addObserver("floorp.browser.workspaces.enabled", () =>
  setEnabled(Services.prefs.getBoolPref("floorp.browser.workspaces.enabled")),
);

createEffect(() => {
  Services.prefs.setBoolPref("floorp.browser.workspaces.enabled", enabled());
});

/** Configs */
export const [config, setConfig] = createSignal(
  zWorkspacesServicesConfigs.parse(
    JSON.parse(
      Services.prefs.getStringPref("floorp.workspaces.configs", getOldConfigs),
    ),
  ),
);

createEffect(() => {
  Services.prefs.setStringPref(
    "floorp.workspaces.configs",
    JSON.stringify(config()),
  );
});

Services.prefs.addObserver("floorp.workspaces.configs", () =>
  setConfig(
    zWorkspacesServicesConfigs.parse(
      JSON.parse(Services.prefs.getStringPref("floorp.workspaces.configs")),
    ),
  ),
);
