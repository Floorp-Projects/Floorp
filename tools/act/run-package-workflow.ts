#!/usr/bin/env -S deno run --allow-run --allow-env --allow-read --allow-write

import { parseArgs } from "jsr:@std/cli@1.0.6/parse-args";

const DEFAULT_PLATFORMS = ["Linux-x64"] as const;

type Platform = (typeof DEFAULT_PLATFORMS)[number];

const parsed = parseArgs(Deno.args, {
  string: [
    "platform",
    "platforms",
    "workflow",
    "ref",
    "runtime-run-id",
    "gh-token",
    "act-path",
    "platform-image",
    "job",
  ],
  boolean: ["beta", "skip-signing", "help", "dry-run", "continue-on-error"],
  default: {
    workflow: ".github/workflows/package.yml",
    ref: "main",
    beta: false,
    "skip-signing": true,
    "runtime-run-id": "",
    "act-path": "act",
    "platform-image": "ubuntu-latest=ghcr.io/catthehacker/ubuntu:full-latest",
    job: "main",
    "dry-run": false,
    "continue-on-error": false,
  },
});

if (parsed.help) {
  printHelp();
  Deno.exit(0);
}

const extraArgs = parsed._.map(String);
const platforms = resolvePlatforms(parsed.platforms ?? parsed.platform);

if (platforms.length === 0) {
  console.error(
    "No platforms selected. Specify with --platforms or --platform.",
  );
  Deno.exit(1);
}

let githubToken: string;
try {
  githubToken = await resolveGithubToken(String(parsed["gh-token"] ?? ""));
} catch (error) {
  console.error(`Failed to acquire GitHub token via gh CLI: ${error}`);
  console.error("Run 'gh auth login' or pass --gh-token your_token_here.");
  Deno.exit(1);
}

const workflowPath = String(parsed.workflow);
const ref = String(parsed.ref);
const runtimeRunId = String(parsed["runtime-run-id"] ?? "");
const jobName = String(parsed.job);
const betaFlag = Boolean(parsed.beta);
const skipSigning = Boolean(parsed["skip-signing"]);
const actExecutable = String(parsed["act-path"]);
const platformImage = String(parsed["platform-image"]);
const dryRun = Boolean(parsed["dry-run"]);
const continueOnError = Boolean(parsed["continue-on-error"]);

const failures: Array<
  { platform: Platform; code: number } | { platform: Platform; error: Error }
> = [];

for (const platform of platforms) {
  console.log(`\n=== Running act for platform: ${platform} ===`);
  const payload = buildEventPayload({
    ref,
    platform,
    beta: betaFlag,
    skipSigning,
    runtimeRunId,
  });

  const payloadPath = await Deno.makeTempFile({
    prefix: `act-event-${platform.replaceAll(/[^a-zA-Z0-9]/g, "-")}-`,
    suffix: ".json",
  });
  await Deno.writeTextFile(payloadPath, JSON.stringify(payload, null, 2));

  const args = [
    "-W",
    workflowPath,
    "-e",
    payloadPath,
    "-j",
    jobName,
    "-P",
    platformImage,
    "-s",
    `GITHUB_TOKEN=${githubToken}`,
    ...extraArgs,
  ];

  console.log(`act ${args.join(" ")}`);

  if (dryRun) {
    console.log("Dry-run requested; skipping execution.");
    await safeRemove(payloadPath);
    continue;
  }

  try {
    const command = new Deno.Command(actExecutable, {
      args,
      stdout: "inherit",
      stderr: "inherit",
    });
    const { code } = await command.spawn().status;
    if (code !== 0) {
      console.error(`act exited with code ${code} for platform ${platform}`);
      failures.push({ platform, code });
      if (!continueOnError) {
        await safeRemove(payloadPath);
        break;
      }
    }
  } catch (error) {
    console.error(`Failed to execute act for platform ${platform}:`, error);
    failures.push({ platform, error: error as Error });
    if (!continueOnError) {
      await safeRemove(payloadPath);
      break;
    }
  }

  await safeRemove(payloadPath);
}

