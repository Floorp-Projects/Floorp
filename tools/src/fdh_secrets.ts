// SPDX-License-Identifier: MPL-2.0

/**
 * fdh secret-reference denial (FDH Gate 4 4-A, KAN-108).
 *
 * An agent's task state and evidence manifest must never reference repository
 * secrets (`.secrets` or the token names it holds). `fdh evidence` refuses to
 * build a manifest from a state that references them, and `fdh secrets check`
 * exposes the same check. fdh itself never reads `.secrets`; only the task
 * records are scanned, so the check is local-only and deterministic.
 */

import { type FdhTaskState, readTaskState } from "./fdh_task.ts";

/** Repository secret file name that evidence must never reference. */
export const SECRETS_FILE = ".secrets";

/** Token names the repository `.secrets` file holds. */
export const SECRET_TOKEN_NAMES = [
  "SIGNPATH_API_TOKEN",
  "APPLE_DEVELOPER_P12_BASE64",
  "APPLE_DEVELOPER_P12_PASSWORD",
  "APPLE_DEVELOPER_API_KEY_JSON",
] as const;

/** A secret reference found in a task record. */
export interface SecretReference {
  /** JSON path of the string that contains the reference. */
  where: string;
  /** Matched secret marker (`\.secrets` or a token name). */
  marker: string;
}

const SECRET_REFERENCE_RE = new RegExp(
  `(?:\\.secrets\\b|\\b(?:${SECRET_TOKEN_NAMES.join("|")})\\b)`,
);

function scanStrings(
  value: unknown,
  where: string,
  found: SecretReference[],
): void {
  if (typeof value === "string") {
    const match = value.match(SECRET_REFERENCE_RE);
    if (match) found.push({ where, marker: match[0] });
    return;
  }
  if (Array.isArray(value)) {
    value.forEach((item, index) =>
      scanStrings(item, `${where}[${index}]`, found)
    );
    return;
  }
  if (typeof value === "object" && value !== null) {
    for (const [key, item] of Object.entries(value)) {
      scanStrings(item, `${where}.${key}`, found);
    }
  }
}

/** Find every secret reference in a task state (or any JSON value). */
export function findSecretReferences(value: unknown): SecretReference[] {
  const found: SecretReference[] = [];
  scanStrings(value, "$", found);
  return found;
}

/** Throw when a task state references repository secrets. */
export function assertNoSecretReferences(state: FdhTaskState): void {
  const references = findSecretReferences(state);
  if (references.length === 0) return;
  const shown = references
    .slice(0, 5)
    .map((reference) => `${reference.where} -> ${reference.marker}`)
    .join("; ");
  throw new Error(
    `secrets reference denied: task ${state.taskId} references ${references.length} secret marker(s) (${shown})`,
  );
}

/** Read a task state and report its secret references without reading `.secrets`. */
export function checkTaskSecrets(
  worktree: string,
  taskId: string,
): {
  taskId: string;
  denied: boolean;
  references: SecretReference[];
  secretsFileRead: false;
} {
  const state = readTaskState(worktree, taskId);
  const references = findSecretReferences(state);
  return {
    taskId,
    denied: references.length > 0,
    references,
    secretsFileRead: false,
  };
}
