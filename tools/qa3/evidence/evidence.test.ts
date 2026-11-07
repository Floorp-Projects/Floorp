// SPDX-License-Identifier: MPL-2.0
/**
 * EvidenceManifest v0 fixture contract (ADR 8061068 section 1.5).
 *
 * Scheduler (`scheduler.ts`) and Runner (`runner.ts`) consume the same fixture
 * registry through `fixtures.ts`; this suite proves that they accept the same
 * valid fixtures, reject the same invalid fixtures with the same codes, and
 * differ only on the old-epoch rule that ADR 1.2 assigns to the centre.
 *
 * Run with: `deno test --allow-read tools/qa3/evidence/`
 */

import type {
  EvidenceManifest,
  ReproductionResult,
  SchemaVersion,
  ValidationError,
} from "./types.ts";
import {
  loadSchema,
  loadTransitions,
  validateManifest,
  validateReportAsScheduler,
} from "./validate.ts";
import { loadFixtureRegistry, sharedFixturePaths } from "./fixtures.ts";
import * as scheduler from "./scheduler.ts";
import * as runner from "./runner.ts";

function assert(condition: unknown, message: string): asserts condition {
  if (!condition) throw new Error(message);
}

function assertEqual(
  actual: unknown,
  expected: unknown,
  message: string,
): void {
  const left = JSON.stringify(actual);
  const right = JSON.stringify(expected);
  if (left !== right) {
    throw new Error(`${message}\n  actual:   ${left}\n  expected: ${right}`);
  }
}

function codesOf(errors: ValidationError[]): string[] {
  return errors.map((entry) => entry.code);
}

function decisionCodes(
  decisions: { path: string; codes: string[] }[],
  path: string,
): string[] {
  const found = decisions.find((entry) => entry.path === path);
  assert(found, `fixture ${path} was not validated`);
  return found.codes;
}

Deno.test("schema declares every ADR 1.3 evidence element", () => {
  const schema = loadSchema() as {
    $schema: string;
    required: string[];
    definitions: Record<
      string,
      { properties?: Record<string, { enum?: unknown[] }> }
    >;
  };
  assertEqual(
    schema.$schema,
    "http://json-schema.org/draft-07/schema#",
    "draft-07 keeps the schema readable by the repository's JSON tooling",
  );
  assertEqual(
    schema.required,
    [
      "schemaVersion",
      "envelope",
      "runtime",
      "attempts",
      "operations",
      "assertions",
      "environmentFidelity",
      "inputFidelity",
      "visualJudge",
      "cleanup",
      "limitations",
      "reproduction",
    ],
    "ADR 1.3 required elements",
  );
  assertEqual(
    schema.definitions.reproduction.properties?.result?.enum,
    ["reproduced", "not-reproduced", "inconclusive"],
    "reproduction axis",
  );
  assertEqual(
    schema.definitions.visualJudge.properties?.source?.enum,
    ["human", "agent"],
    "visual judge",
  );
});

Deno.test("state-transition table is closed and names cancel_requested", () => {
  const table = loadTransitions();
  assertEqual(
    [...table.states].sort(),
    Object.keys(table.transitions).sort(),
    "every state has a transition row",
  );
  for (const state of table.states) {
    for (const target of table.transitions[state]) {
      assert(
        table.states.includes(target),
        `${state} -> ${target} leaves the table`,
      );
      assert(target !== state, `${state} must not transition to itself`);
    }
  }
  for (const terminal of table.terminalStates) {
    assertEqual(table.transitions[terminal], [], `${terminal} is terminal`);
  }
  assertEqual(table.initialStates, ["pending"], "single initial state");
  assert(
    table.states.includes(table.cancelRequestedState),
    "cancel state exists",
  );
});

Deno.test("Scheduler and Runner consume the same fixture list", () => {
  const registry = loadFixtureRegistry();
  const shared = sharedFixturePaths(registry);
  assertEqual(scheduler.fixturePaths(), shared, "Scheduler fixture list");
  assertEqual(runner.fixturePaths(), shared, "Runner fixture list");
  assertEqual(
    new Set(scheduler.fixturePaths()).size,
    scheduler.fixturePaths().length,
    "no duplicated fixture path",
  );
  assert(shared.length >= 8, "registry carries valid and invalid fixtures");
});

