// SPDX-License-Identifier: MPL-2.0

import { parseArgs } from "@std/cli";
import { PROJECT_ROOT } from "./defines.ts";

type SmokeStep = {
  name: string;
  args: string[];
};

type SmokeMode = "all" | "unit" | "runtime";

const HELP = `
Usage: deno run -A tools/src/smoke_runner.ts [options]

Options:
  --mode <all|unit|runtime>   Run subset of smoke suite (default: all)
  --help, -h                  Show this help
`.trim();

function resolveMode(value: string | undefined): SmokeMode {
  if (!value) {
    return "all";
  }

  const normalized = value.toLowerCase();
  if (
    normalized === "all" ||
    normalized === "unit" ||
    normalized === "runtime"
  ) {
    return normalized;
  }

  // Backward compatibility: old browser mode now maps to runtime-only checks.
  if (normalized === "browser") {
    return "runtime";
  }

  throw new Error(
    `Invalid --mode value: ${value}. Allowed: all, unit, runtime`,
  );
}

function createSmokeSteps(mode: SmokeMode): SmokeStep[] {
  const unitSteps: SmokeStep[] = [
    {
      name: "colocated runner unit tests",
      args: ["test", "tools/src/colocated_test_runner.test.ts"],
    },
  ];

  const runtimeSteps: SmokeStep[] = [
    {
      name: "runtime scan: floorp chrome test targets",
      args: ["task", "test", "--list", "--layer", "chrome"],
    },
    {
      name: "runtime scan: floorp esm test targets",
      args: ["task", "test", "--list", "--layer", "esm"],
    },
    {
      name: "runtime scan: floorp pages test targets",
      args: ["task", "test", "--list", "--layer", "pages"],
    },
    {
      name: "runtime check: floorp source directories",
      args: ["check", "tools", "bridge", "browser-features", "i18n", "libs"],
    },
    {
      name: "runtime lint: floorp source directories",
      args: ["lint", "tools", "bridge", "browser-features", "i18n", "libs"],
    },
  ];

  if (mode === "unit") {
    return unitSteps;
  }

  if (mode === "runtime") {
    return runtimeSteps;
  }

  return [...unitSteps, ...runtimeSteps];
}

async function runStep(denoPath: string, step: SmokeStep): Promise<void> {
  console.log(`\n[smoke] ${step.name}`);
  console.log(`[smoke] > deno ${step.args.join(" ")}`);

  const child = new Deno.Command(denoPath, {
    args: step.args,
    cwd: PROJECT_ROOT,
    stdout: "inherit",
    stderr: "inherit",
  }).spawn();

  const status = await child.status;
  if (!status.success) {
    throw new Error(`Step failed (${step.name}), exit code: ${status.code}`);
  }
}

async function main(): Promise<void> {
  const parsed = parseArgs(Deno.args, {
    string: ["mode"],
    boolean: ["help"],
    alias: {
      h: "help",
    },
    default: {
      mode: "all",
    },
  });

  if (parsed.help) {
    console.log(HELP);
    return;
  }

  const mode = resolveMode(parsed.mode);
  const steps = createSmokeSteps(mode);
  const denoPath = Deno.execPath();
  const failures: string[] = [];

  const startedAt = Date.now();
  for (const step of steps) {
    try {
      await runStep(denoPath, step);
    } catch (error) {
      const message = error instanceof Error ? error.message : String(error);
      failures.push(message);
      console.error(`[smoke] step failed: ${message}`);
      console.error("[smoke] continuing with remaining steps...");
    }
  }

  const elapsed = Date.now() - startedAt;
  console.log(`\n[smoke] complete (${steps.length} step(s), ${elapsed}ms)`);

  if (failures.length > 0) {
    throw new Error(
      `Smoke failed in ${failures.length} step(s): ${failures.join(" | ")}`,
    );
  }
}

if (import.meta.main) {
  try {
    await main();
    Deno.exit(0);
  } catch (error) {
    const message = error instanceof Error ? error.message : String(error);
    console.error(`\n[smoke] failed: ${message}`);
    Deno.exit(1);
  }
}
