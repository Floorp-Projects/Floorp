#!/usr/bin/env -S deno run -A
// SPDX-License-Identifier: MPL-2.0

/**
 * Real process/session regression for #2890. Run against a built Floorp or the
 * dev overlay with its servers running (MODE=dev, never the colocated runner).
 *
 * deno run -A tools/tab-stacks-session-regression.ts --binary <floorp.exe>
 *
 * Windows only: process identity includes the executable and creation time.
 * Profiles and evidence are retained under _dist/tab-stacks-session-regression.
 * Only this command's child process/verified descendants can be terminated.
 */
import { parseArgs } from "@std/cli/parse-args";
import { join, resolve } from "@std/path";
import { MarionetteClient } from "./src/browser_connector.ts";
import { BIN_PATH_EXE, PROJECT_ROOT } from "./src/defines.ts";
import type {
  ProcessIdentity,
  WindowState,
} from "./src/tab_stacks_session_types.ts";

const pause = (ms = 150) => new Promise<void>((r) => setTimeout(r, ms));
const args = parseArgs(Deno.args, {
  string: ["binary", "output"],
  boolean: ["help"],
  default: { binary: BIN_PATH_EXE },
});
if (args.help) {
  console.log(
    "deno run -A tools/tab-stacks-session-regression.ts [--binary <floorp.exe>] [--output <new directory>]\n" +
      `Default binary: ${BIN_PATH_EXE}\n` +
      "Requires Windows and a built Floorp overlay. For development, build MODE=dev and leave its Vite servers running; MODE=test is rejected.\n" +
      "Creates its own profile and port, then quits/relaunches and force-terminates only its owned browser. Evidence remains under _dist.\n" +
      "This verifies the current source overlay, not a publicly released Floorp 12.20.2 package.",
  );
  Deno.exit(0);
}
if (Deno.build.os !== "windows") {
  throw new Error(
    "This runner currently requires Windows process identity APIs",
  );
}
const binary = resolve(args.binary);
const runId = crypto.randomUUID();
const parent = join(PROJECT_ROOT, "_dist", "tab-stacks-session-regression");
await Deno.mkdir(parent, { recursive: true });
const output = args.output
  ? resolve(args.output)
  : await Deno.makeTempDir({ dir: parent, prefix: "run-" });
if (args.output) await Deno.mkdir(output);
const profile = join(output, "profile");
await Deno.mkdir(profile);
const listener = Deno.listen({ hostname: "127.0.0.1", port: 0 });
const port = listener.addr.port;
listener.close();
const evidence: Record<string, unknown> = {
  runId,
  binary,
  profile,
  port,
  started: new Date().toISOString(),
  results: [],
};
const revision = await new Deno.Command("git", {
  args: ["rev-parse", "HEAD"],
  cwd: PROJECT_ROOT,
  stdout: "piped",
}).output();
assert(revision.success, "Source revision is available");
evidence.sourceRevision = new TextDecoder().decode(revision.stdout).trim();
const results = evidence.results as Record<string, unknown>[];

function assert(value: unknown, message: string): asserts value {
  if (!value) throw new Error(message);
}

async function processes(): Promise<ProcessIdentity[]> {
  const result = await new Deno.Command("powershell.exe", {
    args: [
      "-NoProfile",
      "-NonInteractive",
      "-Command",
      "$ErrorActionPreference='Stop'; @(Get-CimInstance Win32_Process | ForEach-Object { [pscustomobject]@{ pid=[int]$_.ProcessId; parent=[int]$_.ParentProcessId; created=if ($_.CreationDate) {$_.CreationDate.ToUniversalTime().ToString('o')} else {''}; executable=[string]$_.ExecutablePath; command=[string]$_.CommandLine } }) | ConvertTo-Json -Compress",
    ],
    stdout: "piped",
    stderr: "piped",
  }).output();
  assert(result.success, new TextDecoder().decode(result.stderr));
  return JSON.parse(new TextDecoder().decode(result.stdout));
}

