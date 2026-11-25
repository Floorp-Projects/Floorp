// SPDX-License-Identifier: MPL-2.0

import { assert, assertEquals } from "@std/assert";
import { HUB_PANEL_MENU_M3E_GATE_SCRIPT } from "./hub_panel_menu_m3e_gate_script.ts";

Deno.test("real generated gate matches exact pending identity and preserves terminal outcome", async () => {
  await new Function(
    "assert",
    "assertEquals",
    `${HUB_PANEL_MENU_M3E_GATE_SCRIPT}
    return (async () => {
      const input = { txId: "tx", operation: "update", targetVersion: "version", targetDigest: "digest", expectedRevision: 4, timeoutMs: 1000 };
      const gate = armM3eGate(input);
      const pending = { txId: "tx", op: "update", phase: "OLD_STOPPED", targetVersion: "version", targetDigest: "digest" };
      for (const mutation of [{ txId: "other" }, { op: "enable" }, { phase: "PREPARED" }, { targetVersion: "other" }, { targetDigest: "other" }]) {
        assertEquals(reachM3eGate(gate, { pending: { ...pending, ...mutation } }), undefined);
        assertEquals(gate.armed, true);
      }
      const host = {
        async update() { await reachM3eGate(gate, { pending }); return { txId: "tx", status: "succeeded", selectedVersion: "digest", request: { expectedRevision: 4 } }; },
        diagnostics() { return { revision: 8, selected: { version: "v2" }, selectedPackageSha256: "digest" }; },
        cleanupDiagnosticsForTests() { return { proven: true }; },
      };
      const reached = await startM3eGate(gate, input, host);
      assertEquals(reached.barrierReached, true);
      assertEquals(reachM3eGate(gate, { pending }), undefined);
      assertEquals(releaseM3eGate(gate), true);
      assertEquals(releaseM3eGate(gate), false);
      const terminal = await awaitM3eGate(gate, host, 1000);
      assertEquals(terminal.terminal.outcome, { txId: "tx", status: "succeeded", expectedRevision: 4, revision: 8, selectedVersion: "v2", selectedDigest: "digest", engineSelectedVersion: "digest" });
      assert(terminal.cleanup.proven);
    })();`,
  )(assert, assertEquals);
});

Deno.test("real generated gate handles early rejection and disarms late callbacks after timeout", async () => {
  await new Function(
    "assertEquals",
    `${HUB_PANEL_MENU_M3E_GATE_SCRIPT}
    return (async () => {
      const input = { txId: "tx", operation: "update", timeoutMs: 5 };
      const rejected = armM3eGate(input);
      const failure = await startM3eGate(rejected, input, { update() { return Promise.reject(new Error("private marker")); } });
      assertEquals(failure.barrierReached, false);
      assertEquals(failure.terminal, { settled: true, rejected: true, outcome: null });
      assertEquals(rejected.released, true);
      const gate = armM3eGate(input);
      const timedOut = await startM3eGate(gate, input, { update() { return new Promise(() => {}); } });
      assertEquals(timedOut, { barrierReached: false, timedOut: true });
      assertEquals(gate.armed, false);
      assertEquals(reachM3eGate(gate, { pending: { txId: "tx", op: "update", phase: "OLD_STOPPED", targetVersion: null, targetDigest: null } }), undefined);
      const terminal = await awaitM3eGate(gate, { cleanupDiagnosticsForTests() {} }, 5);
      assertEquals(terminal, { timedOut: true });
      const held = armM3eGate(input);
      const pending = { txId: "tx", op: "update", phase: "OLD_STOPPED", targetVersion: null, targetDigest: null };
      const host = {
        async update() { await reachM3eGate(held, { pending }); return { txId: "tx", status: "succeeded", selectedVersion: "digest", request: { expectedRevision: 4 } }; },
        diagnostics() { return { revision: 8, selected: { version: "v2" }, selectedPackageSha256: "digest" }; },
      };
      assertEquals((await startM3eGate(held, input, host)).barrierReached, true);
      await held.operationPromise;
      assertEquals(held.released, true);
      assertEquals(await awaitM3eGate(held, host, 5), { timedOut: true });
    })();`,
  )(assertEquals);
});
