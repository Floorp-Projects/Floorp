// SPDX-License-Identifier: MPL-2.0

export const HUB_PANEL_MENU_M3E_GATE_SCRIPT = `
  function releaseM3eGate(gate) {
    if (!gate) return false;
    gate.armed = false;
    gate.releaseRequested = true;
    if (gate.timer !== null) { clearTimeout(gate.timer); gate.timer = null; }
    if (gate.release) { const release = gate.release; gate.release = null; release(); }
    const first = !gate.released;
    gate.released = true;
    return first;
  }
  function reachM3eGate(gate, snapshot) {
    const pending = snapshot.pending;
    if (!gate?.armed || !pending || pending.txId !== gate.txId || pending.op !== gate.operation || pending.phase !== gate.phase || pending.targetVersion !== gate.targetVersion || pending.targetDigest !== gate.targetDigest) return;
    gate.armed = false;
    gate.reached = true;
    const stopped = new Promise((resolve) => { gate.release = resolve; });
    gate.resolveReached({ barrierReached: true, txId: pending.txId, operation: pending.op, targetVersion: pending.targetVersion, targetDigest: pending.targetDigest });
    if (gate.releaseRequested) releaseM3eGate(gate);
    return stopped;
  }
  function armM3eGate(input) {
    let resolveReached;
    const reachedPromise = new Promise((resolve) => { resolveReached = resolve; });
    return { armed: true, reached: false, released: false, releaseRequested: false, timedOut: false, txId: input.txId, operation: input.operation, targetVersion: input.targetVersion ?? null, targetDigest: input.targetDigest ?? null, phase: input.phase ?? "OLD_STOPPED", resolveReached, reachedPromise, release: null, timer: null, operationPromise: null };
  }
  function safeM3eTerminal(outcome, host) {
    const statuses = ["succeeded", "failed_restored", "failed_quarantined", "storage_failed"];
    if (!outcome || !statuses.includes(outcome.status)) throw new Error("invalid M3e terminal outcome");
    const diagnostics = host.diagnostics();
    return { settled: true, rejected: false, outcome: { txId: outcome.txId, status: outcome.status, expectedRevision: outcome.request.expectedRevision, revision: diagnostics.revision, selectedVersion: diagnostics.selected?.version ?? null, selectedDigest: diagnostics.selectedPackageSha256, engineSelectedVersion: outcome.selectedVersion } };
  }
  async function startM3eGate(gate, input, host) {
    if (!gate?.armed || input.txId !== gate.txId || input.operation !== gate.operation || (input.targetVersion ?? null) !== gate.targetVersion || (input.targetDigest ?? null) !== gate.targetDigest) throw new Error("M3e operation does not match its armed barrier");
    const timeoutMs = input.timeoutMs;
    if (!Number.isInteger(timeoutMs) || timeoutMs <= 0 || timeoutMs > 30000) throw new Error("invalid M3e gate deadline");
    gate.operationPromise = Promise.resolve().then(() => host[input.operation](input.txId, input.expectedRevision, input.packageSha256)).then((outcome) => safeM3eTerminal(outcome, host), () => ({ settled: true, rejected: true, outcome: null }));
    const timeout = new Promise((resolve) => {
      gate.timer = setTimeout(() => { gate.timedOut = true; releaseM3eGate(gate); resolve({ barrierReached: false, timedOut: true }); }, timeoutMs);
    });
    const result = await Promise.race([gate.reachedPromise, gate.operationPromise.then((terminal) => ({ barrierReached: false, terminal })), timeout]);
    if (!result.barrierReached) releaseM3eGate(gate);
    return result;
  }
  async function awaitM3eGate(gate, host, timeoutMs) {
    if (!gate?.operationPromise || !Number.isInteger(timeoutMs) || timeoutMs <= 0 || timeoutMs > 30000) throw new Error("invalid M3e terminal wait");
    let timer;
    try {
      return await Promise.race([
        gate.operationPromise.then((terminal) => gate.timedOut ? { timedOut: true } : { terminal, cleanup: host.cleanupDiagnosticsForTests() }),
        new Promise((resolve) => { timer = setTimeout(() => { releaseM3eGate(gate); resolve({ timedOut: true }); }, timeoutMs); }),
      ]);
    } finally { if (timer !== undefined) clearTimeout(timer); }
  }
`;
