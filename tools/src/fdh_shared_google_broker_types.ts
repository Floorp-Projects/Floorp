// SPDX-License-Identifier: MPL-2.0
import type { SharedAccessPolicy } from "./fdh_shared_access_types.ts";
import type { SharedMcpWorkspaceBinding } from "./fdh_shared_mcp_types.ts";

export interface SharedGoogleBrokerPolicy extends SharedAccessPolicy {
  /** Public Google OAuth client ID, pinned by the deployment. */
  readonly expectedGoogleClientId: string;
}

/** Trusted root IO only: pin Keycloak origin/realm/endpoints and credentials out of
 * band. identity_provider MUST originate in a user-session note, not a user
 * attribute or arbitrary token mapper. Return current, complete JSON records.
 * Revoke pre-hardening sessions and require a fresh Google login after installing
 * this configuration: current settings cannot attest historical login settings.
 * No endpoint, credential, session-note provenance or Google signature is
 * authenticated by this pure adapter. No callback may be supplied by a request. */
export interface SharedGoogleBrokerTrustedIO {
  readonly introspect: (token: string) => Promise<unknown>;
  readonly readUser: (brokerSubject: string) => Promise<unknown>;
  readonly readFederatedIdentities: (brokerSubject: string) => Promise<unknown>;
  readonly readGoogleProviderConfig: () => Promise<unknown>;
  readonly readGoogleProviderMappers: () => Promise<unknown>;
  readonly readUserProfile: () => Promise<unknown>;
  /** Trusted current Unix seconds, checked before and after IO. */
  readonly now: () => number;
}

export type SharedGoogleBrokerResolver = (
  token: unknown,
  access: unknown,
) => Promise<SharedMcpWorkspaceBinding | null>;

export interface GoogleBrokerFixture {
  introspection: Record<string, unknown>;
  user: Record<string, unknown>;
  identities: unknown;
  provider: Record<string, unknown>;
  mappers: unknown;
  profile: Record<string, unknown>;
}
