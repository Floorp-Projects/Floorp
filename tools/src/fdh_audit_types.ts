// SPDX-License-Identifier: MPL-2.0
import type { ReviewVerdict } from "./fdh_review.ts";

export type AuditTarget =
  | {
    kind: "pull-request";
    repo: "Floorp" | "Floorp-Runtime";
    pr: number;
    head: string;
  }
  | { kind: "jira-issue"; key: string }
  | {
    kind: "ci-run";
    repo: "Floorp" | "Floorp-Runtime";
    run: number;
    head: string;
  };

export interface AuditEvent {
  id: string;
  traceId: string;
  operation:
    | "review"
    | "evidence"
    | "qa"
    | "github-change"
    | "jira-change"
    | "release";
  target: AuditTarget;
  agentRunId?: string;
  toolRunId?: string;
  runnerRunId?: string;
  outcome:
    | { kind: "intent" }
    | {
      kind: "result";
      intentId: string;
      status: "succeeded" | "failed" | "denied";
      artifactSha256?: string;
    }
    | {
      kind: "receipt";
      artifactSha256: string;
      reviewVerdict?: ReviewVerdict;
    };
}

export type AuditDenial =
  | "unauthenticated"
  | "unauthorized"
  | "invalid-input"
  | "conflict";

export interface AuditCheckpoint {
  sequence: number;
  mac: string;
}

export interface AuditRecord extends AuditCheckpoint {
  receivedAt: string;
  actor: string | null;
  event: AuditEvent | { id: string; denial: AuditDenial };
}
