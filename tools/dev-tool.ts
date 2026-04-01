#!/usr/bin/env -S deno run -A
// SPDX-License-Identifier: MPL-2.0

import * as path from "@std/path";
import { parseArgs } from "@std/cli/parse-args";
import { withBrowser } from "./src/browser_connector.ts";
import { PLATFORM, PROJECT_ROOT } from "./src/defines.ts";
// Lazy imports for rebuild — avoids type-checking injector.ts at dev-tool level
async function loadBuildModules() {
  const Builder = await import("./src/builder.ts");
  const Injector = await import("./src/injector.ts");
  const Update = await import("./src/update.ts");
  return { Builder, Injector, Update };
}

const PID_FILE = path.join(PROJECT_ROOT, "_dist", "dev-server.pid");

const HELP = `
Usage: deno run -A tools/dev-tool.ts <command> [options]

Process management:
  start                     Start feles-build dev (background)
  stop                      Stop all dev processes cleanly
  restart                   Restart dev processes
  rebuild                   Rebuild assets without restarting browser
  smoke [options]           Run smoke test suite

Browser commands:
  status                    Check browser connection
  screenshot [options]      Take a screenshot
  eval <script>             Evaluate JavaScript in browser
  console [options]         Get browser console messages
  navigate <url>            Navigate to a URL
  dom <selector>            Inspect DOM elements
  title                     Get page title

Options:
  --context, -c <ctx>       Context: "chrome" or "content" (default: chrome)
  --selector, -s <sel>      Screenshot: capture a specific element
  --output, -o <path>       Screenshot: output file path (default: _dist/screenshot.png)
  --limit, -l <n>           Console: max messages (default: 50)
  --filter, -f <pattern>    Console: filter by text pattern
  --level <level>           Console: filter by level (error/warn/info/debug/all, default: all)
  --mode <all|unit|runtime> Smoke: choose smoke subset (default: all)
  --help, -h                Show this help
`.trim();

const args = parseArgs(Deno.args, {
  string: ["context", "selector", "output", "limit", "filter", "level", "mode"],
  alias: {
    c: "context",
    s: "selector",
    o: "output",
    h: "help",
    l: "limit",
    f: "filter",
  },
  boolean: ["help"],
  default: { context: "chrome" },
});

const command = args._[0] as string | undefined;

if (!command || args.help) {
  console.log(HELP);
  Deno.exit(0);
}

// Validate --context
const VALID_CONTEXTS = ["chrome", "content"] as const;
if (!VALID_CONTEXTS.includes(args.context as (typeof VALID_CONTEXTS)[number])) {
  console.error(
    `Invalid --context "${args.context}". Allowed: chrome, content`,
  );
  Deno.exit(1);
}
const context = args.context as "chrome" | "content";

async function main() {
  switch (command) {
    case "start":
      await cmdStart();
      break;
    case "stop":
      await cmdStop();
      break;
    case "restart":
      await cmdStop();
      await cmdStart();
      break;
    case "rebuild":
      await cmdRebuild();
      break;
    case "smoke":
      await cmdSmoke();
      break;
    case "status":
      await cmdStatus();
      break;
    case "screenshot":
      await cmdScreenshot();
      break;
    case "eval":
      await cmdEval();
      break;
    case "console":
      await cmdConsole();
      break;
    case "navigate":
      await cmdNavigate();
      break;
    case "dom":
      await cmdDom();
      break;
    case "title":
      await cmdTitle();
      break;
    default:
      console.error(`Unknown command: ${command}`);
      console.log(HELP);
      Deno.exit(1);
  }
}

