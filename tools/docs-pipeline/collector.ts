// SPDX-License-Identifier: MPL-2.0

import * as path from "@std/path";
import { PROJECT_ROOT } from "../src/defines.ts";
import type {
  ArchitectureInventory,
  CiWorkflowEntry,
  DenoTaskEntry,
  DocsInventory,
  FeatureCatalogEntry,
  FelesCommandEntry,
  FloorpOsApiInventory,
  OsApiRouteEntry,
  SettingsRouteEntry,
  SourceRef,
  WindowActorEntry,
} from "./types.ts";

const TEXT_DECODER = new TextDecoder();

function toRepoPath(absPath: string): string {
  return path.relative(PROJECT_ROOT, absPath).replaceAll("\\", "/");
}

function source(
  pathFromRoot: string,
  text: string,
  pattern: string,
): SourceRef {
  const index = text.indexOf(pattern);
  if (index === -1) {
    return { path: pathFromRoot };
  }

  return {
    path: pathFromRoot,
    line: text.slice(0, index).split(/\r?\n/).length,
  };
}

async function readRepoText(pathFromRoot: string): Promise<string> {
  return await Deno.readTextFile(path.join(PROJECT_ROOT, pathFromRoot));
}

export function parseDenoTasksFromText(
  denoJsonText: string,
  pathFromRoot = "deno.json",
): DenoTaskEntry[] {
  const parsed = JSON.parse(denoJsonText) as {
    tasks?: Record<string, string>;
  };
  const tasks = parsed.tasks ?? {};

  return Object.entries(tasks)
    .sort(([left], [right]) => left.localeCompare(right))
    .map(([name, command]) => ({
      name,
      command,
      source: source(pathFromRoot, denoJsonText, `"${name}"`),
    }));
}

export function parseFelesCommandsFromText(
  felesBuildText: string,
  pathFromRoot = "tools/feles-build.ts",
): FelesCommandEntry[] {
  const commands = new Set<string>();
  for (const match of felesBuildText.matchAll(/case\s+"([^"]+)":/g)) {
    const name = match[1];
    if (!name.startsWith("-")) {
      commands.add(name);
    }
  }

  return [...commands].sort().map((name) => {
    const usageMatch = felesBuildText.match(
      new RegExp(`Usage: feles-build ${name}[^\\n"]*`),
    );
    return {
      name,
      usage: usageMatch?.[0],
      source: source(pathFromRoot, felesBuildText, `case "${name}"`),
    };
  });
}

export function extractChromeFeatureDiscovery(
  modText: string,
  pathFromRoot = "browser-features/chrome/common/mod.ts",
): ArchitectureInventory["chromeFeatureDiscovery"] {
  const match = modText.match(/import\.meta\.glob\("([^"]+)"\)/);
  return {
    globPattern: match?.[1] ?? "",
    source: source(pathFromRoot, modText, "import.meta.glob"),
  };
}

