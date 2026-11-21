// SPDX-License-Identifier: MPL-2.0
export interface WorkspaceAuthProbeOptions {
  readonly clientId: string;
  readonly clientSecret?: string;
  readonly redirectUri: string;
  readonly timeoutMs: number;
}
export interface WorkspaceAuthProbeResult {
  readonly schemaVersion: 1;
  readonly scope: "workspace-oauth-probe";
  readonly codeExchangeVerified: true;
  readonly refreshVerified: true;
  readonly localLogout: true;
  readonly providerRevocationConfirmed: boolean;
  readonly identity: {
    readonly subject: string;
    readonly email: string;
    readonly hostedDomain: string;
    readonly issuer: string;
  };
  readonly mcpAuthorizationGranted: false;
  readonly ac05Qualified: false;
}
/** Trusted adapter seam for offline callback-server lifecycle tests only. */
export interface WorkspaceAuthProbeServer {
  shutdown(): Promise<void>;
}
export interface WorkspaceAuthProbeServerOptions {
  readonly hostname: "127.0.0.1";
  readonly port: number;
  readonly signal: AbortSignal;
  readonly onListen: () => void;
  readonly onError: () => Response;
}
export interface WorkspaceAuthProbeDependencies {
  readonly client?: Pick<
    import("./fdh_workspace_oauth.ts").WorkspaceOAuthClient,
    | "beginAuthorization"
    | "completeAuthorization"
    | "clearPending"
    | "refreshSession"
    | "logout"
  >;
  readonly serve?: (
    options: WorkspaceAuthProbeServerOptions,
    handler: (request: Request) => Promise<Response>,
  ) => WorkspaceAuthProbeServer;
  readonly announce?: (url: string) => void;
}

export interface WorkspaceAuthCliDependencies {
  readonly readEnv?: (name: string) => string | undefined;
  readonly probe?: (
    options: WorkspaceAuthProbeOptions,
  ) => Promise<WorkspaceAuthProbeResult>;
}
export interface WorkspaceAuthCliResult {
  readonly exitCode: number;
  readonly stdout: string;
}
