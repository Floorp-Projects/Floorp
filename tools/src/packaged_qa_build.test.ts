// SPDX-License-Identifier: MPL-2.0
import { assertEquals, assertThrows } from "@std/assert";
import * as path from "@std/path";
import { packagedQaEnabled, registerPackagedQa } from "./packaged_qa_build.ts";

Deno.test("QA opt-in cannot silently alter normal or development builds", () => {
  assertEquals(packagedQaEnabled("production", undefined, undefined), false);
  assertEquals(packagedQaEnabled("production", "", ""), false);
  assertEquals(
    packagedQaEnabled("production", "1", '["selected.test.ts"]'),
    true,
  );
  for (
    const [mode, flag, modules] of [
      ["production", "", '["selected.test.ts"]'],
      ["production", "1", "[]"],
      ["production", "1", "null"],
      ["production", "1", '[""]'],
      ["dev", "1", '["selected.test.ts"]'],
      ["test", "1", '["selected.test.ts"]'],
    ]
  ) assertThrows(() => packagedQaEnabled(mode, flag, modules));
});

Deno.test("pre-mach registration retains production resources and rejects stale QA", () => {
  const directory = Deno.makeTempDirSync({ prefix: "floorp-packaged-qa-" });
  const canonical = Deno.realPathSync(directory);
  try {
    const manifestPath = path.join(directory, "jar.mn");
    const production =
      "noraneko.jar:\n% content noraneko %nora-content/ contentaccessible=yes\n% override original target\n nora-content/core.js (core.js)";
    Deno.writeTextFileSync(manifestPath, production);
    Deno.mkdirSync(path.join(directory, "qa/chunks"), { recursive: true });
    Deno.writeTextFileSync(
      path.join(directory, "qa/packaged-tests.js"),
      "entry",
    );
    assertThrows(() => registerPackagedQa(directory));
    assertEquals(Deno.readTextFileSync(manifestPath), production);
    Deno.writeTextFileSync(
      path.join(directory, "qa/qa-test-manifest.json"),
      "{}",
    );
    Deno.writeTextFileSync(
      path.join(directory, "qa/chunks/selected-123.js"),
      "chunk",
    );
    assertEquals(registerPackagedQa(directory), [
      "qa/chunks/selected-123.js",
      "qa/packaged-tests.js",
      "qa/qa-test-manifest.json",
    ]);
    const registered = Deno.readTextFileSync(manifestPath);
    assertEquals(
      registered,
      production +
        "\n nora-content/qa/chunks/selected-123.js (qa/chunks/selected-123.js)\n nora-content/qa/packaged-tests.js (qa/packaged-tests.js)\n nora-content/qa/qa-test-manifest.json (qa/qa-test-manifest.json)\n",
    );
    assertThrows(() => registerPackagedQa(directory));
    assertEquals(Deno.readTextFileSync(manifestPath), registered);
  } finally {
    assertEquals(Deno.realPathSync(directory), canonical);
    Deno.removeSync(canonical, { recursive: true });
  }
});

Deno.test("actual package workflow guard prevents signed, reusable or release QA runs", () => {
  const workflow = Deno.readTextFileSync(
    new URL("../../.github/workflows/package.yml", import.meta.url),
  ).replaceAll("\r\n", "\n");
  const start = workflow.indexOf("      - name: Restrict packaged QA");
  const end = workflow.indexOf(
    "      - name: Require Runtime artifact token",
    start,
  );
  const script = workflow.slice(start, end).split("          script: |\n")[1]
    .split("\n").map((line) => line.slice(12)).join("\n");
  const execute = new Function("context", "process", script);
  const env = {
    QA_MODULES: '["selected.test.ts"]',
    QA_MODE: "stable-v2-validation",
    QA_SKIP_SIGNING: "true",
    QA_SIGNED_VALIDATION: "false",
    QA_PLATFORM: "Linux-x64",
  };
  execute({ eventName: "workflow_dispatch" }, { env });
  for (
    const override of [
      { QA_MODE: "stable-v2" },
      { QA_MODE: "legacy" },
      { QA_SKIP_SIGNING: "false" },
      { QA_SIGNED_VALIDATION: "true" },
      { QA_PLATFORM: "Windows-x64" },
      { QA_MODULES: "[]" },
      { QA_MODULES: "{}" },
    ]
  ) {
    assertThrows(() =>
      execute({ eventName: "workflow_dispatch" }, {
        env: { ...env, ...override },
      })
    );
  }
  assertThrows(() => execute({ eventName: "workflow_call" }, { env }));
});
