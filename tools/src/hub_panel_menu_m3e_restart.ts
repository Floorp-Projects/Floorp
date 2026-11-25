// SPDX-License-Identifier: MPL-2.0

import { isAbsolute, join, resolve } from "@std/path";
import {
  executeHubPanelMenuM3eBrowserScript,
  sendHubPanelMenuM3eBrowserCommand,
} from "./hub_panel_menu_m3e_observations.ts";
import { assertHubPanelMenuM3eSignedState } from "./hub_panel_menu_m3e_lifecycle.ts";
import { waitForHubPanelMenuM3eFixtureIdle } from "./hub_panel_menu_m3e_fixture.ts";
import type { HubPanelMenuM3eRestartContext } from "./hub_panel_menu_m3e_restart.types.ts";
import type { HubPanelMenuM3eLifecycleEvidence } from "./hub_panel_menu_m3e_lifecycle.types.ts";

function check(value: unknown, label: string): asserts value {
  if (!value) throw new Error(`M3e restart ${label}`);
}
function record(value: unknown): Record<string, unknown> {
  check(
    value && typeof value === "object" && !Array.isArray(value),
    "record missing",
  );
  return value as Record<string, unknown>;
}
const same = (left: unknown, right: unknown) =>
  JSON.stringify(left) === JSON.stringify(right);
async function sha256(bytes: Uint8Array): Promise<string> {
  return Array.from(
    new Uint8Array(
      await crypto.subtle.digest("SHA-256", Uint8Array.from(bytes)),
    ),
    (value) => value.toString(16).padStart(2, "0"),
  ).join("");
}

export function resolveHubPanelMenuM3eSelectedCachePath(
  profilePath: string,
  testId: string,
  snapshot: unknown,
  digest: string,
): string {
  check(
    isAbsolute(profilePath) && resolve(profilePath) === profilePath &&
      /^[a-f0-9-]{36}$/.test(testId) && /^[a-f0-9]{64}$/.test(digest),
    "owned cache identity differs",
  );
  const cache = record(record(snapshot).cache);
  check(Array.isArray(cache.entries), "cache entries missing");
  const entries = cache.entries.map(record).filter((entry) =>
    entry.type === "file" && entry.sha256 === digest
  );
  check(entries.length === 1, "selected cache file missing or ambiguous");
  const relative = entries[0].path;
  check(
    typeof relative === "string" && relative.length > 0 &&
      !isAbsolute(relative) &&
      relative.split(/[\\/]/).every((part) =>
        part !== ".." && part !== "." && part.length > 0
      ),
    "unsafe cache path",
  );
  return join(
    profilePath,
    "floorp-signed-package-cache-v1",
    "tests",
    testId,
    "hub-panel-menu",
    relative,
  );
}

async function selectedCacheBytes(
  profilePath: string,
  testId: string,
  snapshot: unknown,
  digest: string,
) {
  const path = resolveHubPanelMenuM3eSelectedCachePath(
    profilePath,
    testId,
    snapshot,
    digest,
  );
  let current = profilePath;
  check(!(await Deno.lstat(current)).isSymlink, "profile symlink");
  for (const segment of path.slice(profilePath.length + 1).split("/")) {
    current = join(current, segment);
    check(!(await Deno.lstat(current)).isSymlink, "cache symlink");
  }
  const stat = await Deno.lstat(path);
  check(
    stat.isFile && stat.size > 0 && stat.size <= 4 * 1024 * 1024,
    "cache size differs",
  );
  const bytes = await Deno.readFile(path);
  check(
    bytes.length === stat.size && await sha256(bytes) === digest,
    "cache bytes differ before owned mutation",
  );
  return { path, bytes };
}

export class HubPanelMenuM3eRestartFailure extends Error {
  constructor(readonly evidence: HubPanelMenuM3eLifecycleEvidence) {
    super(
      "M3e restart/recovery verification failed; bounded observations retained",
    );
  }
}

