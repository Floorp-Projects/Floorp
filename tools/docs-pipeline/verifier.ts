// SPDX-License-Identifier: MPL-2.0

import * as path from "@std/path";
import { PROJECT_ROOT } from "../src/defines.ts";
import { seedCodexDocs } from "./codex.ts";
import {
  DETERMINISTIC_GENERATED_PAGE_PATHS,
  REQUIRED_GENERATED_PAGE_PATHS,
} from "./types.ts";
import type { DocsInventory } from "./types.ts";

export type VerificationIssue = {
  path: string;
  message: string;
};

const STALE_COMMANDS = [
  /\bdeno\s+task\s+dev(?![-:\w])/g,
  /\bdeno\s+task\s+build(?![-:\w])/g,
  /\bdeno\s+task\s+clean(?![-:\w])/g,
];

const DENO_TASK_PATTERN =
  /\bdeno\s+task\s+([A-Za-z0-9_-](?:[A-Za-z0-9:_-]*[A-Za-z0-9_-])?)/g;
const SECRET_IDENTIFIER_PATTERN =
  /\b[A-Z0-9_]*(TOKEN|PASS|PASSWORD|SECRET|API_KEY)[A-Z0-9_]*\b/g;
const ALLOWED_PUBLIC_ENV_NAMES = new Set([
  "DOCS_LLM_API_KEY",
  "DOCS_AUDIT_LLM_API_KEY",
  "OLLAMA_API_KEY",
]);

const SOURCE_PATH_PATTERN =
  /(?:browser-features|bridge|tools|\.github\/workflows|static|docs\/|deno\.json|package\.json)[A-Za-z0-9_./-]*/;
