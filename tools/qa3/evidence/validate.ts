// SPDX-License-Identifier: MPL-2.0
/**
 * EvidenceManifest v0 validator (TypeScript mirror of `validate.py`).
 *
 * The JSON Schema file next to this module is the contract; this module adds
 * the checks a JSON Schema cannot express (unknown schemaVersion, the
 * state-transition table, cancel_requested agreement, old-epoch rejection, ID
 * issuer namespaces, entity kinds, relationship endpoints, operation order and
 * required limitation markers). Error codes are identical to the Python side so
 * Scheduler and Runner reject the same fixtures with the same reason.
 */

import type {
  Entity,
  EvidenceManifest,
  StateTransitionTable,
  ValidationError,
  ValidationErrorCode,
  ValidationOptions,
  ValidationResult,
} from "./types.ts";

export const SCHEMA_VERSION = 1;
export const UNVERIFIED = "UNVERIFIED";

export const ISSUER_NAMESPACE: Record<string, string> = {
  "scheduler": "scheduler:",
  "executor": "executor:",
  "host": "host:",
  "host-authorization": "host-authorization:",
};

export const ENTITY_KINDS = [
  "task",
  "job",
  "run",
  "attempt",
  "session",
  "workspace",
  "approval",
] as const;

export const CORE_RELATIONSHIPS = [
  "task-has-job",
  "job-has-run",
  "run-has-attempt",
  "run-uses-session",
  "run-in-workspace",
] as const;

/** Limitation substrings that must accompany an explicit absence in the record. */
export const LIMITATION_MARKERS: Record<string, string> = {
  "dsh-pin": "DSH",
  "commit": "commit",
  "arch": "arch",
  "ownership": "ownership",
  "visual": "visual",
  "timestamp": "timestamp",
};

const SCHEMA_URL = new URL("./evidence-manifest.schema.json", import.meta.url);
const TRANSITIONS_URL = new URL("./state-transitions.json", import.meta.url);

type JsonObject = Record<string, unknown>;

let schemaCache: JsonObject | null = null;
let transitionsCache: StateTransitionTable | null = null;

function readJson(url: URL): JsonObject {
  return JSON.parse(Deno.readTextFileSync(url)) as JsonObject;
}

export function loadSchema(): JsonObject {
  if (schemaCache === null) schemaCache = readJson(SCHEMA_URL);
  return schemaCache;
}

export function loadTransitions(): StateTransitionTable {
  if (transitionsCache === null) {
    const data = readJson(TRANSITIONS_URL) as unknown as StateTransitionTable;
    if (data.schemaVersion !== 1) {
      throw new Error("unsupported state-transition table schema");
    }
    transitionsCache = data;
  }
  return transitionsCache;
}

function error(
  code: ValidationErrorCode,
  path: string,
  message: string,
): ValidationError {
  return { code, path: path || "/", message };
}

function resolveRef(ref: string, root: JsonObject): JsonObject {
  if (!ref.startsWith("#/")) throw new Error(`unsupported reference ${ref}`);
  let node: unknown = root;
  for (const raw of ref.slice(2).split("/")) {
    const token = raw.replace(/~1/g, "/").replace(/~0/g, "~");
    if (
      typeof node !== "object" || node === null ||
      !(token in (node as JsonObject))
    ) {
      throw new Error(`unresolvable reference ${ref}`);
    }
    node = (node as JsonObject)[token];
  }
  return node as JsonObject;
}

function typeMatches(value: unknown, expected: string): boolean {
  switch (expected) {
    case "object":
      return typeof value === "object" && value !== null &&
        !Array.isArray(value);
    case "array":
      return Array.isArray(value);
    case "string":
      return typeof value === "string";
    case "boolean":
      return typeof value === "boolean";
    case "integer":
      return typeof value === "number" && Number.isInteger(value);
    case "number":
      return typeof value === "number" && Number.isFinite(value);
    case "null":
      return value === null;
    default:
      return false;
  }
}

function typeOk(value: unknown, expected: unknown): boolean {
  if (Array.isArray(expected)) {
    return expected.some((item) => typeMatches(value, item as string));
  }
  return typeMatches(value, expected as string);
}

function sameValue(left: unknown, right: unknown): boolean {
  if (typeof left === "number" && typeof right === "number") {
    return Number.isNaN(left) && Number.isNaN(right) ? true : left === right;
  }
  return left === right;
}

