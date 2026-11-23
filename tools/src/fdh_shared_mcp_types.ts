// SPDX-License-Identifier: MPL-2.0
import type {
  SharedAccessPolicy,
  VerifiedSharedAccessToken,
} from "./fdh_shared_access_types.ts";

/** Deployment configuration, never derived from request Host/Origin/headers. */
export type SharedMcpPolicy = SharedAccessPolicy;

/** Result of a trusted online broker binding check, NOT user profile/input claims.
 * The deployment must introspect active session and resolve the Google federated
 * subject from its broker, not substitute its realm subject or editable attribute. */
export interface SharedMcpWorkspaceBinding {
  readonly kind: "verified-online-google-binding";
  readonly active: true;
  readonly sessionProvider: "google";
  readonly authorizationSubject: string;
  readonly identityKey: string;
  readonly identityIssuer: "https://accounts.google.com";
  readonly email: string;
  readonly emailVerified: true;
  readonly hostedDomain: "floorp.app";
}

/** Trusted deployment/test seams only; never exposed in HTTP or an operator flag. */
export interface SharedMcpTrustedDependencies {
  readonly verifyAccessToken: (
    token: string,
  ) => Promise<VerifiedSharedAccessToken>;
  readonly resolveOnlineWorkspaceBinding: (
    token: string,
    access: VerifiedSharedAccessToken,
  ) => Promise<SharedMcpWorkspaceBinding | null>;
}

export type SharedMcpHandler = (request: Request) => Promise<Response>;