async function cmdStart() {
  // Check if already running
  try {
    const existingPid = Deno.readTextFileSync(PID_FILE).trim();
    // Verify process is actually alive
    const check = new Deno.Command(
      PLATFORM === "windows" ? "tasklist" : "ps",
      PLATFORM === "windows"
        ? {
            args: ["/FI", `PID eq ${existingPid}`, "/NH"],
            stdout: "piped",
            stderr: "null",
          }
        : { args: ["-p", existingPid], stdout: "piped", stderr: "null" },
    );
    const out = await check.output();
    const text = new TextDecoder().decode(out.stdout);
    if (text.includes(existingPid)) {
      console.log(`Dev server already running (PID ${existingPid}).`);
      console.log(
        "Use 'dev-tool stop' to stop it first, or 'dev-tool restart'.",
      );
      return;
    }
  } catch {
    // No PID file or process not found — proceed
  }

  console.log("Starting feles-build dev...");

  const logFile = path.join(PROJECT_ROOT, "_dist", "dev-server.log");
  const marionetteFile = path.join(
    PROJECT_ROOT,
    "_dist",
    "marionette-port.txt",
  );

  // Remove stale state files
  for (const f of [marionetteFile, PID_FILE]) {
    try {
      Deno.removeSync(f);
    } catch {
      /* ignore */
    }
  }

  // Spawn a fully detached process that survives parent exit
  const denoPath = Deno.execPath();

  // Use bash to spawn a background process (works on all platforms with bash)
  const setsid = PLATFORM !== "windows" ? "setsid " : "";
  const cmd = new Deno.Command("bash", {
    args: [
      "-c",
      `${setsid}"${denoPath}" task feles-build dev > "${logFile}" 2>&1 & echo $!`,
    ],
    cwd: PROJECT_ROOT,
    stdout: "piped",
    stderr: "null",
  });
  const out = await cmd.output();
  const pid = new TextDecoder().decode(out.stdout).trim();

  if (!pid || isNaN(Number(pid))) {
    console.error("Failed to start. Check log:", logFile);
    Deno.exit(1);
  }
  Deno.writeTextFileSync(PID_FILE, pid);
  console.log(`Started (PID ${pid}). Waiting for Marionette...`);

  // Poll for marionette-port.txt
  const maxWait = 120; // seconds
  for (let i = 0; i < maxWait; i++) {
    await new Promise((r) => setTimeout(r, 1000));
    try {
      const port = Deno.readTextFileSync(marionetteFile).trim();
      if (port) {
        console.log(`Browser ready! Marionette on port ${port}.`);
        console.log(`Log: ${logFile}`);
        return;
      }
    } catch {
      // not yet
    }
  }

  console.error(`Timed out after ${maxWait}s. Check ${logFile} for errors.`);
  Deno.exit(1);
}

async function cmdStop() {
  let pid: string | null = null;
  try {
    pid = Deno.readTextFileSync(PID_FILE).trim();
  } catch {
    // no PID file
  }

  console.log("Stopping dev processes...");

  if (PLATFORM === "windows") {
    // 1. Kill saved PID tree
    if (pid) {
      await runSilent("taskkill", ["/PID", pid, "/T", "/F"]);
    }

    // 2. Kill all feles-build related deno processes
    //    (bash & spawns outside the process tree, so /T may miss children)
    const findCmd = new Deno.Command("powershell", {
      args: [
        "-NoProfile",
        "-Command",
        `Get-WmiObject Win32_Process | Where-Object { $_.Name -eq 'deno.exe' -and $_.CommandLine -like '*feles-build*' } | ForEach-Object { $_.ProcessId }`,
      ],
      stdout: "piped",
      stderr: "null",
    });
    const findOut = await findCmd.output();
    const pids = new TextDecoder()
      .decode(findOut.stdout)
      .trim()
      .split(/\r?\n/)
      .filter(Boolean);
    for (const p of pids) {
      await runSilent("taskkill", ["/PID", p, "/T", "/F"]);
    }

    // 3. Kill only dev floorp.exe (uses test profile, not the user's regular browser)
    const findBrowser = new Deno.Command("powershell", {
      args: [
        "-NoProfile",
        "-Command",
        `Get-WmiObject Win32_Process | Where-Object { $_.Name -eq 'floorp.exe' -and $_.CommandLine -like '*_dist*profile*test*' } | ForEach-Object { $_.ProcessId }`,
      ],
      stdout: "piped",
      stderr: "null",
    });
    const browserOut = await findBrowser.output();
    const browserPids = new TextDecoder()
      .decode(browserOut.stdout)
      .trim()
      .split(/\r?\n/)
      .filter(Boolean);
    for (const p of browserPids) {
      await runSilent("taskkill", ["/PID", p, "/F"]);
    }
    if (browserPids.length === 0 && pid) {
      // Fallback: find floorp by parent PID (never kill by image name alone)
      const findChild = new Deno.Command("powershell", {
        args: [
          "-NoProfile",
          "-Command",
          `Get-WmiObject Win32_Process | Where-Object { $_.Name -eq 'floorp.exe' -and $_.ParentProcessId -eq ${pid} } | ForEach-Object { $_.ProcessId }`,
        ],
        stdout: "piped",
        stderr: "null",
      });
      const childOut = await findChild.output();
      const childPids = new TextDecoder()
        .decode(childOut.stdout)
        .trim()
        .split(/\r?\n/)
        .filter(Boolean);
      for (const p of childPids) {
        await runSilent("taskkill", ["/PID", p, "/F"]);
      }
    }

    // 4. Wait for cleanup
    await new Promise((r) => setTimeout(r, 1000));
  } else {
    if (pid) {
      await runSilent("kill", ["-TERM", `-${pid}`]);
      await new Promise((r) => setTimeout(r, 1000));
      await runSilent("kill", ["-9", `-${pid}`]);
    }
  }

  // Clean up state files
  for (const f of [
    PID_FILE,
    path.join(PROJECT_ROOT, "_dist", "marionette-port.txt"),
  ]) {
    try {
      Deno.removeSync(f);
    } catch {
      /* ignore */
    }
  }

  console.log("Stopped.");
}

