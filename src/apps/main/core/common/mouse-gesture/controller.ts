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
import {
  executeGestureAction,
  getActionDisplayName,
} from "./utils/gestures.ts";

export class MouseGestureController {
  private isGestureActive = false;
  private isContextMenuPrevented = false;
  private preventionTimeoutId: number | null = null;
  private gesturePattern: GestureDirection[] = [];
  private lastX = 0;
  private lastY = 0;
  private mouseTrail: { x: number; y: number }[] = [];
  private display: GestureDisplay;
  private activeActionName = "";
  private eventListenersAttached = false;

  constructor() {
    this.display = new GestureDisplay();
    this.init();
  }

  private init(): void {
    if (this.eventListenersAttached) return;

    if (typeof globalThis !== "undefined") {
      globalThis.addEventListener("mousedown", this.handleMouseDown);
      globalThis.addEventListener("mousemove", this.handleMouseMove);
      globalThis.addEventListener("mouseup", this.handleMouseUp);
      globalThis.addEventListener("contextmenu", this.handleContextMenu, true);
      this.eventListenersAttached = true;
    }
  }

  public destroy(): void {
    if (typeof globalThis !== "undefined" && this.eventListenersAttached) {
      globalThis.removeEventListener("mousedown", this.handleMouseDown);
      globalThis.removeEventListener("mousemove", this.handleMouseMove);
      globalThis.removeEventListener("mouseup", this.handleMouseUp);
      globalThis.removeEventListener(
        "contextmenu",
        this.handleContextMenu,
        true,
      );
      this.eventListenersAttached = false;
    }

    if (this.preventionTimeoutId !== null) {
      clearTimeout(this.preventionTimeoutId);
      this.preventionTimeoutId = null;
    }

    this.resetGestureState();
    this.display.destroy();
  }

  private getAdjustedClientCoords(event: MouseEvent): { x: number; y: number } {
    return {
      x: event.clientX,
      y: event.clientY,
    };
  }

  private handleMouseDown = (event: MouseEvent): void => {
    if (event.button !== 2 || !isEnabled()) {
      return;
    }

    this.isContextMenuPrevented = true;
    if (this.preventionTimeoutId !== null) {
      clearTimeout(this.preventionTimeoutId);
      this.preventionTimeoutId = null;
    }

    this.isGestureActive = true;
    this.gesturePattern = [];

    const coords = this.getAdjustedClientCoords(event);
    this.lastX = coords.x;
    this.lastY = coords.y;
    this.mouseTrail = [coords];
    this.activeActionName = "";

    this.display.show();
    this.display.updateTrail(this.mouseTrail);

    if (event.target instanceof Element) {
      event.preventDefault();
    }
  };

  private handleMouseMove = (event: MouseEvent): void => {
    if (!this.isGestureActive || !isEnabled()) {
      return;
    }

    const config = getConfig();
    const { sensitivity } = config;
    const coords = this.getAdjustedClientCoords(event);
    this.mouseTrail.push(coords);
    const dx = coords.x - this.lastX;
    const dy = coords.y - this.lastY;

    // Use vector magnitude to determine whether to register movement, so
    // diagonal movements are recognized reliably as well.
    if (Math.hypot(dx, dy) > sensitivity) {
      let direction: GestureDirection | null = null;

      // Quantize to 8 directions (right, downRight, down, downLeft, left, upLeft, up, upRight)
      const angle = Math.atan2(dy, dx); // dy>0 means moving down in screen coords
      const deg = (angle * 180) / Math.PI;
      const norm = (deg + 360) % 360; // 0..360

      if (norm >= 337.5 || norm < 22.5) direction = "right";
      else if (norm >= 22.5 && norm < 67.5) direction = "downRight";
      else if (norm >= 67.5 && norm < 112.5) direction = "down";
      else if (norm >= 112.5 && norm < 157.5) direction = "downLeft";
      else if (norm >= 157.5 && norm < 202.5) direction = "left";
      else if (norm >= 202.5 && norm < 247.5) direction = "upLeft";
      else if (norm >= 247.5 && norm < 292.5) direction = "up";
      else if (norm >= 292.5 && norm < 337.5) direction = "upRight";

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
          this.activeActionName = getActionDisplayName(matchingAction.action);
        } else {
          this.activeActionName = "";
        }
      }

      this.lastX = coords.x;
      this.lastY = coords.y;
    }

    this.display.updateTrail(this.mouseTrail);
    this.display.updateActionName(this.activeActionName);
  };

  private handleMouseUp = (event: MouseEvent): void => {
    if (!this.isGestureActive || event.button !== 2 || !isEnabled()) return;

    const config = getConfig();
    const minDistance = config.contextMenu.minDistance;
    const preventionTimeout = config.contextMenu.preventionTimeout;

    const totalMovement = this.getTotalMovement();
    if (totalMovement < minDistance) {
      this.isContextMenuPrevented = false;
      this.resetGestureState();
      return;
    }

    if (this.gesturePattern.length > 0) {
      const patternString = patternToString(this.gesturePattern);
      const matchingAction = config.actions.find(
        (action) => patternToString(action.pattern) === patternString,
      );

      if (matchingAction) {
        setTimeout(() => {
          executeGestureAction(matchingAction.action);
          this.resetGestureState();
          this.preventionTimeoutId = setTimeout(() => {
            this.isContextMenuPrevented = false;
            this.preventionTimeoutId = null;
          }, preventionTimeout);
        }, 100);

        return;
      }
    }

    this.preventionTimeoutId = setTimeout(() => {
      this.isContextMenuPrevented = false;
      this.preventionTimeoutId = null;
    }, preventionTimeout);

    this.resetGestureState();
  };

  private getTotalMovement(): number {
    if (this.mouseTrail.length < 2) return 0;

    const startPoint = this.mouseTrail[0];
    const lastPoint = this.mouseTrail[this.mouseTrail.length - 1];

    const dx = lastPoint.x - startPoint.x;
    const dy = lastPoint.y - startPoint.y;

    return Math.sqrt(dx * dx + dy * dy);
  }

  private resetGestureState(): void {
    this.isGestureActive = false;
    this.gesturePattern = [];
    this.mouseTrail = [];
    this.activeActionName = "";
    this.display.hide();
  }

  private handleContextMenu = (event: MouseEvent): void => {
    if ((this.isGestureActive || this.isContextMenuPrevented) && isEnabled()) {
      event.preventDefault();
      event.stopPropagation();
    }
  };
}