function check(
  instance: unknown,
  schema: JsonObject,
  root: JsonObject,
  path: string,
  errors: ValidationError[],
): void {
  if (typeof schema !== "object" || schema === null) {
    errors.push(error("schema-violation", path, "unsupported schema node"));
    return;
  }

  if (typeof schema["$ref"] === "string") {
    check(instance, resolveRef(schema["$ref"], root), root, path, errors);
    return;
  }

  if ("type" in schema && !typeOk(instance, schema["type"])) {
    errors.push(
      error(
        "schema-violation",
        path,
        `expected type ${JSON.stringify(schema["type"])}, got ${
          instance === null
            ? "null"
            : Array.isArray(instance)
            ? "array"
            : typeof instance
        }`,
      ),
    );
    return;
  }

  if ("const" in schema && !sameValue(instance, schema["const"])) {
    errors.push(
      error(
        "schema-violation",
        path,
        `must equal ${JSON.stringify(schema["const"])}`,
      ),
    );
  }

  if ("enum" in schema && Array.isArray(schema["enum"])) {
    const allowed = schema["enum"] as unknown[];
    if (!allowed.some((item) => sameValue(instance, item))) {
      errors.push(
        error(
          "schema-violation",
          path,
          `must be one of ${JSON.stringify(allowed)}`,
        ),
      );
    }
  }

  if (typeof instance === "string") {
    if (
      typeof schema["minLength"] === "number" &&
      instance.length < schema["minLength"]
    ) {
      errors.push(
        error(
          "schema-violation",
          path,
          `shorter than ${schema["minLength"]} characters`,
        ),
      );
    }
    if (
      typeof schema["maxLength"] === "number" &&
      instance.length > schema["maxLength"]
    ) {
      errors.push(
        error(
          "schema-violation",
          path,
          `longer than ${schema["maxLength"]} characters`,
        ),
      );
    }
    if (typeof schema["pattern"] === "string") {
      if (!new RegExp(schema["pattern"]).test(instance)) {
        errors.push(
          error(
            "schema-violation",
            path,
            `does not match ${schema["pattern"]}`,
          ),
        );
      }
    }
  }

  if (typeof instance === "number") {
    if (typeof schema["minimum"] === "number" && instance < schema["minimum"]) {
      errors.push(
        error("schema-violation", path, `below minimum ${schema["minimum"]}`),
      );
    }
    if (typeof schema["maximum"] === "number" && instance > schema["maximum"]) {
      errors.push(
        error("schema-violation", path, `above maximum ${schema["maximum"]}`),
      );
    }
  }

  if (
    typeof instance === "object" && instance !== null &&
    !Array.isArray(instance)
  ) {
    const object = instance as JsonObject;
    const required = Array.isArray(schema["required"])
      ? (schema["required"] as string[])
      : [];
    for (const key of required) {
      if (!(key in object)) {
        errors.push(
          error(
            "missing-required-evidence",
            `${path}/${key}`,
            `required field '${key}' is missing`,
          ),
        );
      }
    }
    const properties = (schema["properties"] ?? {}) as Record<
      string,
      JsonObject
    >;
    for (const [key, value] of Object.entries(object)) {
      if (key in properties) {
        check(value, properties[key], root, `${path}/${key}`, errors);
      } else if (schema["additionalProperties"] === false) {
        errors.push(
          error("schema-violation", `${path}/${key}`, "unexpected field"),
        );
      }
    }
  }

  if (Array.isArray(instance)) {
    if (
      typeof schema["minItems"] === "number" &&
      instance.length < schema["minItems"]
    ) {
      errors.push(
        error(
          "schema-violation",
          path,
          `needs at least ${schema["minItems"]} items`,
        ),
      );
    }
    if (
      typeof schema["maxItems"] === "number" &&
      instance.length > schema["maxItems"]
    ) {
      errors.push(
        error(
          "schema-violation",
          path,
          `allows at most ${schema["maxItems"]} items`,
        ),
      );
    }
    if (typeof schema["items"] === "object" && schema["items"] !== null) {
      const items = schema["items"] as JsonObject;
      instance.forEach((item, index) => {
        check(item, items, root, `${path}/${index}`, errors);
      });
    }
  }

  if (Array.isArray(schema["allOf"])) {
    for (const sub of schema["allOf"] as JsonObject[]) {
      check(instance, sub, root, path, errors);
    }
  }

  if (Array.isArray(schema["anyOf"])) {
    const branches = (schema["anyOf"] as JsonObject[]).map((sub) =>
      subErrors(instance, sub, root, path)
    );
    if (!branches.some((branch) => branch.length === 0)) {
      errors.push(error("schema-violation", path, "no anyOf branch matched"));
    }
  }

  if (Array.isArray(schema["oneOf"])) {
    const branches = (schema["oneOf"] as JsonObject[]).map((sub) =>
      subErrors(instance, sub, root, path)
    );
    const matched = branches.filter((branch) => branch.length === 0).length;
    if (matched !== 1) {
      errors.push(
        error(
          "schema-violation",
          path,
          `oneOf matched ${matched} branches, expected exactly 1`,
        ),
      );
    }
  }
}

