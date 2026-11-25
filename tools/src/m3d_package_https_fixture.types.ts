// SPDX-License-Identifier: MPL-2.0

export type M3dPackageHttpsMode =
  | "valid-streaming"
  | "redirect"
  | "status-rejected"
  | "content-type-rejected"
  | "content-encoding-rejected"
  | "oversized-declared"
  | "oversized-streamed"
  | "truncated-length"
  | "wrong-package"
  | "stalled-body"
  | "read-failure"
  | "equal-content-length"
  | "conflicting-content-length"
  | "cookie-seed"
  | "cookie-control";

export type M3dPackageHttpsOriginKind = "trusted" | "untrusted" | "raw";

export interface M3dPackageHttpsRequestObservation {
  readonly origin: M3dPackageHttpsOriginKind;
  readonly mode: M3dPackageHttpsMode | "redirect-target";
  readonly routePath: string | null;
  readonly featureId: string | null;
  readonly packageSha256: string | null;
  readonly cookiePresent: boolean;
  readonly seedCookieMarkerMatch: boolean;
  readonly acquisitionCookieMarkerMatch: boolean;
  readonly authorizationPresent: boolean;
  readonly authorizationMarkerMatch: boolean;
  readonly referrerPresent: boolean;
  readonly referrerMarkerMatch: boolean;
}

export interface M3dPackageHttpsSnapshot {
  readonly requests: number;
  readonly activeStreams: number;
  readonly activeConnections: number;
  readonly cancellations: number;
  readonly redirectTargets: number;
  readonly observations: readonly M3dPackageHttpsRequestObservation[];
  readonly observationOverflow: boolean;
  readonly ports: Readonly<{
    trusted: number;
    untrusted: number;
    raw: number;
  }>;
}

export interface M3dPackageHttpsCleanupEvidence {
  readonly closed: boolean;
  readonly listenersClosed: boolean;
  readonly activeConnectionsClosed: boolean;
  readonly stallsReleased: boolean;
  readonly acceptLoopsSettled: boolean;
  readonly cleanupErrorCount: number;
}

export interface M3dPackageHttpsFixture {
  readonly trustedOrigin: string;
  readonly untrustedOrigin: string;
  readonly rawOrigin: string;
  readonly caCertificatePem: string;
  setMode(mode: M3dPackageHttpsMode): void;
  snapshot(): M3dPackageHttpsSnapshot;
  releaseStalls(): Promise<void>;
  close(): Promise<M3dPackageHttpsCleanupEvidence>;
}