export function extractWindowActors(
  browserGlueText: string,
  pathFromRoot = "browser-features/modules/modules/BrowserGlue.sys.mts",
): ArchitectureInventory["windowActors"] {
  const actorBlockStart = browserGlueText.indexOf("const JS_WINDOW_ACTORS");
  const actorBlockEnd = browserGlueText.indexOf(
    "ActorManagerParent.addJSWindowActors",
  );
  const actorBlock = actorBlockStart !== -1 && actorBlockEnd > actorBlockStart
    ? browserGlueText.slice(actorBlockStart, actorBlockEnd)
    : "";
  const actorNames = new Set<string>();
  for (
    const match of actorBlock.matchAll(/^\s{2,6}([A-Z][A-Za-z0-9_]+):\s+\{/gm)
  ) {
    actorNames.add(match[1]);
  }

  return {
    registrationApi: browserGlueText.includes(
        "ActorManagerParent.addJSWindowActors",
      )
      ? "ActorManagerParent.addJSWindowActors"
      : "unknown",
    actorCount: actorNames.size,
    source: source(
      pathFromRoot,
      browserGlueText,
      "ActorManagerParent.addJSWindowActors",
    ),
  };
}

export function extractWindowActorEntries(
  browserGlueText: string,
  pathFromRoot = "browser-features/modules/modules/BrowserGlue.sys.mts",
): WindowActorEntry[] {
  const actorBlockStart = browserGlueText.indexOf("const JS_WINDOW_ACTORS");
  const actorBlockEnd = browserGlueText.indexOf(
    "ActorManagerParent.addJSWindowActors",
  );
  const actorBlock = actorBlockStart !== -1 && actorBlockEnd > actorBlockStart
    ? browserGlueText.slice(actorBlockStart, actorBlockEnd)
    : "";

  return [...actorBlock.matchAll(/^\s{2,6}([A-Z][A-Za-z0-9_]+):\s+\{/gm)]
    .map((match) => ({
      name: match[1],
      source: source(pathFromRoot, browserGlueText, `${match[1]}:`),
    }))
    .sort((left, right) => left.name.localeCompare(right.name));
}

export function extractSettingsRoutes(
  appText: string,
  pathFromRoot = "browser-features/pages-settings/src/App.tsx",
): SettingsRouteEntry[] {
  const imports = new Map<string, string>();
  for (
    const match of appText.matchAll(
      /^import\s+(?:\{\s*([A-Za-z0-9_]+)\s*\}|([A-Za-z0-9_]+))\s+from\s+["']([^"']+)["'];/gm,
    )
  ) {
    const localName = match[1] ?? match[2];
    if (localName) {
      imports.set(localName, match[3]);
    }
  }

  const routes: SettingsRouteEntry[] = [];
  for (
    const match of appText.matchAll(
      /<Route\s+path="([^"]+)"\s+element=\{<([A-Za-z0-9_]+)(?:\s+[^>]*)?\s*\/>\}/g,
    )
  ) {
    const component = match[2];
    const route = match[1].startsWith("/") ? match[1] : `/${match[1]}`;
    routes.push({
      route,
      component: imports.get(component) ?? component,
      source: source(pathFromRoot, appText, `path="${match[1]}"`),
    });
  }

  return routes.sort((left, right) => left.route.localeCompare(right.route));
}

