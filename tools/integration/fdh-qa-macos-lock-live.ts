// SPDX-License-Identifier: MPL-2.0
// Explicit live check of the macOS QA target lock (KAN-144): task A acquires
// and re-acquires; task B is refused for acquire and cleanup; preflight shows
// the holder; A's cleanup releases it. Exits 1 on any unexpected outcome.
// Run:
//   deno run --allow-read --allow-run=ssh \
//     tools/integration/fdh-qa-macos-lock-live.ts <target.json>
import {
  createSshTransport,
  MacosQaError,
  MacosQaTarget,
  parseMacosQaTargetConfig,
} from "../src/fdh_qa_macos.ts";

const file = Deno.args[0];
if (file === undefined) throw new Error("usage: <target.json>");
const config = parseMacosQaTargetConfig(
  JSON.parse(await Deno.readTextFile(file)),
);
const target = new MacosQaTarget(config, createSshTransport(config));
const refused = async (fn: () => Promise<unknown>) => {
  try {
    await fn();
    return "UNEXPECTED SUCCESS";
  } catch (error) {
    return error instanceof MacosQaError ? error.result : String(error);
  }
};
const out: Record<string, unknown> = {};
try {
  out.acquireA = await target.acquire("kan-144-lock-a");
  out.reacquireA = await target.acquire("kan-144-lock-a");
  out.acquireB = await refused(() => target.acquire("kan-144-lock-b"));
  out.preflightWhileHeld = await target.preflight();
  out.cleanupB = await refused(() => target.cleanup("kan-144-lock-b"));
} finally {
  // Release A's lock even when a step above threw.
  out.cleanupA = await target.cleanup("kan-144-lock-a");
}
out.preflightAfter = await target.preflight();
const lockError = (value: unknown) =>
  typeof value === "object" && value !== null &&
  (value as Record<string, unknown>).error === "lock-held";
const held = out.preflightWhileHeld as { lockHolder: string | null };
const after = out.preflightAfter as { lockHolder: string | null };
const ok = lockError(out.acquireB) && lockError(out.cleanupB) &&
  held.lockHolder === "kan-144-lock-a" &&
  (out.cleanupA as { clean: boolean }).clean && after.lockHolder === null;
out.passed = ok;
console.log(JSON.stringify(out, null, 1));
Deno.exit(ok ? 0 : 1);
