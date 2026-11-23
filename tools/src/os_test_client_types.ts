// SPDX-License-Identifier: MPL-2.0

export type OsTestClientOptions = {
  baseUrl: string;
  tokenFile?: string;
};

export type OsTestClientDependencies = {
  readTokenFile?: (path: string, maxBytes: number) => Promise<Uint8Array>;
  fetch?: typeof fetch;
};

export type OsTestClient = {
  baseUrl: string;
  request: (path: string, init?: RequestInit, stream?: boolean) => Promise<{
    status: number;
    body: unknown;
  }>;
};

export type OsTestClientArgs = {
  wrapperArgs: string[];
  verifierArgs: string[];
  baseUrlFlag?: string;
  tokenFileFlag?: string;
};