const BACKTICKED_SOURCE_PATH_PATTERN =
  /`((?:browser-features|bridge|tools|\.github\/workflows|static|docs\/|deno\.json|package\.json)[A-Za-z0-9_./-]*)/g;

function normalizeSlashes(value: string): string {
  return value.replaceAll("\\", "/");
}

function isAllowedDriftContext(text: string): boolean {
  return /\b(outdated|stale|drift|unsupported|invalid)\b/i.test(text);
}

function repoPathExists(repoPath: string): boolean {
  try {
    const stat = Deno.statSync(path.join(PROJECT_ROOT, ...repoPath.split("/")));
    return stat.isFile || stat.isDirectory;
  } catch {
    return false;
  }
}

function hasUnsafeSecretReference(line: string): boolean {
  if (/\bsecrets\./.test(line)) {
    return true;
  }
  SECRET_IDENTIFIER_PATTERN.lastIndex = 0;
  for (const match of line.matchAll(SECRET_IDENTIFIER_PATTERN)) {
    if (!ALLOWED_PUBLIC_ENV_NAMES.has(match[0])) {
      return true;
    }
  }
  return false;
}

function localDocsFileExists(
  docsRoot: string,
  currentFile: string,
  target: string,
): boolean {
  const withoutHash = target.split("#")[0];
  if (
    !withoutHash || withoutHash.startsWith("http:") ||
    withoutHash.startsWith("https:")
  ) {
    return true;
  }
  if (withoutHash.startsWith("mailto:") || withoutHash.startsWith("#")) {
    return true;
  }
  if (withoutHash.startsWith("/docs/")) {
    return true;
  }

  const base = path.join(docsRoot, path.dirname(currentFile));
  const candidate = path.resolve(base, withoutHash);
  const candidates = [
    candidate,
    `${candidate}.md`,
    `${candidate}.mdx`,
    path.join(candidate, "index.md"),
    path.join(candidate, "index.mdx"),
  ];

  return candidates.some((candidatePath) => {
    const relative = path.relative(docsRoot, candidatePath);
    if (relative.startsWith("..")) {
      return false;
    }
    try {
      return Deno.statSync(candidatePath).isFile;
    } catch {
      return false;
    }
  });
}

function verifyText(
  filePath: string,
  text: string,
  knownDenoTasks: ReadonlySet<string>,
  docsRoot?: string,
): VerificationIssue[] {
  const issues: VerificationIssue[] = [];
  const lines = text.split(/\r?\n/);
  let inFence = false;

  lines.forEach((line, index) => {
    const driftContext = lines.slice(Math.max(0, index - 6), index + 1).join(
      "\n",
    );

    if (/^\s*```/.test(line)) {
      inFence = !inFence;
    }

    for (const staleCommand of STALE_COMMANDS) {
      staleCommand.lastIndex = 0;
      if (staleCommand.test(line) && !isAllowedDriftContext(driftContext)) {
        issues.push({
          path: filePath,
          message: `line ${index + 1}: stale command example is not allowed`,
        });
      }
    }

    if (
      line.includes("ActorManagerParent.addActors") &&
      !isAllowedDriftContext(driftContext)
    ) {
      issues.push({
        path: filePath,
        message: `line ${
          index + 1
        }: use ActorManagerParent.addJSWindowActors instead`,
      });
    }

    for (const match of line.matchAll(DENO_TASK_PATTERN)) {
      const taskName = match[1];
      if (
        !knownDenoTasks.has(taskName) &&
        !isAllowedDriftContext(driftContext)
      ) {
        issues.push({
          path: filePath,
          message: `line ${index + 1}: unknown deno task command: ${taskName}`,
        });
      }
    }

    if (!inFence && /<[^>\n]*>/.test(line)) {
      issues.push({
        path: filePath,
        message: `line ${index + 1}: raw angle brackets are not MDX-safe`,
      });
    }

    if (!inFence && /^\s*(import|export)\s/.test(line)) {
      issues.push({
        path: filePath,
        message: `line ${index + 1}: MDX ESM is not allowed`,
      });
    }

    if (!inFence && /\{[^}\n]*\}/.test(line)) {
      issues.push({
        path: filePath,
        message: `line ${index + 1}: MDX expressions are not allowed`,
      });
    }

    if (/\\[nrt]/.test(line)) {
      issues.push({
        path: filePath,
        message: `line ${
          index + 1
        }: literal escape sequence is not readable MDX`,
      });
    }

    if (hasUnsafeSecretReference(line)) {
      issues.push({
        path: filePath,
        message: `line ${
          index + 1
        }: generated docs must not expose secret or credential identifiers`,
      });
    }
  });

  if (!SOURCE_PATH_PATTERN.test(text)) {
    issues.push({
      path: filePath,
      message: "generated docs must include at least one Floorp source path",
    });
  }

  for (const match of text.matchAll(BACKTICKED_SOURCE_PATH_PATTERN)) {
    const sourcePath = match[1].replace(/:\d+$/, "");
    if (!repoPathExists(sourcePath)) {
      issues.push({
        path: filePath,
        message: `referenced source path does not exist: ${sourcePath}`,
      });
    }
  }

  if (docsRoot) {
    for (const match of text.matchAll(/\[[^\]]+\]\(([^)]+)\)/g)) {
      const target = match[1].trim();
      if (!localDocsFileExists(docsRoot, filePath, target)) {
        issues.push({
          path: filePath,
          message: `broken local Markdown link: ${target}`,
        });
      }
    }
  }

  return issues;
}

