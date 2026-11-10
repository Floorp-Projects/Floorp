// SPDX-License-Identifier: MPL-2.0

/**
 * Captures the design matrix: for each design × theme, starts the given
 * Floorp build with a fresh temporary profile, drives it over Marionette and
 * records window-only screenshots plus computed styles of key surfaces.
 */

import * as path from "@std/path";
import { MarionetteClient } from "../src/browser_connector.ts";
import type {
  ComboRecord,
  Design,
  Rect,
  RunManifest,
  StateKey,
  StateRecord,
  StateStyles,
  ThemeKey,
} from "./types.ts";

export interface CaptureOptions {
  exe: string;
  outDir: string;
  designs: Design[];
  themes: ThemeKey[];
  port: number;
  headless: boolean;
  width: number;
  height: number;
  /** Parent directory for temporary profiles (default: OS temp dir). */
  tmpRoot?: string;
  log: (msg: string) => void;
}

const FIXTURES = path.join(import.meta.dirname!, "fixtures");

const BUILTIN_THEMES: Record<"none" | "light" | "dark", string> = {
  none: "default-theme@mozilla.org",
  light: "firefox-compact-light@mozilla.org",
  dark: "firefox-compact-dark@mozilla.org",
};

const LOCAL_THEMES: Record<"lwt-header" | "lwt-bottom", string> = {
  "lwt-header": "lwt-header",
  "lwt-bottom": "lwt-bottom",
};

/** Surfaces whose computed colours are recorded in every state. */
const SURFACES: Array<[string, string]> = [
  ["root", ":root"],
  ["body", "body"],
  ["navigator-toolbox", "#navigator-toolbox"],
  ["TabsToolbar", "#TabsToolbar"],
  ["nav-bar", "#nav-bar"],
  ["selected-tab", "@selected-tab"],
  ["urlbar-background", "#urlbar-background"],
  ["browser", "#browser"],
  ["panel-sidebar-box", "#panel-sidebar-box"],
  ["nora-statusbar", "#nora-statusbar"],
];

const SURFACE_PROPS = [
  "background-color",
  "background-image",
  "color",
  "opacity",
  "display",
  "visibility",
];

const MENU_PROPS = [
  "background-color",
  "color",
  "border-top-color",
  "border-top-left-radius",
  "padding-top",
  "--panel-background",
  "--panel-color",
  "--menu-background-color",
  "--menu-color",
  "--menuitem-hover-background-color",
];

function userPrefs(
  design: Design,
  port: number,
  proxyPort: number,
): Record<string, unknown> {
  return {
    "marionette.port": port,
    // Marionette's recommended automation prefs stop Activity Stream from
    // rendering about:home; the prefs below set what this tool needs instead.
    "remote.prefs.recommended": false,
    "browser.startup.homepage_override.mstone": "ignore",
    "browser.newtab.preload": false,
    "floorp.design.configs": JSON.stringify({
      globalConfigs: { userInterface: design },
      uiCustomization: { disableFloorpStart: true },
    }),
    // First-run and update pages.
    "browser.startup.page": 0,
    "browser.startup.homepage": "about:blank",
    "startup.homepage_welcome_url": "",
    "startup.homepage_welcome_url.additional": "",
    "browser.aboutwelcome.enabled": false,
    "floorp.browser.welcome.page.shown": true,
    "floorp.releaseNotes.choiceConfirmed": true,
    "floorp.releaseNotes.promptShown": true,
    "browser.shell.checkDefaultBrowser": false,
    "app.update.disabledForTesting": true,
    "browser.uitour.enabled": false,
    "datareporting.policy.dataSubmissionEnabled": false,
    "toolkit.telemetry.reportingpolicy.firstRun": false,
    "browser.tabs.warnOnClose": false,
    "browser.sessionstore.resume_from_crash": false,
    "extensions.update.enabled": false,
    "extensions.getAddons.cache.enabled": false,
    // All traffic goes to the tool's proxy, which serves the fixture page and
    // refuses everything else: what is shown comes from the build only.
    "network.proxy.type": 1,
    "network.proxy.http": "127.0.0.1",
    "network.proxy.http_port": proxyPort,
    "network.proxy.ssl": "127.0.0.1",
    "network.proxy.ssl_port": proxyPort,
    "network.proxy.share_proxy_settings": true,
    "services.settings.server": "data:,#remote-settings-dummy/v1",
    // about:home: keep the search box and the shortcut tiles, nothing remote.
    "browser.newtabpage.activity-stream.default.sites":
      "https://www.example.com/,https://www.example.org/,https://www.example.net/",
    "browser.newtabpage.activity-stream.feeds.section.topstories": false,
    "browser.newtabpage.activity-stream.feeds.system.topstories": false,
    "browser.newtabpage.activity-stream.showSponsored": false,
    "browser.newtabpage.activity-stream.showSponsoredTopSites": false,
    "browser.newtabpage.activity-stream.showWeather": false,
    "browser.newtabpage.activity-stream.system.showWeather": false,
    "browser.newtabpage.activity-stream.feeds.weatherfeed": false,
    "browser.newtabpage.activity-stream.feeds.snippets": false,
    "browser.newtabpage.activity-stream.asrouter.userprefs.cfr.addons": false,
    "browser.newtabpage.activity-stream.asrouter.userprefs.cfr.features": false,
    // Determinism.
    // Pin the OS colour scheme to light: otherwise theme "none" (the default
    // theme follows the system) renders like "dark" on a dark-mode machine and
    // results depend on the machine. Built-in Dark and LWTs set their own.
    "ui.systemUsesDarkTheme": 0,
    "ui.caretBlinkTime": 0,
    "ui.prefersReducedMotion": 1,
    "toolkit.cosmeticAnimations.enabled": false,
    "layout.css.scrollbar-fade-out.enabled": false,
    "privacy.sanitize.useOldClearHistoryDialog": false,
  };
}

