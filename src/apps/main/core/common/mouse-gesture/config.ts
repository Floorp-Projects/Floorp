/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { createEffect, createRoot, createSignal, onCleanup } from "solid-js";
import { z } from "zod";

export const MOUSE_GESTURE_ENABLED_PREF = "floorp.mousegesture.enabled";
export const MOUSE_GESTURE_CONFIG_PREF = "floorp.mousegesture.config";

// Define the gesture direction types
export type GestureDirection = "up" | "down" | "left" | "right";
export type GesturePattern = GestureDirection[];

// Define the gesture action schema
export const zGestureAction = z.object({
  name: z.string(),
  pattern: z.array(z.enum(["up", "down", "left", "right"])),
  action: z.string(),
  description: z.string().optional(),
});
export type GestureAction = z.infer<typeof zGestureAction>;

// Define the config schema
export const zMouseGestureConfig = z.object({
  enabled: z.boolean().default(true),
  sensitivity: z.number().min(1).max(100).default(20),
  showTrail: z.boolean().default(true),
  trailColor: z.string().default("#37ff00"),
  trailWidth: z.number().min(1).max(10).default(2),
  actions: z.array(zGestureAction).default([]),
});
export type MouseGestureConfig = z.infer<typeof zMouseGestureConfig>;

// Default configuration
export const defaultConfig: MouseGestureConfig = {
  enabled: true,
  sensitivity: 20,
  showTrail: true,
  trailColor: "#37ff00",
  trailWidth: 2,
  actions: [
    {
      name: "Back",
      pattern: ["left"],
      action: "goBack",
      description: "Navigate back to the previous page",
    },
    {
      name: "Forward",
      pattern: ["right"],
      action: "goForward",
      description: "Navigate forward to the next page",
    },
    {
      name: "Reload",
      pattern: ["up", "down"],
      action: "reload",
      description: "Reload the current page",
    },
    {
      name: "Close Tab",
      pattern: ["down", "right"],
      action: "closeTab",
      description: "Close the current tab",
    },
    {
      name: "New Tab",
      pattern: ["down", "up"],
      action: "newTab",
      description: "Open a new tab",
    },
  ],
};

export const strDefaultConfig = JSON.stringify(defaultConfig);

// Singleton class to manage mouse gesture configuration
class MouseGestureConfigManager {
  private static instance: MouseGestureConfigManager;

  public enabled: ReturnType<typeof createSignal<boolean>> = createSignal(
    defaultConfig.enabled,
  );
  public config: ReturnType<typeof createSignal<MouseGestureConfig>> =
    createSignal<MouseGestureConfig>(defaultConfig);

  private constructor() {
    createRoot(() => {
      // Initialize preferences if they don't exist
      if (!Services.prefs.prefHasUserValue(MOUSE_GESTURE_ENABLED_PREF)) {
        Services.prefs.setBoolPref(
          MOUSE_GESTURE_ENABLED_PREF,
          defaultConfig.enabled,
        );
      }

      if (!Services.prefs.prefHasUserValue(MOUSE_GESTURE_CONFIG_PREF)) {
        Services.prefs.setStringPref(
          MOUSE_GESTURE_CONFIG_PREF,
          strDefaultConfig,
        );
      }

      // Initialize signals with values from preferences
      this.enabled = createSignal(
        Services.prefs.getBoolPref(
          MOUSE_GESTURE_ENABLED_PREF,
          defaultConfig.enabled,
        ),
      );

      this.config = createSignal<MouseGestureConfig>(
        zMouseGestureConfig.parse(
          JSON.parse(
            Services.prefs.getStringPref(
              MOUSE_GESTURE_CONFIG_PREF,
              strDefaultConfig,
            ),
          ),
        ),
      );

      const [enabled] = this.enabled;
      const [config] = this.config;

      // Save changes to preferences
      createEffect(() => {
        Services.prefs.setBoolPref(MOUSE_GESTURE_ENABLED_PREF, enabled());
      });

      createEffect(() => {
        Services.prefs.setStringPref(
          MOUSE_GESTURE_CONFIG_PREF,
          JSON.stringify(config()),
        );
      });

      // Set up observers for preference changes
      const enabledObserver = () => {
        this.enabled[1](
          Services.prefs.getBoolPref(
            MOUSE_GESTURE_ENABLED_PREF,
            defaultConfig.enabled,
          ),
        );
      };

      Services.prefs.addObserver(MOUSE_GESTURE_ENABLED_PREF, enabledObserver);
      onCleanup(() => {
        Services.prefs.removeObserver(
          MOUSE_GESTURE_ENABLED_PREF,
          enabledObserver,
        );
      });

      const configObserver = () => {
        try {
          this.config[1](
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
          // Reset to default if parsing fails
          this.config[1](defaultConfig);
          Services.prefs.setStringPref(
            MOUSE_GESTURE_CONFIG_PREF,
            strDefaultConfig,
          );
        }
      };

      Services.prefs.addObserver(MOUSE_GESTURE_CONFIG_PREF, configObserver);
      onCleanup(() => {
        Services.prefs.removeObserver(
          MOUSE_GESTURE_CONFIG_PREF,
          configObserver,
        );
      });
    });
  }

  public static getInstance(): MouseGestureConfigManager {
    if (!MouseGestureConfigManager.instance) {
      MouseGestureConfigManager.instance = new MouseGestureConfigManager();
    }
    return MouseGestureConfigManager.instance;
  }
}

// Export public API
const configManager = MouseGestureConfigManager.getInstance();
export const isEnabled = () => configManager.enabled[0]();
export const setEnabled = (value: boolean) => configManager.enabled[1](value);
export const getConfig = () => configManager.config[0]();
export const setConfig = (value: MouseGestureConfig) =>
  configManager.config[1](value);

// Utility functions for pattern matching
export function patternToString(pattern: GesturePattern): string {
  return pattern.join("-");
}

export function stringToPattern(str: string): GesturePattern {
  return str.split("-") as GesturePattern;
}
