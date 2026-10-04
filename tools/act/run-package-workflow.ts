#!/usr/bin/env -S deno run --allow-run --allow-env --allow-read --allow-write

// deno-lint-ignore no-import-prefix
import { parseArgs } from "jsr:@std/cli@1.0.6/parse-args";

const DEFAULT_PLATFORMS = ["Linux-x64"] as const;
const PLATFORMS = [
  "Windows-x64",
  "Linux-x64",
  "Linux-aarch64",
  "macOS-x64",
] as const;

export type Platform = (typeof PLATFORMS)[number];

export async function main(args: string[] = Deno.args): Promise<number> {
  const parsed = parseArgs(args, {
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
      "platform-image": "ubuntu-22.04=ghcr.io/catthehacker/ubuntu:full-latest",
      job: "main",
      "dry-run": false,
      "continue-on-error": false,
    },
  });

  if (parsed.help) {
    printHelp();
    return 0;
  }

  if (parsed["gh-token"]) {
    console.error(
      "--gh-token was removed because command-line secrets are visible to other processes. " +
        "Use FLOORP_ACT_GITHUB_TOKEN or authenticate gh instead.",
    );
    return 2;
  }

  const extraArgs = parsed._.map(String);
  const platforms = resolvePlatforms(parsed.platforms ?? parsed.platform);
  if (platforms.length === 0) {
    console.error(
      "No platforms selected. Specify with --platforms or --platform.",
    );
    return 1;
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

  let githubToken = "";
  if (!dryRun) {
    try {
      githubToken = await resolveGithubToken();
    } catch (error) {
      console.error(`Failed to acquire a GitHub token: ${error}`);
      console.error(
        "Set FLOORP_ACT_GITHUB_TOKEN or run 'gh auth login'. The token is never printed.",
      );
      return 1;
    }
  }

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

    const actArgs = buildActArguments({
      workflowPath,
      payloadPath,
      jobName,
      platformImage,
      extraArgs,
    });
    console.log(formatActCommand(actArgs, extraArgs.length));

    if (dryRun) {
      console.log(
        "Dry-run requested; skipping token acquisition and act execution.",
      );
      await safeRemove(payloadPath);
      continue;
    }

    try {
      const command = new Deno.Command(actExecutable, {
        args: actArgs,
        env: { GITHUB_TOKEN: githubToken },
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
    return 1;
  }

  console.log("\nAll requested act runs completed successfully.");
  return 0;
}

export function resolvePlatforms(value?: string | string[]): Platform[] {
  if (!value) {
    return [...DEFAULT_PLATFORMS];
  }
  const raw = Array.isArray(value)
    ? value.flatMap((entry) => entry.split(","))
    : value.split(",");
  const cleaned = raw.map((entry) => entry.trim()).filter(Boolean);
  const unknown = cleaned.filter(
    (entry) => !PLATFORMS.includes(entry as Platform),
  );
  if (unknown.length > 0) {
    console.warn(`Warning: unknown platforms ignored -> ${unknown.join(", ")}`);
  }
  const known = cleaned.filter((entry): entry is Platform =>
    PLATFORMS.includes(entry as Platform)
  );
  return Array.from(new Set(known));
}

export function buildActArguments({
  workflowPath,
  payloadPath,
  jobName,
  platformImage,
  extraArgs,
}: {
  workflowPath: string;
  payloadPath: string;
  jobName: string;
  platformImage: string;
  extraArgs: string[];
}): string[] {
  return [
    "-W",
    workflowPath,
    "-e",
    payloadPath,
    "-j",
    jobName,
    "-P",
    platformImage,
    "-s",
    "GITHUB_TOKEN",
    ...extraArgs,
  ];
}

export function formatActCommand(
  args: string[],
  extraArgumentCount: number,
): string {
  const visibleCount = Math.max(0, args.length - extraArgumentCount);
  const visible = args.slice(0, visibleCount).map(shellDisplayArgument).join(
    " ",
  );
  const suffix = extraArgumentCount > 0
    ? ` [${extraArgumentCount} additional argument(s) omitted]`
    : "";
  return `act ${visible}${suffix}`;
}

function shellDisplayArgument(value: string): string {
  if (/^[A-Za-z0-9_./:=@+-]+$/.test(value)) return value;
  return JSON.stringify(value);
}

async function resolveGithubToken(): Promise<string> {
  const fromEnvironment = Deno.env.get("FLOORP_ACT_GITHUB_TOKEN")?.trim();
  if (fromEnvironment) return fromEnvironment;

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
  if (!token) throw new Error("gh auth token returned an empty token");
  return token;
}

export function buildEventPayload({
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
  --platforms <csv>        Comma-separated list of platforms to test (default: Linux-x64)
  --platform <name>        Repeatable alternative to --platforms
  --workflow <path>        Workflow file path (default: .github/workflows/package.yml)
  --ref <git-ref>          Ref used in the payload (default: main)
  --runtime-run-id <id>    Reuse runtime artifact workflow run ID (default: empty)
  --beta                   Enable beta mode (default: false)
  --skip-signing           Skip signing steps (default: true)
  --act-path <path>        act executable (default: act in PATH)
  --platform-image <map>   Runner image map for act -P (default: ubuntu-22.04=ghcr.io/catthehacker/ubuntu:full-latest)
  --job <name>             Workflow job name to run (default: main)
  --dry-run                Print a secret-free command without token acquisition or execution
  --continue-on-error      Keep running other platforms even if one fails
  --help                   Show this message

AUTHENTICATION:
  Set FLOORP_ACT_GITHUB_TOKEN, or authenticate with gh. The token is passed to
  act through its child environment and is never placed in act argv or logs.

EXAMPLES:
  deno run -A tools/act/run-package-workflow.ts
  deno run -A tools/act/run-package-workflow.ts --platforms Linux-x64,macOS-x64 --beta --skip-signing=false
  deno run -A tools/act/run-package-workflow.ts --dry-run -- --pull
`,
  );
}

if (import.meta.main) {
  Deno.exit(await main());
}
