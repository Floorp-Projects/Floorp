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
    const coords = this.getAdjustedClientCoords(event);
    this.mouseTrail.push(coords);

    // パフォーマンス向上のため、一定のポイント数毎にパターンマッチングを実行
    if (this.mouseTrail.length % 3 === 0 || this.mouseTrail.length > 20) {
      const recognizedPattern = this.recognizeGesturePattern(config);
      if (recognizedPattern) {
        const matchingAction = this.findMatchingAction(
          config,
          recognizedPattern,
        );
        if (matchingAction) {
          this.gesturePattern = recognizedPattern;
          this.activeActionName = getActionDisplayName(matchingAction.action);
        } else {
          this.activeActionName = "";
        }
      } else {
        this.activeActionName = "";
      }
    }

    this.lastX = coords.x;
    this.lastY = coords.y;
    this.display.updateTrail(this.mouseTrail);
    this.display.updateActionName(this.activeActionName);
  };

  private handleMouseUp = (event: MouseEvent): void => {
    if (!this.isGestureActive || event.button !== 2 || !isEnabled()) return;

    const config = getConfig();
    const preventionTimeout = config.contextMenu.preventionTimeout;

    // 最終的なパターン認識を実行
    const finalPattern = this.recognizeGesturePattern(config);

    if (finalPattern && finalPattern.length > 0) {
      const matchingAction = this.findMatchingAction(config, finalPattern);

      if (matchingAction) {
        this.gesturePattern = finalPattern;
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

    // ジェスチャーが認識されない場合、最小移動距離をチェック
    const totalMovement = this.getTotalMovement();
    const minMovement = Math.max(config.contextMenu.minDistance, 10); // 最小値を設定

    if (totalMovement < minMovement) {
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

  private quantizeDirection(dx: number, dy: number): GestureDirection | null {
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

  /**
   * 新しいジェスチャー認識アルゴリズム
   * 全体的なトレイルの形状を分析して、設定されたパターンとマッチするかを判定
   */
  private recognizeGesturePattern(
    config: ReturnType<typeof getConfig>,
  ): GestureDirection[] | null {
    if (this.mouseTrail.length < 2) {
      return null;
    }

    // 最小移動距離を大幅に削減（小さなジェスチャーに対応）
    const totalDistance = this.getTotalTrailDistance();
    const minDistance = Math.max(3, config.contextMenu.minDistance * 0.3); // より小さな最小距離
    if (totalDistance < minDistance) {
      return null;
    }

    // 各設定されたパターンとの類似度を計算
    const bestMatch = this.findBestPatternMatch(config);
    return bestMatch;
  }

  /**
   * トレイル全体の距離を計算
   */
  private getTotalTrailDistance(): number {
    if (this.mouseTrail.length < 2) return 0;

    let totalDistance = 0;
    for (let i = 1; i < this.mouseTrail.length; i++) {
      const prev = this.mouseTrail[i - 1];
      const curr = this.mouseTrail[i];
      totalDistance += Math.hypot(curr.x - prev.x, curr.y - prev.y);
    }
    return totalDistance;
  }

  /**
   * 設定されたパターンの中から最も類似度の高いものを探す
   */
  private findBestPatternMatch(
    config: ReturnType<typeof getConfig>,
  ): GestureDirection[] | null {
    const configActions = config.actions;
    if (configActions.length === 0) {
      return null;
    }

    let bestPattern: GestureDirection[] | null = null;
    let bestSimilarity = 0;

    // 感度に応じて類似度閾値を動的に調整
    const sensitivityFactor = (config.sensitivity || 40) / 100; // 0.0-1.0の範囲
    const minSimilarity = Math.max(0.4, 0.8 - sensitivityFactor * 0.3); // 0.4-0.8の範囲

    for (const action of configActions) {
      const similarity = this.calculatePatternSimilarity(action.pattern);
      if (similarity > bestSimilarity && similarity >= minSimilarity) {
        bestSimilarity = similarity;
        bestPattern = action.pattern;
      }
    }

    return bestPattern;
  }

  /**
   * 現在のトレイルと指定されたパターンの類似度を計算
   */
  private calculatePatternSimilarity(
    targetPattern: GestureDirection[],
  ): number {
    if (targetPattern.length === 0) return 0;

    // 単一方向のパターンは簡単な方法で判定
    if (targetPattern.length === 1) {
      return this.calculateSingleDirectionSimilarity(targetPattern[0]);
    }

    // トレイルを正規化されたベクトルに変換
    const normalizedTrail = this.normalizeTrail();
    if (normalizedTrail.length === 0) return 0;

    // パターンを理想的なベクトル系列に変換
    const idealVectors = this.patternToVectors(targetPattern);

    // DTW（Dynamic Time Warping）アルゴリズムで類似度を計算
    const similarity = this.calculateDTWSimilarity(
      normalizedTrail,
      idealVectors,
    );

    return similarity;
  }

  /**
   * 単一方向パターンの類似度を計算（より簡潔で高速）
   */
  private calculateSingleDirectionSimilarity(
    targetDirection: GestureDirection,
  ): number {
    if (this.mouseTrail.length < 2) return 0;

    const start = this.mouseTrail[0];
    const end = this.mouseTrail[this.mouseTrail.length - 1];
    const totalDx = end.x - start.x;
    const totalDy = end.y - start.y;
    const totalMagnitude = Math.hypot(totalDx, totalDy);

    if (totalMagnitude === 0) return 0;

    const normalizedDx = totalDx / totalMagnitude;
    const normalizedDy = totalDy / totalMagnitude;

    const targetVectors = this.patternToVectors([targetDirection]);
    if (targetVectors.length === 0) return 0;

    const targetVec = targetVectors[0];

    const cosineSimilarity = normalizedDx * targetVec.dx +
      normalizedDy * targetVec.dy;

    return Math.max(0, cosineSimilarity);
  }

  /**
   * トレイルを正規化されたベクトル系列に変換
   */
  private normalizeTrail(): { dx: number; dy: number; magnitude: number }[] {
    if (this.mouseTrail.length < 2) return [];

    const vectors: { dx: number; dy: number; magnitude: number }[] = [];

    for (let i = 1; i < this.mouseTrail.length; i++) {
      const prev = this.mouseTrail[i - 1];
      const curr = this.mouseTrail[i];
      const dx = curr.x - prev.x;
      const dy = curr.y - prev.y;
      const magnitude = Math.hypot(dx, dy);

      if (magnitude > 0.1) {
        // 極めて小さな移動のみ無視（0.1ピクセル以下）
        vectors.push({
          dx: dx / magnitude,
          dy: dy / magnitude,
          magnitude,
        });
      }
    }

    return vectors;
  }

  /**
   * パターンを理想的なベクトル系列に変換
   */
  private patternToVectors(
    pattern: GestureDirection[],
  ): { dx: number; dy: number }[] {
    const directionMap: Record<GestureDirection, { dx: number; dy: number }> = {
      right: { dx: 1, dy: 0 },
      downRight: { dx: 0.707, dy: 0.707 },
      down: { dx: 0, dy: 1 },
      downLeft: { dx: -0.707, dy: 0.707 },
      left: { dx: -1, dy: 0 },
      upLeft: { dx: -0.707, dy: -0.707 },
      up: { dx: 0, dy: -1 },
      upRight: { dx: 0.707, dy: -0.707 },
    };

    return pattern.map((direction) => directionMap[direction]);
  }

  /**
   * DTW（Dynamic Time Warping）を使用した類似度計算
   */
  private calculateDTWSimilarity(
    trailVectors: { dx: number; dy: number; magnitude: number }[],
    idealVectors: { dx: number; dy: number }[],
  ): number {
    if (trailVectors.length === 0 || idealVectors.length === 0) return 0;

    const n = trailVectors.length;
    const m = idealVectors.length;

    // DTWマトリックス
    const dtw: number[][] = Array(n + 1)
      .fill(null)
      .map(() => Array(m + 1).fill(Number.POSITIVE_INFINITY));
    dtw[0][0] = 0;

    for (let i = 1; i <= n; i++) {
      for (let j = 1; j <= m; j++) {
        const trailVec = trailVectors[i - 1];
        const idealVec = idealVectors[j - 1];

        // コサイン類似度を計算
        const dotProduct = trailVec.dx * idealVec.dx +
          trailVec.dy * idealVec.dy;
        const cosineSimilarity = Math.max(0, dotProduct); // 0-1の範囲に正規化
        const cost = 1 - cosineSimilarity;

        dtw[i][j] = cost +
          Math.min(
            dtw[i - 1][j], // 挿入
            dtw[i][j - 1], // 削除
            dtw[i - 1][j - 1], // マッチ
          );
      }
    }

    // 正規化された類似度を返す（0-1の範囲）
    const maxCost = Math.max(n, m);
    const normalizedDistance = dtw[n][m] / maxCost;
    return Math.max(0, 1 - normalizedDistance);
  }
}
