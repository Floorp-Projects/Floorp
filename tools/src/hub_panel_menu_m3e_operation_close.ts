// SPDX-License-Identifier: MPL-2.0

import { assertHubPanelMenuM3eSignedState } from "./hub_panel_menu_m3e_lifecycle.ts";
import {
  executeHubPanelMenuM3eBrowserScript,
  runHubPanelMenuM3eGatedOperation,
} from "./hub_panel_menu_m3e_observations.ts";
import { waitForHubPanelMenuM3eFixtureIdle } from "./hub_panel_menu_m3e_fixture.ts";
import type { HubPanelMenuM3eLifecycleContext } from "./hub_panel_menu_m3e_lifecycle.types.ts";

function record(value: unknown): Record<string, unknown> {
  if (!value || typeof value !== "object" || Array.isArray(value)) {
    throw new Error("M3e operation-close record missing");
  }
  return value as Record<string, unknown>;
}
function check(value: unknown): asserts value {
  if (!value) throw new Error("M3e operation-close evidence differs");
}

export function assertHubPanelMenuM3eOperationCloseTerminal(
  value: unknown,
  expectedRevision: number,
  digest: string,
): void {
  const terminal = record(record(value).terminal);
  const outcome = record(terminal.outcome);
  check(
    terminal.settled === true && terminal.rejected === false &&
      outcome.status === "succeeded" &&
      outcome.expectedRevision === expectedRevision &&
      Number(outcome.revision) > expectedRevision &&
      outcome.selectedVersion === "hub-menu-v2" &&
      outcome.selectedDigest === digest &&
      outcome.engineSelectedVersion === digest,
  );
}

export async function runHubPanelMenuM3eOperationClose(
  context: HubPanelMenuM3eLifecycleContext,
): Promise<void> {
  const { client, bundle, fixture, runCase } = context;
  for (
    const caseId of [
      "close-window-during-operation",
      "close-host-during-operation",
    ]
  ) {
    await runCase(caseId, async () => {
      const testId = crypto.randomUUID();
      const payload = { testId, rowId: "C" };
      const call = (
        input: Parameters<typeof executeHubPanelMenuM3eBrowserScript>[1],
      ) => executeHubPanelMenuM3eBrowserScript(client, input);
      const lifecycle = (
        action: string,
        extra: Readonly<Record<string, unknown>> = {},
      ) =>
        call({ kind: "lifecycle", payload: { ...payload, action, ...extra } });
      const v1 = bundle.packages["hub-menu-v1"];
      const v2 = bundle.packages["hub-menu-v2"];
      await lifecycle("prepare-windows");
      await lifecycle("observe-generations");
      await call({
        kind: "create-host",
        payload: { ...payload, policy: bundle.policy },
      });
      await call({
        kind: "host-operation",
        payload: {
          ...payload,
          operation: "stage",
          packageBase64: context.v1DownloadedBase64,
        },
      });
      const operate = async (operation: string, packageSha256?: string) => {
        const observed = record(
          await call({
            kind: "host-operation",
            payload: { ...payload, operation: "observe" },
          }),
        );
        const outcome = record(
          await call({
            kind: "host-operation",
            payload: {
              ...payload,
              operation,
              packageSha256,
              txId: crypto.randomUUID(),
              expectedRevision: record(observed.diagnostics).revision,
            },
          }),
        );
        check(outcome.status === "succeeded");
        return outcome;
      };
      await operate("install", v1.packageSha256);
      await operate("enable");
      const before = record(await lifecycle("snapshot"));
      assertHubPanelMenuM3eSignedState(before, {
        selected: v1,
        highWater: v1,
        enabled: true,
        health: "verified",
        normalWindows: 2,
        label: context.label,
      });
      fixture.setMode("valid-streaming");
      const requestsBefore = fixture.snapshot().requests;
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
      );
      const https = await waitForHubPanelMenuM3eFixtureIdle(fixture, 5000);
      check(
        https.requests === requestsBefore + 1 &&
          https.observations.at(-1)?.packageSha256 === v2.packageSha256 &&
          !https.observationOverflow,
      );
      await call({
        kind: "host-operation",
        payload: {
          ...payload,
          operation: "stage",
          packageBase64: detached.packageBase64,
        },
      });
      await lifecycle("retain", { tag: "operation-window" });
      const revision = Number(record(before.diagnostics).revision);
      let atGate: unknown;
      let postClose: unknown;
      const terminal = await runHubPanelMenuM3eGatedOperation(call, {
        ...payload,
        txId: crypto.randomUUID(),
        operation: "update",
        expectedRevision: revision,
        packageSha256: v2.packageSha256,
        targetVersion: v2.packageSha256,
        targetDigest: v2.packageSha256,
      }, async () => {
        atGate = await lifecycle(
          caseId === "close-window-during-operation"
            ? "operation-close-window"
            : "operation-begin-host-close",
        );
        if (caseId === "close-host-during-operation") {
          postClose = await lifecycle("reject-operation", {
            operation: "stage",
            packageBase64: context.v1DownloadedBase64,
            expectedCode: "operation-failed",
          });
        }
      });
      assertHubPanelMenuM3eOperationCloseTerminal(
        terminal,
        revision,
        v2.packageSha256,
      );
      let cleanup: unknown;
      if (caseId === "close-window-during-operation") {
        const active = record(await lifecycle("snapshot"));
        assertHubPanelMenuM3eSignedState(active, {
          selected: v2,
          highWater: v2,
          enabled: true,
          health: "verified",
          normalWindows: 1,
          label: context.label,
        });
        postClose = await lifecycle("operation-closed-window-command", {
          tag: "operation-window",
        });
        cleanup = await call({ kind: "close-host", payload });
      } else {cleanup =
          record(await lifecycle("operation-await-host-close")).cleanup;}
      check(record(cleanup).proven === true);
      const final = record(await lifecycle("snapshot"));
      assertHubPanelMenuM3eSignedState(final, {
        selected: v2,
        highWater: v2,
        enabled: true,
        health: "verified",
        normalWindows: caseId === "close-window-during-operation" ? 1 : 2,
        label: context.label,
        closed: true,
      });
      const rejected = await lifecycle("reject-operation", {
        operation: "stage",
        packageBase64: context.v1DownloadedBase64,
        expectedCode: "operation-failed",
      });
      await lifecycle("close-windows");
      return {
        testId,
        barrier: "OLD_STOPPED",
        atGate: JSON.stringify(atGate),
        terminal: JSON.stringify(terminal),
        cleanup: JSON.stringify(cleanup),
        postClose: JSON.stringify(postClose),
        postCloseStage: JSON.stringify(rejected),
        final: JSON.stringify(final),
      };
    });
  }
}
