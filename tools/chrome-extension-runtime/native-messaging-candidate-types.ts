// SPDX-License-Identifier: MPL-2.0
/// <reference types="jsr:@types/gecko" />
import type { NativePortErrorResult } from "./native-port-errors-test-types.ts";
export interface MessagingCandidateConfig {
  hostName: string;
  manifestPath: string;
  hostPath: string;
  trafficPath: string;
  outputPath: string;
  token: string;
  platform: "linux" | "windows" | "mac";
}
export interface MessagingCandidateManifest {
  name: string;
  path: string;
  allowed_origins: string[];
}
export interface MessagingCandidateAddon {
  uninstall(): Promise<void>;
}
export interface MessagingCandidateAddonManager {
  installTemporaryAddon(file: nsIFile): Promise<MessagingCandidateAddon>;
}
export interface MessagingCandidateExtension {
  baseURI: nsIURI;
  principal: nsIPrincipal;
  backgroundState: string;
  backgroundContext: { viewType: string } | null;
}
export interface MessagingCandidateParent {
  GlobalManager: {
    extensionMap: Map<string, MessagingCandidateExtension>;
  };
}
export interface MessagingCandidateBrowser {
  contentTitle: string;
  messageManager: {
    loadFrameScript(source: string, delayed: boolean): void;
  };
}
export interface MessagingCandidateTab {
  linkedBrowser: MessagingCandidateBrowser;
  isConnected: boolean;
}
export interface MessagingCandidateWindow extends Window {
  gBrowser: {
    addTab(
      url: string,
      options: { triggeringPrincipal: nsIPrincipal },
    ): MessagingCandidateTab;
    removeTab(tab: MessagingCandidateTab): void;
  };
}
export interface MessagingCandidatePayload {
  token: string;
  context: "document" | "worker";
  step: string;
  unicode: string;
  nested: { values: (string | number | boolean | null)[] };
}
export interface MessagingCandidateHostReply {
  pid: number;
  serial: number;
  argv: string[];
  message: MessagingCandidatePayload;
}
export interface MessagingCandidateSuccess {
  context: "document" | "worker";
  isWorker: boolean;
  runtimeId: string;
  oneShot: MessagingCandidateHostReply;
  connected: MessagingCandidateHostReply[];
  normalPorts: NativePortErrorResult[];
}
export interface MessagingCandidateDenial {
  oneShotError: string;
  connectError: string;
  disconnects: NativePortErrorResult[];
}
export interface MessagingCandidateResponse {
  requestId: string;
  result?: MessagingCandidateSuccess | MessagingCandidateDenial;
  error?: string;
}
export interface MessagingCandidateHostEvent {
  event: "start" | "receive" | "send" | "exit";
  pid: number;
  argv?: string[];
  serial?: number;
  message?: MessagingCandidatePayload;
  bytes?: number;
  messages?: number;
  clean?: boolean;
  error?: string | null;
}
export interface MessagingCandidateEvidence {
  stage: "authored-actual-native-messaging-api-gate";
  passed: boolean;
  token: string;
  runId: string;
  platform: MessagingCandidateConfig["platform"];
  manifestPath: string;
  hostPath: string;
  allowedOriginalId: string;
  allowedOrigin: string;
  successes: MessagingCandidateSuccess[];
  denials: { caller: string; result: MessagingCandidateDenial }[];
  hostEvents: MessagingCandidateHostEvent[];
  error?: string;
}
