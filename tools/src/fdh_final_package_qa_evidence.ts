// SPDX-License-Identifier: MPL-2.0
import {
  QA_ARCHIVE_LIMITS,
  verifyFinalPackageQaArchive,
} from "./fdh_final_package_qa_archive.ts";
import { validateFinalPackageQaConsistency } from "./fdh_final_package_qa_gate.ts";
import type {
  FinalPackageQaExpectation,
  FinalQaTarget,
} from "./fdh_final_package_qa_gate_types.ts";
import type {
  EvidenceByteSnapshot,
  FinalPackageQaEvidenceRequest,
  FinalPackageQaEvidenceResult,
  FinalPackageQaEvidenceScope,
} from "./fdh_final_package_qa_evidence_types.ts";

const MIB = 1024 * 1024;
/** Byte/depth budgets, NOT heap bounds. Native/parsed object overhead remains.
 * Four sequential archive calls each cap expansion at 16MiB: <=64MiB aggregate.
 * Metadata accepts only canonical pretty-two-space JSON with optional final LF. */
export const QA_EVIDENCE_LIMITS = Object.freeze({
  metadataBytes: MIB,
  totalMetadataBytes: 2 * MIB,
  jsonDepth: 64,
  compressedBytes: 16 * MIB,
  archives: 4,
  packageArtifacts: 64,
  expandedBytes: 64 * MIB,
});
const TARGETS: readonly FinalQaTarget[] = [
  "windows/x86_64",
  "linux/x86_64",
  "mac/x86_64",
  "mac/aarch64",
];
const SCOPE: FinalPackageQaEvidenceScope = Object.freeze({
  scope: "offline-archive-integrity-and-attestation-consistency-only",
  transportAuthenticityVerified: false,
  apiAuthenticityVerified: false,
  nativeQaVerified: false,
  liveApprovalVerified: false,
  publicationAuthorized: false,
});
const TA = Object.getPrototypeOf(Uint8Array.prototype);
const KIND = Object.getOwnPropertyDescriptor(TA, Symbol.toStringTag)!.get!;
const LENGTH = Object.getOwnPropertyDescriptor(TA, "length")!.get!;
const BUFFER = Object.getOwnPropertyDescriptor(TA, "buffer")!.get!;
const RESIZABLE = Object.getOwnPropertyDescriptor(
  ArrayBuffer.prototype,
  "resizable",
)!.get!;
const SET = Uint8Array.prototype.set;
const PARSE = JSON.parse, STRINGIFY = JSON.stringify;
function requireValue(value: unknown): asserts value {
  if (!value) throw new Error("invalid offline input");
}
function record(value: unknown): Record<string, unknown> {
  requireValue(
    typeof value === "object" && value !== null && !Array.isArray(value),
  );
  return value as Record<string, unknown>;
}
/** Accessors are rejected without invocation. No custom iterator is consumed. */
function fields(
  value: unknown,
  keys: readonly string[],
): Record<string, unknown> {
  requireValue(typeof value === "object" && value !== null);
  requireValue(Reflect.ownKeys(value).length === keys.length);
  const result: Record<string, unknown> = Object.create(null);
  for (const key of keys) {
    const d = Object.getOwnPropertyDescriptor(value, key);
    requireValue(d && "value" in d);
    result[key] = d.value;
  }
  return result;
}
function four(value: unknown): unknown[] {
  requireValue(Array.isArray(value));
  const f = fields(value, ["0", "1", "2", "3", "length"]);
  requireValue(f.length === 4);
  return [f[0], f[1], f[2], f[3]];
}
function metadata(text: unknown): { text: string; size: number } {
  requireValue(
    typeof text === "string" && text.length <= QA_EVIDENCE_LIMITS.metadataBytes,
  );
  const size = new TextEncoder().encode(text).length;
  requireValue(size <= QA_EVIDENCE_LIMITS.metadataBytes);
  return { text, size };
}
function parse(text: string): Record<string, unknown> {
  let depth = 0, quoted = false, escaped = false;
  for (const c of text) {
    if (quoted) {
      if (escaped) escaped = false;
      else if (c === "\\") escaped = true;
      else if (c === '"') quoted = false;
    } else if (c === '"') quoted = true;
    else if (c === "{" || c === "[") {
      requireValue(++depth <= QA_EVIDENCE_LIMITS.jsonDepth);
    } else if (c === "}" || c === "]") requireValue(--depth >= 0);
  }
  requireValue(!depth && !quoted);
  const value = record(PARSE(text));
  const canonical = STRINGIFY(value, null, 2);
  requireValue(text === canonical || text === canonical + "\n");
  return value;
}
function byteInput(artifactId: unknown, value: unknown): EvidenceByteSnapshot {
  // Internal-slot check MUST precede prototype/properties: forwarding Proxies
  // cannot be normalized into arrays before resource validation.
  requireValue(
    ArrayBuffer.isView(value) &&
      Reflect.apply(KIND, value, []) === "Uint8Array",
  );
  requireValue(Object.getPrototypeOf(value) === Uint8Array.prototype);
  requireValue(
    !["buffer", "length", "byteLength", "byteOffset"].some((k) =>
      Object.hasOwn(value, k)
    ),
  );
  const backing: unknown = Reflect.apply(BUFFER, value, []);
  requireValue(
    backing instanceof ArrayBuffer &&
      Object.getPrototypeOf(backing) === ArrayBuffer.prototype,
  );
  requireValue(
    !Object.hasOwn(backing, "resizable") &&
      !Reflect.apply(RESIZABLE, backing, []),
  );
  const length: number = Reflect.apply(LENGTH, value, []);
  requireValue(length >= 22 && length <= QA_ARCHIVE_LIMITS.archiveBytes);
  requireValue(
    typeof artifactId === "number" && Number.isSafeInteger(artifactId) &&
      artifactId > 0,
  );
  return { artifactId, bytes: value as Uint8Array, length };
}
/** Pure offline composition. No filesystem, network, collector, native QA,
 * live approval or publisher operation; no caller-decoded evidence accepted.
 * All policy/metadata/byte copies are private before the first await. */
