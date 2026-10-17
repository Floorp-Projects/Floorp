# Fixed Web Panel actions: offline synthetic contracts

This is QA3-10 groundwork for one Floorp feature. It accepts bounded in-memory
controller and observer JSONL through separate functions, applies the existing
`tools/final_web_panel_http.py` predicates, and produces a deterministic replay
receipt or a human-review-required regression candidate. It launches no browser,
HTTP server, process or model and reads no environment, credential or file.

Run the host contracts from the repository root:

```sh
python3 -B -m unittest discover -s tools/agentic_qa -p 'test_*.py' -v
```

The existing Linux contract workflow runs these units. The required source plan
remains 109 cases; synthetic negatives here are not extra product cases.

## Fixed actions and observations

The only action IDs are `observe`, `web-panel.mobile-on`,
`web-panel.mobile-off`, and `web-panel.navigate-two-hop`. The action envelope
has exactly `schemaVersion: 1`, `kind: "action"`, `runId`, monotonic `step`
starting at 1, and `actionId`. There are no action parameters, arbitrary URLs,
selectors, scripts, shell commands, paths, retries, skips or expectation
overrides.

A complete replay requires ON → navigate → OFF → navigate → ON → navigate in
that order. Additional `observe` actions can occur anywhere within the 32-step
bound, retaining all original steps. Toggle acknowledgements alone cannot pass.
Initial mobile state is fixed to false.

Observer envelopes use `kind: "observation"` and add exactly `mobileEnabled`,
`redirect`, `normalTabUas`, and `otherPanelUas`. Every action needs its own
matching run/step/action observation. Non-navigation HTTP arrays must be empty.
Each navigation needs exactly three ordered redirect events and exactly one UA
observation for each other role. The existing oracle checks the exact loopback
URL/host/location/method/status, derived phase nonce, case and independently
declared expected UA. Normal tabs and other panels must retain desktop UA in all
three intervals. Each redirect event has the existing oracle's exact fields:
`nonce`, `caseId`, `hop`, `url`, `host`, `method`, `status`, `location`,
`userAgent`.

`ReplayContext` is supplied separately by a trusted caller. It pins a closed run
ID, exact `http://127.0.0.1:<port>` origin, distinct bounded desktop/mobile UA
baselines and full Floorp/Runtime source commit declarations. Policy and oracle
IDs are fixed in source. No action or observer frame can change these values.
Source declarations and synthetic role labels are not authenticated evidence of
a checkout, build, distinct browser, target, worker, or actual HTTP request. The
context digest binds these declarations; the deterministic per-step nonce is
correlation only, not an unforgeable capability.

Each LF-terminated UTF-8 record is at most 32 KiB including LF; each channel is
at most 512 KiB, with at most 32 actions. Decoders reject partial/blank records,
duplicate keys at every level, nonfinite/floating/oversized numbers,
bool-as-int, unpaired surrogates, excessive nesting, strings and arrays. Outputs
are bounded to 32 KiB. These are in-memory bounds, not stream deadlines or OS
memory limits: the caller has already materialized each byte string. No
transport is provided.

## Findings and regression candidates

`replay(context, action_wire, observation_wire)` returns `FIXTURE_PASS` only for
a complete synthetic transcript satisfying this contract. Missing or invalid
evidence is `INCOMPLETE`. Present wrong observations yield `FINDING`. The first
observed oracle finding latches, including its original step/action and fixed
signature; later clean observations cannot erase it. Later channel/budget errors
are recorded separately in `incompletionReason`, leaving `complete: false` and
the original finding intact. Inputs rejected before their prefix is evaluated
(for example an oversized channel) provide no evaluated-prefix finding claim.
There is no cleanup or persistence callback that can overwrite replay status.

When an independent role is missing, complete assertions from the other roles
are still evaluated and retained. A partial redirect chain cannot satisfy the
three-hop oracle; its missing events are never filled in or treated as observed.

Receipts bind original channel bytes and each attempted record using SHA-256.
They contain closed action IDs and oracle codes, never raw page/UA/header or
exception text. Hashes are correlation values, not secret redaction or
encryption; do not put credentials in the inputs. All outputs explicitly retain
`offline-synthetic-fixture`, native `NOT_EXECUTED`, product/worker `UNVERIFIED`,
`releaseEvidence: false`, and `publicationAuthorized: false` where applicable.

`regression_candidate` replays the original bytes itself and requires a complete
finding. It retains the full action sequence and bindings; it does not shrink
away an obligation or accept a caller-provided receipt. `compare_synthetic_pair`
uses one context, action wire and oracle for both the bad and fixed fixture. A
passing second synthetic transcript is not proof of a product code fix. These
functions return bounded data for human review and cannot change source,
expectations, policy, quarantine, merge or release state. The synthetic test
corpus models wrong UA, ignored toggles and leakage; it is not an actual newly
discovered Floorp vulnerability or real Agent exploration result.

## External page and secret boundary

`project_untrusted_page` produces only byte count, digest and a fixed
non-actionable tag from at most 64 KiB of arbitrary page bytes. It drops all raw
text, including action-looking JSON, URLs and instructions. Its output is not a
valid action envelope. There is no planner that consumes raw pages or observer
payloads, no model API and no worker bridge in this increment.

API separation, schema validation, synthetic secret-sentinel tests and labels do
not authenticate a producer or establish OS/IPC/credential isolation. A future
caller that routes raw page text directly into its controller channel can still
violate that boundary. A future live adapter must qualify an authenticated
controller/observer transport, separate model and browser processes, owned
disposable profile, secret-free worker environment, exact action bindings, fresh
bounded HTTP fixtures for repeated phases, durable first-attempt history, and
bounded all-OS startup/stdout/cleanup/deadlines. These are unimplemented.

There is no native adapter, model-driven campaign, process supervisor, shrinker,
PGO-fix proof, signed final-artifact evidence or publication authorization here.
Formal QA3 native/product qualification remains NO_GO. This contract adds no
restriction to the owner's separate existing-gate 12.20.2 release decision.
