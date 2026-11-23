// SPDX-License-Identifier: MPL-2.0
/// <reference lib="deno.ns" />

import type { M3dPackageHttpsOriginKind } from "./m3d_package_https_fixture.types.ts";

export interface Identity {
  keyPem: string;
  certPem: string;
}

export interface ParsedRequest {
  method: string;
  target: string;
  headers: Map<string, string[]>;
}

export interface Stall {
  connection: Deno.TlsConn;
  remainder: Uint8Array;
  resolve(): void;
  peerClosed: Promise<void>;
  peerTask: Promise<void>;
  task: Promise<void>;
  releaseRequested: boolean;
  cancelled: boolean;
}

export type RequestHandler = (
  origin: M3dPackageHttpsOriginKind,
  request: ParsedRequest,
  connection: Deno.TlsConn,
) => Promise<void>;