function sameProcess(a: ProcessIdentity, b: ProcessIdentity): boolean {
  return a.pid === b.pid && a.created === b.created &&
    a.executable.toLowerCase() === b.executable.toLowerCase();
}

async function portOpen(): Promise<boolean> {
  try {
    const conn = await Deno.connect({ hostname: "127.0.0.1", port });
    conn.close();
    return true;
  } catch {
    return false;
  }
}

async function until<T>(
  check: () => Promise<T | null> | T | null,
  description: string,
  timeout = 60_000,
): Promise<T> {
  const deadline = Date.now() + timeout;
  let last: unknown;
  while (Date.now() < deadline) {
    try {
      const value = await check();
      if (value !== null) return value;
    } catch (error) {
      last = error;
    }
    await pause();
  }
  throw new Error(`${description} timed out${last ? `: ${last}` : ""}`);
}

const prefs: Record<string, string | number | boolean> = {
  "marionette.enabled": true,
  "marionette.port": port,
  "remote.active-protocols": 0,
  "browser.startup.page": 3,
  "browser.startup.homepage": "about:blank",
  "browser.sessionstore.resume_from_crash": true,
  "browser.sessionstore.max_resumed_crashes": -1,
  "browser.sessionstore.interval": 1000,
  "browser.shell.checkDefaultBrowser": false,
  "browser.startup.homepage_override.mstone": "ignore",
  "browser.tabs.warnOnClose": false,
  "browser.warnOnQuit": false,
  "browser.aboutwelcome.enabled": false,
  "browser.tabs.groups.enabled": true,
  "sidebar.revamp": false,
  "sidebar.verticalTabs": false,
  "floorp.tabstacks.enabled": true,
  "floorp.workspaces.enabled": false,
  "nora.dev.allow_http_loader": true,
  "security.chrome_baseline_csp.enabled": false,
  "security.disallow_privileged_https_script_loads": false,
  "security.allow_parent_unrestricted_js_loads": true,
};
await Deno.writeTextFile(
  join(profile, "user.js"),
  Object.entries(prefs).map(([key, value]) =>
    `user_pref(${JSON.stringify(key)}, ${JSON.stringify(value)});`
  ).join("\n"),
);

const windowSnapshot = `
  return Array.from(Services.wm.getEnumerator("navigator:browser"))
    .filter(w => !w.closed).map(w => {
      const gb = w.gBrowser;
      const bar = w.document.getElementById("floorp-stack-bar")?.getBoundingClientRect();
      const tabs = w.document.getElementById("TabsToolbar")?.getBoundingClientRect();
      const nav = w.document.getElementById("nav-bar")?.getBoundingClientRect();
      const url = tab => {
        const state = JSON.parse(SessionStore.getTabState(tab));
        return state.entries?.[(state.index ?? 1) - 1]?.url ?? "";
      };
      return {
        ready: !!w.gBrowserInit?.delayedStartupFinished && !!gb?.tabContainer,
        tabs: gb ? Array.from(gb.tabs, url) : [],
        groups: gb ? Array.from(gb.tabGroups, g => ({
          id: g.id, label: g.label,
          stacked: g.getAttribute("data-floorp-stack") === "true",
          tabs: Array.from(g.tabs, url),
        })) : [],
        bars: w.document.querySelectorAll("#floorp-stack-bar").length,
        proxies: w.document.querySelectorAll("#floorp-stack-items .floorp-stack-tab").length,
        layout: {
          barHeight: bar?.height ?? 0,
          barWidth: bar?.width ?? 0,
          belowTabs: !!bar && !!tabs && bar.top >= tabs.bottom - 1,
          aboveNavigation: !!bar && !!nav && bar.bottom <= nav.top + 1,
        },
        selectedGroup: gb?.selectedTab?.group?.id ?? null,
      };
    });
`;

class Browser {
  readonly owned = new Map<number, ProcessIdentity>();
  client?: MarionetteClient;
  exited = false;
  browserIdentity?: ProcessIdentity;
  readonly child: Deno.ChildProcess;
  readonly logs: Promise<unknown>[];

