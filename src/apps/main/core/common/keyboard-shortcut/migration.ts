/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import type { KeyboardShortcutConfig, ShortcutConfig } from "./type.ts";

const LEGACY_PREF = "floorp.browser.nora.csk.data";

interface LegacyModifiers {
  alt: boolean;
  ctrl: boolean;
  meta: boolean;
  shift: boolean;
}

interface LegacyShortcutConfig {
  key: string;
  modifiers: LegacyModifiers;
}

type LegacyConfig = Record<string, LegacyShortcutConfig>;

export function migrateLegacyConfig(): KeyboardShortcutConfig | null {
  try {
    if (!Services.prefs.prefHasUserValue(LEGACY_PREF)) {
      return null;
    }

    const legacyData = JSON.parse(
      Services.prefs.getStringPref(LEGACY_PREF, "{}"),
    ) as LegacyConfig;

    const shortcuts: Record<string, ShortcutConfig> = {};
    for (const [action, config] of Object.entries(legacyData)) {
      if (typeof config === "object" && config !== null) {
        shortcuts[action] = {
          key: config.key,
          modifiers: config.modifiers,
          action: action,
        };
      }
    }

    return {
      enabled: true,
      shortcuts,
    };
  } catch (e) {
    console.error("Failed to migrate legacy keyboard shortcut config:", e);
    return null;
  }
}

export function clearLegacyConfig() {
  try {
    if (Services.prefs.prefHasUserValue(LEGACY_PREF)) {
      Services.prefs.clearUserPref(LEGACY_PREF);
    }
  } catch (e) {
    console.error("Failed to clear legacy keyboard shortcut config:", e);
  }
}
