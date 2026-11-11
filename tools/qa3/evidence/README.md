# EvidenceManifest v0 (2-D / KAN-106)

Evidence schema v0 on top of the QA3 receipt, implementing the frozen ADR
[FDH — B-01/B-05 契約ADR（v0.1・凍結）](https://floorp-projects.atlassian.net/wiki/spaces/FI/pages/8061068)
sections 1.1–1.5. This directory is additive: it does **not** change the QA3
receipt semantics, the fixed source plan, or any release gate.

## Files

| Path | Role |
| --- | --- |
| `evidence-manifest.schema.json` | JSON Schema draft-07 contract (the source of truth) |
| `state-transitions.json` | Envelope state-transition table (shared by Python and TypeScript) |
| `types.ts` | Importable TypeScript types for tools |
| `validate.ts` / `validate.py` | Same rules, same error codes, two runtimes |
| `convert.py` | Converters for the three existing formats + CLI |
| `fixtures.ts`, `scheduler.ts`, `runner.ts` | Minimal Scheduler/Runner wiring over one shared fixture registry |
| `fixtures/registry.json` | The one fixture list both sides consume |
| `fixtures/inputs/` | Vendored conversion inputs (QA3 receipt, final-package QA/provenance); two of them are CRLF originals, so provenance is compared after EOL normalisation because the repository uses `* text=auto` |
| `fixtures/converted/` | Manifests produced by `convert.py regenerate` |
| `fixtures/valid/`, `fixtures/invalid/` | Handwritten valid/invalid examples |
| `../../test_evidence.py` | Python contract + conversion tests |

## Document shape

The document *is* the versioned Envelope: `schemaVersion` (currently `1`) is
checked before anything else, and an unknown value is rejected as
`unsupported-schema-version`.

```
schemaVersion, sourceFormat?
envelope   { epoch, principal, entities[7], relationships, taskStatus,
             cancelRequested, stateTransition }
runtime    { os{name,arch}, build{commit,id,…}, packageDigest, toolchain{dsh,floorp} }
attempts[] { run, attempt, startedAt, endedAt, result, exitCode?, notes? }
operations[]{ input, target, order, observation }
assertions[]{ name, expected, actual, passed }
environmentFidelity { host, network, matchesTarget, deviations, notes }
inputFidelity       { inputs[{path,sha256,bytes?}], unchangedDuringRun, deviations, notes }
visualJudge         { source: human|agent, scope, notes } | null
cleanup             { result, notes, details? }
limitations[]       (≥1: what this evidence does NOT prove)
reproduction        { result: reproduced|not-reproduced|inconclusive, notes, defectRef? }
```

### ADR 1.1 — 主体と ID

| kind | issuer (ID 発行元) | ID namespace | ownerRole |
| --- | --- | --- | --- |
| task | scheduler | `scheduler:` | task-owner |
| job | scheduler | `scheduler:` | job-owner / executor |
| run | executor | `executor:` | run-owner |
| attempt | executor | `executor:` | run-owner |
| session | host | `host:` | session-owner |
| workspace | host | `host:` | workspace-owner |
| approval | host-authorization | `host-authorization:` | approver (+ `decision`) |

The validators require all seven kinds, reject an `id` outside its issuer
namespace (`invalid-id-namespace`), reject dangling relationship/attempt
references (`unknown-entity-reference`) and require the five core relationships
(`task-has-job`, `job-has-run`, `run-has-attempt`, `run-uses-session`,
`run-in-workspace`).

### ADR 1.2 — 状態遷移と cancel_requested

`state-transitions.json` is the single source of truth:

```
pending            → scheduled | awaiting-approval | failed | cancelled
scheduled          → running | awaiting-approval | cancel_requested | cancelled
awaiting-approval  → running | cancel_requested | failed | cancelled
running            → succeeded | failed | cancel_requested | awaiting-approval
cancel_requested   → cancelled | succeeded | failed | awaiting-approval
succeeded / failed / cancelled → (terminal, no outgoing transition)
```

`envelope.stateTransition` must be a listed transition and its `to` must equal
`taskStatus.state`, otherwise `invalid-transition`.
`envelope.cancelRequested` (Envelope side) and `taskStatus.cancelRequested`
(TaskStatus body side) must agree, and `cancel_requested` as a state must agree
with the flag — otherwise `cancel-state-mismatch`.

**Epoch:** the Scheduler (central intake) always knows the current epoch and
rejects older reports (`stale-epoch`); without a known epoch it refuses the
report (`missing-current-epoch`). The Runner validates without a central epoch,
because an old Runner's external writes are not stopped by central rejection —
external side effects are re-checked for ownership and approval by the
Connector. This is the only intended Scheduler/Runner difference.

### Required limitation markers

The converters never invent data. An explicit absence must be mirrored in
`limitations` (otherwise `missing-limitation`):

| condition | limitation must mention |
| --- | --- |
| `runtime.toolchain.dsh.pin == "UNVERIFIED"` | `DSH` |
| `runtime.os.arch == "UNVERIFIED"` | `arch` |
| `runtime.build.commit` or `toolchain.floorp.commit` is `null` | `commit` |
| any entity `owner == "UNVERIFIED"` | `ownership` |
| `visualJudge === null` | `visual` |
| an attempt with `startedAt`/`endedAt === null` | `timestamp` |

## Converters (ADR 1.4)

```sh
python3 tools/qa3/evidence/convert.py receipt fixtures/inputs/qa3-source-receipt.json --check
python3 tools/qa3/evidence/convert.py final-package fixtures/inputs/final-package-web-panel-ua-report.json --check
python3 tools/qa3/evidence/convert.py work docs/qa/kit-v0.3.0-review-fixes/followup-evidence/floorp-independent-negative-01.json --check
python3 tools/qa3/evidence/convert.py regenerate   # rewrite fixtures/converted/*
```

Committed conversions (all pass the schema after conversion):

| Source format | Source record | Converted fixture |
| --- | --- | --- |
| QA3 receipt | `fixtures/inputs/qa3-source-receipt.json` (gate1 source shadow receipt: PASS / UNVERIFIED / 109 cases) | `converted/qa3-source-receipt.json` |
| final-package QA/provenance | `fixtures/inputs/final-package-native-verification.json` | `converted/final-package-native-verification.json` |
| final-package QA/provenance | `fixtures/inputs/final-package-web-panel-ua-report.json` | `converted/final-package-web-panel-ua-report.json` |
| work/follow-up | `docs/qa/kit-v0.3.0-review-fixes/followup-evidence/docs-public-independent-close-02.json` | `converted/work-close-p2-doc-01.json` (`reproduced`, defect `P2-DOC-01`) |
| work/follow-up | `…/floorp-independent-negative-01.json` | `converted/work-independent-negative-01.json` (`not-reproduced`) |
| work/follow-up | `…/floorp-source106-clean-commit-01.json` | `converted/work-source106-clean-commit-01.json` (`inconclusive`, real start/end timestamps) |

`sourceFormat` therefore covers all three ADR 1.4 formats, and the three work
records exercise the whole `reproduced / not-reproduced / inconclusive` axis.

## Fixtures and shared Scheduler/Runner wiring

`fixtures/registry.json` is the only fixture list. `fixtures.ts` loads it once;
`scheduler.ts` and `runner.ts` both call `sharedFixturePaths()` and both expose
`validateSharedFixtures()`, so neither component can drift onto its own fixture
set. The Python tests walk the same registry.

* valid: 6 converted manifests + 2 handwritten ones (human visual judge;
  consistent `cancel_requested`).
* invalid: unknown `schemaVersion`, invalid transition, missing `runtime` /
  `cleanup` / `visualJudge` / `assertions` / `envelope`, empty `limitations`,
  `cancel_requested` mismatch, missing entity kind, unknown entity reference,
  wrong ID namespace, non-human/agent visual judge, and a stale epoch.

## Tests

```sh
# Python: schema contract, conversions, reject cases, shared registry
python3 -B -m unittest discover -s tools/qa3 -p 'test_evidence.py' -v

# TypeScript: types, schema contract, Scheduler/Runner shared fixtures, reject cases
deno test --allow-read tools/qa3/evidence/

# Existing QA3 contract tests (some need a disposable Linux x86_64 worker)
python3 -B -m unittest discover -s tools/qa3 -p 'test_*.py' -v
```

## Limits of v0

* `visualJudge` is `null` for every converted record because none of the three
  source formats records a human/agent look-and-feel judgement; the handwritten
  valid fixture shows the `human` shape.
* Wall-clock attempt times are absent from the QA3 receipt, the final-package
  records and two of the three work records, so those attempts carry `null`
  timestamps with a `timestamp` limitation. Only
  `floorp-source106-clean-commit-01.json` carries real times (its `endedAt` is
  derived from `startedAt` + recorded seconds, stated in the attempt `notes`).
* Unknown scalars use the QA3 `UNVERIFIED` sentinel (`owner`, `os.arch`, DSH
  pin/version, missing Python version) — never a guess.
* The Envelope `principal` of a converted manifest is the converter that
  assembled it (`fdh-evidence-converter`, granted by the authenticated DSH
  host); the original actor of the source record is not recorded by these
  formats.
