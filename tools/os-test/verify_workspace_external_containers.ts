#!/usr/bin/env -S deno run -A
// SPDX-License-Identifier: MPL-2.0

/**
 * Real HTTP isolation regression for #2823. Requires a patched Floorp Runtime
 * and this checkout's chrome overlay. An unpatched Runtime fails the preflight.
 *
 * Hot paths (caller owns the browser):
 *   deno run --frozen -A tools/os-test/verify_workspace_external_containers.ts --port 2828
 * Cold paths (runner owns temporary profiles and browser processes):
 *   deno run --frozen -A tools/os-test/verify_workspace_external_containers.ts --binary /path/to/floorp
 *
 * Run on an exclusive test browser/display. Neither mode registers an OS default
 * browser. Cold mode checks the real command-line handler, including multiple
 * URLs, persisted cookie/storage jars, forced default and private startup.
 */

import { parseArgs } from "@std/cli/parse-args";
import { assert, assertEquals } from "@std/assert";
import { join, resolve } from "@std/path";
import { MarionetteClient } from "../src/browser_connector.ts";

const args = parseArgs(Deno.args, {
  string: ["binary"],
  default: { port: 2828 },
});
const key = `floorp2823_${crypto.randomUUID().replaceAll("-", "")}`;
const storePref = "floorp.workspaces.v4.store";
const forcePref =
  "browser.link.force_default_user_context_id_for_external_opens";
const defaultID = "01234567-89ab-cdef-0123-456789abcdef";
const pause = () => new Promise((r) => setTimeout(r, 100));
const results: Record<string, unknown> = {};

const server = Deno.serve(
  { hostname: "127.0.0.1", port: 0, onListen() {} },
  (req) => {
    const url = new URL(req.url);
    if (url.pathname !== "/probe") return new Response("", { status: 404 });
    const requestCookie = (req.headers.get("cookie") ?? "").split(";")
      .map((entry) => entry.trim()).find((entry) =>
        entry.startsWith(`${key}=`)
      ) ?? "";
    const seed = url.searchParams.get("seed");
    return new Response(
      `<!doctype html><meta charset="utf-8"><title>Container isolation</title>
<script>
window.requestCookie = ${JSON.stringify(requestCookie)};
const key = ${JSON.stringify(key)};
const seed = ${JSON.stringify(seed)};
if (seed !== null) {
  document.cookie = key + '=' + seed + ';path=/;SameSite=Lax;max-age=3600';
  localStorage.setItem(key, seed);
}
window.fixtureReady = true;
</script>`,
      { headers: { "content-type": "text/html", "cache-control": "no-store" } },
    );
  },
);
const base = `http://127.0.0.1:${server.addr.port}/probe`;
const urlFor = (label: string, seed?: string) =>
  `${base}?case=${label}${seed === undefined ? "" : `&seed=${seed}`}`;

type IdentitySnapshot = {
  tab: number;
  browser: number;
  browsingContext: number;
  principal: number;
  privateBrowsingId: number;
  workspace: string | null;
};
type ContentSnapshot = {
  url: string;
  ready: boolean;
  requestCookie: string;
  cookie: string;
  storage: string | null;
};

// Returns the Marionette tab handle; its UUID must be selected explicitly when
// changing chrome's selectedTab, since Marionette doesn't follow that change.
const selectTab = `
gBrowser.selectedTab = tab;
tab.setAttribute('floorp2823-test', arguments[2] || 'probe');
const {NavigableManager} = ChromeUtils.importESModule('chrome://remote/content/shared/NavigableManager.sys.mjs');
return NavigableManager.getIdForBrowser(tab.linkedBrowser);`;

class Probe {
  constructor(readonly client: MarionetteClient) {}

  async chrome<T>(script: string, parameters: unknown[] = []): Promise<T> {
    await this.client.setContext("chrome");
    return await this.client.executeScript(script, parameters) as T;
  }

  async switchTo(handle: string): Promise<void> {
    await this.client.setContext("content");
    await this.client.send("WebDriver:SwitchToWindow", { handle, focus: true });
  }

