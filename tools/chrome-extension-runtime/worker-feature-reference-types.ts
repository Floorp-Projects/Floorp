// SPDX-License-Identifier: MPL-2.0
import type { CDPResult } from "./cdp-types.ts";
import type { CaptureReference } from "./offscreen-test-types.ts";
import type {
  NativeMemberShape,
  WorkerKind,
} from "./worker-member-reference-types.ts";
export interface WorkerFeatureCase {
  kind: WorkerKind;
  name: string;
  permissions: string[];
  hostPermissions: string[];
  extensionId?: string;
  workerTargetId?: string;
  members?: Record<string, NativeMemberShape>;
  safeCalls?: Record<
    string,
    { resolved?: unknown; name?: string; message?: string }
  >;
  detachedCalls?: {
    selfId: string;
    url: string;
    power: string;
    release: string;
    capturedTabs: unknown[];
    foreignThis: {
      selfId: string;
      url: string;
      power: string;
      release: string;
      capturedTabs: unknown[];
    };
  };
}
export interface WorkerFeatureReport {
  reference: CaptureReference;
  output: string;
  passed: boolean;
  scope: string;
  cases: WorkerFeatureCase[];
  version?: CDPResult<"Browser.getVersion">;
  error?: string;
}
