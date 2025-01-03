/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import {
  createEffect,
  createSignal,
  onCleanup,
} from "solid-js";
import { zPwaConfig, type TPwaConfig } from "./type";
import { defaultEnabled, strDefaultConfig } from "./default-pref";

// Initialize default preferences if they don't exist
if (!Services.prefs.prefHasUserValue("floorp.browser.ssb.enabled")) {
  Services.prefs.setBoolPref("floorp.browser.ssb.enabled", defaultEnabled);
}

if (!Services.prefs.prefHasUserValue("floorp.browser.ssb.config")) {
  Services.prefs.setStringPref("floorp.browser.ssb.config", strDefaultConfig);
}

/** enable/disable PWA */
export const [enabled, setEnabled] = createSignal(
  Services.prefs.getBoolPref("floorp.browser.ssb.enabled", defaultEnabled),
);

createEffect(() => {
  Services.prefs.setBoolPref("floorp.browser.ssb.enabled", enabled());
});

const enabledObserver = () =>
  setEnabled(Services.prefs.getBoolPref("floorp.browser.ssb.enabled"));
Services.prefs.addObserver("floorp.browser.ssb.enabled", enabledObserver);
onCleanup(() => {
  Services.prefs.removeObserver("floorp.browser.ssb.enabled", enabledObserver);
});

/** Config */
export const [config, setConfig] = createSignal<TPwaConfig>(
  zPwaConfig.parse(
    JSON.parse(
      Services.prefs.getStringPref(
        "floorp.browser.ssb.config",
        strDefaultConfig,
      ),
    ),
  ),
);

createEffect(() => {
  Services.prefs.setStringPref(
    "floorp.browser.ssb.config",
    JSON.stringify(config()),
  );
});

const configObserver = () =>
  setConfig(
    zPwaConfig.parse(
      JSON.parse(
        Services.prefs.getStringPref(
          "floorp.browser.ssb.config",
          strDefaultConfig,
        ),
      ),
    ),
  );

Services.prefs.addObserver("floorp.browser.ssb.config", configObserver);
onCleanup(() => {
  Services.prefs.removeObserver("floorp.browser.ssb.config", configObserver);
});
