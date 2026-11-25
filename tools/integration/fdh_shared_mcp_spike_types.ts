// SPDX-License-Identifier: MPL-2.0
export interface ProtocolSpikeReport {
  scope: "protocol-only";
  identity: "nonGoogle";
  mcpAuthorizationGranted: false;
  ac05Qualified: false;
  experimental: true;
  audienceAndScopeAccepted: boolean;
  invalidTargetRejected: boolean;
  missingScopeRejected: boolean;
  wrongScopeRejected: boolean;
  expiredTokenRejected: boolean;
  unboundMachineMcpRejected: boolean;
}
/** Trusted offline test seam; never accepted from CLI arguments. */
export interface ProtocolCliDependencies {
  readonly execute: () => Promise<ProtocolSpikeReport>;
  readonly report: (report: ProtocolSpikeReport) => void;
  readonly error: () => void;
}
export interface ProtocolContainerOwnership {
  containerID: string;
  image: string;
  name: string;
  nonce: string;
}
export interface ProtocolCleanupDependencies {
  recover: () => Promise<ProtocolContainerOwnership | null>;
  assertOwned: (ownership: ProtocolContainerOwnership) => Promise<void>;
  stop: (id: string) => Promise<void>;
  confirmAbsent: (id: string) => Promise<boolean>;
}
export type JsonRecord = Record<string, unknown>;

/** In-process trusted capability only; never serialized or accepted from HTTP. */
export interface EphemeralKeycloakContext {
  readonly issuer: string;
  readonly resource: string;
  readonly realmPath: string;
  readonly audienceMapper: JsonRecord;
  readonly machineSecret: string;
  readonly assertLive: () => void;
  readonly adminRest: (
    path: string,
    method: string,
    body?: unknown,
  ) => Promise<unknown>;
  readonly strictLocalFetch: (
    url: string,
    init?: RequestInit,
  ) => Promise<Response>;
  readonly configuration: (
    clientId: string,
    secret?: string,
  ) => Promise<import("openid-client").Configuration>;
}
