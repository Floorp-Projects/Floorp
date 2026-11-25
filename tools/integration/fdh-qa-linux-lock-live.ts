// SPDX-License-Identifier: MPL-2.0
// Explicit live check of the Linux container QA target lock (KAN-144): task A
// creates the container and re-acquires as a no-op; task B is refused for
// acquire and cleanup; preflight shows A as holder; A's cleanup removes the
// container. Needs the image to exist. Exits 1 on any unexpected outcome.
// Run:
//   deno run --allow-read --allow-run=ssh \
//     tools/integration/fdh-qa-linux-lock-live.ts <target.json>
import {
  createLinuxTransport,
  LinuxQaError,
  LinuxQaTarget,
  parseLinuxQaTargetConfig,
} from "../src/fdh_qa_linux.ts";

const file = Deno.args[0];
if (file === undefined) throw new Error("usage: <target.json>");
const config = parseLinuxQaTargetConfig(
  JSON.parse(await Deno.readTextFile(file)),
);
const target = await LinuxQaTarget.create(config, createLinuxTransport(config));
const refused = async (fn: () => Promise<unknown>) => {
  try {
    await fn();
    return "UNEXPECTED SUCCESS";
  } catch (error) {
    return error instanceof LinuxQaError ? error.result : String(error);
  }
};
const out: Record<string, unknown> = { image: target.image };
try {
  out.acquireA = await target.acquire("kan-144-lock-a");
  out.reacquireA = await target.acquire("kan-144-lock-a");
  out.acquireB = await refused(() => target.acquire("kan-144-lock-b"));
  out.preflightWhileHeld = await target.preflight();
  out.cleanupB = await refused(() => target.cleanup("kan-144-lock-b"));
} finally {
  // Remove A's container even when a step above threw.
  out.cleanupA = await target.cleanup("kan-144-lock-a");
}
out.preflightAfter = await target.preflight();
const held = (value: unknown) =>
  typeof value === "object" && value !== null &&
  (value as Record<string, unknown>).error === "container-held";
const during = out.preflightWhileHeld as { containerHolder: string | null };
const after = out.preflightAfter as { containerHolder: string | null };
const ok = held(out.acquireB) && held(out.cleanupB) &&
  during.containerHolder === "kan-144-lock-a" &&
  (out.cleanupA as { clean: boolean }).clean && after.containerHolder === null;
out.passed = ok;
console.log(JSON.stringify(out, null, 1));
Deno.exit(ok ? 0 : 1);
