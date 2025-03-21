/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import {
  type GesturePattern,
  getConfig,
  isEnabled,
  type MouseGestureConfig,
  setEnabled,
} from "./config";
import { MouseGestureController } from "./controller";

export class MouseGestureService {
  private static instance: MouseGestureService;
  private controller: MouseGestureController | null = null;

  private constructor() {
    this.initialize();
  }

  /**
   * Get the singleton instance of the service
   */
  public static getInstance(): MouseGestureService {
    if (!MouseGestureService.instance) {
      MouseGestureService.instance = new MouseGestureService();
    }
    return MouseGestureService.instance;
  }

  /**
   * Initialize the service
   */
  private initialize(): void {
    // Initialize the controller if enabled
    if (isEnabled()) {
      this.createController();
    }
  }

  /**
   * Create the controller
   */
  private createController(): void {
    if (!this.controller) {
      this.controller = new MouseGestureController();
    }
  }

  /**
   * Destroy the controller
   */
  private destroyController(): void {
    if (this.controller) {
      this.controller.destroy();
      this.controller = null;
    }
  }

  /**
   * Check if mouse gestures are enabled
   */
  public isEnabled(): boolean {
    return isEnabled();
  }

  /**
   * Enable or disable mouse gestures
   */
  public setEnabled(value: boolean): void {
    const currentState = isEnabled();

    // Update preference
    setEnabled(value);

    // Create or destroy controller as needed
    if (value && !currentState) {
      this.createController();
    } else if (!value && currentState) {
      this.destroyController();
    }
  }

  /**
   * Get the current configuration
   */
  public getConfig(): MouseGestureConfig {
    return getConfig();
  }

  /**
   * Convert a gesture pattern to a human-readable string
   */
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

// Export the service instance
export const mouseGestureService = MouseGestureService.getInstance();