function prefLine(name: string, value: unknown): string {
  return `user_pref(${JSON.stringify(name)}, ${JSON.stringify(value)});`;
}

const sleep = (ms: number) => new Promise((r) => setTimeout(r, ms));

const FIXTURE_TYPES: Record<string, string> = {
  ".html": "text/html; charset=utf-8",
  ".svg": "image/svg+xml",
  ".json": "application/json",
};

/**
 * A tiny HTTP proxy: requests for http://design-matrix.test/<file> get the
 * file from `fixtures/`, every other request (including HTTPS CONNECT) is
 * refused, so the browser never reaches the network.
 */
function startFixtureProxy(port: number): Deno.HttpServer {
  return Deno.serve({
    hostname: "127.0.0.1",
    port,
    onListen: () => {},
  }, async (req) => {
    let url: URL;
    try {
      url = new URL(req.url);
    } catch {
      return new Response("blocked", { status: 403 });
    }
    if (req.method !== "GET" || url.hostname !== FIXTURE_HOST) {
      return new Response("blocked", { status: 403 });
    }
    const name = url.pathname.replace(/^\/+/, "");
    if (!/^[\w.-]+$/.test(name)) {
      return new Response("not found", { status: 404 });
    }
    try {
      const body = await Deno.readFile(path.join(FIXTURES, name));
      return new Response(body, {
        headers: {
          "content-type": FIXTURE_TYPES[path.extname(name)] ??
            "application/octet-stream",
          "cache-control": "no-store",
        },
      });
    } catch {
      return new Response("not found", { status: 404 });
    }
  });
}

async function waitForPortFile(profile: string, timeoutMs: number) {
  const file = path.join(profile, "MarionetteActivePort");
  const end = Date.now() + timeoutMs;
  while (Date.now() < end) {
    try {
      const port = parseInt((await Deno.readTextFile(file)).trim(), 10);
      if (!isNaN(port)) return port;
    } catch {
      // not written yet
    }
    await sleep(250);
  }
  throw new Error(`Marionette did not start within ${timeoutMs}ms`);
}

async function removeWithRetry(dir: string): Promise<void> {
  for (let i = 0; i < 20; i++) {
    try {
      await Deno.remove(dir, { recursive: true });
      return;
    } catch (e) {
      if (e instanceof Deno.errors.NotFound) return;
      await sleep(500);
    }
  }
  throw new Error(`Could not delete temporary profile ${dir}`);
}

/** `pgrep` could not be started (not installed or not on PATH). */
class PgrepUnavailable extends Error {}

/**
 * PIDs of all descendants of `pid` (POSIX, via `pgrep -P`), deepest first.
 * Throws `PgrepUnavailable` when `pgrep` cannot be run, so the caller can say
 * that only the direct child will be stopped.
 */