if (failures.length > 0) {
  console.error("\nSome act runs failed:");
  for (const failure of failures) {
    if ("code" in failure) {
      console.error(`  - ${failure.platform}: exit code ${failure.code}`);
    } else {
      console.error(`  - ${failure.platform}: ${failure.error.message}`);
    }
  }
  Deno.exit(1);
}

console.log("\nAll requested act runs completed successfully.");

function resolvePlatforms(value?: string | string[]): Platform[] {
  if (!value) {
    return [...DEFAULT_PLATFORMS];
  }
  const raw = Array.isArray(value)
    ? value.flatMap((v) => v.split(","))
    : value.split(",");
  const cleaned = raw.map((entry) => entry.trim()).filter(Boolean);
  const unknown = cleaned.filter(
    (entry) => !DEFAULT_PLATFORMS.includes(entry as Platform),
  );
  if (unknown.length > 0) {
    console.warn(`Warning: unknown platforms ignored -> ${unknown.join(", ")}`);
  }
  const known = cleaned.filter((entry): entry is Platform =>
    DEFAULT_PLATFORMS.includes(entry as Platform),
  );
  return Array.from(new Set(known));
}

async function resolveGithubToken(provided: string): Promise<string> {
  if (provided) {
    return provided.trim();
  }
  const command = new Deno.Command("gh", {
    args: ["auth", "token"],
    stdout: "piped",
    stderr: "piped",
  });
  const { code, stdout, stderr } = await command.output();
  if (code !== 0) {
    const errorText = new TextDecoder().decode(stderr).trim();
    throw new Error(errorText || `gh auth token exited with code ${code}`);
  }
  const token = new TextDecoder().decode(stdout).trim();
  if (!token) {
    throw new Error("gh auth token returned an empty token");
  }
  return token;
}

function buildEventPayload({
  ref,
  platform,
  beta,
  skipSigning,
  runtimeRunId,
}: {
  ref: string;
  platform: Platform;
  beta: boolean;
  skipSigning: boolean;
  runtimeRunId: string;
}) {
  return {
    ref,
    inputs: {
      platform,
      beta: String(beta),
      runtime_artifact_workflow_run_id: runtimeRunId,
      skip_signing: String(skipSigning),
    },
  };
}

async function safeRemove(path: string) {
  try {
    await Deno.remove(path);
  } catch (error) {
    if (!(error instanceof Deno.errors.NotFound)) {
      console.warn(`Warning: failed to remove temp file ${path}:`, error);
    }
  }
}

function printHelp() {
  console.log(
    `run-package-workflow.ts - Deno helper to execute package.yml via act

USAGE:
  deno run --allow-run --allow-env --allow-read --allow-write tools/act/run-package-workflow.ts [options] [-- extra act args]

OPTIONS:
  --platforms <csv>        Comma-separated list of platforms to test (default: Linux-x64,Linux-aarch64)
  --platform <name>        Repeatable alternative to --platforms
  --workflow <path>        Workflow file path (default: .github/workflows/package.yml)
  --ref <git-ref>          Ref used in the payload (default: main)
  --runtime-run-id <id>    Reuse runtime artifact workflow run ID (default: empty)
  --beta                   Enable beta mode (default: false)
  --skip-signing           Skip signing steps (default: true)
  --act-path <path>        act executable (default: act in PATH)
  --platform-image <map>   Runner image map for act -P (default: ubuntu-latest=catthehacker/ubuntu:act-latest)
  --gh-token <token>       Provide GitHub token directly (otherwise gh auth token is used)
  --job <name>             Workflow job name to run (default: main)
  --dry-run                Print commands without executing act
  --continue-on-error      Keep running other platforms even if one fails
  --help                   Show this message

EXAMPLES:
  deno run -A tools/act/run-package-workflow.ts
  deno run -A tools/act/run-package-workflow.ts --platforms Linux-x64,macOS-x64 --beta --skip-signing=false
  deno run -A tools/act/run-package-workflow.ts -- --pull
`,
  );
}