  constructor(readonly phase: string) {
    this.child = new Deno.Command(binary, {
      args: [
        "--no-remote",
        "--profile",
        profile,
        "--marionette",
        "--remote-allow-system-access",
      ],
      env: { MOZ_HEADLESS: "1", MOZ_NO_REMOTE: "1" },
      stdin: "null",
      stdout: "piped",
      stderr: "piped",
    }).spawn();
    this.child.status.then(() => this.exited = true);
    this.logs = ["stdout", "stderr"].map(async (stream) => {
      const file = await Deno.open(join(output, `${phase}-${stream}.log`), {
        createNew: true,
        write: true,
      });
      await this.child[stream as "stdout" | "stderr"].pipeTo(file.writable);
    });
  }

  async capture(): Promise<void> {
    const snapshot = await processes();
    const root = snapshot.find((p) => p.pid === this.child.pid);
    assert(root && root.created, `Owned root ${this.child.pid} must be alive`);
    assert(
      root.executable.toLowerCase() === binary.toLowerCase() &&
        root.command.includes(profile),
      "Owned root executable and isolated profile match the launch",
    );
    const previousRoot = this.owned.get(root.pid);
    assert(
      !previousRoot || sameProcess(previousRoot, root),
      "Root PID was reused",
    );
    this.owned.set(root.pid, root);
    let changed = true;
    while (changed) {
      changed = false;
      for (const p of snapshot) {
        const owner = this.owned.get(p.parent);
        const currentOwner = snapshot.find((candidate) =>
          candidate.pid === p.parent
        );
        if (
          !this.owned.has(p.pid) && owner && currentOwner &&
          sameProcess(owner, currentOwner) && p.created >= owner.created &&
          p.executable.toLowerCase() === binary.toLowerCase()
        ) {
          this.owned.set(p.pid, p);
          changed = true;
        }
      }
    }
  }

  async connect(): Promise<void> {
    await this.capture();
    await until(async () => {
      assert(!this.exited, `${this.phase} browser exited before Marionette`);
      return await portOpen() ? true : null;
    }, "Marionette listener");
    this.client = await MarionetteClient.connect(port);
    await this.client.setContext("chrome");
    await this.capture();
    const runtime = await this.client.executeScript(`return {
      pid:Services.appinfo.processID, name:Services.appinfo.name,
      version:Services.appinfo.version, buildID:Services.appinfo.appBuildID,
      platformVersion:Services.appinfo.platformVersion,
      profile:Services.dirsvc.get("ProfD", Ci.nsIFile).path,
    };`);
    evidence[`${this.phase}Runtime`] = runtime;
    this.browserIdentity = this.owned.get(runtime.pid);
    assert(
      this.browserIdentity && runtime.profile === profile,
      "Marionette belongs to the launched process or its verified descendant and isolated profile",
    );
    const mode = await this.client.executeScript(
      'return Services.prefs.getStringPref("nora.startup.mode", "production");',
    );
    assert(
      mode !== "test",
      "Use MODE=dev overlay, not the colocated test bootstrap",
    );
    await until(async () => {
      const states = await this.snapshot();
      return states.length && states.every((w) => w.ready) ? states : null;
    }, "Browser windows ready");
    await this.capture();
  }

  async snapshot(): Promise<WindowState[]> {
    assert(this.client, "Browser is connected");
    return await this.client.executeScript(windowSnapshot);
  }

  async asyncScript(script: string): Promise<unknown> {
    assert(this.client, "Browser is connected");
    const result = await this.client.send("WebDriver:ExecuteAsyncScript", {
      script: `const done = arguments[arguments.length - 1];
        (async () => { ${script} })().then(
          value => done({ok:true, value}),
          error => done({ok:false, error:String(error), stack:error.stack}));`,
      args: [],
      scriptTimeout: 30_000,
    });
    assert(result.value?.ok, JSON.stringify(result.value));
    return result.value.value;
  }