async function descendants(pid: number): Promise<number[]> {
  let res: Deno.CommandOutput;
  try {
    res = await new Deno.Command("pgrep", {
      args: ["-P", String(pid)],
      stdout: "piped",
      stderr: "null",
    }).output();
  } catch (e) {
    if (e instanceof Deno.errors.NotFound) {
      throw new PgrepUnavailable("pgrep not found");
    }
    return []; // e.g. the process is gone
  }
  const out: number[] = [];
  const kids = new TextDecoder().decode(res.stdout).split(/\s+/)
    .map((s) => parseInt(s, 10)).filter((n) => !isNaN(n));
  for (const kid of kids) {
    out.push(...await descendants(kid), kid);
  }
  return out;
}

/**
 * Kills the process we started and every process below it, and nothing else.
 * Windows: `taskkill /T` on our launcher PID (kept alive by
 * `-wait-for-browser`). Linux/macOS: the descendants of our PID are collected
 * with `pgrep -P` before anything is killed, then each gets SIGKILL. Without
 * `pgrep` only our PID is killed, and `log` says so.
 */
async function killTree(
  child: Deno.ChildProcess,
  log: (msg: string) => void,
): Promise<void> {
  try {
    if (Deno.build.os === "windows") {
      await new Deno.Command("taskkill", {
        args: ["/PID", String(child.pid), "/T", "/F"],
        stdout: "null",
        stderr: "null",
      }).output();
      return;
    }
    let pids: number[] = [];
    try {
      pids = await descendants(child.pid);
    } catch (e) {
      if (!(e instanceof PgrepUnavailable)) throw e;
      log(
        `[design-matrix] warning: pgrep not found; killing only pid ${child.pid}, its child processes may keep running`,
      );
    }
    try {
      child.kill("SIGKILL");
    } catch {
      // already gone
    }
    for (const pid of pids) {
      try {
        Deno.kill(pid, "SIGKILL");
      } catch {
        // already gone
      }
    }
  } catch {
    // already gone
  }
}

async function waitExit(
  child: Deno.ChildProcess,
  timeoutMs: number,
): Promise<boolean> {
  const exited = await Promise.race([
    child.status.then(() => true),
    sleep(timeoutMs).then(() => false),
  ]);
  return exited;
}

// ---------------------------------------------------------------------------
// Chrome-context scripts (run in the browser window via Marionette).

const WAIT_FOR = `
const waitFor = async (fn, ms) => {
  const end = Date.now() + ms;
  while (Date.now() < end) {
    try { if (fn()) return true; } catch (_) {}
    await new Promise((r) => setTimeout(r, 100));
  }
  return false;
};`;

function asyncScript(body: string): string {
  return `const done = arguments[arguments.length - 1];
${WAIT_FOR}
(async () => { ${body} })().then(done, (e) => done({ error: String(e) }));`;
}

const SCRIPT_STATUS = `return {
  ready: !!(window.gBrowserInit && gBrowserInit.delayedStartupFinished),
  config: Services.prefs.getStringPref("floorp.design.configs", ""),
  appVersion: Services.appinfo.version,
  platformVersion: Services.appinfo.platformVersion,
  buildID: Services.appinfo.appBuildID,
  dpr: window.devicePixelRatio,
};`;

/**
 * The fixture page is served by the tool's own proxy (see `startFixtureProxy`)
 * under a fixed host name, so the URL bar text does not depend on where the
 * repository is checked out or which port is used.
 */
const FIXTURE_HOST = "design-matrix.test";
const PAGE_URL = `http://${FIXTURE_HOST}/page.html`;

const SCRIPT_RESET_TABS = asyncScript(`
  const keep = gBrowser.selectedTab;
  for (const t of [...gBrowser.tabs]) if (t !== keep) gBrowser.removeTab(t);
  return gBrowser.tabs.length;
`);

const SCRIPT_ENABLE_THEME = asyncScript(`
  const [id] = arguments;
  const { AddonManager } = ChromeUtils.importESModule(
    "resource://gre/modules/AddonManager.sys.mjs");
  const addon = await AddonManager.getAddonByID(id);
  if (!addon) throw new Error("theme not found: " + id);
  await addon.enable();
  await waitFor(() => Services.prefs.getStringPref("extensions.activeThemeID", "") === id, 10000);
  await new Promise((r) => setTimeout(r, 1500));
  return {
    id: Services.prefs.getStringPref("extensions.activeThemeID", ""),
    lwtheme: document.documentElement.hasAttribute("lwtheme"),
  };
`);

