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
  private accumulatedDX = 0;
  private accumulatedDY = 0;
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
    this.accumulatedDX = 0;
    this.accumulatedDY = 0;
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
    const registerThreshold =
      this.getDirectionalRegistrationThreshold(config);
    const coords = this.getAdjustedClientCoords(event);
    this.mouseTrail.push(coords);
    const deltaX = coords.x - this.lastX;
    const deltaY = coords.y - this.lastY;
    this.accumulatedDX += deltaX;
    this.accumulatedDY += deltaY;
    const accumulatedDistance = Math.hypot(
      this.accumulatedDX,
      this.accumulatedDY,
    );

    // Use accumulated vector magnitude to determine whether to register
    // movement, so diagonal and incremental movements are recognized.
    if (accumulatedDistance >= registerThreshold) {
      const direction = this.quantizeDirection(
        this.accumulatedDX,
        this.accumulatedDY,
      );

      if (
        direction &&
        (this.gesturePattern.length === 0 ||
          this.gesturePattern[this.gesturePattern.length - 1] !== direction)
      ) {
        this.gesturePattern.push(direction);
        const matchingAction = this.findMatchingAction(config);

        if (matchingAction) {
          this.activeActionName = getActionDisplayName(matchingAction.action);
        } else {
          this.activeActionName = "";
        }
      }

      this.accumulatedDX = 0;
      this.accumulatedDY = 0;
    }

    this.lastX = coords.x;
    this.lastY = coords.y;
    this.display.updateTrail(this.mouseTrail);
    this.display.updateActionName(this.activeActionName);
  };

  private handleMouseUp = (event: MouseEvent): void => {
    if (!this.isGestureActive || event.button !== 2 || !isEnabled()) return;

    const config = getConfig();
    const minDistance = config.contextMenu.minDistance;
    const preventionTimeout = config.contextMenu.preventionTimeout;

    let pattern = this.gesturePattern;
    if (pattern.length === 0) {
      pattern = this.inferPatternFromTrail(config);
    }

    if (pattern.length > 0) {
      const matchingAction = this.findMatchingAction(config, pattern);

      if (matchingAction) {
        this.gesturePattern = pattern;
        this.activeActionName = getActionDisplayName(matchingAction.action);
        this.display.updateActionName(this.activeActionName);
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

    const totalMovement = this.getTotalMovement();
    if (totalMovement < minDistance) {
      this.isContextMenuPrevented = false;
      this.resetGestureState();
      return;
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
    this.accumulatedDX = 0;
    this.accumulatedDY = 0;
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

  private quantizeDirection(
    dx: number,
    dy: number,
  ): GestureDirection | null {
    if (!dx && !dy) {
      return null;
    }

    const angle = Math.atan2(dy, dx);
    const deg = (angle * 180) / Math.PI;
    const norm = (deg + 360) % 360;

    if (norm >= 337.5 || norm < 22.5) return "right";
    if (norm >= 22.5 && norm < 67.5) return "downRight";
    if (norm >= 67.5 && norm < 112.5) return "down";
    if (norm >= 112.5 && norm < 157.5) return "downLeft";
    if (norm >= 157.5 && norm < 202.5) return "left";
    if (norm >= 202.5 && norm < 247.5) return "upLeft";
    if (norm >= 247.5 && norm < 292.5) return "up";
    if (norm >= 292.5 && norm < 337.5) return "upRight";

    return null;
  }

  private findMatchingAction(
    config: ReturnType<typeof getConfig>,
    pattern: GestureDirection[] = this.gesturePattern,
  ) {
    if (pattern.length === 0) {
      return null;
    }

    const patternString = patternToString(pattern);
    return (
      config.actions.find(
        (action) => patternToString(action.pattern) === patternString,
      ) || null
    );
  }

  private getFallbackSegmentThreshold(
    config: ReturnType<typeof getConfig>,
  ): number {
    const base = Math.max(2, Math.ceil(config.contextMenu.minDistance * 0.7));
    const scaled = Math.max(base, Math.round(config.sensitivity * 0.12));
    const ceiling = Math.max(base, Math.round(config.sensitivity * 0.32));
    return Math.min(scaled, ceiling);
  }

  private inferPatternFromTrail(
    config: ReturnType<typeof getConfig>,
  ): GestureDirection[] {
    if (this.mouseTrail.length < 2) {
      return [];
    }

    const threshold = this.getFallbackSegmentThreshold(config);
    const toleranceDistance = Math.max(
      threshold * 0.45,
      Math.round(config.contextMenu.minDistance * 0.75),
    );
    const pattern: GestureDirection[] = [];

    let currentDirection: GestureDirection | null = null;
    let currentDistance = 0;

    for (let i = 1; i < this.mouseTrail.length; i++) {
      const prev = this.mouseTrail[i - 1];
      const point = this.mouseTrail[i];
      const dx = point.x - prev.x;
      const dy = point.y - prev.y;
      const stepDistance = Math.hypot(dx, dy);

      if (stepDistance === 0) {
        continue;
      }

      const stepDirection = this.quantizeDirection(dx, dy);
      if (!stepDirection) {
        continue;
      }

      if (currentDirection === stepDirection || currentDirection === null) {
        currentDirection = stepDirection;
        currentDistance += stepDistance;
        continue;
      }

      if (
        currentDirection &&
        (currentDistance >= threshold || currentDistance >= toleranceDistance)
      ) {
        if (
          pattern.length === 0 ||
          pattern[pattern.length - 1] !== currentDirection
        ) {
          pattern.push(currentDirection);
        }
      }

      currentDirection = stepDirection;
      currentDistance = stepDistance;
    }

    if (
      currentDirection &&
      (currentDistance >= threshold || currentDistance >= toleranceDistance)
    ) {
      if (
        pattern.length === 0 ||
        pattern[pattern.length - 1] !== currentDirection
      ) {
        pattern.push(currentDirection);
      }
    }

    if (pattern.length === 0) {
      const start = this.mouseTrail[0];
      const end = this.mouseTrail[this.mouseTrail.length - 1];
      const dx = end.x - start.x;
      const dy = end.y - start.y;
      const overallDistance = Math.hypot(dx, dy);
      const overallDirection = this.quantizeDirection(dx, dy);
      const minimumOverall = Math.max(
        Math.round(config.contextMenu.minDistance * 0.85),
        Math.min(threshold, config.contextMenu.minDistance + 1),
      );

      if (overallDirection && overallDistance >= minimumOverall) {
        return [overallDirection];
      }
    }

    return pattern;
  }

  private getDirectionalRegistrationThreshold(
    config: ReturnType<typeof getConfig>,
  ): number {
    const base = Math.max(2, Math.ceil(config.contextMenu.minDistance * 0.6));
    const scaled = Math.max(base, Math.round(config.sensitivity * 0.12));
    const ceiling = Math.max(base, Math.round(config.sensitivity * 0.35));
    return Math.min(scaled, ceiling);
  }
}
