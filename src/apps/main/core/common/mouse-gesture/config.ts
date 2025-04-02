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
import { z } from "zod";
import { getActionDisplayName } from "./utils/gestures";

export const MOUSE_GESTURE_ENABLED_PREF = "floorp.mousegesture.enabled";
export const MOUSE_GESTURE_CONFIG_PREF = "floorp.mousegesture.config";

export type GestureDirection = "up" | "down" | "left" | "right";
export type GesturePattern = GestureDirection[];

export const zGestureAction = z.object({
  pattern: z.array(z.enum(["up", "down", "left", "right"])),
  action: z.string(),
});
export type GestureAction = z.infer<typeof zGestureAction>;

export const zMouseGestureConfig = z.object({
  enabled: z.boolean().default(true),
  sensitivity: z.number().min(1).max(100).default(40),
  showTrail: z.boolean().default(true),
  trailColor: z.string().default("#37ff00"),
  trailWidth: z.number().min(1).max(10).default(2),
  actions: z.array(zGestureAction).default([]),
});
export type MouseGestureConfig = z.infer<typeof zMouseGestureConfig>;

export const defaultConfig: MouseGestureConfig = {
  enabled: true,
  sensitivity: 40,
  showTrail: true,
  trailColor: "#37ff00",
  trailWidth: 6,
  actions: [
    {
      pattern: ["left"],
      action: "gecko-back",
    },
    {
      pattern: ["right"],
      action: "gecko-forward",
    },
    {
      pattern: ["up", "down"],
      action: "gecko-reload",
    },
    {
      pattern: ["down", "right"],
      action: "gecko-close-tab",
    },
    {
      pattern: ["down", "up"],
      action: "gecko-open-new-tab",
    },
    {
      pattern: ["up"],
      action: "gecko-scroll-to-top",
    },
    {
      pattern: ["down"],
      action: "gecko-scroll-to-bottom",
    },
    {
      pattern: ["left", "down"],
      action: "gecko-scroll-up",
    },
    {
      pattern: ["right", "down"],
      action: "gecko-scroll-down",
    },
  ],
};

export const strDefaultConfig = JSON.stringify(defaultConfig);

function createEnabled(): [Accessor<boolean>, Setter<boolean>] {
  const [enabled, setEnabled] = createSignal(
    Services.prefs.getBoolPref(
      MOUSE_GESTURE_ENABLED_PREF,
      defaultConfig.enabled,
    ),
  );

  createEffect(() => {
    Services.prefs.setBoolPref(MOUSE_GESTURE_ENABLED_PREF, enabled());
  });

  const enabledObserver = () => {
    setEnabled(
      Services.prefs.getBoolPref(
        MOUSE_GESTURE_ENABLED_PREF,
        defaultConfig.enabled,
      ),
    );
  };

  Services.prefs.addObserver(MOUSE_GESTURE_ENABLED_PREF, enabledObserver);
  onCleanup(() => {
    Services.prefs.removeObserver(MOUSE_GESTURE_ENABLED_PREF, enabledObserver);
  });

  return [enabled, setEnabled];
}

function createConfig(): [
  Accessor<MouseGestureConfig>,
  Setter<MouseGestureConfig>,
] {
  const [config, setConfig] = createSignal<MouseGestureConfig>(
    zMouseGestureConfig.parse(
      JSON.parse(
        Services.prefs.getStringPref(
          MOUSE_GESTURE_CONFIG_PREF,
          strDefaultConfig,
        ),
      ),
    ),
  );

  createEffect(() => {
    Services.prefs.setStringPref(
      MOUSE_GESTURE_CONFIG_PREF,
      JSON.stringify(config()),
    );
  });

  const configObserver = () => {
    try {
      setConfig(
        zMouseGestureConfig.parse(
          JSON.parse(
            Services.prefs.getStringPref(
              MOUSE_GESTURE_CONFIG_PREF,
              strDefaultConfig,
            ),
          ),
        ),
      );
    } catch (e) {
      console.error("Failed to parse mouse gesture configuration:", e);
      setConfig(defaultConfig);
      Services.prefs.setStringPref(MOUSE_GESTURE_CONFIG_PREF, strDefaultConfig);
    }
  };

  Services.prefs.addObserver(MOUSE_GESTURE_CONFIG_PREF, configObserver);
  onCleanup(() => {
    Services.prefs.removeObserver(MOUSE_GESTURE_CONFIG_PREF, configObserver);
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
export const setConfig = (value: MouseGestureConfig) => _setConfig(value);

export function patternToString(pattern: GesturePattern): string {
  return pattern.join("-");
}

export function stringToPattern(str: string): GesturePattern {
  return str.split("-") as GesturePattern;
}

export function getGestureActionName(gestureAction: GestureAction): string {
  return getActionDisplayName(gestureAction.action);
}
