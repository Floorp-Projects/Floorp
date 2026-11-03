// SPDX-License-Identifier: MPL-2.0
export interface QuietLifetimeConfig {
  outputPath: string;
  hostName: string;
  manifestPath: string;
  hostPath: string;
  trafficPath: string;
  token: string;
  platform: "linux" | "windows" | "mac";
  quietMilliseconds: 36000;
  expectedBuildId: string;
}
export interface QuietHostReply {
  pid: number;
  serial: number;
  argv: string[];
  message: { command: string; requestId: string; workerGeneration: string };
}
export interface QuietWorkerReply {
  workerGeneration: string;
  host?: QuietHostReply;
  error?: string;
  nativePortMissing?: boolean;
}
export interface QuietDocumentState {
  started: boolean;
  controlReady: { kind: string; workerGeneration: string } | null;
  controlDisconnected: boolean;
  controlDisconnectError: string | null;
  controlMessages: unknown[];
  initial: QuietWorkerReply | null;
  quietStartDocumentTime: number;
  documentTime: number;
}
export interface QuietProbeResult {
  passiveBefore: QuietDocumentState;
  response: QuietWorkerReply;
  passiveAfter: QuietDocumentState;
}
export interface QuietDocumentResponse {
  requestId: string;
  value?: QuietDocumentState | QuietProbeResult;
  error?: string;
}
export interface QuietLifetimeCase {
  scriptType: "classic" | "module";
  native: boolean;
  quietStartedAt: number;
  quietFinishedAt?: number;
  start?: QuietDocumentState;
  passiveAfterQuiet?: QuietDocumentState;
  probe?: QuietProbeResult;
  hostTrafficBytesBefore?: number;
  hostTrafficBytesAfter?: number;
  passed: boolean;
  error?: string;
}
export interface QuietLifetimeEvidence {
  stage: "authored-real-native-port-quiet-lifetime";
  token: string;
  runId: string;
  passed: boolean;
  cases: QuietLifetimeCase[];
  cleanupErrors: string[];
  error?: string;
}