/** What the browser actually applied: design, theme and colour scheme. */
const SCRIPT_APPLIED = `
let userInterface = null;
try {
  userInterface = JSON.parse(Services.prefs.getStringPref("floorp.design.configs", "{}"))
    ?.globalConfigs?.userInterface ?? null;
} catch (_) {}
return {
  userInterface,
  themeID: Services.prefs.getStringPref("extensions.activeThemeID", ""),
  lwtheme: document.documentElement.hasAttribute("lwtheme"),
  systemDark: window.matchMedia("(-moz-system-dark-theme)").matches,
};`;

const SCRIPT_FOCUS_CONTENT = asyncScript(`
  gURLBar.blur();
  gBrowser.selectedBrowser.focus();
  await new Promise((r) => setTimeout(r, 300));
  return true;
`);

const SCRIPT_STYLES = `
const [surfaces, props] = arguments;
const norm = (s) => s.replace(/moz-extension:\\/\\/[^/]+\\//g, "moz-extension://<id>/");
const out = {};
for (const [name, sel] of surfaces) {
  let el = null;
  if (sel === "@selected-tab") {
    const tab = gBrowser.selectedTab;
    el = tab?.querySelector(".tab-background") ?? tab ?? null;
  } else {
    el = document.querySelector(sel);
  }
  if (!el) { out[name] = null; continue; }
  const cs = getComputedStyle(el);
  const v = {};
  for (const p of props) {
    let s = norm(cs.getPropertyValue(p).trim());
    if (s.length > 400) s = s.slice(0, 400) + "...";
    v[p] = s;
  }
  const r = el.getBoundingClientRect();
  v.rect = Math.round(r.x) + "," + Math.round(r.y) + " " + Math.round(r.width) + "x" + Math.round(r.height);
  out[name] = v;
}
return out;`;

const SCRIPT_WAIT_CONTEXT_MENU = asyncScript(`
  const popup = document.getElementById("contentAreaContextMenu");
  const shown = await waitFor(() => popup.state === "open", 4000);
  await new Promise((r) => setTimeout(r, 300));
  const [props] = arguments;
  const read = (el) => {
    if (!el) return null;
    const cs = getComputedStyle(el);
    const v = {};
    for (const p of props) v[p] = cs.getPropertyValue(p).trim();
    return v;
  };
  const item = [...popup.children].find((c) => c.localName === "menuitem" && !c.hidden);
  const sep = [...popup.children].find((c) => c.localName === "menuseparator" && !c.hidden);
  const styles = { popup: read(popup), "first-menuitem": read(item), separator: read(sep) };
  popup.hidePopup();
  await waitFor(() => popup.state === "closed", 2000);
  return { shown, styles };
`);

const SCRIPT_ENTER_FULLSCREEN = asyncScript(`
  window.fullScreen = true;
  const entered = await waitFor(() => document.documentElement.getAttribute("sizemode") === "fullscreen", 8000);
  await new Promise((r) => setTimeout(r, 1500));
  FullScreen.showNavToolbox();
  await new Promise((r) => setTimeout(r, 1500));
  return {
    entered,
    overlap: Services.prefs.getBoolPref("userChrome.fullscreen.overlap", false),
    autohide: Services.prefs.getBoolPref("browser.fullscreen.autohide", true),
  };
`);

const SCRIPT_EXIT_FULLSCREEN = asyncScript(`
  window.fullScreen = false;
  const left = await waitFor(() => document.documentElement.getAttribute("sizemode") !== "fullscreen", 8000);
  await new Promise((r) => setTimeout(r, 1000));
  return left;
`);

const SCRIPT_OPEN_CLEAR_HISTORY = asyncScript(`
  const { Sanitizer } = ChromeUtils.importESModule("resource:///modules/Sanitizer.sys.mjs");
  Sanitizer.showUI(window);
  const frameSel = ".dialogFrame";
  const loaded = await waitFor(() => {
    const f = document.querySelector(frameSel);
    const d = f?.contentDocument;
    return d && d.readyState === "complete" && d.documentURI.includes("sanitize");
  }, 8000);
  // Let the async data-size labels settle.
  await new Promise((r) => setTimeout(r, 2500));
  const frame = document.querySelector(frameSel);
  const doc = frame?.contentDocument;
  const read = (el) => {
    if (!el) return null;
    const cs = (el.ownerGlobal ?? window).getComputedStyle(el);
    const r = el.getBoundingClientRect();
    return { "background-color": cs.backgroundColor, color: cs.color,
      rect: Math.round(r.x) + "," + Math.round(r.y) + " " + Math.round(r.width) + "x" + Math.round(r.height) };
  };
  // The cache and site-data sizes in these labels vary run to run (and can
  // change after this point): mask the label from the end of the checkbox
  // square, plus 80px for a longer size text, but not past the dialog.
  const dpr = window.devicePixelRatio;
  const fr = frame?.getBoundingClientRect();
  const masks = [];
  for (const id of ["cache", "cookiesAndStorage"]) {
    for (const el of doc?.querySelectorAll("#" + id) ?? []) {
      const r = el.getBoundingClientRect();
      if (!fr || r.width === 0 || r.height === 0) continue;
      const x = fr.x + r.x + 24;
      masks.push({ x: Math.floor(x * dpr), y: Math.floor((fr.y + r.y) * dpr),
        width: Math.ceil((Math.min(fr.right, fr.x + r.right + 80) - x) * dpr),
        height: Math.ceil(r.height * dpr) });
    }
  }
  return {
    loaded,
    masks,
    styles: {
      "dialog-box": read(frame?.closest(".dialogBox")),
      "dialog-root": read(doc?.documentElement),
    },
  };
`);

