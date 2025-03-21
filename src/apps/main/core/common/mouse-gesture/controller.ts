/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import {
  type GestureDirection,
  getConfig,
  isEnabled,
  patternToString,
} from "./config.ts";
import { GestureDisplay } from "./components/GestureDisplay.tsx";
import { executeGestureAction } from "./utils/gestures.ts";

/**
 * Controller for mouse gesture functionality
 */
export class MouseGestureController {
  private isGestureActive = false;
  private gesturePattern: GestureDirection[] = [];
  private lastX = 0;
  private lastY = 0;
  private startX = 0;
  private startY = 0;
  private mouseTrail: { x: number; y: number }[] = [];
  private display: GestureDisplay;
  private activeActionName = "";

  constructor() {
    this.display = new GestureDisplay();
    this.init();
  }

  private init(): void {
    // Add global event listeners
    if (typeof globalThis !== "undefined") {
      globalThis.addEventListener("mousedown", this.handleMouseDown);
      globalThis.addEventListener("mousemove", this.handleMouseMove);
      globalThis.addEventListener("mouseup", this.handleMouseUp);
      globalThis.addEventListener("contextmenu", this.handleContextMenu, true);
    }
  }

  public destroy(): void {
    // Remove global event listeners
    if (typeof globalThis !== "undefined") {
      globalThis.removeEventListener("mousedown", this.handleMouseDown);
      globalThis.removeEventListener("mousemove", this.handleMouseMove);
      globalThis.removeEventListener("mouseup", this.handleMouseUp);
      globalThis.removeEventListener(
        "contextmenu",
        this.handleContextMenu,
        true,
      );
    }
    this.display.destroy();
  }

  private handleMouseDown = (event: MouseEvent): void => {
    // right mouse button
    if (event.button !== 2 || !isEnabled()) return;

    this.isGestureActive = true;
    this.gesturePattern = [];
    this.lastX = event.clientX;
    this.lastY = event.clientY;
    this.startX = event.clientX;
    this.startY = event.clientY;
    this.mouseTrail = [{ x: event.clientX, y: event.clientY }];
    this.activeActionName = "";

    this.display.show();
    this.display.updateTrail(this.mouseTrail);
  };

  private handleMouseMove = (event: MouseEvent): void => {
    if (!this.isGestureActive || !isEnabled()) return;

    const config = getConfig();
    const { sensitivity } = config;

    this.mouseTrail.push({ x: event.clientX, y: event.clientY });

    const dx = event.clientX - this.lastX;
    const dy = event.clientY - this.lastY;

    if (Math.abs(dx) > sensitivity || Math.abs(dy) > sensitivity) {
      let direction: GestureDirection | null = null;

      if (Math.abs(dx) > Math.abs(dy)) {
        direction = dx > 0 ? "right" : "left";
      } else {
        direction = dy > 0 ? "down" : "up";
      }

      if (
        direction &&
        (this.gesturePattern.length === 0 ||
          this.gesturePattern[this.gesturePattern.length - 1] !== direction)
      ) {
        this.gesturePattern.push(direction);
        const patternString = patternToString(this.gesturePattern);
        const matchingAction = config.actions.find(
          (action) => patternToString(action.pattern) === patternString,
        );

        if (matchingAction) {
          this.activeActionName = matchingAction.name;
        } else {
          this.activeActionName = "";
        }
      }

      this.lastX = event.clientX;
      this.lastY = event.clientY;
    }

    this.display.updateTrail(this.mouseTrail);
    this.display.updateActionName(this.activeActionName);
  };

  private handleMouseUp = (event: MouseEvent): void => {
    if (!this.isGestureActive || event.button !== 2 || !isEnabled()) return;

    if (this.gesturePattern.length > 0) {
      const patternString = patternToString(this.gesturePattern);
      const matchingAction = getConfig().actions.find(
        (action) => patternToString(action.pattern) === patternString,
      );

      if (matchingAction) {
        setTimeout(() => {
          executeGestureAction(matchingAction.action);
          this.resetGestureState();
        }, 150);

        return;
      }
    }

    // Reset if no matching action
    this.resetGestureState();
  };

  private resetGestureState(): void {
    this.isGestureActive = false;
    this.gesturePattern = [];
    this.mouseTrail = [];
    this.activeActionName = "";
    this.display.hide();
  }

  private handleContextMenu = (event: MouseEvent): void => {
    if (this.isGestureActive && isEnabled()) {
      event.preventDefault();
      event.stopPropagation();
    }
  };
}
