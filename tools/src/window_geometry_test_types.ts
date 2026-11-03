// SPDX-License-Identifier: MPL-2.0
export interface WindowGeometrySnapshot {
  left: number;
  top: number;
  width: number;
  height: number;
}
export type WindowGeometryRequest = {
  [Key in keyof WindowGeometrySnapshot]: number | null;
};
export interface WindowGeometryEvent {
  type: string;
  isTrusted: boolean;
  target: WindowGeometryNativeWindow;
}
export type WindowGeometryListener = (event: WindowGeometryEvent) => void;
export interface WindowGeometryEventTarget {
  addEventListener(type: string, listener: WindowGeometryListener): void;
  removeEventListener(type: string, listener: WindowGeometryListener): void;
}
export interface WindowGeometryNativeWindow extends WindowGeometryEventTarget {
  screenX: number;
  screenY: number;
  outerWidth: number;
  outerHeight: number;
  closed: boolean;
  windowRoot: WindowGeometryEventTarget;
}
export interface WindowGeometryCloseHandler {
  close(): void;
}
export interface WindowGeometryContext {
  active: boolean;
  unloaded: boolean;
  canAccessWindow(window: WindowGeometryNativeWindow): boolean;
  callOnClose(handler: WindowGeometryCloseHandler): void;
  forgetOnClose(handler: WindowGeometryCloseHandler): void;
}
export interface WindowGeometryCompletion {
  completion: "unchanged" | "synchronous" | "acknowledged" | "timeout";
  nativeEvents: { move: boolean; resize: boolean };
}
export type WindowGeometryUpdater = (
  context: WindowGeometryContext,
  window: WindowGeometryNativeWindow,
  options: WindowGeometryRequest,
  update: () => void,
) => Promise<WindowGeometryCompletion>;
