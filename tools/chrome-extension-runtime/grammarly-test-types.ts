// SPDX-License-Identifier: MPL-2.0
// Evidence contracts for the immutable Grammarly package and native CDP observations.
import type { Protocol } from "devtools-protocol/types/protocol.d.ts";
import type { ProtocolMapping } from "devtools-protocol/types/protocol-mapping.d.ts";
import type { CDPResult } from "./cdp-types.ts";
import type { FrozenCorpusExtension } from "./corpus-types.ts";

export interface GrammarlyCorpus {
  extensions: FrozenCorpusExtension[];
}
export interface GrammarlyReference {
  browserVersion: string;
  binary: { sha256: string };
}
export interface PrivateChromeTrust {
  wrapper: string;
  publicRootsRetained: string[];
  authoredCaImported: boolean;
  hostTrustModified: false;
  certificateValidationDisabled: false;
}
export interface NativeRect {
  x: number;
  y: number;
  width: number;
  height: number;
  top: number;
  left: number;
  right: number;
  bottom: number;
}
export interface GrammarlySnapshot {
  url: string;
  ready: string;
  text: string;
  writing?: string;
  productNodes: { tag: string; rect: NativeRect }[];
  buttons: {
    text: string;
    aria: string | null;
    title: string;
    rect: NativeRect;
  }[];
}
export interface GrammarlyContentControl {
  tag: string;
  role: string | null;
  aria: string | null;
  title: string | null;
  className: string | null;
  text: string;
  rect: NativeRect;
}
export interface GrammarlyEvidence {
  id: string;
  name: string;
  version?: string;
  packageSha256?: string;
  result: string;
  primaryOperationResult: string;
  unchangedScriptCount?: number;
  unchangedScriptsSha256?: string;
  afterNativeInput?: GrammarlySnapshot;
  afterProductInitialization?: GrammarlySnapshot;
  nativeWorker?: Protocol.Target.TargetInfo;
  afterCheckRequest?: GrammarlySnapshot;
  contentControls?: GrammarlyContentControl[];
  nativePopup?: Protocol.Target.TargetInfo;
  popup?: GrammarlySnapshot;
  blockers?: string[];
  observedAccountUI?: string;
  primaryOperationExplanation?: string;
  error?: string;
}
export interface GrammarlyReport {
  stage: string;
  reference: GrammarlyReference;
  displayMode: string;
  fixture: { url: string; sha256: string; authoredText: string };
  network: {
    trust: PrivateChromeTrust;
    proxy: { protocol: string; hostname: string; port: string | number };
    bypass: string;
  };
  extensions: GrammarlyEvidence[];
  primaryOperationPassRate: null;
  output: string;
  version?: CDPResult<"Browser.getVersion">;
  nativeResponses?: { url: string; status: number; mimeType: string }[];
  nativeWebSockets?: {
    method: string;
    url?: string;
    status?: number;
    errorMessage?: string;
  }[];
  nativeRequestFailures?: {
    errorText: string;
    type?: Protocol.Network.ResourceType;
  }[];
}
export type NativeEvent<Method extends keyof ProtocolMapping.Events> = {
  method: Method;
  params: ProtocolMapping.Events[Method][0];
  sessionId?: string;
};
export type NativeWebSocketEvent = {
  [
    Method in Extract<
      keyof ProtocolMapping.Events,
      `Network.webSocket${string}`
    >
  ]: NativeEvent<Method>;
}[Extract<keyof ProtocolMapping.Events, `Network.webSocket${string}`>];
