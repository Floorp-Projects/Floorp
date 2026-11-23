// SPDX-License-Identifier: MPL-2.0
import type {
  SharedServicePublicSettings,
  UnconfiguredSharedService,
} from "./fdh_shared_config_types.ts";

/** Public bootstrap is an allowlist, never the server configuration. */
export type SharedEntryBootstrap =
  | (SharedServicePublicSettings & { readonly loginAvailable: true })
  | UnconfiguredSharedService;

export type SharedEntryHandler = (request: Request) => Promise<Response>;