  async content(url: string): Promise<ContentSnapshot> {
    const deadline = Date.now() + 30_000;
    while (Date.now() < deadline) {
      await this.client.setContext("content");
      const data = await this.client.executeScript(
        `
let cookie = '', storage = null;
try {
  cookie = document.cookie.split(';').map(s => s.trim()).find(s => s.startsWith(arguments[0] + '=')) || '';
  storage = localStorage.getItem(arguments[0]);
} catch {}
return {url: location.href, ready: document.readyState === 'complete' && !!window.fixtureReady,
  requestCookie: window.requestCookie || '', cookie, storage};`,
        [key],
      ) as ContentSnapshot | null;
      if (data?.url === url && data.ready) return data;
      await pause();
    }
    throw new Error(`fixture did not load: ${url}`);
  }

  async ready(requireOverlay = true): Promise<void> {
    const deadline = Date.now() + 30_000;
    while (Date.now() < deadline) {
      if (
        await this.chrome<boolean>(
          `return !!window.gBrowserInit?.delayedStartupFinished &&
        (!arguments[0] || typeof window.workspacesFuncs?.getCurrentWorkspaceUserContextId === 'function');`,
          [requireOverlay],
        )
      ) break;
      await pause();
    }
    assert(
      await this.chrome<boolean>(
        `const helper = ChromeUtils.importESModule('resource:///modules/BrowserContentHandler.sys.mjs');
      return typeof helper.getWorkspaceUserContextIdForExternalOpen === 'function' &&
        (!arguments[0] || typeof window.workspacesFuncs?.getCurrentWorkspaceUserContextId === 'function');`,
        [requireOverlay],
      ),
      "both the shipping Runtime patch and this checkout's Workspace overlay must be loaded",
    );
  }

  async direct(url: string, cid: number, label: string): Promise<string> {
    const handle = await this.chrome<string>(
      `const tab = gBrowser.addTab(arguments[0], {
      userContextId: arguments[1], triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
      inBackground: false}); ${selectTab}`,
      [url, cid, label],
    );
    await this.switchTo(handle);
    return handle;
  }

  async external(url: string, where: string): Promise<void> {
    const handle = await this.chrome<string>(
      `const {URILoadingHelper} = ChromeUtils.importESModule('resource:///modules/URILoadingHelper.sys.mjs');
const uri = Services.io.newURI(arguments[0]);
const guess = URILoadingHelper.guessUserContextId(uri);
const bc = browserDOMWindow.openURI(uri, null, Ci.nsIBrowserDOMWindow[arguments[1]],
  Ci.nsIBrowserDOMWindow.OPEN_EXTERNAL, Services.scriptSecurityManager.getSystemPrincipal());
const tab = gBrowser.getTabForBrowser(bc.embedderElement); ${selectTab}`,
      [url, where, where],
    );
    await this.switchTo(handle);
  }

  async check(
    label: string,
    url: string,
    cid: number,
    marker: string | null,
    privateId = 0,
    seeded = false,
  ): Promise<void> {
    const content = await this.content(url);
    const identity = await this.chrome<IdentitySnapshot>(
      `const tab = gBrowser.selectedTab, b = tab.linkedBrowser;
return {tab: Number(tab.getAttribute('usercontextid') || 0), browser: Number(b.getAttribute('usercontextid') || 0),
  browsingContext: b.browsingContext.originAttributes.userContextId,
  principal: b.contentPrincipal.originAttributes.userContextId,
  privateBrowsingId: b.contentPrincipal.originAttributes.privateBrowsingId,
  workspace: tab.getAttribute('floorpWorkspaceId')};`,
    );
    results[label] = { identity, content };
    for (
      const field of ["tab", "browser", "browsingContext", "principal"] as const
    ) {
      assertEquals(identity[field], cid, `${label}: ${field}`);
    }
    assertEquals(
      identity.privateBrowsingId,
      privateId,
      `${label}: private mode`,
    );
    assertEquals(
      identity.workspace,
      await this.chrome<string>(
        "return workspacesFuncs.getSelectedWorkspaceID();",
      ),
      `${label}: workspace assignment`,
    );
    assertEquals(
      content.cookie,
      marker === null ? "" : `${key}=${marker}`,
      `${label}: document cookie`,
    );
    assertEquals(content.storage, marker, `${label}: localStorage`);
    if (!seeded) {
      assertEquals(
        content.requestCookie,
        content.cookie,
        `${label}: actual HTTP cookie jar`,
      );
    }
  }

