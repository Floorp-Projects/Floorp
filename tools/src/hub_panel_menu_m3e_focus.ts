// SPDX-License-Identifier: MPL-2.0

import {
  executeHubPanelMenuM3eBrowserScript,
  hubPanelMenuM3eProtocolError,
  runHubPanelMenuM3eGatedOperation,
  sendHubPanelMenuM3eBrowserCommand,
} from "./hub_panel_menu_m3e_observations.ts";
import type { HubPanelMenuM3eLifecycleContext } from "./hub_panel_menu_m3e_lifecycle.types.ts";
import type { HubPanelMenuM3eLifecycleEvidence } from "./hub_panel_menu_m3e_lifecycle.types.ts";

export class HubPanelMenuM3eFocusFailure extends Error {
  constructor(readonly evidence: HubPanelMenuM3eLifecycleEvidence) {
    super("M3e focus verification failed; bounded observations retained");
  }
}

export function assertHubPanelMenuM3eGatedCancellation(
  caseId: string,
  value: unknown,
): void {
  const proof = record(value);
  const expected = ({
    "focus-pointer-cancellation": "pointermove",
    "focus-keyboard-cancellation": "keydown",
    "focus-movement-cancellation": "focusin",
    "focus-blur-cancellation": "blur",
    "focus-menu-close-cancellation": "popuphidden",
    "focus-navigation-cancellation": "focusin",
    "focus-window-close-intent-cancellation": "unload",
  } as Record<string, string>)[caseId];
  check(
    !!expected && proof.claimLiveAtAction === true &&
      typeof proof.claimInvalidatedAt === "number" &&
      typeof proof.actionAt === "number" &&
      typeof proof.gateCheckedAt === "number" &&
      proof.actionAt <= proof.claimInvalidatedAt &&
      proof.claimInvalidatedAt <= proof.gateCheckedAt &&
      proof.gateCheckedAt - proof.actionAt < 2500 && proof.buttonCount === 0 &&
      proof.activeButton === false,
    "claim was not cancelled while OLD_STOPPED remained gated",
  );
  check(
    Array.isArray(proof.events) &&
      proof.events.map(record).some((event) =>
        event.type === expected && event.trusted === true &&
        Number(event.at) >= Number(proof.actionAt) &&
        Number(event.at) <= Number(proof.gateCheckedAt)
      ),
    "named trusted cancellation event missing before gate release",
  );
}

export async function withHubPanelMenuM3eFocusCleanup<T>(
  operation: () => Promise<T>,
  dispose: () => Promise<unknown>,
): Promise<T> {
  let primary: unknown;
  let failed = false;
  let result!: T;
  try {
    result = await operation();
  } catch (error) {
    failed = true;
    primary = error;
  }
  try {
    await dispose();
  } catch (error) {
    if (!failed) throw error;
    if (primary instanceof HubPanelMenuM3eFocusFailure) {
      Object.assign(primary.evidence, { disposalFailed: true });
    }
  }
  if (failed) throw primary;
  return result;
}

function check(value: unknown, message: string): asserts value {
  if (!value) throw new Error(`M3e focus ${message}`);
}

function record(value: unknown): Record<string, unknown> {
  check(
    typeof value === "object" && value !== null && !Array.isArray(value),
    "observation missing",
  );
  return value as Record<string, unknown>;
}

