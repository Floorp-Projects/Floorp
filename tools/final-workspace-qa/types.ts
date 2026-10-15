// SPDX-License-Identifier: MPL-2.0

export type TcpDial = (options: Deno.ConnectOptions) => Promise<Deno.TcpConn>;

export interface PilotCase {
  id: string;
  name: string;
  minimumAssertions: number;
}

export interface AssertionEvent {
  message: string;
  passed: boolean;
}

export interface CaseResult {
  id: string;
  name: string;
  status: "PASS" | "FAIL" | "NOT_RUN";
  assertions: AssertionEvent[];
  originalError: string | null;
  error: string | null;
}

export interface BrowserIdentity {
  appBuildID: string;
  platformBuildID: string;
  version: string;
  coreResource: string;
  resolvedCoreURL: string;
  startupMode: string;
  httpLoaderAllowed: boolean;
  workspaceAPI: boolean;
}

export interface BrowserResult {
  schema: "qa3-final-workspace-browser-v1";
  nonce: string;
  bootstrap: BrowserIdentity;
  cases: CaseResult[];
}

export interface PilotRequest {
  schema: "qa3-final-workspace-request-v1";
  controllerHead: string;
  executionRun: number;
  executionAttempt: 1;
  floorpHead: string;
  bundleRun: number;
  bundleAttempt: 1;
  bundleArtifact: { id: number; name: string; digest: string };
  runtimeHead: string;
  runtimeRun: number;
  runtimeAttempt: 1;
  target: "linux-x86_64";
}

export interface ProductSubject {
  request: PilotRequest;
  manifestSetId: string;
  member: { name: string; size: number; sha512: string };
  expectedAppBuildID: string;
  expectedPlatformBuildID: string;
  expectedBuildID2: string;
  expectedFirefoxVersion: string;
  compileProfileQualification: "UNVERIFIED";
  publicationAuthorized: false;
}

export interface PreparedProductSubject extends ProductSubject {
  binarySha256: string;
  binaryRelativePath: string;
}