export async function verifyFinalPackageQaEvidence(
  request: FinalPackageQaEvidenceRequest,
): Promise<FinalPackageQaEvidenceResult> {
  let stage: "snapshot" | "mapping" | "archive" | "consistency" = "snapshot";
  try {
    const input = fields(request, [
      "expectedJson",
      "observationJson",
      "archives",
    ]);
    const expectedText = metadata(input.expectedJson),
      observedText = metadata(input.observationJson);
    requireValue(
      expectedText.size + observedText.size <=
        QA_EVIDENCE_LIMITS.totalMetadataBytes,
    );
    const descriptors = four(input.archives).map((v) =>
      fields(v, ["artifactId", "bytes"])
    );
    const validated = descriptors.map((v) => byteInput(v.artifactId, v.bytes));
    requireValue(
      validated.reduce((n, v) => n + v.length, 0) <=
        QA_EVIDENCE_LIMITS.compressedBytes,
    );
    const snapshots = validated.map((v) => {
      const bytes = new Uint8Array(v.length);
      Reflect.apply(SET, bytes, [v.bytes]);
      return { artifactId: v.artifactId, bytes };
    });
    const policy = parse(expectedText.text),
      observed = parse(observedText.text);
    fields(observed, [
      "packageRun",
      "qaRun",
      "qaJobs",
      "packageArtifacts",
      "qaArtifacts",
    ]);
    fields(policy, [
      "repository",
      "packageRun",
      "qaRun",
      "bundle",
      "targets",
      "now",
      "maxAgeMs",
      "windowsPolicy",
    ]);
    fields(policy.targets, TARGETS);
    requireValue(
      Array.isArray(observed.qaJobs) && observed.qaJobs.length === 4,
    );
    requireValue(
      Array.isArray(observed.qaArtifacts) && observed.qaArtifacts.length === 4,
    );
    requireValue(
      Array.isArray(observed.packageArtifacts) &&
        observed.packageArtifacts.length >= 1 &&
        observed.packageArtifacts.length <= QA_EVIDENCE_LIMITS.packageArtifacts,
    );
    stage = "mapping";
    const expected = policy as unknown as FinalPackageQaExpectation;
    const ids = new Set<number>(), names = new Set<string>();
    const selections = TARGETS.map((target) => {
      const identity = record(record(record(policy.targets)[target]).evidence);
      requireValue(
        typeof identity.id === "number" && Number.isSafeInteger(identity.id) &&
          identity.id > 0,
      );
      requireValue(
        typeof identity.name === "string" &&
          /^[A-Za-z0-9_.-]{1,240}$/.test(identity.name),
      );
      requireValue(
        typeof identity.digest === "string" &&
          /^sha256:[0-9a-f]{64}$/.test(identity.digest),
      );
      requireValue(!ids.has(identity.id) && !names.has(identity.name));
      ids.add(identity.id);
      names.add(identity.name);
      const matches = (observed.qaArtifacts as unknown[]).map(record).filter(
        (v) => v.id === identity.id,
      );
      requireValue(
        matches.length === 1 && matches[0].name === identity.name &&
          matches[0].digest === identity.digest,
      );
      const archives = snapshots.filter((v) => v.artifactId === identity.id);
      requireValue(archives.length === 1);
      return {
        target,
        identity: {
          id: identity.id,
          name: identity.name,
          digest: identity.digest,
        },
        bytes: archives[0].bytes,
      };
    });
    requireValue(new Set(snapshots.map((v) => v.artifactId)).size === 4);
    stage = "archive";
    const evidence = [], summaries = [];
    for (const selection of selections) {
      const result = await verifyFinalPackageQaArchive({
        bytes: selection.bytes,
        artifact: selection.identity,
        expected: selection.identity,
      });
      requireValue(result.archiveIntegrityMatched);
      evidence.push({
        artifactId: result.artifact.id,
        observedArchiveDigest: result.artifact.digest,
        entries: [{ name: "input.json", value: result.input }, {
          name: "source-after-qa.json",
          value: result.sourceAfterQa,
        }],
      });
      summaries.push({
        target: selection.target,
        ...result.artifact,
        entryCount: result.entryCount,
        expandedBytes: result.expandedBytes,
      });
    }
    stage = "consistency";
    const consistency = validateFinalPackageQaConsistency(expected, {
      ...observed,
      evidence,
    });
    return {
      ...SCOPE,
      offlineEvidenceMatched: true,
      offlineArchiveIntegrity: {
        archiveIntegrityMatched: true,
        artifacts: summaries,
      },
      attestationConsistency: consistency,
    };
  } catch {
    // Never expose attacker messages, payloads or partial successes.
    return { ...SCOPE, offlineEvidenceMatched: false, stage };
  }
}
