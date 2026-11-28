// SPDX-License-Identifier: MPL-2.0

import assert from "node:assert/strict";
import { spawnSync } from "node:child_process";
import { readFileSync } from "node:fs";
import path from "node:path";
import test from "node:test";
import { fileURLToPath } from "node:url";
import {
  createNodeBuildPlan,
  normalizeNodeBuildMode,
  PACKAGED_ASSET_MOUNTS,
  PAGE_BUILDS,
} from "./src/node_build_plan.ts";
import { SOURCE_LINKS } from "./src/source_link_plan.ts";

const baseOptions = {
  buildid2: "019b23c1-2278-7000-8000-000000000000",
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
  const expectedPages = [
    "pages-newtab",
    "pages-settings",
    "pages-welcome",
    "pages-notes",
    "pages-modal-child",
    "pages-profile-manager",
    "pages-aboutDialog",
  ];
  assert.equal(plan.commands.length, expectedPages.length + 3);
  assert.equal(plan.commands[0]?.name, "bridge/startup");
  assert.equal(plan.commands[1]?.name, "bridge/loader-features");
  assert.equal(plan.commands.at(-1)?.name, "bridge/loader-modules");
  assert.deepEqual(
    plan.commands.slice(2, -1).map((entry) => entry.name),
    expectedPages,
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

test("source links cover chrome, i18n, and modules before compilation", () => {
  assert.deepEqual(SOURCE_LINKS, [
    {
      link: ["bridge", "loader-features", "link-features-chrome"],
      target: ["browser-features", "chrome"],
    },
    {
      link: ["bridge", "loader-features", "link-i18n"],
      target: ["i18n"],
    },
    {
      link: ["bridge", "loader-modules", "link-modules"],
      target: ["browser-features", "modules"],
    },
  ]);
});

test("packaged asset mounts cover every page exactly once and match moz.build", () => {
  const pageMounts = PACKAGED_ASSET_MOUNTS.filter(([name]) =>
    name.startsWith("pages-")
  );
  assert.deepEqual(
    pageMounts.map(([name]) => name),
    PAGE_BUILDS.map(([name]) => name),
  );

  const repositoryRoot = path.resolve(
    path.dirname(fileURLToPath(import.meta.url)),
    "..",
  );
  const mozBuild = readFileSync(path.join(repositoryRoot, "moz.build"), "utf8");
  const mozBuildMounts = [...mozBuild.matchAll(/"_dist\/noraneko\/([^"]+)"/g)]
    .map((match) => match[1]);
  assert.deepEqual(
    [...PACKAGED_ASSET_MOUNTS.map(([name]) => name)].sort(),
    mozBuildMounts.sort(),
  );
});

test("CLI rejects a non-UUIDv7 build ID before building", () => {
  const script = path.join(
    path.dirname(fileURLToPath(import.meta.url)),
    "node-build.ts",
  );
  for (
    const buildid2 of [
      "nodephase1prod2",
      "019B23C1-2278-7000-8000-000000000000",
    ]
  ) {
    const result = spawnSync(
      process.execPath,
      [script, "--buildid2", buildid2],
      { encoding: "utf8" },
    );
    assert.notEqual(result.status, 0);
    assert.match(result.stderr, /UUIDv7/);
  }
});

test("mode validation rejects unsupported modes", () => {
  assert.equal(normalizeNodeBuildMode(" Production "), "production");
  assert.throws(
    () => normalizeNodeBuildMode("watch"),
    /Unsupported build mode/,
  );
});