const SCRIPT_CLOSE_CLEAR_HISTORY = asyncScript(`
  const frame = document.querySelector(".dialogFrame");
  frame?.contentWindow?.close();
  return await waitFor(() => !document.querySelector(".dialogFrame")?.contentDocument?.documentURI?.includes("sanitize"), 5000);
`);

// ---------------------------------------------------------------------------

interface Session {
  client: MarionetteClient;
  outDir: string;
  prefix: string;
  record: ComboRecord;
}

async function shot(
  s: Session,
  state: StateKey,
  rec: StateRecord,
  masks: Rect[] = [],
): Promise<StateRecord["shot"]> {
  await s.client.setContext("chrome");
  // Wait until two consecutive captures are identical, so late paints
  // (sidebar icons, async labels) do not leak into the result.
  let png = await s.client.takeScreenshot({ full: false });
  let settled = false;
  for (let i = 0; i < SETTLE_MAX_TRIES; i++) {
    await sleep(SETTLE_INTERVAL_MS);
    const next = await s.client.takeScreenshot({ full: false });
    if (sameBytes(png, next)) {
      settled = true;
      break;
    }
    png = next;
  }
  if (!settled) {
    rec.problems.push("window kept changing; screenshot may be noisy");
  }
  const file = `${s.prefix}__${state}.png`;
  await Deno.writeFile(path.join(s.outDir, file), png);
  return { file, masks };
}

const SETTLE_MAX_TRIES = 10;
const SETTLE_INTERVAL_MS = 700;

function sameBytes(a: Uint8Array, b: Uint8Array): boolean {
  if (a.length !== b.length) return false;
  for (let i = 0; i < a.length; i++) if (a[i] !== b[i]) return false;
  return true;
}

async function styles(s: Session): Promise<StateStyles> {
  await s.client.setContext("chrome");
  return (await s.client.executeScript(SCRIPT_STYLES, [
    SURFACES,
    SURFACE_PROPS,
  ])) as StateStyles;
}

async function asyncChrome(
  s: Session,
  script: string,
  args: unknown[] = [],
): Promise<Record<string, unknown>> {
  await s.client.setContext("chrome");
  const res = await s.client.send("WebDriver:ExecuteAsyncScript", {
    script,
    args,
  });
  const value = res?.value as Record<string, unknown> | undefined;
  if (value && typeof value === "object" && "error" in value) {
    throw new Error(String(value.error));
  }
  return value ?? {};
}

async function navigateContent(s: Session, url: string): Promise<void> {
  await s.client.setContext("content");
  await s.client.navigate(url);
  await s.client.executeScript(
    "document.activeElement?.blur?.(); return true;",
  );
  await asyncChrome(s, SCRIPT_FOCUS_CONTENT);
}

async function applyTheme(s: Session, theme: ThemeKey): Promise<string> {
  let id: string;
  if (theme === "none" || theme === "light" || theme === "dark") {
    id = BUILTIN_THEMES[theme];
  } else {
    const res = await s.client.send("Addon:Install", {
      path: path.join(FIXTURES, LOCAL_THEMES[theme]),
      temporary: true,
    });
    id = String(res?.value ?? res);
  }
  const result = await asyncChrome(s, SCRIPT_ENABLE_THEME, [id]);
  if (result.id !== id) {
    throw new Error(`theme ${id} did not become active (${result.id})`);
  }
  return id;
}

/**
 * Checks that the browser applied the requested design and theme, so a
 * capture is never stored under the wrong label.
 */