async function runSilent(cmd: string, args: string[]): Promise<void> {
  try {
    await new Deno.Command(cmd, {
      args,
      stdout: "null",
      stderr: "null",
    }).output();
  } catch {
    /* ignore */
  }
}

async function cmdRebuild() {
  // In dev mode, Vite dev servers handle HMR for loader-features.
  // Only rebuild startup + loader-modules + inject XHTML.
  // (Rebuilding loader-features overwrites symlinked files and crashes Marionette)
  console.log("Rebuilding assets (startup + modules + inject)...");
  const { Injector, Update } = await loadBuildModules();
  const { packageVersion, runInParallel } = await import("./src/builder.ts");
  const { writeBuildid2 } = await import("./src/update.ts");

  const buildid2 = Update.generateUuidV7();
  const buildid2Path = path.join(PROJECT_ROOT, "_dist", "buildid2");
  writeBuildid2(buildid2Path, buildid2);

  const denoPath = Deno.execPath();
  const version = packageVersion();

  await runInParallel([
    [
      [denoPath, "task", "build", "--env.MODE=dev"],
      path.join(PROJECT_ROOT, "bridge/startup"),
    ],
    [
      [
        denoPath,
        "task",
        "build",
        `--env.__BUILDID2__=${buildid2}`,
        `--env.__VERSION2__=${version}`,
      ],
      path.join(PROJECT_ROOT, "bridge/loader-modules"),
    ],
  ]);

  Injector.run("dev");
  await Injector.injectXhtmlFromTs(true);
  console.log("Rebuild complete. (loader-features is handled by HMR)");
}

async function cmdSmoke() {
  const mode = typeof args.mode === "string" ? args.mode : "all";
  const denoPath = Deno.execPath();

  const child = new Deno.Command(denoPath, {
    args: ["run", "-A", "tools/src/smoke_runner.ts", "--mode", mode],
    cwd: PROJECT_ROOT,
    stdout: "inherit",
    stderr: "inherit",
  }).spawn();

  const status = await child.status;
  if (!status.success) {
    Deno.exit(status.code);
  }
}

async function cmdConsole() {
  const limit = parseInt(args.limit ?? "50", 10);
  const filter = args.filter ?? "";
  const level = args.level ?? "all";

  await withBrowser(async (client) => {
    await client.setContext("chrome");
    const raw = await client.executeScript(`
      const messages = Services.console.getMessageArray();
      return JSON.stringify(messages.slice(-${limit * 3}).map(m => {
        if (m instanceof Ci.nsIScriptError) {
          return {
            level: m.flags & 0x1 ? 'error' : m.flags & 0x2 ? 'warn' : 'info',
            message: m.errorMessage,
            source: m.sourceName,
            line: m.lineNumber,
            category: m.category,
            time: m.timeStamp,
          };
        }
        return {
          level: 'info',
          message: m.message || String(m),
          source: '',
          line: 0,
          category: '',
          time: 0,
        };
      }));
    `);

    let messages: Array<{
      level: string;
      message: string;
      source: string;
      line: number;
      category: string;
      time: number;
    }> = JSON.parse(raw);

    // Filter by level
    if (level !== "all") {
      messages = messages.filter((m) => m.level === level);
    }

    // Filter by text
    if (filter) {
      const pat = filter.toLowerCase();
      messages = messages.filter(
        (m) =>
          m.message.toLowerCase().includes(pat) ||
          m.source.toLowerCase().includes(pat),
      );
    }

    // Limit
    messages = messages.slice(-limit);

    if (messages.length === 0) {
      console.log("No matching console messages.");
      return;
    }

    for (const m of messages) {
      const src = m.source ? ` (${m.source}${m.line ? ":" + m.line : ""})` : "";
      const tag = m.level.toUpperCase().padEnd(5);
      console.log(`[${tag}] ${m.message}${src}`);
    }

    console.log(`\n${messages.length} message(s) shown.`);
  });
}