export function assertHubPanelMenuM3eRestartJournal(
  before: unknown,
  after: unknown,
  enabled: boolean,
  quarantined: boolean,
): void {
  const first = record(before);
  const next = record(after);
  const prior = record(record(first.journal).lifecycle);
  const journal = record(next.journal);
  const lifecycle = record(journal.lifecycle);
  const diagnostics = record(next.diagnostics);
  check(
    same(first.metadata, next.metadata) &&
      same(record(first.journal).highWater, journal.highWater) &&
      same(journal.packages, next.metadata),
    "catalog or high-water changed",
  );
  check(
    lifecycle.pending === null && lifecycle.desiredEnabled === enabled &&
      diagnostics.desiredEnabled === enabled &&
      diagnostics.revision === lifecycle.stateRevision,
    "terminal state differs",
  );
  check(
    next.evaluationCount === (enabled && !quarantined ? 1 : 0),
    "fresh-process evaluation count differs",
  );
  if (!enabled) {
    check(same(first.journal, next.journal), "disabled reopen mutated journal");
  } else {
    const outcome = record(lifecycle.lastOutcome);
    check(
      Number(lifecycle.stateRevision) > Number(prior.stateRevision) &&
        record(outcome.request).op === "startup_recovery" &&
        outcome.status === (quarantined ? "failed_quarantined" : "succeeded"),
      "startup recovery outcome differs",
    );
  }
  if (quarantined) {
    check(
      diagnostics.blocked === true &&
        diagnostics.blockCategory === "lifecycle-blocked" &&
        typeof lifecycle.blockedReason === "string" &&
        lifecycle.blockedReason.length > 0 &&
        lifecycle.rollbackVersion === null && lifecycle.rollbackDigest === null,
      "quarantine differs",
    );
    check(
      lifecycle.selectedDigest === prior.selectedDigest &&
        lifecycle.selectedHealth === prior.selectedHealth &&
        same(lifecycle.selectedHealthBuild, prior.selectedHealthBuild),
      "historical quarantined selection differs",
    );
    const registry = record(next.registry);
    check(
      registry.connections === 2 && registry.published === false &&
        registry.enabled === false && registry.staged === false &&
        registry.pendingAcknowledgements === 0 && next.registryOwner === null,
      "quarantined registry published",
    );
    check(
      Array.isArray(next.ui) && next.ui.length === 3 &&
        next.ui.map(record).every((item) =>
          item.private === true
            ? item.count === 1 && item.privateNodeUnchanged === true
            : item.count === 0
        ),
      "quarantined UI published",
    );
  }
}

