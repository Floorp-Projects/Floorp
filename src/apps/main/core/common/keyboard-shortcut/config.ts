/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import {
  type Accessor,
  createEffect,
  createSignal,
  onCleanup,
  type Setter,
} from "solid-js";
import { createRootHMR } from "@nora/solid-xul";
import { actions } from "../mouse-gesture/utils/actions.ts";
import {
  type KeyboardShortcutConfig,
  type ShortcutConfig,
  zKeyboardShortcutConfig,
} from "./type.ts";
import { clearLegacyConfig, migrateLegacyConfig } from "./migration.ts";

export const KEYBOARD_SHORTCUT_ENABLED_PREF = "floorp.keyboardshortcut.enabled";
export const KEYBOARD_SHORTCUT_CONFIG_PREF = "floorp.keyboardshortcut.config";

const createDefaultShortcuts = (): Record<string, ShortcutConfig> => {
  const defaultShortcuts: Record<string, ShortcutConfig> = {};

  actions.forEach((action) => {
    switch (action.name) {
      case "newTab":
        defaultShortcuts[action.name] = {
          modifiers: {
            alt: true,
            ctrl: false,
            meta: false,
            shift: false,
          },
          key: "T",
          action: action.name,
        };
        break;
      case "closeTab":
        defaultShortcuts[action.name] = {
          modifiers: {
            alt: true,
            ctrl: false,
            meta: false,
            shift: false,
          },
          key: "W",
          action: action.name,
        };
        break;
      case "reload":
        defaultShortcuts[action.name] = {
          modifiers: {
            alt: true,
            ctrl: false,
            meta: false,
            shift: false,
          },
          key: "R",
          action: action.name,
        };
        break;
    }
  });

  return defaultShortcuts;
};

export const defaultConfig: KeyboardShortcutConfig = {
  enabled: true,
  shortcuts: createDefaultShortcuts(),
};

export const strDefaultConfig = JSON.stringify(defaultConfig);

function createEnabled(): [Accessor<boolean>, Setter<boolean>] {
  const [enabled, setEnabled] = createSignal(
    Services.prefs.getBoolPref(
      KEYBOARD_SHORTCUT_ENABLED_PREF,
      defaultConfig.enabled,
    ),
  );

  createEffect(() => {
    Services.prefs.setBoolPref(KEYBOARD_SHORTCUT_ENABLED_PREF, enabled());
  });

  const enabledObserver = () => {
    setEnabled(
      Services.prefs.getBoolPref(
        KEYBOARD_SHORTCUT_ENABLED_PREF,
        defaultConfig.enabled,
      ),
    );
  };

  Services.prefs.addObserver(KEYBOARD_SHORTCUT_ENABLED_PREF, enabledObserver);
  onCleanup(() => {
    Services.prefs.removeObserver(
      KEYBOARD_SHORTCUT_ENABLED_PREF,
      enabledObserver,
    );
  });

  return [enabled, setEnabled];
}

function createConfig(): [
  Accessor<KeyboardShortcutConfig>,
  Setter<KeyboardShortcutConfig>,
] {
  const legacyConfig = migrateLegacyConfig();
  if (legacyConfig) {
    clearLegacyConfig();
    Services.prefs.setStringPref(
      KEYBOARD_SHORTCUT_CONFIG_PREF,
      JSON.stringify(legacyConfig),
    );
  }

  const [config, setConfig] = createSignal<KeyboardShortcutConfig>(
    zKeyboardShortcutConfig.parse(
      JSON.parse(
        Services.prefs.getStringPref(
          KEYBOARD_SHORTCUT_CONFIG_PREF,
          strDefaultConfig,
        ),
      ),
    ),
  );

  createEffect(() => {
    Services.prefs.setStringPref(
      KEYBOARD_SHORTCUT_CONFIG_PREF,
      JSON.stringify(config()),
    );
  });

  const configObserver = () => {
    try {
      setConfig(
        zKeyboardShortcutConfig.parse(
          JSON.parse(
            Services.prefs.getStringPref(
              KEYBOARD_SHORTCUT_CONFIG_PREF,
              strDefaultConfig,
            ),
          ),
        ),
      );
    } catch (e) {
      console.error("Failed to parse keyboard shortcut configuration:", e);
      setConfig(defaultConfig);
      Services.prefs.setStringPref(
        KEYBOARD_SHORTCUT_CONFIG_PREF,
        strDefaultConfig,
      );
    }
  };

  Services.prefs.addObserver(KEYBOARD_SHORTCUT_CONFIG_PREF, configObserver);
  onCleanup(() => {
    Services.prefs.removeObserver(
      KEYBOARD_SHORTCUT_CONFIG_PREF,
      configObserver,
    );
  });

  return [config, setConfig];
}

export const [_enabled, _setEnabled] = createRootHMR(
  createEnabled,
  import.meta.hot,
);
export const [_config, _setConfig] = createRootHMR(
  createConfig,
  import.meta.hot,
);

export const isEnabled = () => _enabled();
export const setEnabled = (value: boolean) => _setEnabled(value);
export const getConfig = () => _config();
export const setConfig = (value: KeyboardShortcutConfig) => _setConfig(value);

export function shortcutToString(shortcut: ShortcutConfig): string {
  const modifiers = [];
  if (shortcut.modifiers.alt) modifiers.push("Alt");
  if (shortcut.modifiers.ctrl) modifiers.push("Ctrl");
  if (shortcut.modifiers.meta) modifiers.push("Meta");
  if (shortcut.modifiers.shift) modifiers.push("Shift");

  return [...modifiers, shortcut.key.toUpperCase()].join("+");
}

export function stringToShortcut(str: string): ShortcutConfig {
  const parts = str.split("+").map((part) => part.toLowerCase());
  const key = parts.pop() || "";
  const modifiers = {
    alt: parts.includes("alt"),
    ctrl: parts.includes("ctrl"),
    meta: parts.includes("meta"),
    shift: parts.includes("shift"),
  };

  return { modifiers, key, action: "" }; // action は後で設定する必要があります
}
