// SPDX-License-Identifier: MPL-2.0

import assert from "node:assert/strict";
import test from "node:test";
import {
  createNodeBuildPlan,
  normalizeNodeBuildMode,
  PAGE_BUILDS,
} from "./src/node_build_plan.ts";

const baseOptions = {
  buildid2: "build-id",
  version: "1.2.3",
  projectRoot: "/repo",
  nodeExecutable: "/bin/node",
  tsdownExecutable: "/repo/node_modules/tsdown/dist/run.mjs",
};

test("dev plan preserves the existing three-package build order", () => {
  const plan = createNodeBuildPlan({ ...baseOptions, mode: "dev" });
  assert.deepEqual(
    plan.commands.map((entry) => entry.name),
    ["bridge/startup", "bridge/loader-modules", "bridge/loader-features"],
  );
  assert.equal(plan.usesPackagedAssets, false);
  assert.equal(plan.packagedQa, false);
});

test("production plan builds every page and loader-modules last", () => {
  const plan = createNodeBuildPlan({ ...baseOptions, mode: "production" });
  assert.equal(plan.commands.length, PAGE_BUILDS.length + 3);
  assert.equal(plan.commands[0]?.name, "bridge/startup");
  assert.equal(plan.commands[1]?.name, "bridge/loader-features");
  assert.equal(plan.commands.at(-1)?.name, "bridge/loader-modules");
  assert.deepEqual(
    plan.commands.slice(2, -1).map((entry) => entry.name),
    PAGE_BUILDS.map(([name]) => name),
  );
  assert.equal(plan.usesPackagedAssets, true);
});

test("packaged QA uses the test-packaged startup mode and appends the QA build", () => {
  const plan = createNodeBuildPlan({
    ...baseOptions,
    mode: "production",
    packagedQa: true,
  });
  assert.equal(plan.packagedQa, true);
  assert.equal(plan.commands.length, PAGE_BUILDS.length + 4);
  assert.equal(
    plan.commands[0]?.args.some((arg) => arg === "--env.MODE=test-packaged"),
    true,
  );
  assert.equal(
    plan.commands.at(-1)?.args.includes("vite.packaged-test.config.ts"),
    true,
  );
});

test("build commands contain no Deno executable or task indirection", () => {
  for (const mode of ["dev", "production"]) {
    const plan = createNodeBuildPlan({ ...baseOptions, mode });
    for (const command of plan.commands) {
      assert.equal(command.command, "/bin/node");
      assert.equal(command.args.includes("deno"), false);
      assert.equal(command.args.includes("task"), false);
    }
  }
});

test("mode validation rejects unsupported modes", () => {
  assert.equal(normalizeNodeBuildMode(" Production "), "production");
  assert.throws(
    () => normalizeNodeBuildMode("watch"),
    /Unsupported build mode/,
  );
});
