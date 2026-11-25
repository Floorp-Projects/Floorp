// SPDX-License-Identifier: MPL-2.0
/** Operator configuration only. Google client secret belongs to the AS, not FDH. */
export interface SharedServiceSettings {
  readonly publicOrigin: string;
  readonly issuer: string;
  readonly gatewayLoginUrl: string;
  readonly expectedGoogleClientId: string;
  readonly readerClientId: string;
  readonly readerSecretEnv: string;
}
export interface SharedServicePublicSettings {
  readonly mode: "configured";
  readonly publicOrigin: string;
  readonly loginPath: "/auth/login";
  readonly mcpResource: string;
  readonly apiAuthStatusPath: "/api/auth/status";
  readonly productionQualified: false;
  readonly protectedOperationsAllowed: false;
}
export interface SharedServiceConfiguration {
  readonly mode: "configured";
  readonly public: SharedServicePublicSettings;
  readonly server: Readonly<SharedServiceSettings>;
}
export interface UnconfiguredSharedService {
  readonly mode: "unconfigured";
  readonly loginAvailable: false;
  readonly productionQualified: false;
  readonly protectedOperationsAllowed: false;
}
export type SharedServiceConfig =
  | SharedServiceConfiguration
  | UnconfiguredSharedService;
