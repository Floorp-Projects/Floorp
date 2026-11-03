// SPDX-License-Identifier: MPL-2.0
import type { CDPMethod, CDPResult } from "./cdp-types.ts";
export interface DebuggerFixtureConfig {
  url: string;
  upload: string;
  uploadSize: number;
  nativeOverrides?: boolean;
}
export interface DebuggerTab {
  id: number;
  status?: string;
}
export interface DebuggerSource {
  tabId: number;
}
export interface PublicDebuggerAPI {
  tabs: {
    create(options: { url: string; active: boolean }): Promise<DebuggerTab>;
    get(id: number): Promise<DebuggerTab>;
    update(id: number, options: { url: string }): Promise<DebuggerTab>;
    remove(id: number): Promise<void>;
    setZoom(id: number, zoom: number): Promise<void>;
  };
  debugger: {
    sendCommand<M extends CDPMethod>(
      source: DebuggerSource,
      method: M,
      params?: Record<string, unknown>,
    ): Promise<CDPResult<M>>;
    attach(source: DebuggerSource, version: string): Promise<void>;
    detach(source: DebuggerSource): Promise<void>;
    getTargets(): Promise<{ tabId?: number; attached: boolean }[]>;
    onEvent: {
      addListener(
        listener: (
          source: DebuggerSource,
          method: string,
          params?: Record<string, unknown>,
        ) => void,
      ): void;
    };
  };
}
export interface ContractProbeReport {
  passed: boolean;
  cases: Record<string, boolean>;
  error?: string;
  stack?: string;
}
export interface DebuggerProbeReport extends ContractProbeReport {
  passed: boolean;
  cases: Record<string, boolean>;
  events: string[];
  zoomMetrics?: CDPResult<"Page.getLayoutMetrics">;
  error?: string;
  stack?: string;
}
export type DebuggerFixtureGlobals = typeof globalThis & {
  chrome: PublicDebuggerAPI;
  floorpDebuggerConfig: DebuggerFixtureConfig;
  floorpReferenceReport?: DebuggerProbeReport;
};
export interface DebuggerComparisonReport {
  passed: boolean;
  scope: string;
  reference: { browserVersion: string; binary: { sha256: string } };
  output: string;
  fixture?: { name: string; version: string; id: string; sha256: string };
  chromeVersion?: CDPResult<"Browser.getVersion">;
  chromeBinarySha256?: string;
  chrome?: ContractProbeReport;
  floorp?: ContractProbeReport;
  matches?: boolean;
  error?: string;
  stack?: string;
}
export interface DebuggerRunnerResult {
  status: number | null;
  stdout: string;
  stderr: string;
  error?: Error;
}
export interface DebuggerEcho {
  userAgent: string;
  body: string;
}