function subErrors(
  instance: unknown,
  schema: JsonObject,
  root: JsonObject,
  path: string,
): ValidationError[] {
  const errors: ValidationError[] = [];
  check(instance, schema, root, path, errors);
  return errors;
}

function limitationRequirements(manifest: EvidenceManifest): string[] {
  const requirements: string[] = [];
  const runtime = manifest.runtime ?? ({} as EvidenceManifest["runtime"]);
  const toolchain = runtime.toolchain ??
    ({} as EvidenceManifest["runtime"]["toolchain"]);
  if (toolchain.dsh?.pin === UNVERIFIED) requirements.push("dsh-pin");
  if (runtime.os?.arch === UNVERIFIED) requirements.push("arch");
  if (runtime.build?.commit === null || toolchain.floorp?.commit === null) {
    requirements.push("commit");
  }
  const entities = (manifest.envelope?.entities ?? []) as Entity[];
  if (entities.some((entity) => entity?.owner === UNVERIFIED)) {
    requirements.push("ownership");
  }
  if (manifest.visualJudge === null) requirements.push("visual");
  for (const attempt of manifest.attempts ?? []) {
    if (attempt.startedAt === null || attempt.endedAt === null) {
      requirements.push("timestamp");
      break;
    }
  }
  return requirements;
}

function checkLimitations(
  manifest: EvidenceManifest,
  errors: ValidationError[],
): void {
  const limitations = (manifest.limitations ?? []).map((item) =>
    item.toLowerCase()
  );
  for (const requirement of limitationRequirements(manifest)) {
    const marker = LIMITATION_MARKERS[requirement];
    if (!limitations.some((item) => item.includes(marker.toLowerCase()))) {
      errors.push(
        error(
          "missing-limitation",
          "/limitations",
          `a limitation mentioning '${marker}' is required by this record`,
        ),
      );
    }
  }
}

