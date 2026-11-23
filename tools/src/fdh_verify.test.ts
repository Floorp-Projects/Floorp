// SPDX-License-Identifier: MPL-2.0

import { assertEquals, assertThrows } from "@std/assert";
import type { RecordedCommandSpec } from "./fdh_task.ts";
import {
  BUILD_PHASES,
  type BuildPhase,
  buildVerifyCommands,
  type BuildVerifyCommandSpec,
  isBuildPhase,
  parseBuildPhase,
} from "./fdh_verify.ts";

Deno.test("parseBuildPhase defaults to all and accepts each documented phase", () => {
  assertEquals(parseBuildPhase(), "all");
  for (const phase of BUILD_PHASES) {
    assertEquals(parseBuildPhase(phase), phase);
  }
  assertEquals(BUILD_PHASES.length, 3);
});

Deno.test("parseBuildPhase rejects empty and unknown phases", () => {
  for (const raw of ["", "after", "ALL", "before-mach ", "inject"]) {
    const error: unknown = assertThrows(() => parseBuildPhase(raw));
    assertEquals(error instanceof Error, true);
    const message = (error as Error).message;
    assertEquals(message.includes("Unknown build phase"), true);
    assertEquals(message.includes("before-mach|after-mach"), true);
  }
});

Deno.test("isBuildPhase accepts only exact documented values", () => {
  assertEquals(isBuildPhase("all"), true);
  assertEquals(isBuildPhase("before-mach"), true);
  assertEquals(isBuildPhase("after-mach"), true);
  assertEquals(isBuildPhase(""), false);
  assertEquals(isBuildPhase("all "), false);
});

Deno.test("buildVerifyCommands(all) orders source compile before inject", () => {
  const specs = buildVerifyCommands("all");
  assertEquals(specs.length, 2);
  assertEquals(
    specs.map((spec) => spec.displayCommand),
    [
      "deno task feles-build build --phase before-mach",
      "deno task feles-build build --phase after-mach",
    ],
  );
  assertEquals(
    specs.map((spec) => spec.assertionName),
    ["build-exit-code", "build-inject-exit-code"],
  );
  assertEquals(specs[0].args, [
    "task",
    "feles-build",
    "build",
    "--phase",
    "before-mach",
  ]);
  assertEquals(specs[1].args, [
    "task",
    "feles-build",
    "build",
    "--phase",
    "after-mach",
  ]);
  for (const spec of specs) {
    assertEquals(spec.expected, "exit code 0");
  }
});

Deno.test("buildVerifyCommands(before-mach) runs source compile only", () => {
  const specs = buildVerifyCommands("before-mach");
  assertEquals(specs.length, 1);
  assertEquals(specs[0].assertionName, "build-exit-code");
  assertEquals(specs[0].args.slice(0, 2), ["task", "feles-build"]);
  assertEquals(specs[0].args.includes("--phase"), true);
  assertEquals(specs[0].args[4], "before-mach");
});

Deno.test("buildVerifyCommands(after-mach) records the inject-only assertion", () => {
  const specs = buildVerifyCommands("after-mach");
  assertEquals(specs.length, 1);
  assertEquals(specs[0].assertionName, "build-inject-exit-code");
  assertEquals(specs[0].args[4], "after-mach");
});

Deno.test("buildVerifyCommands returns fresh mutable specs per call", () => {
  const first = buildVerifyCommands("all");
  first[0].args.push("--extra");
  first[0].assertionName = "mutated";
  const second = buildVerifyCommands("all");
  assertEquals(second[0].args, [
    "task",
    "feles-build",
    "build",
    "--phase",
    "before-mach",
  ]);
  assertEquals(second[0].assertionName, "build-exit-code");
});

Deno.test("buildVerifyCommands output composes into a RecordedCommandSpec", () => {
  const specs: BuildVerifyCommandSpec[] = buildVerifyCommands("all");
  for (const spec of specs) {
    const full: RecordedCommandSpec = {
      command: "/deno",
      cwd: "/tmp/worktree",
      ...spec,
    };
    assertEquals(full.displayCommand, spec.displayCommand);
    assertEquals(full.args, spec.args);
    assertEquals(full.assertionName, spec.assertionName);
    assertEquals(full.expected, "exit code 0");
    assertEquals(full.command, "/deno");
    assertEquals(full.cwd, "/tmp/worktree");
  }
  assertEquals(
    specs.map((spec) => spec.displayCommand) satisfies string[],
    specs.map((spec) => spec.displayCommand),
  );
});

Deno.test("parseBuildPhase + buildVerifyCommands cover the CLI contract", () => {
  const byDefault: BuildPhase = parseBuildPhase();
  const defaultSpecs = buildVerifyCommands(byDefault);
  assertEquals(defaultSpecs.length, 2);
  const afterOnly = buildVerifyCommands(parseBuildPhase("after-mach"));
  assertEquals(afterOnly.length, 1);
  assertEquals(afterOnly[0].assertionName, "build-inject-exit-code");
});