export function assertHubPanelMenuM3eFocusResult(
  caseId: string,
  value: unknown,
): void {
  const result = record(value);
  const elapsed = Number(result.at) - Number(result.preparedAt);
  check(
    result.oldConnected === false && Number.isFinite(elapsed),
    "old node or clock differs",
  );
  if (caseId === "focus-bounded-expiry") {
    check(
      elapsed > 3000 && result.activeButton === false &&
        result.foreground === true && result.popupOpen === true &&
        result.activeSettings === true && result.fallbackVisible === true &&
        result.buttonCount === 1,
      "expiry restored focus or lost foreground/fallback",
    );
  } else {
    check(
      elapsed >= 0 && elapsed < 2500,
      "result exceeded the live claim window",
    );
    if (caseId === "focus-restoration") {
      check(
        result.foreground === true && result.popupOpen === true &&
          result.activeButton === true && result.buttonCount === 1 &&
          result.nodeId !== result.oldNodeId,
        "positive restoration differs",
      );
    } else {
      check(
        caseId === "focus-window-close-intent-cancellation"
          ? result.closed === true
          : result.closed !== true && result.buttonCount === 1,
        "cancellation window state differs",
      );
      check(
        result.claimLiveAtAction === true &&
          typeof result.claimInvalidatedAt === "number" &&
          result.claimInvalidatedAt >= Number(result.actionAt) &&
          result.activeButton === false &&
          Number(result.actionAt) >= Number(result.preparedAt) &&
          Number(result.at) - Number(result.actionAt) < 2500,
        "cancellation restored focus or expired",
      );
      assertHubPanelMenuM3eGatedCancellation(caseId, result.gatedCancellation);
      check(Array.isArray(result.events), "event audit missing");
      const events = result.events.map(record);
      const expectedType = ({
        "focus-pointer-cancellation": "pointermove",
        "focus-keyboard-cancellation": "keydown",
        "focus-movement-cancellation": "focusin",
        "focus-blur-cancellation": "blur",
        "focus-menu-close-cancellation": "popuphidden",
        "focus-navigation-cancellation": "focusin",
        "focus-window-close-intent-cancellation": "unload",
      } as Record<string, string>)[caseId];
      check(
        expectedType &&
          events.some((event) =>
            event.type === expectedType && event.trusted === true &&
            Number(event.at) >= Number(result.actionAt)
          ),
        "real cancellation event missing",
      );
      check(
        !events.some((event) => event.newButton),
        "new generation received transient focus",
      );
    }
  }
}