function verifyInventory(inventory: DocsInventory): VerificationIssue[] {
  const issues: VerificationIssue[] = [];

  const taskNames = new Set(
    inventory.commands.denoTasks.map((task) => task.name),
  );
  if (!taskNames.has("feles-build")) {
    issues.push({
      path: "inventory",
      message: "deno.json must define the feles-build task",
    });
  }

  const felesCommands = new Set(
    inventory.commands.felesBuild.map((command) => command.name),
  );
  for (const command of ["dev", "test", "stage", "build", "misc"]) {
    if (!felesCommands.has(command)) {
      issues.push({
        path: "inventory",
        message: `feles-build command missing from inventory: ${command}`,
      });
    }
  }

  if (
    inventory.architecture.chromeFeatureDiscovery.globPattern !== "./*/index.ts"
  ) {
    issues.push({
      path: "inventory",
      message: "chrome feature discovery glob must be ./*/index.ts",
    });
  }

  if (
    inventory.architecture.windowActors.registrationApi !==
      "ActorManagerParent.addJSWindowActors"
  ) {
    issues.push({
      path: "inventory",
      message: "Window Actor registration API drifted",
    });
  }

  for (
    const source of [
      ...inventory.commands.denoTasks.map((task) => task.source.path),
      ...inventory.commands.felesBuild.map((command) => command.source.path),
      ...inventory.architecture.layers.map((layer) => layer.source.path),
      ...inventory.architecture.referenceSources.map((entry) =>
        entry.source.path
      ),
      inventory.architecture.chromeFeatureDiscovery.source.path,
      inventory.architecture.windowActors.source.path,
      inventory.architecture.bridgeLoader.source.path,
      inventory.architecture.loaderDevServer.source.path,
      ...inventory.features.chromeCommon.map((feature) => feature.source.path),
      ...inventory.features.chromeStatic.map((feature) => feature.source.path),
      ...inventory.features.settingsRoutes.map((route) => route.source.path),
      ...inventory.features.windowActors.map((actor) => actor.source.path),
      inventory.floorpOsApi.server.path,
      inventory.floorpOsApi.router.path,
      inventory.floorpOsApi.sharedAutomationRoutes.path,
      inventory.floorpOsApi.automotorManager.path,
      inventory.floorpOsApi.settingsPage.path,
      ...inventory.floorpOsApi.verification.map((entry) => entry.path),
      ...inventory.floorpOsApi.routeModules.map((module) => module.source.path),
      ...inventory.floorpOsApi.routeModules.flatMap((module) =>
        module.routes.map((route) => route.source.path)
      ),
    ]
  ) {
    if (!repoPathExists(source)) {
      issues.push({
        path: "inventory",
        message: `source path does not exist: ${source}`,
      });
    }
  }

  return issues;
}

function requireCiReferenceCommands(
  filePath: string,
  text: string,
  inventory: DocsInventory,
): VerificationIssue[] {
  if (!filePath.endsWith("development/reference/ci-test-reference.mdx")) {
    return [];
  }

  const issues: VerificationIssue[] = [];
  const normalizedText = text
    .replaceAll("&gt;", ">")
    .replaceAll("&lt;", "<")
    .replaceAll("&amp;", "&");
  const colocatedWorkflow = inventory.ci.workflows.find((workflow) =>
    workflow.path === ".github/workflows/colocated_runner_test.yml"
  );
  const expectedCommands =
    colocatedWorkflow?.runCommands.filter((command) =>
      command ===
        "deno run -A tools/runtime-lock/runtime_lock_cli.ts validate-lock" ||
      command === "deno task test:smoke" ||
      command === "deno task test:host" ||
      command === "deno task test:firefox-tests" ||
      command.startsWith("deno task feles-build test") ||
      command.startsWith("deno task firefox-tests:collect") ||
      command === "deno task firefox-tests:triage-browser" ||
      command === "deno task firefox-tests:prepare-browser" ||
      command.startsWith("deno task test --layer all") ||
      command === "deno test -A tools/src/colocated_test_runner.test.ts"
    ) ?? [];

  for (const command of expectedCommands) {
    if (!normalizedText.includes(command)) {
      issues.push({
        path: filePath,
        message: `CI reference is missing workflow run command: ${command}`,
      });
    }
  }

  if (expectedCommands.length > 0 && /\|\s*None\b/.test(normalizedText)) {
    issues.push({
      path: filePath,
      message:
        "CI reference must not collapse workflows with runCommands to None",
    });
  }

  if (/\btest:integration\b/.test(normalizedText)) {
    issues.push({
      path: filePath,
      message:
        "CI reference must cite actual workflow run commands instead of test:integration shorthand",
    });
  }

  return issues;
}