function semanticChecks(
  manifest: EvidenceManifest,
  currentEpoch: number | undefined,
  errors: ValidationError[],
): void {
  const envelope = manifest.envelope;
  const transitions = loadTransitions();
  const table = transitions.transitions;
  const state = envelope.taskStatus.state;

  if (currentEpoch !== undefined && envelope.epoch < currentEpoch) {
    errors.push(
      error(
        "stale-epoch",
        "/envelope/epoch",
        `report epoch ${envelope.epoch} is older than current epoch ${currentEpoch}`,
      ),
    );
  }

  const step = envelope.stateTransition;
  if (step.to !== state) {
    errors.push(
      error(
        "invalid-transition",
        "/envelope/stateTransition",
        `stateTransition.to '${step.to}' does not equal taskStatus.state '${state}'`,
      ),
    );
  }
  const allowed = table[step.from];
  if (allowed === undefined) {
    errors.push(
      error(
        "invalid-transition",
        "/envelope/stateTransition/from",
        `unknown state '${step.from}'`,
      ),
    );
  } else if (!allowed.includes(step.to)) {
    errors.push(
      error(
        "invalid-transition",
        "/envelope/stateTransition",
        `'${step.from}' -> '${step.to}' is not in the state-transition table`,
      ),
    );
  }

  const bodyCancel = envelope.taskStatus.cancelRequested;
  const envelopeCancel = envelope.cancelRequested;
  const cancelState = transitions.cancelRequestedState;
  if (bodyCancel !== envelopeCancel) {
    errors.push(
      error(
        "cancel-state-mismatch",
        "/envelope/cancelRequested",
        "Envelope cancelRequested disagrees with taskStatus.cancelRequested",
      ),
    );
  }
  if ((state === cancelState) !== bodyCancel) {
    errors.push(
      error(
        "cancel-state-mismatch",
        "/envelope/taskStatus",
        `cancel_requested meaning must hold for state '${state}' with cancelRequested=${bodyCancel}`,
      ),
    );
  }

  const byId = new Map<string, Entity>();
  envelope.entities.forEach((entity, index) => {
    const pointer = `/envelope/entities/${index}`;
    const namespace = ISSUER_NAMESPACE[entity.issuer];
    if (namespace === undefined) {
      errors.push(
        error(
          "invalid-id-namespace",
          `${pointer}/issuer`,
          `unknown issuer '${entity.issuer}'`,
        ),
      );
      return;
    }
    if (!entity.id.startsWith(namespace)) {
      errors.push(
        error(
          "invalid-id-namespace",
          `${pointer}/id`,
          `id '${entity.id}' is not in the '${namespace}' namespace of issuer '${entity.issuer}'`,
        ),
      );
    }
    if (byId.has(entity.id)) {
      errors.push(
        error(
          "duplicate-entity-id",
          `${pointer}/id`,
          `duplicate entity id '${entity.id}'`,
        ),
      );
    }
    byId.set(entity.id, entity);
  });

  const presentKinds = new Set(envelope.entities.map((entity) => entity.kind));
  for (const kind of ENTITY_KINDS) {
    if (!presentKinds.has(kind)) {
      errors.push(
        error(
          "missing-entity",
          "/envelope/entities",
          `entity kind '${kind}' is required by the Envelope`,
        ),
      );
    }
  }

  const relationshipTypes = new Set<string>();
  envelope.relationships.forEach((rel, index) => {
    const pointer = `/envelope/relationships/${index}`;
    relationshipTypes.add(rel.type);
    for (const side of ["from", "to"] as const) {
      if (!byId.has(rel[side])) {
        errors.push(
          error(
            "unknown-entity-reference",
            `${pointer}/${side}`,
            `unknown entity id '${rel[side]}'`,
          ),
        );
      }
    }
  });
  for (const required of CORE_RELATIONSHIPS) {
    if (!relationshipTypes.has(required)) {
      errors.push(
        error(
          "missing-relationship",
          "/envelope/relationships",
          `relationship '${required}' is required by the Envelope`,
        ),
      );
    }
  }

  manifest.attempts.forEach((attempt, index) => {
    if (!byId.has(attempt.run)) {
      errors.push(
        error(
          "unknown-entity-reference",
          `/attempts/${index}/run`,
          `unknown run id '${attempt.run}'`,
        ),
      );
    }
  });

  const orders = manifest.operations.map((operation) => operation.order);
  const expected = orders.map((_, index) => index + 1);
  const sorted = [...orders].sort((a, b) => a - b);
  if (sorted.some((value, index) => value !== expected[index])) {
    errors.push(
      error(
        "invalid-operation-order",
        "/operations",
        `operation order must be unique and 1..${orders.length}, got ${
          JSON.stringify(orders)
        }`,
      ),
    );
  }

  checkLimitations(manifest, errors);
}

/**
 * Validate one EvidenceManifest document.
 *
 * `role: "scheduler"` mirrors central intake: the current epoch must be known,
 * otherwise the report is rejected with `missing-current-epoch`. The Runner
 * validates without a central epoch (ADR 1.2: an old Runner's external writes
 * are not stopped by central rejection).
 */
export function validateManifest(
  value: unknown,
  options: ValidationOptions = {},
): ValidationError[] {
  const { currentEpoch, role = "runner" } = options;

  if (typeof value !== "object" || value === null || Array.isArray(value)) {
    return [error("schema-violation", "/", "manifest must be an object")];
  }
  const manifest = value as unknown as EvidenceManifest & JsonObject;

  if (!("schemaVersion" in manifest)) {
    return [
      error(
        "missing-required-evidence",
        "/schemaVersion",
        "required field 'schemaVersion' is missing",
      ),
    ];
  }
  if (manifest.schemaVersion !== SCHEMA_VERSION) {
    return [
      error(
        "unsupported-schema-version",
        "/schemaVersion",
        `unsupported schemaVersion ${
          JSON.stringify(manifest.schemaVersion)
        }; expected ${SCHEMA_VERSION}`,
      ),
    ];
  }

  if (role === "scheduler" && currentEpoch === undefined) {
    return [
      error(
        "missing-current-epoch",
        "/envelope/epoch",
        "central intake requires the current epoch",
      ),
    ];
  }

  const schema = loadSchema();
  const errors: ValidationError[] = [];
  check(manifest, schema, schema, "", errors);
  if (errors.length > 0) return errors;

  semanticChecks(manifest, currentEpoch, errors);
  return errors;
}

/** Central (Scheduler) intake: same rules plus the current epoch requirement. */
export function validateReportAsScheduler(
  value: unknown,
  currentEpoch: number | undefined,
): ValidationError[] {
  return validateManifest(value, { currentEpoch, role: "scheduler" });
}

export function validate(
  value: unknown,
  options: ValidationOptions = {},
): ValidationResult {
  const errors = validateManifest(value, options);
  return { ok: errors.length === 0, errors };
}
