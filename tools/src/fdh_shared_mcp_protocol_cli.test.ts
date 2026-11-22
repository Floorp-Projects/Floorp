// SPDX-License-Identifier: MPL-2.0
import { assertEquals } from "@std/assert";
import { runProtocolSpikeCli } from "../integration/fdh_shared_mcp_spike.ts";
import type { ProtocolSpikeReport } from "../integration/fdh_shared_mcp_spike_types.ts";
const good: ProtocolSpikeReport = {
  scope: "protocol-only",
  identity: "nonGoogle",
  mcpAuthorizationGranted: false,
  ac05Qualified: false,
  experimental: true,
  audienceAndScopeAccepted: true,
  invalidTargetRejected: true,
  missingScopeRejected: true,
  wrongScopeRejected: true,
  expiredTokenRejected: true,
  unboundMachineMcpRejected: true,
};
const checks = [
  "audienceAndScopeAccepted",
  "invalidTargetRejected",
  "missingScopeRejected",
  "wrongScopeRejected",
  "expiredTokenRejected",
  "unboundMachineMcpRejected",
] as const;
Deno.test("protocol CLI passes only six successful checks, not qualification flags", async () => {
  assertEquals(
    await runProtocolSpikeCli(["--execute-protocol-only"], {
      execute: () => Promise.resolve({ ...good }),
      report: () => {},
      error: () => {},
    }),
    0,
  );
});
for (const key of checks) {
  Deno.test("protocol CLI fails when " + key + " is false", async () => {
    let reports = 0;
    assertEquals(
      await runProtocolSpikeCli(["--execute-protocol-only"], {
        execute: () => Promise.resolve({ ...good, [key]: false }),
        report: () => {
          reports++;
        },
        error: () => {},
      }),
      1,
    );
    assertEquals(reports, 1);
  });
}
Deno.test("protocol CLI rejects flags before execution", async () => {
  let ran = false;
  const result = await runProtocolSpikeCli(["--unknown"], {
    execute: () => {
      ran = true;
      return Promise.resolve({ ...good });
    },
    report: () => {},
    error: () => {},
  });
  assertEquals(result, 2);
  assertEquals(ran, false);
});
Deno.test("protocol CLI execution failure stays nonzero without reflecting error", async () => {
  let errors = 0;
  assertEquals(
    await runProtocolSpikeCli(["--execute-protocol-only"], {
      execute: () => Promise.reject(Error("PRIVATE_FIXTURE")),
      report: () => {
        throw Error("not reached");
      },
      error: () => {
        errors++;
      },
    }),
    1,
  );
  assertEquals(errors, 1);
});
