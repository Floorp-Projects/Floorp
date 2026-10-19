// SPDX-License-Identifier: MPL-2.0
import type {
  MessagingCandidateAddon,
  MessagingCandidateTab,
} from "./native-messaging-candidate-types.ts";
export type MessageRecord = Record<string, unknown>;
export type MessageRealm = "document" | "worker";
export interface MessageJSONConfig {
  outputPath: string;
  token: string;
}
export interface MessageGroup {
  name: string;
  expectedCount: number;
  observations: MessageRecord[];
  failures: string[];
}
export interface MessageJSONEvidence {
  stage: "authored-actual-native-message-json-overloads";
  token: string;
  runId: string;
  passed: boolean;
  groups: MessageGroup[];
  cleanupErrors: string[];
  error?: string;
}
export interface MessageControlReply {
  requestId: string;
  value?: unknown;
  error?: string;
}
export interface MessageJSONReference {
  senderRows: MessageRecord[];
  responseRows: MessageRecord[];
  senderCount: number;
  responseCount: number;
}
export interface MessageOverloadReference {
  extensionId: string;
  cases: MessageRecord[];
  missingReceiverCases: MessageRecord[];
}
export interface MessageEdgeReference {
  extensionId: string;
  responses: MessageRecord[];
  options: MessageRecord[];
  ports: MessageRecord[];
}

export interface MessageResource {
  packagePath: string;
  addon?: MessagingCandidateAddon;
  tab?: MessagingCandidateTab;
  active: boolean;
}