async function verifyApplied(
  s: Session,
  design: Design,
  theme: ThemeKey,
  themeID: string,
): Promise<void> {
  await s.client.setContext("chrome");
  const a = (await s.client.executeScript(SCRIPT_APPLIED)) as Record<
    string,
    unknown
  >;
  if (a.userInterface !== design) {
    throw new Error(
      `design not applied: requested ${design}, browser has ${a.userInterface}`,
    );
  }
  if (a.themeID !== themeID) {
    throw new Error(
      `theme not applied: requested ${themeID}, browser has ${a.themeID}`,
    );
  }
  const isLwt = theme === "lwt-header" || theme === "lwt-bottom";
  if (isLwt && !a.lwtheme) {
    throw new Error(`theme ${theme} is active but [lwtheme] is not set`);
  }
  if (a.systemDark) {
    throw new Error(
      "system colour scheme is dark despite ui.systemUsesDarkTheme=0",
    );
  }
}

async function runStates(s: Session): Promise<void> {
  const states = s.record.states;
  const run = async (
    key: StateKey,
    fn: (rec: StateRecord) => Promise<void>,
  ) => {
    const rec: StateRecord = {
      shot: null,
      styles: {},
      notes: [],
      problems: [],
    };
    states[key] = rec;
    try {
      await fn(rec);
    } catch (e) {
      const msg = e instanceof Error ? e.message : String(e);
      rec.problems.push(`error: ${msg}`);
    }
  };

  await navigateContent(s, PAGE_URL);

  await run("main", async (rec) => {
    rec.styles = await styles(s);
    rec.shot = await shot(s, "main", rec);
  });

  // WebDriver screenshots do not include native popups, so the context menu
  // is recorded as computed styles only.
  await run("contextmenu", async (rec) => {
    await s.client.setContext("content");
    await s.client.send("WebDriver:PerformActions", {
      actions: [{
        type: "pointer",
        id: "design-matrix-mouse",
        parameters: { pointerType: "mouse" },
        actions: [
          { type: "pointerMove", x: 400, y: 320, origin: "viewport" },
          { type: "pointerDown", button: 2 },
          { type: "pointerUp", button: 2 },
        ],
      }],
    });
    const res = await asyncChrome(s, SCRIPT_WAIT_CONTEXT_MENU, [MENU_PROPS]);
    await s.client.setContext("content");
    await s.client.send("WebDriver:ReleaseActions", {});
    if (!res.shown) rec.problems.push("context menu did not open");
    rec.styles = res.styles as StateStyles;
  });

  await run("fullscreen", async (rec) => {
    const res = await asyncChrome(s, SCRIPT_ENTER_FULLSCREEN);
    if (!res.entered) rec.problems.push("window did not enter fullscreen");
    rec.notes.push(
      `userChrome.fullscreen.overlap=${res.overlap} browser.fullscreen.autohide=${res.autohide}`,
    );
    try {
      rec.styles = await styles(s);
      rec.shot = await shot(s, "fullscreen", rec);
    } finally {
      await asyncChrome(s, SCRIPT_EXIT_FULLSCREEN);
    }
  });

  await run("clear-history", async (rec) => {
    const res = await asyncChrome(s, SCRIPT_OPEN_CLEAR_HISTORY);
    if (!res.loaded) rec.problems.push("Clear History dialog did not load");
    try {
      rec.styles = res.styles as StateStyles;
      rec.shot = await shot(
        s,
        "clear-history",
        rec,
        (res.masks as Rect[] | undefined) ?? [],
      );
    } finally {
      await asyncChrome(s, SCRIPT_CLOSE_CLEAR_HISTORY);
    }
  });

  await run("newtab", async (rec) => {
    await navigateContent(s, "about:home");
    await s.client.setContext("content");
    // Wait for Activity Stream to render its tiles.
    const end = Date.now() + 8000;
    while (Date.now() < end) {
      const ok = await s.client.executeScript(
        "return !!document.querySelector('.top-sites-list .top-site-outer, .search-wrapper');",
      );
      if (ok) break;
      await sleep(200);
    }
    await sleep(1500);
    await asyncChrome(s, SCRIPT_FOCUS_CONTENT);
    rec.styles = await styles(s);
    rec.shot = await shot(s, "newtab", rec);
  });
}

interface BuildInfo {
  appVersion: string;
  platformVersion: string;
  buildID: string;
}

