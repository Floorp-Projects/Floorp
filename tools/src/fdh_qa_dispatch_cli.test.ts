// SPDX-License-Identifier: MPL-2.0
import { assert, assertEquals } from "@std/assert";
import { runQaCli } from "./fdh_qa_cli.ts";
import type { QaCliTestDependencies } from "./fdh_qa_cli_types.ts";
const flags = [
  "--task-id",
  "qa-owned",
  "--agent",
  "lead",
  "--source-run-id",
  "17",
  "--source-attempt",
  "2",
  "--source-head",
  "a".repeat(40),
  "--qa-head",
  "b".repeat(40),
  "--mode",
  "unsigned-validation",
];
function fixture() {
  let calls = 0;
  let action = "";
  const deps: QaCliTestDependencies = {
    collect: () => Promise.reject(new Error("collector must not run")),
    assignment: (_request, selected) => {
      calls++;
      action = selected;
      return Promise.resolve({
        status: "assigned",
        key: "owned",
        runId: 23,
        runAttempt: 1,
        qaPassed: false,
        nativeQaVerified: false,
        publicationAuthorized: false,
      });
    },
  };
  return { deps, calls: () => calls, action: () => action };
}
Deno.test("QA assignment help and default plans never invoke runtime", async () => {
  const f = fixture();
  for (const verb of ["plan", "dispatch", "reconcile"]) {
    assertEquals((await runQaCli(["qa", verb, "--help"], f.deps)).exitCode, 0);
    const result = await runQaCli(["qa", verb, ...flags], f.deps);
    assertEquals(result.exitCode, 0);
    const body = JSON.parse(result.stdout);
    assertEquals(body.status, "planned");
    assertEquals(body.plan.ref, "main");
    assertEquals(body.qaPassed, false);
  }
  assertEquals(f.calls(), 0);
});
Deno.test("QA assignment explicit execute routes once and never claims QA pass", async () => {
  const f = fixture();
  const result = await runQaCli(
    ["qa", "dispatch", ...flags, "--execute"],
    f.deps,
  );
  assertEquals(result.exitCode, 0);
  assertEquals(f.calls(), 1);
  assertEquals(f.action(), "execute");
  const body = JSON.parse(result.stdout);
  assertEquals(body.status, "assigned");
  assertEquals(body.qaPassed, false);
  assertEquals(body.publicationAuthorized, false);
});
Deno.test("QA reconcile requires explicit execute for effects", async () => {
  const f = fixture();
  await runQaCli(["qa", "reconcile", ...flags, "--execute"], f.deps);
  assertEquals(f.action(), "reconcile");
  assertEquals(f.calls(), 1);
});
Deno.test("QA assignment duplicate or capability override arguments have zero effects", async () => {
  const f = fixture();
  for (
    const extra of [
      ["--execute", "--execute"],
      ["--task-id=other"],
      ["--repository=evil/repo"],
      ["--host=evil"],
      ["--command=sh"],
      ["--token=PRIVATE"],
    ]
  ) {
    const result = await runQaCli(
      ["qa", "dispatch", ...flags, ...extra],
      f.deps,
    );
    assertEquals(result.exitCode, 2);
    assert(!result.stdout.includes("PRIVATE"));
  }
  assertEquals(
    (await runQaCli(["qa", "plan", ...flags, "--execute"], f.deps)).exitCode,
    2,
  );
  assertEquals(f.calls(), 0);
});
Deno.test("QA assignment malformed source identity is refused before effects", async () => {
  const f = fixture();
  const invalid = [...flags];
  invalid[5] = "017";
  assertEquals(
    (await runQaCli(["qa", "dispatch", ...invalid, "--execute"], f.deps))
      .exitCode,
    2,
  );
  assertEquals(f.calls(), 0);
});
Deno.test("QA assignment transport exceptions never expose private details", async () => {
  const f = fixture();
  const result = await runQaCli(["qa", "dispatch", ...flags, "--execute"], {
    ...f.deps,
    assignment: () => Promise.reject(new Error("SENSITIVE-STDERR")),
  });
  assertEquals(result.exitCode, 1);
  assert(!result.stdout.includes("SENSITIVE"));
  assertEquals(JSON.parse(result.stdout).qaPassed, false);
});
