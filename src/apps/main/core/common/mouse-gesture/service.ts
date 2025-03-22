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

let globalController: MouseGestureController | null = null;

export class MouseGestureService {
  private controller: MouseGestureController | null = null;
  private lastConfigString = "";

  constructor() {
    if (globalController) {
      globalController.destroy();
      globalController = null;
    }

    this.initialize();

    createEffect(() => {
      const config = getConfig();
      const configString = JSON.stringify(config);

      if (this.lastConfigString && this.lastConfigString !== configString) {
        console.log("Mouse gesture config changed, recreating controller");
        this.destroyController();
        if (isEnabled()) {
          this.createController();
        }
      }

      this.lastConfigString = configString;
    });
  }

  private initialize(): void {
    this.lastConfigString = JSON.stringify(getConfig());
    if (isEnabled()) {
      this.createController();
    }
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
    const currentState = isEnabled();

    setEnabled(value);

    if (value && !currentState) {
      this.createController();
    } else if (!value && currentState) {
      this.destroyController();
    }
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

export const mouseGestureService = createRootHMR(
  createMouseGestureService,
  import.meta.hot,
);