async function cmdStatus() {
  await withBrowser(async (client) => {
    await client.setContext("chrome");
    const info = await client.executeScript(`
      return JSON.stringify({
        title: document.title,
        url: document.documentURI,
        tabCount: typeof gBrowser !== 'undefined' ? gBrowser.tabs.length : 'N/A',
        activeTab: typeof gBrowser !== 'undefined' ? gBrowser.selectedTab.getAttribute('label') : 'N/A',
      })
    `);
    const data = JSON.parse(info);
    console.log("Connected to browser.");
    console.log(`  Title: ${data.title}`);
    console.log(`  URL: ${data.url}`);
    console.log(`  Tabs: ${data.tabCount}`);
    console.log(`  Active tab: ${data.activeTab}`);
  });
}

async function cmdTitle() {
  await withBrowser(async (client) => {
    await client.setContext(context);
    const title = await client.getTitle();
    console.log(title);
  });
}

async function cmdScreenshot() {
  const outputPath =
    args.output ?? path.join(PROJECT_ROOT, "_dist", "screenshot.png");

  await withBrowser(async (client) => {
    await client.setContext(context);

    let data: Uint8Array;
    if (args.selector) {
      // Find element by selector, then screenshot it
      const elements = await client.findElements(String(args.selector));
      if (elements.length === 0) {
        console.error(`Element not found: ${args.selector}`);
        Deno.exit(1);
      }
      data = await client.takeScreenshot({ element: elements[0] });
    } else {
      data = await client.takeScreenshot({ full: true });
    }

    Deno.writeFileSync(outputPath, data);
    console.log(`Screenshot saved to ${outputPath} (${data.length} bytes)`);
  });
}

async function cmdEval() {
  const script = args._.slice(1).join(" ");
  if (!script) {
    console.error("Usage: dev-tool eval <script>");
    Deno.exit(1);
  }

  await withBrowser(async (client) => {
    await client.setContext(context);
    const result = await client.executeScript(`return (${script})`);
    if (typeof result === "string") {
      console.log(result);
    } else {
      console.log(JSON.stringify(result, null, 2));
    }
  });
}

async function cmdNavigate() {
  const url = args._[1] as string | undefined;
  if (!url) {
    console.error("Usage: dev-tool navigate <url>");
    Deno.exit(1);
  }

  await withBrowser(async (client) => {
    await client.setContext(context);
    await client.navigate(String(url));
    const title = await client.getTitle();
    console.log(`Navigated to ${url}`);
    console.log(`Title: ${title}`);
  });
}

async function cmdDom() {
  const selector = args._[1] as string | undefined;
  if (!selector) {
    console.error("Usage: dev-tool dom <selector>");
    Deno.exit(1);
  }

  await withBrowser(async (client) => {
    await client.setContext(context);

    const elementIds = await client.findElements(String(selector));
    if (elementIds.length === 0) {
      console.log(`No elements found for: ${selector}`);
      return;
    }

    console.log(`Found ${elementIds.length} element(s):\n`);

    for (const [i, id] of elementIds.entries()) {
      const info = await client.executeScript(
        `
        const el = arguments[0];
        return JSON.stringify({
          tagName: el.tagName?.toLowerCase(),
          id: el.id || undefined,
          className: el.className || undefined,
          textContent: el.textContent?.slice(0, 200),
          childCount: el.children?.length,
          attributes: Object.fromEntries(
            Array.from(el.attributes ?? []).map(a => [a.name, a.value])
          ),
        });
      `,
        [{ "element-6066-11e4-a52e-4f735466cecf": id }],
      );
      console.log(`[${i}]`, JSON.parse(info));
    }
  });
}

main()
  .then(() => Deno.exit(0))
  .catch((err) => {
    console.error(`Error: ${err.message}`);
    Deno.exit(1);
  });