async function captureCombo(
  opts: CaptureOptions,
  tmpRoot: string,
  design: Design,
  theme: ThemeKey,
): Promise<{ record: ComboRecord; info: BuildInfo | null }> {
  const record: ComboRecord = { design, theme, states: {}, errors: [] };
  if (run.interrupted) {
    record.errors.push("interrupted before start");
    return { record, info: null };
  }
  const profile = await Deno.makeTempDir({
    dir: tmpRoot,
    prefix: `${design}-${theme}-`,
  });
  await Deno.writeTextFile(
    path.join(profile, "user.js"),
    Object.entries(userPrefs(design, opts.port, opts.port + 1))
      .map(([k, v]) => prefLine(k, v)).join("\n") + "\n",
  );

  const env: Record<string, string> = {
    MOZ_REMOTE_ALLOW_SYSTEM_ACCESS: "1",
    MOZ_CRASHREPORTER_DISABLE: "1",
    MOZ_HEADLESS_WIDTH: String(opts.width),
    MOZ_HEADLESS_HEIGHT: String(opts.height),
  };
  const args = ["-no-remote", "-profile", profile, "-marionette"];
  if (opts.headless) args.push("-headless");
  // Keep the Windows launcher process alive until the browser exits, so the
  // PID we hold always owns the browser process tree.
  if (Deno.build.os === "windows") args.push("-wait-for-browser");
  // Checked again right before spawning: Ctrl+C may have arrived while the
  // profile was being written.
  if (run.interrupted) {
    await removeWithRetry(profile);
    record.errors.push("interrupted before start");
    return { record, info: null };
  }
  const child = new Deno.Command(opts.exe, {
    args,
    env,
    stdin: "null",
    stdout: "null",
    stderr: "null",
  }).spawn();
  run.child = child;
  run.profile = profile;

  let client: MarionetteClient | null = null;
  let info: BuildInfo | null = null;
  try {
    const port = await waitForPortFile(profile, 60_000);
    client = await MarionetteClient.connect(port);
    await client.setContext("chrome");

    // Wait for startup and for the design config to be applied.
    const end = Date.now() + 60_000;
    let status: Record<string, unknown> = {};
    while (Date.now() < end) {
      status = await client.executeScript(SCRIPT_STATUS);
      if (status.ready && String(status.config).includes("appliedUserJs")) {
        break;
      }
      await sleep(500);
    }
    if (!status.ready) throw new Error("browser window did not finish startup");
    info = {
      appVersion: String(status.appVersion),
      platformVersion: String(status.platformVersion),
      buildID: String(status.buildID),
    };
    await client.send("WebDriver:SetWindowRect", {
      x: 0,
      y: 0,
      width: opts.width,
      height: opts.height,
    });
    await sleep(3000);
    const s: Session = {
      client,
      outDir: opts.outDir,
      prefix: `${design}__${theme}`,
      record,
    };
    await asyncChrome(s, SCRIPT_RESET_TABS);
    const themeID = await applyTheme(s, theme);
    await verifyApplied(s, design, theme, themeID);
    await runStates(s);
  } catch (e) {
    record.errors.push(e instanceof Error ? e.message : String(e));
  } finally {
    if (client) {
      try {
        await client.sendWithTimeout(
          "Marionette:Quit",
          { flags: ["eForceQuit"] },
          10_000,
        );
      } catch {
        // connection closes during shutdown
      }
      await client.close();
    }
    if (!(await waitExit(child, 20_000))) {
      opts.log(`  browser pid ${child.pid} did not exit; killing its tree`);
      await killTree(child, opts.log);
      await waitExit(child, 10_000);
    }
    await removeWithRetry(profile);
    run.child = null;
    run.profile = null;
  }
  return { record, info };
}

/**
 * State of the current capture, so Ctrl+C can stop the browser we started,
 * delete its profile and the temporary root, and keep the loop from
 * starting another browser meanwhile.
 */
const run: {
  interrupted: boolean;
  child: Deno.ChildProcess | null;
  profile: string | null;
  tmpRoot: string | null;
  proxy: Deno.HttpServer | null;
} = {
  interrupted: false,
  child: null,
  profile: null,
  tmpRoot: null,
  proxy: null,
};

function onInterrupt(): void {
  if (run.interrupted) return;
  run.interrupted = true;
  console.error("[design-matrix] interrupted; stopping the browser it started");
  (async () => {
    const { child, profile, tmpRoot, proxy } = run;
    if (child) {
      await killTree(child, (msg) => console.error(msg));
      await waitExit(child, 10_000);
    }
    if (profile) await removeWithRetry(profile).catch(() => {});
    if (tmpRoot) await removeWithRetry(tmpRoot).catch(() => {});
    await proxy?.shutdown().catch(() => {});
    Deno.exit(130);
  })();
}