Deno.test("every valid fixture passes on both sides", () => {
  const registry = loadFixtureRegistry();
  for (const relative of registry.valid) {
    const manifest = readManifest(relative);
    assertEqual(
      codesOf(validateManifest(manifest)),
      [],
      `runner rejects valid fixture ${relative}`,
    );
    assertEqual(
      codesOf(validateReportAsScheduler(manifest, manifest.envelope.epoch)),
      [],
      `scheduler rejects valid fixture ${relative}`,
    );
  }
});

function readManifest(relative: string): EvidenceManifest {
  // The JSON Schema is the contract; the cast keeps this suite readable while
  // `validateManifest` performs the actual structural check.
  return JSON.parse(
    Deno.readTextFileSync(new URL(`./fixtures/${relative}`, import.meta.url)),
  ) as EvidenceManifest;
}

Deno.test("every invalid fixture is rejected with the registered code", () => {
  const registry = loadFixtureRegistry();
  for (const entry of registry.invalid) {
    const manifest = readManifest(entry.path);
    const currentEpoch = entry.currentEpoch ?? manifest.envelope.epoch;
    const schedulerCodes = codesOf(
      validateReportAsScheduler(manifest, currentEpoch),
    );
    const runnerCodes = codesOf(validateManifest(manifest));
    if (entry.runnerAccepts) {
      assertEqual(
        runnerCodes,
        [],
        `ADR 1.2: the Runner keeps writing for ${entry.path}`,
      );
      assertEqual(
        schedulerCodes,
        [entry.code],
        `central intake must reject ${entry.path}`,
      );
    } else {
      assert(
        schedulerCodes.includes(entry.code),
        `${entry.path}: scheduler codes ${
          JSON.stringify(schedulerCodes)
        } miss ${entry.code}`,
      );
      assertEqual(
        runnerCodes,
        schedulerCodes,
        `Scheduler and Runner disagree for ${entry.path}`,
      );
    }
  }
});

Deno.test("unknown schemaVersion is rejected", () => {
  const manifest = readManifest("invalid/unknown-schema-version.json");
  assertEqual(
    codesOf(validateManifest(manifest)),
    ["unsupported-schema-version"],
    "unknown schemaVersion",
  );
  const missing = readManifest(
    "valid/manifest-visual-judge-human.json",
  ) as unknown as Record<string, unknown>;
  delete missing.schemaVersion;
  assertEqual(
    codesOf(validateManifest(missing)),
    ["missing-required-evidence"],
    "missing schemaVersion is missing evidence, not a silent pass",
  );
});

Deno.test("invalid transition is rejected", () => {
  const manifest = readManifest("invalid/invalid-transition.json");
  assert(
    codesOf(validateManifest(manifest)).includes("invalid-transition"),
    "pending -> succeeded must not be in the table",
  );

  const terminal = readManifest("valid/manifest-visual-judge-human.json");
  terminal.envelope.taskStatus.state = "succeeded";
  terminal.envelope.stateTransition = { from: "succeeded", to: "succeeded" };
  assert(
    codesOf(validateManifest(terminal)).includes("invalid-transition"),
    "terminal states have no outgoing transitions",
  );

  const legal = readManifest("valid/manifest-visual-judge-human.json");
  legal.envelope.taskStatus.state = "running";
  legal.envelope.stateTransition = {
    from: "awaiting-approval",
    to: "running",
    at: "2026-11-07T21:10:00+09:00",
  };
  assertEqual(
    codesOf(validateManifest(legal)),
    [],
    "awaiting-approval -> running is legal",
  );
});

Deno.test("missing required evidence is rejected", () => {
  const base = readManifest(
    "valid/manifest-visual-judge-human.json",
  ) as unknown as Record<string, unknown>;
  for (
    const field of [
      "runtime",
      "attempts",
      "operations",
      "assertions",
      "environmentFidelity",
      "inputFidelity",
      "visualJudge",
      "cleanup",
      "limitations",
      "reproduction",
      "envelope",
    ]
  ) {
    const broken: Record<string, unknown> = JSON.parse(JSON.stringify(base));
    delete broken[field];
    assertEqual(
      codesOf(validateManifest(broken)),
      ["missing-required-evidence"],
      `dropping ${field} must be rejected`,
    );
  }
  const emptyLimits = JSON.parse(JSON.stringify(base)) as Record<
    string,
    unknown
  >;
  emptyLimits.limitations = [];
  assert(
    codesOf(validateManifest(emptyLimits)).includes("schema-violation"),
    "an empty limitations list is not evidence",
  );
});