export function extractBridgeLoader(
  chromeRootText: string,
  pathFromRoot = "bridge/startup/src/chrome_root.ts",
): ArchitectureInventory["bridgeLoader"] {
  const devLoaderUrl = chromeRootText.match(
    /https?:\/\/[^"'`]+\/loader\/index\.ts/,
  )?.[0] ?? "unknown";
  const testLoaderUrl = chromeRootText.match(
    /https?:\/\/[^"'`]+\/loader\/test\/index\.ts/,
  )?.[0] ?? "unknown";
  const productionLoader = chromeRootText.match(
    /chrome:\/\/[^"'`]+\/core\.js/,
  )?.[0] ?? "unknown";

  return {
    devLoaderUrl,
    testLoaderUrl,
    productionLoader,
    source: source(pathFromRoot, chromeRootText, devLoaderUrl),
  };
}

export function extractLoaderDevServer(
  viteConfigText: string,
  pathFromRoot = "bridge/loader-features/vite.config.ts",
): ArchitectureInventory["loaderDevServer"] {
  const match = viteConfigText.match(/port:\s+(\d+)/);
  return {
    port: Number(match?.[1] ?? 0),
    source: source(pathFromRoot, viteConfigText, "port:"),
  };
}

function joinApiPath(basePath: string, routePath: string): string {
  if (routePath === "/") {
    return basePath || "/";
  }
  const base = basePath.endsWith("/") ? basePath.slice(0, -1) : basePath;
  const route = routePath.startsWith("/") ? routePath : `/${routePath}`;
  return `${base}${route}` || "/";
}

function summarizeOsApiRoute(
  method: OsApiRouteEntry["method"],
  pathName: string,
): string {
  if (pathName === "/health") {
    return "Reports local OS server health.";
  }
  if (pathName === "/browser/events") {
    return "Streams browser-side events such as workspace changes.";
  }
  if (pathName.startsWith("/browser/context")) {
    return "Returns recent browser context for local tools.";
  }
  if (pathName.startsWith("/browser/")) {
    return "Reads browser tabs, history, downloads, or browser event state.";
  }
  if (pathName.includes("/instances") && pathName.includes("screenshot")) {
    return "Captures browser content for automation clients.";
  }
  if (pathName.includes("/instances") && pathName.includes("text")) {
    return "Extracts page or element text for automation clients.";
  }
  if (pathName.includes("/instances") && pathName.includes("click")) {
    return "Performs browser interaction against a managed instance.";
  }
  if (pathName.includes("/instances") && method === "POST") {
    return "Mutates or drives a managed browser automation instance.";
  }
  if (pathName.startsWith("/tabs/")) {
    return "Controls visible tab automation instances.";
  }
  if (pathName.startsWith("/scraper/")) {
    return "Controls headless scraper automation instances.";
  }
  if (pathName.startsWith("/workspaces")) {
    return "Reads or switches Floorp workspace state.";
  }
  return "Floorp OS local HTTP API route.";
}

export function extractOsApiRouteEntries(
  routeText: string,
  pathFromRoot: string,
  basePath: string,
): OsApiRouteEntry[] {
  const routes = new Map<string, OsApiRouteEntry>();
  const routePattern =
    /\b(?:api|b|t|s|w|ns)\.(get|post|delete)(?:<[\s\S]*?>)?\s*\(\s*["']([^"']+)["']/g;

  for (const match of routeText.matchAll(routePattern)) {
    const method = match[1].toUpperCase() as OsApiRouteEntry["method"];
    const routePath = joinApiPath(basePath, match[2]);
    const key = `${method} ${routePath}`;
    routes.set(key, {
      method,
      path: routePath,
      source: source(pathFromRoot, routeText, match[2]),
      summary: summarizeOsApiRoute(method, routePath),
    });
  }

  return [...routes.values()].sort((left, right) =>
    `${left.path} ${left.method}`.localeCompare(`${right.path} ${right.method}`)
  );
}

export function extractSharedOsApiRouteEntries(
  routeText: string,
  pathFromRoot: string,
  basePath: string,
  options: { includeGetElement: boolean },
): OsApiRouteEntry[] {
  const getElementPath = joinApiPath(basePath, "/instances/:id/element");
  return extractOsApiRouteEntries(routeText, pathFromRoot, basePath).filter(
    (route) => options.includeGetElement || route.path !== getElementPath,
  );
}

function buildFloorpOsApiInventory(
  texts: {
    server: string;
    router: string;
    shared: string;
    browser: string;
    scraper: string;
    tabs: string;
    workspaces: string;
    automotor: string;
    settingsPage: string;
  },
): FloorpOsApiInventory {
  return {
    server: source(
      "browser-features/modules/modules/os-server/server.sys.mts",
      texts.server,
      "Floorp OS Local HTTP Server",
    ),
    router: source(
      "browser-features/modules/modules/os-server/router.sys.mts",
      texts.router,
      "export class Router",
    ),
    sharedAutomationRoutes: source(
      "browser-features/modules/modules/os-server/shared/routes.sys.mts",
      texts.shared,
      "registerCommonAutomationRoutes",
    ),
    automotorManager: source(
      "browser-features/modules/modules/os-automotor/OSAutomotor-manager.sys.mts",
      texts.automotor,
      "OSAutomotor-manager",
    ),
    settingsPage: source(
      "browser-features/pages-settings/src/app/floorp-os/page.tsx",
      texts.settingsPage,
      "Floorp",
    ),
    verification: [
      { path: "tools/os-test/verify_os_server_full.ts" },
      { path: "tools/os-test/run_verify_os_server_full_wrapper.ts" },
      { path: "tools/dev-tool.ts" },
    ],
    routeModules: [
      {
        namespace: "server",
        source: {
          path: "browser-features/modules/modules/os-server/server.sys.mts",
        },
        routes: extractOsApiRouteEntries(
          texts.server,
          "browser-features/modules/modules/os-server/server.sys.mts",
          "",
        ),
      },
      {
        namespace: "browser",
        source: {
          path:
            "browser-features/modules/modules/os-server/browser/routes.sys.mts",
        },
        routes: extractOsApiRouteEntries(
          texts.browser,
          "browser-features/modules/modules/os-server/browser/routes.sys.mts",
          "/browser",
        ),
      },
      {
        namespace: "tabs",
        source: {
          path:
            "browser-features/modules/modules/os-server/tabs/routes.sys.mts",
        },
        routes: extractOsApiRouteEntries(
          texts.tabs,
          "browser-features/modules/modules/os-server/tabs/routes.sys.mts",
          "/tabs",
        ),
      },
      {
        namespace: "scraper",
        source: {
          path:
            "browser-features/modules/modules/os-server/scraper/routes.sys.mts",
        },
        routes: extractOsApiRouteEntries(
          texts.scraper,
          "browser-features/modules/modules/os-server/scraper/routes.sys.mts",
          "/scraper",
        ),
      },
      {
        namespace: "workspaces",
        source: {
          path:
            "browser-features/modules/modules/os-server/workspaces/routes.sys.mts",
        },
        routes: extractOsApiRouteEntries(
          texts.workspaces,
          "browser-features/modules/modules/os-server/workspaces/routes.sys.mts",
          "/workspaces",
        ),
      },
      {
        namespace: "tabs shared automation",
        source: {
          path:
            "browser-features/modules/modules/os-server/shared/routes.sys.mts",
        },
        routes: extractSharedOsApiRouteEntries(
          texts.shared,
          "browser-features/modules/modules/os-server/shared/routes.sys.mts",
          "/tabs",
          { includeGetElement: true },
        ),
      },
      {
        namespace: "scraper shared automation",
        source: {
          path:
            "browser-features/modules/modules/os-server/shared/routes.sys.mts",
        },
        routes: extractSharedOsApiRouteEntries(
          texts.shared,
          "browser-features/modules/modules/os-server/shared/routes.sys.mts",
          "/scraper",
          { includeGetElement: false },
        ),
      },
    ],
  };
}

function parseWorkflowTriggers(workflowText: string): string[] {
  const triggers = new Set<string>();
  for (
    const trigger of [
      "workflow_dispatch",
      "pull_request",
      "push",
      "schedule",
      "workflow_call",
      "issues",
      "issue_comment",
    ]
  ) {
    if (workflowText.includes(`${trigger}:`)) {
      triggers.add(trigger);
    }
  }
  return [...triggers].sort();
}

function parseWorkflowPermissions(workflowText: string): string[] {
  const permissions = new Set<string>();
  for (const match of workflowText.matchAll(/^\s{2,}([a-z-]+):\s+(\w+)/gm)) {
    if (
      [
        "actions",
        "contents",
        "discussions",
        "issues",
        "pull-requests",
      ].includes(match[1])
    ) {
      permissions.add(`${match[1]}:${match[2]}`);
    }
  }
  return [...permissions].sort();
}

export function parseWorkflowRunCommands(workflowText: string): string[] {
  const commands = new Set<string>();
  const lines = workflowText.split(/\r?\n/);

  function addCommand(rawCommand: string): void {
    let command = rawCommand.trim();
    while (command.endsWith("\\")) {
      command = command.slice(0, -1).trimEnd();
    }
    command = command.replace(/\s+/g, " ");
    if (
      !command || isShellControlLine(command) || isSensitiveShellLine(command)
    ) {
      return;
    }
    commands.add(command);
  }

  for (let index = 0; index < lines.length; index++) {
    const line = lines[index];
    if (!/^\s*-?\s*run:\s*\|\s*$/.test(line)) {
      const inlineRun = line.match(/^\s*-?\s*run:\s+(.+)$/);
      if (inlineRun) {
        addCommand(inlineRun[1]);
      }
      continue;
    }

    const runIndent = line.match(/^(\s*)/)?.[1].length ?? 0;
    let pendingCommand = "";
    for (index++; index < lines.length; index++) {
      const commandLine = lines[index];
      const trimmed = commandLine.trim();
      const indent = commandLine.match(/^(\s*)/)?.[1].length ?? 0;
      if (trimmed && indent <= runIndent) {
        index--;
        break;
      }
      if (!trimmed || trimmed.startsWith("#")) {
        continue;
      }
      pendingCommand = pendingCommand
        ? `${pendingCommand.replace(/\\\s*$/, "")} ${trimmed}`
        : trimmed;
      if (!trimmed.endsWith("\\")) {
        addCommand(pendingCommand);
        pendingCommand = "";
      }
    }
    if (pendingCommand) {
      addCommand(pendingCommand);
    }
  }

  return [...commands].sort();
}

function isShellControlLine(command: string): boolean {
  return /^(set|if|then|fi|for|do|done|else|elif|trap|cleanup\(\)|\}|break|exit)\b/
    .test(command) ||
    /^[A-Z_][A-Z0-9_]*=/.test(command) ||
    /^\[\[/.test(command);
}

function isSensitiveShellLine(command: string): boolean {
  return /\bsecrets\.|\b[A-Z0-9_]*(TOKEN|PASS|PASSWORD|SECRET|API_KEY)[A-Z0-9_]*\b/
    .test(command);
}

async function collectWorkflows(): Promise<CiWorkflowEntry[]> {
  const workflowsDir = path.join(PROJECT_ROOT, ".github", "workflows");
  const entries: CiWorkflowEntry[] = [];

  for await (const entry of Deno.readDir(workflowsDir)) {
    if (!entry.isFile || !entry.name.endsWith(".yml")) {
      continue;
    }

    const absPath = path.join(workflowsDir, entry.name);
    const repoPath = toRepoPath(absPath);
    const text = await Deno.readTextFile(absPath);
    const name = text.match(/^name:\s*"?([^"\n]+)"?/m)?.[1]?.trim() ??
      entry.name;

    entries.push({
      name,
      path: repoPath,
      triggers: parseWorkflowTriggers(text),
      permissions: parseWorkflowPermissions(text),
      runCommands: parseWorkflowRunCommands(text),
    });
  }

  return entries.sort((left, right) => left.path.localeCompare(right.path));
}

function cleanCommentSummary(comment: string): string {
  return comment
    .split(/\r?\n/)
    .map((line) => line.replace(/^\s*\*\s?/, "").trim())
    .filter((line) => line && !line.startsWith("@"))
    .join(" ")
    .replace(/\s+/g, " ")
    .trim();
}

function summarizeFeatureIndex(name: string, text: string): string {
  const firstRuntimeMarker = Math.min(
    ...[
      text.indexOf("import "),
      text.indexOf("export "),
      text.indexOf("@noraComponent"),
    ].filter((index) => index >= 0),
  );
  const headerText = Number.isFinite(firstRuntimeMarker)
    ? text.slice(0, firstRuntimeMarker)
    : text;
  const jsDoc = headerText.match(/\/\*\*([\s\S]+?)\*\//);
  const cleaned = jsDoc ? cleanCommentSummary(jsDoc[1]) : "";
  if (cleaned) {
    return cleaned;
  }

  const className = text.match(/export\s+default\s+class\s+([A-Za-z0-9_]+)/)
    ?.[1];
  const localImports = [
    ...new Set(
      [...text.matchAll(/from\s+["']\.\/([^"']+)["']/g)]
        .map((match) => match[1].replace(/\.(tsx?|jsx?|mts|mjs)$/, ""))
        .filter((entry) =>
          !entry.startsWith("types") && !entry.startsWith("type")
        ),
    ),
  ].slice(0, 3);
  const importSummary = localImports.length > 0
    ? ` and wires ${localImports.map((entry) => `\`${entry}\``).join(", ")}`
    : "";
  if (className) {
    return `Owns the ${
      humanizeFeatureName(name)
    } feature through the ${className} chrome component${importSummary}.`;
  }

  if (text.includes("initBeforeSessionStoreInit")) {
    return `Provides pre-session initialization for the ${
      humanizeFeatureName(name)
    } feature.`;
  }
  if (text.includes("export function init")) {
    return `Provides initialization for the ${
      humanizeFeatureName(name)
    } feature.`;
  }
  return `Chrome feature entrypoint for ${humanizeFeatureName(name)}.`;
}

function humanizeFeatureName(name: string): string {
  return name
    .split(/[-_]/)
    .filter(Boolean)
    .map((part) => part === "pwa" ? "PWA" : part)
    .join(" ");
}

function featureEntrypoints(text: string): string[] {
  const entrypoints: string[] = [];
  if (text.includes("initBeforeSessionStoreInit")) {
    entrypoints.push("initBeforeSessionStoreInit");
  }
  if (/\binit\s*\(/.test(text) || /export\s+function\s+init\s*\(/.test(text)) {
    entrypoints.push("init");
  }
  const className = text.match(/export\s+default\s+class\s+([A-Za-z0-9_]+)/)
    ?.[1];
  if (className) {
    entrypoints.push(`default class ${className}`);
  }
  return [...new Set(entrypoints)].sort();
}

async function collectChromeFeatureEntries(
  directoryFromRoot: string,
): Promise<FeatureCatalogEntry[]> {
  const directory = path.join(PROJECT_ROOT, ...directoryFromRoot.split("/"));
  const entries: FeatureCatalogEntry[] = [];

  for await (const entry of Deno.readDir(directory)) {
    if (!entry.isDirectory) {
      continue;
    }
    const sourcePath = `${directoryFromRoot}/${entry.name}/index.ts`;
    try {
      const text = await readRepoText(sourcePath);
      entries.push({
        name: entry.name,
        source: { path: sourcePath },
        summary: summarizeFeatureIndex(entry.name, text),
        entrypoints: featureEntrypoints(text),
      });
    } catch (error) {
      if (error instanceof Deno.errors.NotFound) {
        // Directories without an index.ts are not loader-discovered features.
        continue;
      }
      console.error("[DocsPipeline]", `Failed to read ${sourcePath}`, error);
      throw error;
    }
  }

  return entries.sort((left, right) => left.name.localeCompare(right.name));
}

async function gitSha(): Promise<string> {
  try {
    const output = await new Deno.Command("git", {
      args: ["rev-parse", "HEAD"],
      cwd: PROJECT_ROOT,
      stdout: "piped",
      stderr: "null",
    }).output();
    if (output.success) {
      return TEXT_DECODER.decode(output.stdout).trim();
    }
  } catch {
    // Keep the collector usable outside a git checkout.
  }
  return "unknown";
}

const ARCHITECTURE_REFERENCE_SOURCES:
  ArchitectureInventory["referenceSources"] = [
    {
      area: "Loader entrypoint",
      source: { path: "bridge/loader-features/loader/index.ts" },
      summary:
        "Imports BrowserGlue, initializes i18n, records loader state, filters enabled modules, and runs module lifecycle hooks.",
    },
    {
      area: "Loader module registry",
      source: { path: "bridge/loader-features/loader/modules.ts" },
      summary:
        "Builds common/static module maps from chrome feature entry globs.",
    },
    {
      area: "Loader module hooks",
      source: { path: "bridge/loader-features/loader/modules-hooks.ts" },
      summary:
        "Provides onModuleLoaded promises and internal module load-state resolution.",
    },
    {
      area: "Chrome feature overview",
      source: { path: "browser-features/chrome/README.md" },
      summary:
        "Documents common/static/runtime-oriented chrome feature directories and utility conventions.",
    },
    {
      area: "Static chrome feature discovery",
      source: { path: "browser-features/chrome/static/mod.ts" },
      summary: "Provides static chrome feature entry discovery.",
    },
    {
      area: "Gecko version configuration",
      source: { path: "static/gecko/config/README.md" },
      summary:
        "Documents generated Gecko version files that are intentionally ignored in local checkouts.",
    },
    {
      area: "Settings app entry",
      source: { path: "browser-features/pages-settings/src/main.tsx" },
      summary:
        "Bootstraps the React settings app with theme, i18n, and hash-based routing.",
    },
    {
      area: "Settings routes",
      source: { path: "browser-features/pages-settings/src/App.tsx" },
      summary:
        "Defines settings routes for dashboard, design, sidebar, workspaces, PWA, Floorp OS, accounts, gestures, shortcuts, and updates.",
    },
    {
      area: "Browser test runner",
      source: { path: "tools/src/colocated_test_runner.ts" },
      summary:
        "Discovers browser tests, filters by layer/near path, autostarts feles-build test, and collects browser results.",
    },
    {
      area: "Browser test result collector",
      source: { path: "tools/src/browser_test_collector.ts" },
      summary:
        "Polls Firefox profile prefs.js for structured browser test results written by the browser-side runner.",
    },
    {
      area: "Development inspection CLI",
      source: { path: "tools/dev-tool.ts" },
      summary:
        "Provides start/stop/rebuild plus Marionette-backed status, eval, console, screenshot, navigation, and DOM inspection commands.",
    },
    {
      area: "Build CLI",
      source: { path: "tools/feles-build.ts" },
      summary:
        "Defines Floorp build, dev, stage, test, and misc patch command behavior.",
    },
    {
      area: "Root Deno tasks",
      source: { path: "deno.json" },
      summary:
        "Defines task entrypoints, imports, workspace packages, and docs pipeline commands.",
    },
  ];

export async function collectDocsInventory(): Promise<DocsInventory> {
  const [
    denoJsonText,
    felesBuildText,
    chromeModText,
    browserGlueText,
    chromeRootText,
    loaderViteText,
    settingsAppText,
    chromeCommonFeatures,
    chromeStaticFeatures,
    workflows,
    commit,
    osServerText,
    osRouterText,
    osSharedRoutesText,
    osBrowserRoutesText,
    osScraperRoutesText,
    osTabRoutesText,
    osWorkspaceRoutesText,
    osAutomotorText,
    osSettingsPageText,
  ] = await Promise.all([
    readRepoText("deno.json"),
    readRepoText("tools/feles-build.ts"),
    readRepoText("browser-features/chrome/common/mod.ts"),
    readRepoText("browser-features/modules/modules/BrowserGlue.sys.mts"),
    readRepoText("bridge/startup/src/chrome_root.ts"),
    readRepoText("bridge/loader-features/vite.config.ts"),
    readRepoText("browser-features/pages-settings/src/App.tsx"),
    collectChromeFeatureEntries("browser-features/chrome/common"),
    collectChromeFeatureEntries("browser-features/chrome/static"),
    collectWorkflows(),
    gitSha(),
    readRepoText("browser-features/modules/modules/os-server/server.sys.mts"),
    readRepoText("browser-features/modules/modules/os-server/router.sys.mts"),
    readRepoText(
      "browser-features/modules/modules/os-server/shared/routes.sys.mts",
    ),
    readRepoText(
      "browser-features/modules/modules/os-server/browser/routes.sys.mts",
    ),
    readRepoText(
      "browser-features/modules/modules/os-server/scraper/routes.sys.mts",
    ),
    readRepoText(
      "browser-features/modules/modules/os-server/tabs/routes.sys.mts",
    ),
    readRepoText(
      "browser-features/modules/modules/os-server/workspaces/routes.sys.mts",
    ),
    readRepoText(
      "browser-features/modules/modules/os-automotor/OSAutomotor-manager.sys.mts",
    ),
    readRepoText("browser-features/pages-settings/src/app/floorp-os/page.tsx"),
  ]);

  return {
    schemaVersion: 1,
    generatedAt: new Date().toISOString(),
    floorpCommit: commit,
    sourcePrecedence: [
      "code/config/CI/test scripts",
      "repo docs",
      "external docs",
    ],
    commands: {
      denoTasks: parseDenoTasksFromText(denoJsonText),
      felesBuild: parseFelesCommandsFromText(felesBuildText),
    },
    architecture: {
      layers: [
        {
          name: "Firefox Base",
          source: { path: "static/gecko/pref/override.ini" },
          summary: "Patched Gecko and pref overrides.",
        },
        {
          name: "ESM Modules",
          source: {
            path: "browser-features/modules/modules/BrowserGlue.sys.mts",
          },
          summary: "Firefox ESM modules and Window Actor registration.",
        },
        {
          name: "Bridge",
          source: { path: "bridge/startup/src/chrome_root.ts" },
          summary:
            "Startup bridge that selects dev/test HTTP loaders or chrome:// production code.",
        },
        {
          name: "Chrome UI",
          source: { path: "browser-features/chrome/common/mod.ts" },
          summary: "SolidJS browser chrome features discovered by glob.",
        },
        {
          name: "Pages",
          source: { path: "browser-features/pages-settings/vite.config.ts" },
          summary: "React/Tailwind full-page browser UIs.",
        },
      ],
      referenceSources: ARCHITECTURE_REFERENCE_SOURCES,
      chromeFeatureDiscovery: extractChromeFeatureDiscovery(chromeModText),
      windowActors: extractWindowActors(browserGlueText),
      bridgeLoader: extractBridgeLoader(chromeRootText),
      loaderDevServer: extractLoaderDevServer(loaderViteText),
    },
    ci: {
      workflows,
    },
    features: {
      chromeCommon: chromeCommonFeatures,
      chromeStatic: chromeStaticFeatures,
      settingsRoutes: extractSettingsRoutes(settingsAppText),
      windowActors: extractWindowActorEntries(browserGlueText),
    },
    floorpOsApi: buildFloorpOsApiInventory({
      server: osServerText,
      router: osRouterText,
      shared: osSharedRoutesText,
      browser: osBrowserRoutesText,
      scraper: osScraperRoutesText,
      tabs: osTabRoutesText,
      workspaces: osWorkspaceRoutesText,
      automotor: osAutomotorText,
      settingsPage: osSettingsPageText,
    }),
    knownDriftChecks: [
      "deno task dev",
      "deno task build",
      "deno task clean",
      "ActorManagerParent.addActors",
    ],
  };
}