export async function runHubPanelMenuM3eRestartRecovery(
  context: HubPanelMenuM3eRestartContext,
): Promise<void> {
  const { bundle, fixture } = context;
  const v1 = bundle.packages["hub-menu-v1"];
  const v2 = bundle.packages["hub-menu-v2"];
  let testId = crypto.randomUUID();
  const payload = () => ({ testId, rowId: "C" });
  const call = (
    input: Parameters<typeof executeHubPanelMenuM3eBrowserScript>[1],
  ) => executeHubPanelMenuM3eBrowserScript(context.getClient(), input);
  const lifecycle = (
    action: string,
    extra: Readonly<Record<string, unknown>> = {},
  ) => call({ kind: "lifecycle", payload: { ...payload(), action, ...extra } });
  const snapshot = async () => record(await lifecycle("snapshot"));
  const windowContexts: unknown[] = [];
  const traceWindows = async (stage: string, observation: unknown = null) => {
    const entry: Record<string, unknown> = {
      stage,
      testId,
      at: Date.now(),
      observation,
    };
    windowContexts.push(entry);
    const save = () =>
      Deno.writeTextFile(
        context.windowDiagnosticsPath,
        JSON.stringify(
          { diagnosticsOnly: true, entries: windowContexts.slice(-64) },
          null,
          2,
        ) + "\n",
        { mode: 0o600 },
      );
    await save();
    entry.current = await sendHubPanelMenuM3eBrowserCommand(
      context.getClient(),
      "WebDriver:GetWindowHandle",
      {},
    );
    await save();
    entry.surviving = await sendHubPanelMenuM3eBrowserCommand(
      context.getClient(),
      "WebDriver:GetWindowHandles",
      {},
    );
    await save();
  };
  const operate = async (operation: string, packageSha256?: string) => {
    const observed = record(
      await call({
        kind: "host-operation",
        payload: { ...payload(), operation: "observe" },
      }),
    );
    const outcome = record(
      await call({
        kind: "host-operation",
        payload: {
          ...payload(),
          operation,
          packageSha256,
          txId: crypto.randomUUID(),
          expectedRevision: record(observed.diagnostics).revision,
        },
      }),
    );
    check(outcome.status === "succeeded", "setup operation failed");
    return outcome;
  };
  const close = async () => {
    const cleanup = record(
      await call({ kind: "close-host", payload: payload() }),
    );
    check(cleanup.proven === true, "host cleanup unproven");
    await traceWindows(
      "before-close-windows",
      await lifecycle("window-context"),
    );
    const windows = await lifecycle("close-windows");
    await traceWindows("after-close-windows", windows);
    return cleanup;
  };
  const prepare = async (reopen: boolean) => {
    await traceWindows("before-prepare-windows");
    await lifecycle("prepare-windows", { reopen });
    await traceWindows(
      "after-prepare-windows",
      await lifecycle("window-context"),
    );
    await lifecycle("observe-generations");
    const inert = await snapshot();
    if (reopen) {
      check(
        inert.evaluationCount === 0 &&
          record(inert.registry).published === false &&
          record(inert.registry).connections === 2 &&
          (inert.ui as unknown[]).map(record).every((item) =>
            item.private === true ? item.count === 1 : item.count === 0
          ),
        "pre-reopen evaluation or publication",
      );
    }
    await call({
      kind: reopen ? "reopen-host" : "create-host",
      payload: { ...payload(), policy: bundle.policy },
    });
    return inert;
  };
  const install = async (withRollback: boolean) => {
    await prepare(false);
    await call({
      kind: "host-operation",
      payload: {
        ...payload(),
        operation: "stage",
        packageBase64: context.v1DownloadedBase64,
      },
    });
    await operate("install", v1.packageSha256);
    await operate("enable");
    if (withRollback) {
      fixture.setMode("valid-streaming");
      const requests = fixture.snapshot().requests;
      const detached = record(
        await call({
          kind: "download-package",
          payload: {
            request: {
              origin: fixture.trustedOrigin,
              packageSha256: v2.packageSha256,
              policy: bundle.policy,
            },
          },
        }),
      );
      const bytes = Uint8Array.from(
        atob(String(detached.packageBase64)),
        (character) => character.charCodeAt(0),
      );
      check(
        bytes.length === v2.bytes.length &&
          bytes.every((byte, index) => byte === v2.bytes[index]),
        "HTTPS update bytes differ",
      );
      const https = await waitForHubPanelMenuM3eFixtureIdle(fixture, 5000);
      check(
        https.requests === requests + 1 &&
          https.observations.at(-1)?.packageSha256 === v2.packageSha256 &&
          !https.observationOverflow,
        "HTTPS update route differs",
      );
      await call({
        kind: "host-operation",
        payload: {
          ...payload(),
          operation: "stage",
          packageBase64: detached.packageBase64,
        },
      });
      await operate("update", v2.packageSha256);
    }
  };
  for (
    const caseId of [
      "reopen-enabled-reverified",
      "reopen-disabled-no-evaluation",
      "reopen-corrupt-with-verified-rollback",
      "reopen-corrupt-without-eligible-recovery",
    ]
  ) {
    await context.runCase(caseId, async () => {
      const evidence: Record<string, boolean | number | string | null> = {
        checkpoint: "setup",
      };
      try {
        if (caseId === "reopen-enabled-reverified") await install(true);
        if (caseId === "reopen-disabled-no-evaluation") {
          await operate("disable");
        }
        if (caseId === "reopen-corrupt-with-verified-rollback") {
          await operate("enable");
        }
        if (caseId === "reopen-corrupt-without-eligible-recovery") {
          testId = crypto.randomUUID();
          await install(false);
        }
        const corrupt = caseId.startsWith("reopen-corrupt-");
        const quarantine =
          caseId === "reopen-corrupt-without-eligible-recovery";
        const enabled = caseId !== "reopen-disabled-no-evaluation";
        const selected = quarantine ? v1 : v2;
        const before = await snapshot();
        evidence.testId = testId;
        evidence.before = JSON.stringify(before);
        if (corrupt) {
          check(
            record(record(before.journal).lifecycle).rollbackDigest ===
              (quarantine ? null : v1.packageSha256),
            "eligible rollback setup differs",
          );
        }
        evidence.checkpoint = "orderly-quit-and-same-profile-relaunch";
        const restart = await context.restart(
          corrupt
            ? async () => {
              const { path, bytes } = await selectedCacheBytes(
                context.profilePath,
                testId,
                before,
                selected.packageSha256,
              );
              const changed = Uint8Array.from(bytes);
              changed[0] ^= 255;
              await Deno.writeFile(path, changed);
              const afterSha256 = await sha256(await Deno.readFile(path));
              check(
                afterSha256 !== selected.packageSha256,
                "owned corruption did not change bytes",
              );
              return {
                path,
                beforeSha256: selected.packageSha256,
                afterSha256,
                byteLength: bytes.length,
                selectedCacheOnly: true,
              };
            }
            : undefined,
        );
        evidence.restart = JSON.stringify(restart);
        evidence.checkpoint = "trusted-core-reopen";
        const inert = await prepare(true);
        evidence.beforeTrustedReopen = JSON.stringify(inert);
        evidence.afterTrustedReopenHost = JSON.stringify(
          await call({
            kind: "host-operation",
            payload: { ...payload(), operation: "observe" },
          }),
        );
        if (!enabled) {
          const { path, bytes } = await selectedCacheBytes(
            context.profilePath,
            testId,
            before,
            selected.packageSha256,
          );
          await call({
            kind: "host-operation",
            payload: {
              ...payload(),
              operation: "stage",
              packageBase64: btoa(String.fromCharCode(...bytes)),
            },
          });
          evidence.disabledVerificationOnlyStage = JSON.stringify({
            path,
            packageSha256: selected.packageSha256,
            evaluationExpected: 0,
          });
        }
        if (enabled) {
          evidence.beforeInteractiveMenu = JSON.stringify(await snapshot());
          evidence.checkpoint = "interactive-menu";
          evidence.interactiveMenu = JSON.stringify(
            await lifecycle("interactive-menu", {
              expectedCount: quarantine ? 0 : 1,
              expectedLabel: context.label,
            }),
          );
          check(
            record(JSON.parse(String(evidence.interactiveMenu))).passed ===
              true,
            "interactive menu did not open, verify and close",
          );
        }
        const after = await snapshot();
        evidence.after = JSON.stringify(after);
        evidence.checkpoint = "assert-recovery";
        assertHubPanelMenuM3eRestartJournal(before, after, enabled, quarantine);
        if (!quarantine) {
          assertHubPanelMenuM3eSignedState(after, {
            selected: corrupt ? v1 : v2,
            highWater: v2,
            enabled,
            health: "verified",
            normalWindows: 2,
            label: context.label,
          });
        }
        const terminal = record(record(after.journal).lifecycle).lastOutcome;
        const query = await call({
          kind: "host-operation",
          payload: {
            ...payload(),
            operation: "query",
            txId: record(terminal).txId,
          },
        });
        check(same(terminal, query), "durable terminal query differs");
        evidence.terminal = JSON.stringify(query);
        evidence.reverification = enabled
          ? "Fresh-process trusted reopen calls M3cSignedPackageProvider.load -> cache.read -> verifySignedFeaturePackage before evaluation/publication"
          : "Disabled reopen does not evaluate; the selected cached bytes are then reverified through public verification-only stage without changing the journal";
        if (caseId === "reopen-corrupt-with-verified-rollback" || quarantine) {
          evidence.cleanup = JSON.stringify(await close());
        }
        evidence.checkpoint = "complete";
        return evidence;
      } catch (error) {
        if (
          ["trusted-core-reopen", "interactive-menu", "assert-recovery"]
            .includes(
              String(evidence.checkpoint),
            )
        ) {
          try {
            evidence.failureUI = JSON.stringify(await lifecycle("passive-ui"));
          } catch {
            evidence.failureUI = "passive observation unavailable";
          }
        }
        if (error instanceof Error && error.message.startsWith("M3e ")) {
          evidence.assertion = error.message.slice(0, 300);
        }
        throw new HubPanelMenuM3eRestartFailure(evidence);
      }
    });
  }
}
