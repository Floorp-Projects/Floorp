// SPDX-License-Identifier: MPL-2.0

export interface M3dCommandReadOperation {
  readonly promise: Promise<Uint8Array>;
  readonly cancel: () => Promise<void>;
}

export interface M3dOwnedCommandStatus {
  readonly success: boolean;
}

export interface M3dOwnedCommandOperations {
  readonly stdout: M3dCommandReadOperation;
  readonly stderr: M3dCommandReadOperation;
  readonly status: Promise<M3dOwnedCommandStatus>;
  readonly terminate: () => void;
}

export interface M3dOwnedCommandResult {
  readonly completed: boolean;
  readonly stdout: Uint8Array | null;
  readonly success: boolean | null;
  readonly cleanupProven: boolean;
}