/** Product version from application.ini (e.g. "12.21.1@159.0.1"), if present. */
async function readIniVersion(exe: string): Promise<string | null> {
  for (
    const ini of [
      path.join(path.dirname(exe), "application.ini"),
      path.join(path.dirname(exe), "..", "Resources", "application.ini"),
    ]
  ) {
    try {
      const m = (await Deno.readTextFile(ini)).match(/^Version=(.+)$/m);
      if (m) return m[1].trim();
    } catch {
      // try the next location
    }
  }
  return null;
}

/**
 * Writes `manifest.json` atomically (a temporary file in the same directory,
 * then a rename), so a process killed mid-write never leaves a truncated or
 * half-old manifest behind.
 */
export async function writeManifest(
  outDir: string,
  manifest: RunManifest,
): Promise<void> {
  const file = path.join(outDir, "manifest.json");
  const tmp = `${file}.${Deno.pid}.tmp`;
  await Deno.writeTextFile(tmp, JSON.stringify(manifest, null, 2) + "\n");
  try {
    await Deno.rename(tmp, file);
  } catch (e) {
    await Deno.remove(tmp).catch(() => {});
    throw e;
  }
}

/** The capture was interrupted (Ctrl+C); its manifest is marked incomplete. */
export class CaptureInterrupted extends Error {}

export async function captureMatrix(
  opts: CaptureOptions,
): Promise<RunManifest> {
  await Deno.mkdir(opts.outDir, { recursive: true });
  const startedAt = new Date().toISOString();
  const combos: ComboRecord[] = [];
  let info: BuildInfo | null = null;
  const iniVersion = await readIniVersion(opts.exe);
  const manifestFor = (complete: boolean): RunManifest => ({
    tool: "design-matrix",
    formatVersion: 1,
    complete,
    startedAt,
    finishedAt: complete ? new Date().toISOString() : "",
    exe: opts.exe,
    appVersion: iniVersion ?? info?.appVersion ?? "unknown",
    platformVersion: info?.platformVersion ?? "unknown",
    buildID: info?.buildID ?? "unknown",
    headless: opts.headless,
    windowSize: { width: opts.width, height: opts.height },
    combos: [...combos],
  });
  // Replace any earlier manifest in this directory right away: from here
  // until the last combination is done, the directory holds an incomplete
  // run, whatever stops the tool (Ctrl+C, a crash, a killed process).
  await writeManifest(opts.outDir, manifestFor(false));

  const tmpRoot = await Deno.makeTempDir({
    dir: opts.tmpRoot,
    prefix: "floorp-design-matrix-",
  });
  run.interrupted = false;
  run.tmpRoot = tmpRoot;
  Deno.addSignalListener("SIGINT", onInterrupt);
  try {
    try {
      run.proxy = startFixtureProxy(opts.port + 1);
    } catch (e) {
      throw new Error(
        `cannot listen on proxy port ${opts.port + 1}: ${
          e instanceof Error ? e.message : String(e)
        }`,
      );
    }
    for (const design of opts.designs) {
      for (const theme of opts.themes) {
        if (run.interrupted) break;
        opts.log(`[design-matrix] ${design} × ${theme}`);
        const started = Date.now();
        const res = await captureCombo(opts, tmpRoot, design, theme);
        info ??= res.info;
        combos.push(res.record);
        for (const err of res.record.errors) opts.log(`  error: ${err}`);
        opts.log(`  done in ${((Date.now() - started) / 1000).toFixed(1)}s`);
        // Progress so far, still marked incomplete.
        await writeManifest(opts.outDir, manifestFor(false));
      }
    }
  } finally {
    Deno.removeSignalListener("SIGINT", onInterrupt);
    await run.proxy?.shutdown();
    run.proxy = null;
    await removeWithRetry(tmpRoot);
    run.tmpRoot = null;
  }
  if (run.interrupted) {
    // The combinations not yet started are absent, which a compare would
    // otherwise read as a deliberate --designs/--themes subset.
    throw new CaptureInterrupted(
      `interrupted with ${combos.length} of ${
        opts.designs.length * opts.themes.length
      } combinations started; ${
        path.join(opts.outDir, "manifest.json")
      } is marked incomplete`,
    );
  }
  const manifest = manifestFor(true);
  await writeManifest(opts.outDir, manifest);
  return manifest;
}
