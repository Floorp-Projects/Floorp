// SPDX-License-Identifier: MPL-2.0
import type { MessagingCandidateWindow } from "./native-messaging-candidate-types.ts";
export interface OwnedWebRequestWindow extends MessagingCandidateWindow {
  wrappedJSObject: { __floorpRuntimeFixtureURL?: string };
}
export type OwnedWebRequestKind = "ordinary-document" | "classic" | "module";
export interface OwnedWebRequestRecord {
  name: string;
  url: string;
  requestId: string;
  method: string;
  type: string;
  timeStamp: number;
  observedAt: number;
  statusCode: number | null;
  error: string | null;
}
export interface OwnedWebRequestObservation {
  phase: string;
  kind: OwnedWebRequestKind;
  token: string;
  runtimeId: string;
  isWorker: boolean;
  hasDocument: boolean;
  importWitness: string;
  url: string;
  records: OwnedWebRequestRecord[];
  cleanupErrors: { name: string; error: string }[];
  invalidAdds: { kind: string; error: string | null; registered: boolean }[];
  registrations: { name: string; registered: boolean }[];
  removals: { name: string; before: boolean | null; after: boolean | null }[];
  response?: { status: number; url: string; redirected: boolean; body: string };
  error?: string;
}
export interface OwnedWebRequestCase {
  kind: OwnedWebRequestKind;
  addonId: string;
  expectedRuntimeId: string;
  backgroundViewType?: string;
  observation?: OwnedWebRequestObservation;
  passed: boolean;
  error?: string;
  cleanupErrors: string[];
}
export interface OwnedWebRequestReport {
  passed: boolean;
  runId: string;
  fixtureURL: string;
  cases: OwnedWebRequestCase[];
  error?: string;
}