export async function runHubPanelMenuM3eFocus(
  context: HubPanelMenuM3eLifecycleContext,
): Promise<void> {
  const { client, testId, bundle, runCase } = context;
  const payload = { testId, rowId: "C" };
  const inputFailures: Record<string, unknown>[] = [];
  const inputCommand = (
    command: string,
    params: Readonly<Record<string, unknown>>,
  ) =>
    sendHubPanelMenuM3eBrowserCommand(
      client,
      command,
      params,
      10000,
      async (error) => {
        inputFailures.push({
          command,
          at: Date.now(),
          ...hubPanelMenuM3eProtocolError(error),
        });
        await Deno.writeTextFile(
          context.inputDiagnosticsPath,
          JSON.stringify(
            {
              diagnosticsOnly: true,
              testId,
              failures: inputFailures.slice(0, 32),
            },
            null,
            2,
          ) + "\n",
          { mode: 0o600 },
        );
      },
    );
  const focus = (
    action: string,
    role = "initial",
    extra: Readonly<Record<string, unknown>> = {},
  ) =>
    executeHubPanelMenuM3eBrowserScript(client, {
      kind: "focus",
      payload: { ...payload, action, role, ...extra },
    });
  const call = (
    input: Parameters<typeof executeHubPanelMenuM3eBrowserScript>[1],
  ) => executeHubPanelMenuM3eBrowserScript(client, input);
  async function nativeTab(handles: Readonly<Record<string, unknown>>) {
    const switchWindow = typeof handles.chromeHandle === "string";
    try {
      if (switchWindow) {
        await inputCommand("WebDriver:SwitchToWindow", {
          handle: handles.chromeHandle,
          focus: false,
        });
      }
      await inputCommand(
        "WebDriver:PerformActions",
        {
          actions: [{
            type: "key",
            id: "hub-menu-focus-navigation",
            actions: [{ type: "keyDown", value: "\uE004" }, {
              type: "keyUp",
              value: "\uE004",
            }],
          }],
        },
      );
    } finally {
      try {
        await inputCommand("WebDriver:ReleaseActions", {});
      } finally {
        if (switchWindow) {
          await inputCommand("WebDriver:SwitchToWindow", {
            handle: handles.sandboxHandle,
            focus: false,
          });
        }
      }
    }
  }
  async function prepareFocus(role = "initial") {
    const navigation: Record<string, unknown>[] = [];
    let tabCount = 0;
    let setupStage = "open";
    let state: Record<string, unknown> | null = null;
    try {
      state = record(await focus("open", role));
      const handles = state;
      const deadline = Date.now() + 3000;
      while (
        Date.now() < deadline && tabCount < 24 &&
        (tabCount === 0 || state.activeButton !== true)
      ) {
        check(
          state.foreground === true && state.popupOpen === true,
          "native keyboard setup lost foreground/menu",
        );
        setupStage = "native-tab";
        await nativeTab(handles);
        tabCount++;
        setupStage = "state";
        state = record(await focus("state", role));
        navigation.push({
          activeId: state.activeId,
          activeButton: state.activeButton,
          foreground: state.foreground,
          popupOpen: state.popupOpen,
        });
      }
      setupStage = "validate";
      check(
        state.activeButton === true && state.buttonCount === 1,
        "native Tab traversal did not reach HUB",
      );
      setupStage = "prepare";
      return {
        ...record(await focus("prepare", role)),
        initialization:
          "PR #3186 native Tab traversal through foreground open app menu",
        tabCount,
        navigation,
      };
    } catch {
      throw new HubPanelMenuM3eFocusFailure({
        initialization:
          "PR #3186 native Tab traversal through foreground open app menu",
        tabCount,
        setupStage,
        state: JSON.stringify(state),
        navigation: JSON.stringify(navigation),
      });
    }
  }
  const observe = async () =>
    record(
      record(
        await call({
          kind: "host-operation",
          payload: { ...payload, operation: "observe" },
        }),
      ).diagnostics,
    );
  async function operation(operation: string) {
    const before = await observe();
    const outcome = record(
      await call({
        kind: "host-operation",
        payload: {
          ...payload,
          operation,
          txId: crypto.randomUUID(),
          expectedRevision: before.revision,
        },
      }),
    );
    check(
      outcome.status === "succeeded",
      "reset/disable/enable outcome differs",
    );
    return outcome;
  }
  const cases = [
    "focus-restoration",
    "focus-pointer-cancellation",
    "focus-keyboard-cancellation",
    "focus-movement-cancellation",
    "focus-blur-cancellation",
    "focus-menu-close-cancellation",
    "focus-navigation-cancellation",
    "focus-window-close-intent-cancellation",
    "focus-bounded-expiry",
  ];
  for (const caseId of cases) {
    await runCase(caseId, async () => {
      const before = await observe();
      check(
        record(before.selected).version === "hub-menu-v1",
        "focus baseline is not healthy v1",
      );
      const role = caseId === "focus-window-close-intent-cancellation"
        ? "late"
        : "initial";
      const prepared = await prepareFocus(role);
      let action: unknown;
      let terminal: unknown;
      let result: unknown;
      await withHubPanelMenuM3eFocusCleanup(async () => {
        try {
          const target = bundle.packages["hub-menu-v2"].packageSha256;
          terminal = await runHubPanelMenuM3eGatedOperation(call, {
            ...payload,
            txId: crypto.randomUUID(),
            operation: "update",
            expectedRevision: before.revision,
            packageSha256: target,
            targetVersion: target,
            targetDigest: target,
          }, async () => {
            action = await focus("act", role, { caseId });
            check(
              record(action).precondition === true,
              "gate focus conditions differ",
            );
            if (caseId === "focus-pointer-cancellation") {
              const pointer = record(record(action).pointerTarget);
              try {
                await inputCommand(
                  "WebDriver:PerformActions",
                  {
                    actions: [{
                      type: "pointer",
                      id: "hub-menu-focus-pointer",
                      parameters: { pointerType: "mouse" },
                      actions: [{
                        type: "pointerMove",
                        origin: "viewport",
                        x: pointer.x,
                        y: pointer.y,
                        duration: 0,
                      }],
                    }],
                  },
                );
              } finally {
                await inputCommand(
                  "WebDriver:ReleaseActions",
                  {},
                );
              }
            }
            if (caseId === "focus-keyboard-cancellation") {
              try {
                await inputCommand(
                  "WebDriver:PerformActions",
                  {
                    actions: [{
                      type: "key",
                      id: "hub-panel-menu-m3e-focus",
                      actions: [
                        { type: "keyDown", value: "\uE008" },
                        {
                          type: "keyUp",
                          value: "\uE008",
                        },
                      ],
                    }],
                  },
                );
              } finally {
                await inputCommand(
                  "WebDriver:ReleaseActions",
                  {},
                );
              }
            }
            if (
              caseId !== "focus-restoration" &&
              caseId !== "focus-bounded-expiry"
            ) {
              const gated = record(await focus("gate-result", role));
              assertHubPanelMenuM3eGatedCancellation(
                caseId,
                gated.gatedCancellation,
              );
            }
          });
          const outcome = record(record(record(terminal).terminal).outcome);
          check(
            outcome.status === "succeeded" &&
              outcome.selectedDigest === target &&
              outcome.expectedRevision === before.revision,
            "gated terminal differs",
          );
          result = await focus("result", role);
          assertHubPanelMenuM3eFocusResult(caseId, result);
        } catch {
          try {
            result = await focus("result", role);
          } catch {
            /* Connection safety failures prohibit further commands. */
          }
          throw new HubPanelMenuM3eFocusFailure({
            prepared: JSON.stringify(prepared),
            action: JSON.stringify(action) ?? null,
            terminal: JSON.stringify(terminal) ?? null,
            result: JSON.stringify(result) ?? null,
          });
        }
      }, () => focus("dispose", role));
      if (caseId === "focus-window-close-intent-cancellation") {
        await call({
          kind: "lifecycle",
          payload: { ...payload, action: "new-window" },
        });
      }
      await operation("rollback");
      return {
        prepared: JSON.stringify(prepared),
        action: JSON.stringify(action),
        terminal: JSON.stringify(terminal),
        result: JSON.stringify(result),
        claimDurationMs: 3000,
      };
    });
  }
  await runCase("focus-disable-fallback-no-theft", async () => {
    const prepared = record(await prepareFocus());
    let disabled: unknown;
    let enabled: unknown;
    let fallback: unknown;
    let monitor: unknown;
    await withHubPanelMenuM3eFocusCleanup(async () => {
      disabled = await operation("disable");
      fallback = await focus("result");
      const atFallback = record(fallback);
      check(
        atFallback.foreground === true && atFallback.fallbackVisible === true &&
          atFallback.activeButton === false &&
          Number(atFallback.at) - Number(prepared.preparedAt) < 2500,
        "disable lost visible fallback",
      );
      enabled = await operation("enable");
      const reenabled = record(await focus("result"));
      check(
        reenabled.activeButton === false && reenabled.foreground === true &&
          reenabled.fallbackVisible === true &&
          Number(reenabled.at) - Number(atFallback.fallbackAt) < 2500,
        "re-enable stole focus or exceeded claim window",
      );
      monitor = await focus("monitor");
    }, () => focus("dispose"));
    return {
      note:
        "Disable is serialized behind updates by M3cSerialQueue; this case proves disable fallback/no-theft, not interruption of a pending replacement claim.",
      prepared: JSON.stringify(prepared),
      disabled: JSON.stringify(disabled),
      fallback: JSON.stringify(fallback),
      enabled: JSON.stringify(enabled),
      monitor: JSON.stringify(monitor),
      claimDurationMs: 3000,
    };
  });
}
