// SPDX-License-Identifier: MPL-2.0
export interface WorkspaceSpikeOptions {
  readonly taskId: string;
  readonly clientId: string;
  readonly clientSecretEnv: "FDH_GOOGLE_CLIENT_SECRET";
  readonly timeoutMs: number;
}
export interface WorkspaceSpikeReport {
  readonly scope: "local-google-workspace-mcp-probe";
  readonly experimental: true;
  readonly ac05Qualified: false;
  readonly productionQualified: false;
  readonly protectedOperationsAllowed: false;
  readonly mcpAuthorizationGranted: boolean;
  readonly expiredTokenRejected: boolean;
  readonly refreshProbeSucceeded: boolean;
  readonly revokedTokenRejected: boolean;
  /** Offline cryptographic JWT invalidation is NOT assumed from online refusal. */
  readonly immediateJwtInvalidationObserved: false;
  readonly immediateMcpRejectionObserved: boolean;
  readonly onlineSessionInactiveObserved: boolean;
  readonly offlineJwtStillValidAfterRevocation: boolean;
}
export interface WorkspaceCliDependencies {
  readonly execute: (
    options: WorkspaceSpikeOptions,
  ) => Promise<WorkspaceSpikeReport>;
  readonly report: (report: WorkspaceSpikeReport) => void;
  readonly error: () => void;
}