  async save(): Promise<void> {
    await this.asyncScript(`
      const { TabStateFlusher } = ChromeUtils.importESModule(
        "moz-src:///browser/components/sessionstore/TabStateFlusher.sys.mjs");
      for (const w of Services.wm.getEnumerator("navigator:browser")) {
        await TabStateFlusher.flushWindow(w);
      }
      const { SessionSaver } = ChromeUtils.importESModule(
        "moz-src:///browser/components/sessionstore/SessionSaver.sys.mjs");
      await SessionSaver.run();
      Services.prefs.savePrefFile(null);
    `);
  }

  async stop(crash: boolean): Promise<void> {
    await this.capture();
    if (crash) {
      // Windows can spawn the browser as a child of its launcher. Terminate
      // the actual browser only after rechecking its captured identity.
      assert(this.browserIdentity, "Browser process identity was captured");
      const current = (await processes()).find((p) =>
        p.pid === this.browserIdentity!.pid
      );
      assert(
        current && sameProcess(current, this.browserIdentity),
        "Browser identity still matches before forced termination",
      );
      Deno.kill(current.pid, "SIGKILL");
    } else {
      assert(this.client, "Connected browser required for graceful quit");
      try {
        await this.client.executeScript(
          "Services.startup.quit(Ci.nsIAppStartup.eAttemptQuit);",
        );
      } catch { /* Successful application quit closes the connection. */ }
    }
    await until(
      () => this.exited ? true : null,
      "Owned browser exit",
      20_000,
    );
    await this.client?.close();
    this.client = undefined;
    await Promise.allSettled(this.logs);
    await until(
      async () => {
        const snapshot = await processes();
        const alive = snapshot.filter((p) => {
          const original = this.owned.get(p.pid);
          return original && sameProcess(original, p);
        });
        return alive.length === 0 && !await portOpen() ? true : null;
      },
      "No owned browser processes or Marionette listener remain",
      20_000,
    );
    evidence[`${this.phase}Processes`] = [...this.owned.values()];
  }

  async abort(): Promise<void> {
    if (!this.exited) {
      await this.capture();
      this.child.kill("SIGKILL");
      await this.child.status;
    }
    // Any surviving subprocess must match a previously captured identity.
    for (const current of await processes()) {
      const owned = this.owned.get(current.pid);
      if (owned && sameProcess(owned, current)) {
        Deno.kill(current.pid, "SIGKILL");
      }
    }
    await this.client?.close();
    await Promise.allSettled(this.logs);
  }
}

function fixtureScript(windowIndex: number): string {
  return `
    const windows = Array.from(Services.wm.getEnumerator("navigator:browser"));
    const w = windows[${windowIndex}];
    const gb = w.gBrowser;
    const old = Array.from(gb.tabs);
    const tabs = Array.from({length:4}, (_, i) => gb.addTab(
      "https://example.invalid/floorp2890/${runId}/${windowIndex}/" + i,
      {triggeringPrincipal:Services.scriptSecurityManager.getSystemPrincipal(),
       inBackground:true}));
    const stack = gb.addTabGroup(tabs.slice(0,2), {label:"2890 stack ${windowIndex}"});
    const plain = gb.addTabGroup(tabs.slice(2), {label:"2890 native ${windowIndex}"});
    const kinds = JSON.parse(Services.prefs.getStringPref("floorp.tabstacks.groupKinds", "{}"));
    kinds[plain.id] = "group";
    Services.prefs.setStringPref("floorp.tabstacks.groupKinds", JSON.stringify(kinds));
    gb.selectedTab = tabs[0];
    gb.removeTabs(old, {animate:false});
    return true;
  `;
}