Deno.test("cancel_requested must agree everywhere", () => {
  const mismatch = readManifest("invalid/cancel-mismatch.json");
  assert(
    codesOf(validateManifest(mismatch)).includes("cancel-state-mismatch"),
    "Envelope and TaskStatus disagree",
  );

  const flipped = readManifest("valid/manifest-visual-judge-human.json");
  flipped.envelope.taskStatus.cancelRequested = true;
  assert(
    codesOf(validateManifest(flipped)).includes("cancel-state-mismatch"),
    "state and flag disagree",
  );

  const agreed = readManifest("valid/manifest-cancel-requested.json");
  assertEqual(
    codesOf(validateManifest(agreed)),
    [],
    "consistent cancel_requested passes",
  );
});

Deno.test("old epoch is rejected centrally but not by the Runner", () => {
  const stale = readManifest("invalid/stale-epoch.json");
  assertEqual(
    codesOf(validateManifest(stale)),
    [],
    "Runner-side check has no epoch",
  );
  assertEqual(
    codesOf(validateReportAsScheduler(stale, 1)),
    ["stale-epoch"],
    "central intake rejects the older epoch",
  );
  assertEqual(
    codesOf(validateReportAsScheduler(stale, undefined)),
    ["missing-current-epoch"],
    "central intake without a known epoch refuses the report",
  );

  const schedulerDecisions = scheduler.validateSharedFixtures();
  const runnerDecisions = runner.validateSharedFixtures();
  assertEqual(
    schedulerDecisions.map((entry) => entry.path),
    runnerDecisions.map((entry) => entry.path),
    "both sides walk the same fixtures in the same order",
  );
  const schedulerStale = decisionCodes(
    schedulerDecisions,
    "invalid/stale-epoch.json",
  );
  const runnerStale = decisionCodes(
    runnerDecisions,
    "invalid/stale-epoch.json",
  );
  assertEqual(
    schedulerStale,
    ["stale-epoch"],
    "Scheduler rejects the stale report",
  );
  assertEqual(runnerStale, [], "Runner keeps its external write (ADR 1.2)");
});

Deno.test("Scheduler and Runner shared-fixture decisions agree", () => {
  const registry = loadFixtureRegistry();
  const schedulerDecisions = scheduler.validateSharedFixtures();
  const runnerDecisions = runner.validateSharedFixtures();
  assertEqual(
    schedulerDecisions.length,
    registry.valid.length + registry.invalid.length,
    "every registered fixture is decided",
  );
  for (const entry of registry.invalid) {
    if (entry.runnerAccepts) continue;
    assertEqual(
      decisionCodes(schedulerDecisions, entry.path),
      decisionCodes(runnerDecisions, entry.path),
      `shared rejection for ${entry.path}`,
    );
    assert(
      decisionCodes(runnerDecisions, entry.path).includes(entry.code),
      `Runner must reject ${entry.path} with ${entry.code}`,
    );
  }
  for (const relative of registry.valid) {
    assertEqual(
      decisionCodes(schedulerDecisions, relative),
      [],
      `Scheduler accepts ${relative}`,
    );
    assertEqual(
      decisionCodes(runnerDecisions, relative),
      [],
      `Runner accepts ${relative}`,
    );
  }
});

Deno.test("TypeScript types restrict schemaVersion and reproduction results", () => {
  const version: SchemaVersion = 1;
  assertEqual(version, 1, "schemaVersion literal");
  const reproduction: ReproductionResult = "inconclusive";
  assertEqual(reproduction, "inconclusive", "reproduction literal");

  // @ts-expect-error unknown schemaVersion must not satisfy the literal type
  const badVersion: SchemaVersion = 2;
  assert(typeof badVersion === "number", "type-level rejection only");

  function acceptsReproduction(result: ReproductionResult): ReproductionResult {
    return result;
  }
  // @ts-expect-error only reproduced/not-reproduced/inconclusive are allowed
  acceptsReproduction("maybe");
});