  async closeTab(): Promise<void> {
    await this.chrome(`const t = gBrowser.selectedTab;
      if (t.hasAttribute('floorp2823-test')) gBrowser.removeTab(t, {animate: false});`);
  }

  async chooseURL(url: string): Promise<void> {
    const deadline = Date.now() + 30_000;
    while (Date.now() < deadline) {
      const handle = await this.chrome<string | null>(
        `const tab = gBrowser.tabs.find(t => t.linkedBrowser.currentURI.spec === arguments[0]);
        if (!tab) return null; ${selectTab}`,
        [url, null, "cold"],
      );
      if (handle) {
        await this.switchTo(handle);
        return;
      }
      await pause();
    }
    throw new Error(`command-line tab did not open: ${url}`);
  }
}

async function hot(): Promise<void> {
  const client = await MarionetteClient.connect(Number(args.port));
  const probe = new Probe(client);
  let initialized = false;
  try {
    await probe.ready();
    const contexts = await probe.chrome<{ workspace: number; other: number }>(
      `
const {ContextualIdentityService} = ChromeUtils.importESModule('moz-src:///toolkit/components/contextualidentity/ContextualIdentityService.sys.mjs');
const {NavigableManager} = ChromeUtils.importESModule('chrome://remote/content/shared/NavigableManager.sys.mjs');
window.floorp2823Backup = {
  store: Services.prefs.getStringPref(arguments[0]), selected: workspacesFuncs.getSelectedWorkspaceID(),
  handle: NavigableManager.getIdForBrowser(gBrowser.selectedBrowser),
  prefs: [arguments[1], 'floorp.workspaces.enabled'].map(name => ({name,
    had: Services.prefs.prefHasUserValue(name), value: Services.prefs.getBoolPref(name, name !== arguments[1])})),
};
const workspace = ContextualIdentityService.create('External workspace test', 'fingerprint', 'blue').userContextId;
const other = ContextualIdentityService.create('External explicit/guess test', 'briefcase', 'orange').userContextId;
window.floorp2823Backup.contexts = [workspace, other];
const store = JSON.parse(window.floorp2823Backup.store);
store.data.find(([id]) => id === window.floorp2823Backup.selected)[1].userContextId = workspace;
store.defaultID = arguments[2];
store.data.push([arguments[2], {name:'External default workspace', icon:null, userContextId:other, isSelected:null, isDefault:true}]);
store.order.push(arguments[2]);
Services.prefs.setStringPref(arguments[0], JSON.stringify(store));
Services.prefs.setBoolPref(arguments[1], false);
Services.prefs.setBoolPref('floorp.workspaces.enabled', true);
return {workspace, other};`,
      [storePref, forcePref, defaultID],
    );
    initialized = true;
    const { workspace, other } = contexts;
    for (
      const [cid, marker] of [[0, "default"], [workspace, "workspace"], [
        other,
        "other",
      ]] as const
    ) {
      const url = urlFor(`seed-${cid}`, marker);
      await probe.direct(url, cid, `seed-${cid}`);
      await probe.check(`seed-${cid}`, url, cid, marker, 0, true);
      await probe.closeTab();
    }

    const normal = urlFor("normal");
    const normalHandle = await probe.chrome<string>(
      `BrowserCommands.openTab({url: arguments[0]});
      const tab = gBrowser.selectedTab; ${selectTab}`,
      [normal, null, "normal"],
    );
    await probe.switchTo(normalHandle);
    await probe.check("normal", normal, workspace, "workspace");
    await probe.closeTab();

    for (const where of ["OPEN_NEWTAB", "OPEN_NEWTAB_AFTER_CURRENT"]) {
      const url = urlFor(where);
      assertEquals(
        await probe.chrome(
          `const {URILoadingHelper} = ChromeUtils.importESModule('resource:///modules/URILoadingHelper.sys.mjs');
        return URILoadingHelper.guessUserContextId(Services.io.newURI(arguments[0]));`,
          [url],
        ),
        null,
        "no host guess in the fixture",
      );
      await probe.external(url, where);
      await probe.check(where, url, workspace, "workspace");
      assertEquals(
        (results[where] as { identity: IdentitySnapshot }).identity.workspace,
        await probe.chrome<string>(
          "return workspacesFuncs.getSelectedWorkspaceID();",
        ),
        "workspace assignment",
      );
      await probe.closeTab();
    }

    for (const cid of [0, other]) {
      const explicit = urlFor(`explicit-${cid}`);
      const handle = await probe.chrome<string>(
        `openTrustedLinkIn(arguments[0], 'tab', {
        userContextId: arguments[1], triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal()});
        const tab = gBrowser.selectedTab; ${selectTab}`,
        [explicit, cid, `explicit-${cid}`],
      );
      await probe.switchTo(handle);
      await probe.check(
        `explicit-${cid}`,
        explicit,
        cid,
        cid ? "other" : "default",
      );
      const guessed = urlFor(`guess-${cid}`);
      await probe.external(guessed, "OPEN_NEWTAB");
      await probe.check(
        `guess-${cid}`,
        guessed,
        cid,
        cid ? "other" : "default",
      );
      await probe.closeTab();
      await probe.switchTo(handle);
      await probe.closeTab();
    }

    for (
      const [pref, value, label] of [[forcePref, true, "forced-default"], [
        "floorp.workspaces.enabled",
        false,
        "disabled",
      ]] as const
    ) {
      await probe.chrome(
        "Services.prefs.setBoolPref(arguments[0], arguments[1]);",
        [pref, value],
      );
      const url = urlFor(label);
      await probe.external(url, "OPEN_NEWTAB");
      await probe.check(label, url, 0, "default");
      await probe.closeTab();
      await probe.chrome(
        "Services.prefs.setBoolPref(arguments[0], !arguments[1]);",
        [pref, value],
      );
    }

    const current = urlFor("current-start");
    await probe.direct(current, other, "current");
    await probe.check("current-start", current, other, "other");
    const currentExternal = urlFor("external-current");
    await probe.external(currentExternal, "OPEN_CURRENTWINDOW");
    await probe.check("external-current", currentExternal, other, "other");
    const currentUI = urlFor("ui-current");
    await probe.chrome(
      "openTrustedLinkIn(arguments[0], 'current', {triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal()});",
      [currentUI],
    );
    await probe.check("ui-current", currentUI, other, "other");
    await probe.closeTab();

    // No matching host tab remains. New windows use the profile's default
    // workspace (other), not the opening window's selected workspace.
    const newURL = urlFor("new-window");
    const mainHandle = await probe.chrome<string>(
      "return window.floorp2823Backup.handle;",
    );
    await probe.chrome(
      `window.floorp2823NewWindow = null;
const observer = {observe(win) { Services.obs.removeObserver(this, 'browser-delayed-startup-finished'); window.floorp2823NewWindow = win; }};
Services.obs.addObserver(observer, 'browser-delayed-startup-finished');
browserDOMWindow.openURI(Services.io.newURI(arguments[0]), null, Ci.nsIBrowserDOMWindow.OPEN_NEWWINDOW,
  Ci.nsIBrowserDOMWindow.OPEN_EXTERNAL, Services.scriptSecurityManager.getSystemPrincipal());`,
      [newURL],
    );
    let handle: string | null = null;
    for (let i = 0; i < 300 && !handle; i++) {
      handle = await probe.chrome<string | null>(
        `const win = window.floorp2823NewWindow;
if (!win?.workspacesFuncs || win.gBrowser.selectedBrowser.currentURI.spec !== arguments[0]) return null;
const {NavigableManager} = ChromeUtils.importESModule('chrome://remote/content/shared/NavigableManager.sys.mjs');
return NavigableManager.getIdForBrowser(win.gBrowser.selectedBrowser);`,
        [newURL],
      );
      if (!handle) await pause();
    }
    assert(handle, "external new window initializes");
    await probe.switchTo(handle);
    await probe.check("new-window", newURL, other, "other");
    assertEquals(
      (results["new-window"] as { identity: IdentitySnapshot }).identity
        .workspace,
      defaultID,
      "the new window starts in the default workspace",
    );
    await probe.switchTo(mainHandle);
    await probe.chrome(
      "window.floorp2823NewWindow.close(); delete window.floorp2823NewWindow;",
    );

    const privateHandle = await probe.chrome<string>(
      `window.floorp2823Private = OpenBrowserWindow({private: true});
return window.floorp2823Backup.handle;`,
    );
    let privateTab: string | null = null;
    for (let i = 0; i < 300 && !privateTab; i++) {
      privateTab = await probe.chrome<string | null>(
        `const win = window.floorp2823Private;
if (!win.gBrowserInit?.delayedStartupFinished || !win.workspacesFuncs) return null;
const {NavigableManager} = ChromeUtils.importESModule('chrome://remote/content/shared/NavigableManager.sys.mjs');
return NavigableManager.getIdForBrowser(win.gBrowser.selectedBrowser);`,
      );
      if (!privateTab) await pause();
    }
    assert(privateTab, "private test window initializes");
    await probe.switchTo(privateTab);
    const privateURL = urlFor("private");
    await probe.external(privateURL, "OPEN_NEWTAB");
    await probe.check("private", privateURL, 0, null, 1);
    await probe.switchTo(privateHandle);
    await probe.chrome(
      "window.floorp2823Private.close(); delete window.floorp2823Private;",
    );
  } finally {
    if (initialized) {
      // Return to the owned main window even after a new-window assertion.
      await probe.chrome(
        `const main = [...Services.wm.getEnumerator('navigator:browser')].find(w => w.floorp2823Backup);
        if (main) main.focus();`,
      );
      const mainHandle = await probe.chrome<string>(
        `const main = [...Services.wm.getEnumerator('navigator:browser')].find(w => w.floorp2823Backup);
        return main.floorp2823Backup.handle;`,
      );
      await probe.switchTo(mainHandle);
      // Context 0 persists independently of the two disposable identities.
      const clean = urlFor("cleanup");
      await probe.direct(clean, 0, "cleanup");
      await probe.content(clean);
      await client.setContext("content");
      await client.executeScript("localStorage.removeItem(arguments[0]);", [
        key,
      ]);
      await probe.chrome(
        `const backup = window.floorp2823Backup;
const {ContextualIdentityService} = ChromeUtils.importESModule('moz-src:///toolkit/components/contextualidentity/ContextualIdentityService.sys.mjs');
for (const win of Services.wm.getEnumerator('navigator:browser')) {
  for (const tab of [...win.gBrowser.tabs]) if (tab.hasAttribute('floorp2823-test')) win.gBrowser.removeTab(tab, {animate:false});
}
window.floorp2823NewWindow?.close(); window.floorp2823Private?.close();
for (const id of [0, ...backup.contexts]) Services.cookies.remove('127.0.0.1', arguments[2], '/', {userContextId:id});
Services.prefs.setStringPref(arguments[0], backup.store);
for (const p of backup.prefs) { if (p.had) Services.prefs.setBoolPref(p.name,p.value); else Services.prefs.clearUserPref(p.name); }
for (const id of backup.contexts) ContextualIdentityService.remove(id);
delete window.floorp2823Backup; delete window.floorp2823NewWindow; delete window.floorp2823Private;`,
        [storePref, forcePref, key],
      );
    }
    await client.close();
  }
}