async function verifyDeterministicPagesAreFresh(
  inventory: DocsInventory,
  docsDir: string,
  relativeFiles: ReadonlySet<string>,
): Promise<VerificationIssue[]> {
  const issues: VerificationIssue[] = [];
  const verifyRoot = path.join(PROJECT_ROOT, "_dist", "docs-pipeline");
  await Deno.mkdir(verifyRoot, { recursive: true });
  const tempDir = await Deno.makeTempDir({
    dir: verifyRoot,
    prefix: "verify-deterministic-",
  });

  try {
    await seedCodexDocs(
      tempDir,
      path.join(tempDir, "codex", "generate.md"),
      inventory,
    );

    for (const pagePath of DETERMINISTIC_GENERATED_PAGE_PATHS) {
      if (!relativeFiles.has(pagePath)) {
        continue;
      }

      const expectedPath = path.join(tempDir, ...pagePath.split("/"));
      const actualPath = path.join(docsDir, ...pagePath.split("/"));
      const expected = await Deno.readTextFile(expectedPath);
      const actual = await Deno.readTextFile(actualPath);
      if (
        normalizeVolatileGeneratedText(actual) !==
          normalizeVolatileGeneratedText(expected)
      ) {
        issues.push({
          path: pagePath,
          message:
            "deterministic generated page is stale; rerun docs-pipeline generation",
        });
      }
    }
  } finally {
    await Deno.remove(tempDir, { recursive: true });
  }

  return issues;
}

function normalizeVolatileGeneratedText(text: string): string {
  return text
    .replace(
      /^floorp_commit: .+$/m,
      'floorp_commit: "__FLOORP_COMMIT__"',
    )
    .replace(
      /^Inventory generated from Floorp commit `[^`]+`\.$/m,
      "Inventory generated from Floorp commit `__FLOORP_COMMIT__`.",
    );
}

async function collectMdxFiles(docsDir: string): Promise<string[]> {
  const files: string[] = [];
  for await (const entry of Deno.readDir(docsDir)) {
    const absPath = path.join(docsDir, entry.name);
    if (entry.isDirectory) {
      files.push(...await collectMdxFiles(absPath));
    } else if (entry.isFile && /\.(md|mdx)$/.test(entry.name)) {
      files.push(absPath);
    }
  }
  return files.sort();
}

export async function verifyDocsPipeline(
  inventory: DocsInventory,
  docsDir?: string,
): Promise<VerificationIssue[]> {
  const issues = verifyInventory(inventory);
  if (!docsDir) {
    return issues;
  }

  const files = await collectMdxFiles(docsDir);
  if (files.length === 0) {
    issues.push({
      path: docsDir,
      message: "generated docs directory contains no Markdown/MDX files",
    });
    return issues;
  }

  const relativeFiles = new Set(
    files.map((file) => normalizeSlashes(path.relative(docsDir, file))),
  );
  const requiredPathSet = new Set<string>(REQUIRED_GENERATED_PAGE_PATHS);
  const generatedFiles = files.filter((file) =>
    requiredPathSet.has(normalizeSlashes(path.relative(docsDir, file)))
  );
  for (const requiredPath of REQUIRED_GENERATED_PAGE_PATHS) {
    if (!relativeFiles.has(requiredPath)) {
      issues.push({
        path: docsDir,
        message: `generated docs missing required page: ${requiredPath}`,
      });
    }
  }

  issues.push(
    ...await verifyDeterministicPagesAreFresh(
      inventory,
      docsDir,
      relativeFiles,
    ),
  );

  for (const file of generatedFiles) {
    const text = await Deno.readTextFile(file);
    const knownDenoTasks = new Set(
      inventory.commands.denoTasks.map((task) => task.name),
    );
    issues.push(
      ...verifyText(
        normalizeSlashes(path.relative(docsDir, file)),
        text,
        knownDenoTasks,
        docsDir,
      ),
    );
    issues.push(
      ...requireCiReferenceCommands(
        normalizeSlashes(path.relative(docsDir, file)),
        text,
        inventory,
      ),
    );
  }

  return issues;
}
