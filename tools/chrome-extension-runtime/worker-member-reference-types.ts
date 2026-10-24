// SPDX-License-Identifier: MPL-2.0
import type { CDPResult } from "./cdp-types.ts";
import type { CaptureReference } from "./offscreen-test-types.ts";

export type WorkerKind = "classic" | "module";
export interface NativeMemberShape {
  type: string;
  inNamespace: boolean;
  descriptor: {
    configurable: boolean;
    enumerable: boolean;
    writable?: boolean;
    valueType: string;
    get: string;
    set: string;
  } | null;
}
export interface MemberPermissionState {
  generation: string;
  phase: string;
  sameChrome: boolean;
  sameManagement: boolean;
  sameRuntime: boolean;
  permissions: { management: boolean; nativeMessaging: boolean };
  events: {
    event: string;
    permissions: { permissions: string[]; origins: string[] };
  }[];
  managementKeys: string[];
  runtimeKeys: string[];
  members: Record<string, NativeMemberShape>;
  sameGrantedMembers: Record<string, boolean> | null;
}
export interface NativeMemberCase {
  kind: WorkerKind;
  mode:
    | "undeclared"
    | "optional"
    | "optional-unread"
    | "optional-unread-keys"
    | "optional-unread-ownKeys"
    | "optional-unread-descriptor";
  extensionId: string;
  workerTargetId: string;
  initial?: MemberPermissionState;
  preRequestIdentity?: { generation: string; sameChrome: boolean };
  preGrantRootAccess?: {
    kind: "none" | "keys" | "ownKeys" | "descriptor";
    names?: string[];
    descriptor?: NativeMemberShape["descriptor"];
  };
  ownInfo?: { id: string; name: string };
  decision?: {
    kind: "native-prompt";
    focusedWindow: string;
    pid: number;
    title: string;
    screenshot: string;
  } | { kind: "quiet-grant" };
  granted?: MemberPermissionState;
  grantedLater?: MemberPermissionState;
  freshGrantedPageMembers?: Record<string, string>;
  removed?: boolean;
  revoked?: MemberPermissionState;
  revokedGetAll?: { name?: string; message?: string; resolved?: unknown };
  finalWorkerTargetId?: string;
  sameWorker?: boolean;
}
export interface NativeMemberReport {
  reference: CaptureReference;
  output: string;
  passed: boolean;
  scope: string;
  cases: NativeMemberCase[];
  version?: CDPResult<"Browser.getVersion">;
  error?: string;
}