async function cold(binary: string): Promise<void> {
  const profile = await Deno.makeTempDir({ prefix: "floorp2823-cold-" });
  const listener = Deno.listen({ hostname: "127.0.0.1", port: 0 });
  const port = listener.addr.port;
  listener.close();
  const cid = 9;
  const store = JSON.stringify({
    defaultID,
    order: [defaultID],
    data: [[defaultID, {
      name: "Cold external workspace",
      icon: null,
      userContextId: cid,
      isDefault: true,
      isSelected: null,
    }]],
  });
  const prefs: Record<string, unknown> = {
    "marionette.enabled": true,
    "marionette.port": port,
    "remote.active-protocols": 0,
    "privacy.userContext.enabled": true,
    "floorp.workspaces.enabled": true,
    [storePref]: store,
    [forcePref]: false,
    "browser.startup.page": 0,
    "browser.sessionstore.resume_from_crash": false,
    "browser.shell.checkDefaultBrowser": false,
    "browser.startup.homepage_override.mstone": "ignore",
    "nora.dev.allow_http_loader": true,
    "security.chrome_baseline_csp.enabled": false,
    "security.disallow_privileged_https_script_loads": false,
    "security.allow_parent_unrestricted_js_loads": true,
  };
  const writePrefs = () =>
    Deno.writeTextFile(
      join(profile, "user.js"),
      Object.entries(prefs)
        .map(([name, value]) =>
          `user_pref(${JSON.stringify(name)}, ${JSON.stringify(value)});`
        ).join("\n"),
    );
  await Deno.writeTextFile(
    join(profile, "containers.json"),
    JSON.stringify({
      version: 6,
      lastUserContextId: cid,
      identities: [{
        userContextId: cid,
        public: true,
        name: "Cold external workspace",
        icon: "fingerprint",
        color: "blue",
      }],
      siteAssociations: {},
    }),
  );

  async function launch(
    urls: string[],
    check: (probe: Probe) => Promise<void>,
    privateWindow = false,
  ) {
    await writePrefs();
    const process = new Deno.Command(resolve(binary), {
      args: [
        "--no-remote",
        "--profile",
        profile,
        "--marionette",
        "--remote-allow-system-access",
        ...(privateWindow ? ["--private-window"] : []),
        ...urls,
      ],
      stdout: "null",
      stderr: "null",
    }).spawn();
    let client: MarionetteClient | undefined;
    let didExit = true;
    try {
      const deadline = Date.now() + 30_000;
      while (Date.now() < deadline) {
        try {
          const connection = await Deno.connect({
            hostname: "127.0.0.1",
            port,
          });
          connection.close();
          break;
        } catch {
          await pause();
        }
      }
      client = await MarionetteClient.connect(port);
      const probe = new Probe(client);
      await probe.ready();
      await check(probe);
      await probe.chrome("Services.prefs.savePrefFile(null);");
    } finally {
      if (client) {
        try {
          await client.setContext("chrome");
          await client.executeScript(
            "Services.startup.quit(Ci.nsIAppStartup.eForceQuit);",
          );
        } catch { /* The quit closes the protocol connection. */ }
        try {
          await client.close();
        } catch { /* Already closed. */ }
      } else process.kill("SIGTERM");
      const exited = await Promise.race([
        process.status,
        new Promise<null>((r) => setTimeout(() => r(null), 10_000)),
      ]);
      if (exited === null) {
        process.kill("SIGKILL");
        await process.status;
        didExit = false;
      }
    }
    assert(didExit, "cold fixture browser exits after quit");
  }

  try {
    const fresh = urlFor("cold-fresh", "workspace");
    await launch([fresh], async (probe) => {
      await probe.chooseURL(fresh);
      await probe.check("cold-fresh", fresh, cid, "workspace", 0, true);
      const seed = urlFor("cold-default-seed", "default");
      await probe.direct(seed, 0, "seed-default");
      await probe.check("cold-default-seed", seed, 0, "default", 0, true);
      await probe.closeTab();
    });
    const existing = urlFor("cold-existing");
    await launch([existing], async (probe) => {
      await probe.chooseURL(existing);
      await probe.check("cold-existing", existing, cid, "workspace");
    });
    const multiple = [urlFor("cold-multiple-1"), urlFor("cold-multiple-2")];
    await launch(multiple, async (probe) => {
      for (const url of multiple) {
        await probe.chooseURL(url);
        await probe.check(
          new URL(url).searchParams.get("case")!,
          url,
          cid,
          "workspace",
        );
      }
    });
    prefs[forcePref] = true;
    const forced = urlFor("cold-forced-default");
    await launch([forced], async (probe) => {
      await probe.chooseURL(forced);
      await probe.check("cold-forced-default", forced, 0, "default");
    });
    prefs[forcePref] = false;
    const privateURL = urlFor("cold-private");
    await launch([privateURL], async (probe) => {
      await probe.chooseURL(privateURL);
      await probe.check("cold-private", privateURL, 0, null, 1);
    }, true);
  } finally {
    await Deno.remove(profile, { recursive: true });
  }
}

try {
  if (args.binary) await cold(args.binary);
  else await hot();
  console.log(
    JSON.stringify(
      { passed: true, mode: args.binary ? "cold" : "hot", results },
      null,
      2,
    ),
  );
} catch (error) {
  console.log(
    JSON.stringify(
      { passed: false, mode: args.binary ? "cold" : "hot", results },
      null,
      2,
    ),
  );
  throw error;
} finally {
  await server.shutdown();
}