async function checkedState(
  browser: Browser,
  count: number,
  expected?: WindowState[],
): Promise<WindowState[]> {
  let last: WindowState[] = [];
  const state = await until(
    async () => {
      last = await browser.snapshot();
      if (
        last.length !== count ||
        !last.every((w) =>
          w.ready && w.tabs.length === 4 && w.tabs.every((url) =>
            url.includes(runId)
          ) &&
          w.groups.length === 2 &&
          w.groups.every((g) => g.tabs.length === 2) && w.bars === 1 &&
          w.proxies === 2 && w.layout.barHeight > 0 && w.layout.barWidth > 0 &&
          w.layout.belowTabs && w.layout.aboveNavigation && w.groups.some((g) =>
            g.label.startsWith("2890 stack ") && g.stacked &&
            g.id === w.selectedGroup
          ) && w.groups.some((g) =>
            g.label.startsWith("2890 native ") && !g.stacked
          )
        )
      ) return null;
      return last;
    },
    `All ${count} windows restore native tabs/groups and exactly one stack bar`,
  )
    .catch((error) => {
      evidence.failedWindowState = last;
      throw error;
    });
  const canonical = (windows: WindowState[]) =>
    JSON.stringify(
      windows.map((w) => ({
        tabs: w.tabs,
        groups: w.groups,
        selectedGroup: w.selectedGroup,
      })).sort((a, b) => a.groups[0].id.localeCompare(b.groups[0].id)),
    );
  if (expected) {
    assert(
      canonical(state) === canonical(expected),
      "Restored native URLs, group IDs, labels, membership, kind and selection equal the pre-exit session",
    );
  }
  return state;
}

let active: Browser | undefined;
try {
  active = new Browser("cold-start");
  await active.connect();
  assert(
    (await active.snapshot()).length === 1,
    "Cold profile starts one window",
  );
  await active.client!.executeScript(fixtureScript(0));
  const cold = await checkedState(active, 1);
  results.push({ phase: "normal cold startup", passed: true, windows: cold });
  for (let i = 1; i < 3; i++) {
    await active.client!.executeScript("OpenBrowserWindow(); return true;");
    await until(async () => {
      const states = await active!.snapshot();
      return states.length === i + 1 && states.every((w) => w.ready)
        ? true
        : null;
    }, "Additional browser window ready");
    await active.client!.executeScript(fixtureScript(i));
  }
  const beforeQuit = await checkedState(active, 3);
  await active.save();
  await active.stop(false);
  assert(
    (await Deno.stat(join(profile, "sessionstore.jsonlz4"))).size > 0,
    "Graceful exit writes the real sessionstore.jsonlz4",
  );
  await Deno.copyFile(
    join(profile, "sessionstore.jsonlz4"),
    join(output, "graceful-sessionstore.jsonlz4"),
  );

  active = new Browser("graceful-restore");
  await active.connect();
  const graceful = await checkedState(active, 3, beforeQuit);
  results.push({
    phase: "graceful multi-window restore",
    passed: true,
    windows: graceful,
  });
  await active.save();
  const recovery = join(profile, "sessionstore-backups", "recovery.jsonlz4");
  await until(async () => {
    try {
      return (await Deno.stat(recovery)).size > 0 ? true : null;
    } catch {
      return null;
    }
  }, "Recovery checkpoint written before forced termination");
  await Deno.copyFile(recovery, join(output, "pre-crash-recovery.jsonlz4"));
  evidence.preCrashRecoveryBytes = (await Deno.stat(recovery)).size;
  await active.stop(true);
  try {
    await Deno.stat(join(profile, "sessionstore.jsonlz4"));
    throw new Error(
      "Forced termination unexpectedly left a clean-exit session file",
    );
  } catch (error) {
    if (!(error instanceof Deno.errors.NotFound)) throw error;
  }

  active = new Browser("crash-restore");
  await active.connect();
  const crash = await checkedState(active, 3, graceful);
  results.push({
    phase: "forced-kill crash restore",
    passed: true,
    windows: crash,
  });
  await active.save();
  await active.stop(false);
  active = undefined;
  evidence.passed = true;
} catch (error) {
  evidence.passed = false;
  evidence.error = error instanceof Error ? error.stack : String(error);
  throw error;
} finally {
  if (active) {
    try {
      await active.abort();
    } catch (error) {
      evidence.cleanupError = String(error);
    }
  }
  evidence.finished = new Date().toISOString();
  await Deno.writeTextFile(
    join(output, "results.json"),
    JSON.stringify(evidence, null, 2),
  );
  console.log(`Session regression evidence: ${output}`);
}
console.log(
  "PASS: cold startup, graceful multi-window restore, forced-kill crash restore",
);
