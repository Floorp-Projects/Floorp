/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import {
  _setConfig,
  type GesturePattern,
  getConfig,
  isEnabled,
  type MouseGestureConfig,
  setEnabled,
} from "./config.ts";
import { MouseGestureController } from "./controller.ts";
import { createRootHMR } from "@nora/solid-xul";
import { createEffect } from "solid-js";

interface nsIObserver {
  observe(subject: unknown, topic: string, data: string): void;
}

let globalController: MouseGestureController | null = null;

export class MouseGestureService {
  private controller: MouseGestureController | null = null;
  private lastConfigString = "";
  private configObserver: nsIObserver | null = null;

  constructor() {
    if (globalController) {
      globalController.destroy();
      globalController = null;
    }

    this.initialize();

    createEffect(() => {
      const config = getConfig();
      const configString = JSON.stringify(config);
      const enabled = isEnabled();

      if (this.lastConfigString && this.lastConfigString !== configString) {
        this.destroyController();
        if (enabled) {
          this.createController();
        }

        handleContextMenuAfterMouseUp(enabled);
      }

      this.lastConfigString = configString;
    });

    this.setupEnabledObserver();
  }

  private setupEnabledObserver(): void {
    if (this.configObserver) {
      try {
        Services.prefs.removeObserver(
          "floorp.mousegesture.enabled",
          this.configObserver,
        );
      } catch (e) {
        console.error("[MouseGestureService] Error removing observer:", e);
      }
    }

    this.configObserver = {
      observe: (_subject: unknown, topic: string, data: string) => {
        if (
          topic === "nsPref:changed" && data === "floorp.mousegesture.enabled"
        ) {
          const enabled = Services.prefs.getBoolPref(
            "floorp.mousegesture.enabled",
            false,
          );

          if (enabled && !this.controller) {
            this.createController();
          } else if (!enabled && this.controller) {
            this.destroyController();
          }

          handleContextMenuAfterMouseUp(enabled);
        }
      },
    };

    Services.prefs.addObserver(
      "floorp.mousegesture.enabled",
      this.configObserver,
    );
  }

  private initialize(): void {
    this.lastConfigString = JSON.stringify(getConfig());
    const enabled = isEnabled();

    if (enabled) {
      this.createController();
    }
    handleContextMenuAfterMouseUp(enabled);
  }

  private createController(): void {
    this.destroyController();

    this.controller = new MouseGestureController();
    globalController = this.controller;
  }

  private destroyController(): void {
    if (this.controller) {
      this.controller.destroy();
      this.controller = null;

      if (globalController === this.controller) {
        globalController = null;
      }
    }
  }

  public isEnabled(): boolean {
    return isEnabled();
  }

  public setEnabled(value: boolean): void {
    setEnabled(value);

    if (value && !this.controller) {
      this.createController();
    } else if (!value && this.controller) {
      this.destroyController();
    }

    handleContextMenuAfterMouseUp(value);
  }

  public getConfig(): MouseGestureConfig {
    return getConfig();
  }

  public updateConfig(newConfig: MouseGestureConfig): void {
    setConfig(newConfig);

    if (isEnabled()) {
      this.destroyController();
      this.createController();
    }
  }

  public patternToDisplayString(pattern: GesturePattern): string {
    const directionSymbols: Record<string, string> = {
      up: "↑",
      down: "↓",
      left: "←",
      right: "→",
    };

    return pattern.map((dir) => directionSymbols[dir] || dir).join(" ");
  }
}

function setConfig(config: MouseGestureConfig) {
  _setConfig(config);
}

function createMouseGestureService() {
  return new MouseGestureService();
}

function handleContextMenuAfterMouseUp(enabled: boolean) {
  Services.prefs.setBoolPref("ui.context_menus.after_mouseup", enabled);
}

export const mouseGestureService = createRootHMR(
  createMouseGestureService,
  import.meta.hot,
);
