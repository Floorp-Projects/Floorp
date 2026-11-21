// SPDX-License-Identifier: MPL-2.0

import { parseArgs } from "@std/cli";
import { dirname, fromFileUrl, join, toFileUrl } from "@std/path";
import { MarionetteClient } from "../src/browser_connector.ts";

const root = fromFileUrl(new URL("../../", import.meta.url));
const options = parseArgs(Deno.args, {
  string: ["binary"],
  boolean: ["model-only"],
});
if (!options.binary) {
  console.error(
    "Usage: deno run -A tools/workspace-sync/verify-browser.ts --binary /absolute/path/to/floorp [--model-only]",
  );
  Deno.exit(2);
}
const binary = await Deno.realPath(options.binary);
const base = join(root, "_dist", "workspace-sync-manual");
await Deno.mkdir(base, { recursive: true });
const scratch = await Deno.makeTempDir({ dir: base, prefix: "run-" });
const profile = join(scratch, "profile");
await Deno.mkdir(profile);
const realProfile = await Deno.realPath(profile);

// Resolve the repository's installed esbuild without installing or rewriting any
// workspace dependencies. The usual Deno node_modules layout provides this link.
const executable = await Deno.realPath(
  join(root, "node_modules", ".bin", "esbuild"),
);
const esbuild = await import(
  toFileUrl(join(dirname(executable), "..", "lib", "main.js")).href
);
const bundle = (entry: string, outfile: string) =>
  esbuild.build({
    entryPoints: [join(root, entry)],
    bundle: true,
    format: "esm",
    platform: "neutral",
    outfile: join(scratch, outfile),
  });
await bundle(
  "browser-features/modules/common/workspace-sync.ts",
  "model.sys.mjs",
);
await bundle(
  "browser-features/modules/test/workspaces/workspaceSync.test.mts",
  "model-test.sys.mjs",
);
await bundle(
  "browser-features/modules/test/workspaces/workspaceSyncRecord.test.mts",
  "record-test.sys.mjs",
);
await bundle(
  "browser-features/modules/test/workspaces/workspaceContainers.test.mts",
  "container-model-test.sys.mjs",
);
await bundle(
  "browser-features/modules/test/workspaces/workspaceContainerBindings.test.mts",
  "container-bindings-test.sys.mjs",
);
await bundle(
  "browser-features/modules/test/workspaces/workspaceContainerMigration.test.mts",
  "container-migration-test.sys.mjs",
);
if (!options["model-only"]) {
  const isolatedModuleSource = (source: string) =>
    source.replaceAll(
      "resource://noraneko/modules/workspaces/FloorpWorkspacesSync.sys.mjs",
      "resource://floorp-workspace-sync-validation/engine.sys.mjs",
    ).replaceAll(
      "resource://noraneko/modules/workspaces/WorkspaceContainerBindings.sys.mjs",
      "resource://floorp-workspace-sync-validation/container-bindings.sys.mjs",
    ).replaceAll(
      "resource://noraneko/modules/workspaces/WorkspaceContainerMigration.sys.mjs",
      "resource://floorp-workspace-sync-validation/container-migration.sys.mjs",
    );
  const bindingsPath = join(
    root,
    "browser-features/modules/modules/workspaces/WorkspaceContainerBindings.sys.mts",
  );
  await esbuild.build({
    stdin: {
      contents: isolatedModuleSource(await Deno.readTextFile(bindingsPath))
        .replaceAll(
          '"floorp-workspace-container-bindings.json"',
          '"floorp-workspace-container-bindings-manual.json"',
        ),
      resolveDir: dirname(bindingsPath),
      loader: "ts",
    },
    bundle: true,
    format: "esm",
    platform: "neutral",
    outfile: join(scratch, "container-bindings.sys.mjs"),
  });
  const migrationPath = join(
    root,
    "browser-features/modules/modules/workspaces/WorkspaceContainerMigration.sys.mts",
  );
  await esbuild.build({
    stdin: {
      contents: isolatedModuleSource(await Deno.readTextFile(migrationPath))
        .replaceAll(
          '"floorp-workspaces-migration.json"',
          '"floorp-workspaces-migration-manual.json"',
        ),
      resolveDir: dirname(migrationPath),
      loader: "ts",
    },
    bundle: true,
    format: "esm",
    platform: "neutral",
    outfile: join(scratch, "container-migration.sys.mjs"),
  });
  const enginePath = join(
    root,
    "browser-features/modules/modules/workspaces/FloorpWorkspacesSync.sys.mts",
  );
  await esbuild.build({
    stdin: {
      contents: isolatedModuleSource(await Deno.readTextFile(enginePath))
        .replace(
          '"floorp-workspaces-sync.json"',
          '"floorp-workspaces-sync-legacy-manual.json"',
        ).replace(
          '"floorp-workspaces-sync-v2.json"',
          '"floorp-workspaces-sync-v2-manual.json"',
        ),
      resolveDir: dirname(enginePath),
      loader: "ts",
    },
    bundle: true,
    format: "esm",
    platform: "neutral",
    outfile: join(scratch, "engine.sys.mjs"),
    external: [
      "./WorkspaceContainerBindings.sys.mts",
      "./WorkspaceContainerMigration.sys.mts",
    ],
  });
  const isolatedEngine = join(scratch, "engine.sys.mjs");
  await Deno.writeTextFile(
    isolatedEngine,
    (await Deno.readTextFile(isolatedEngine)).replaceAll(
      "./WorkspaceContainerBindings.sys.mts",
      "resource://floorp-workspace-sync-validation/container-bindings.sys.mjs",
    ).replaceAll(
      "./WorkspaceContainerMigration.sys.mts",
      "resource://floorp-workspace-sync-validation/container-migration.sys.mjs",
    ),
  );

  const workspaceDir = join(root, "browser-features/chrome/common/workspaces");
  const stub = join(scratch, "store.ts");
  await Deno.writeTextFile(
    stub,
    `export const workspacesDataStore={data:new Map(),order:[]};
export function setWorkspacesDataStore(key,value){workspacesDataStore[key]=typeof value==='function'?value(workspacesDataStore[key]):value;}`,
  );
  const adapterPath = join(scratch, "adapter.ts");
  let adapter = isolatedModuleSource(
    await Deno.readTextFile(join(workspaceDir, "workspacesSync.ts")),
  );
  adapter = adapter.replace(
    'from "./data/data.ts"',
    `from ${JSON.stringify(stub)}`,
  )
    .replaceAll('from "./utils/', `from "${workspaceDir}/utils/`)
    .replaceAll('from "./', `from "${workspaceDir}/`)
    .replaceAll('from "#modules/', `from "${root}/browser-features/modules/`)
    .replace(
      "resource://noraneko/modules/workspaces/FloorpWorkspacesSync.sys.mjs",
      "resource://floorp-workspace-sync-validation/engine.sys.mjs",
    );
  await Deno.writeTextFile(adapterPath, adapter);
  await esbuild.build({
    stdin: {
      contents: `export {WorkspacesSyncController} from ${
        JSON.stringify(adapterPath)
      }; export {createRoot} from 'solid-js'; export {workspacesDataStore as testingStore} from ${
        JSON.stringify(stub)
      };`,
      resolveDir: root,
    },
    bundle: true,
    format: "esm",
    platform: "neutral",
    conditions: ["browser"],
    outfile: join(scratch, "adapter.sys.mjs"),
    // System-module tests supply the browser globals that the real chrome entry
    // naturally has. The tab API and SessionStore remain the actual browser's.
    banner: {
      js:
        `const window=Services.wm.getMostRecentWindow("navigator:browser");const document=window.document;const gBrowser=window.gBrowser;const MutationObserver=window.MutationObserver;const AbortController=window.AbortController;globalThis.setTimeout=window.setTimeout.bind(window);globalThis.clearTimeout=window.clearTimeout.bind(window);`,
    },
  });
  const isolatedAdapter = join(scratch, "adapter.sys.mjs");
  await Deno.writeTextFile(
    isolatedAdapter,
    isolatedModuleSource(await Deno.readTextFile(isolatedAdapter)),
  );
}

const listener = Deno.listen({ hostname: "127.0.0.1", port: 0 });
const port = (listener.addr as Deno.NetAddr).port;
listener.close();
const server = Deno.serve(
  { hostname: "127.0.0.1", port: 0, onListen: () => {} },
  async (request) => {
    const url = new URL(request.url);
    if (url.pathname === "/sync-slow") {
      const milliseconds = Number(url.searchParams.get("delay") ?? "1500");
      const bounded = Number.isFinite(milliseconds)
        ? Math.min(Math.max(milliseconds, 0), 5000)
        : 0;
      await new Promise((resolve) => setTimeout(resolve, bounded));
    }
    if (url.pathname === "/sync-no-content") {
      return new Response(null, { status: 204 });
    }
    if (url.pathname === "/sync-history-noise") {
      return new Response(
        '<!doctype html><title>History noise</title><script>let n=0;const target=new URLSearchParams(location.search).get("target");setTimeout(()=>setInterval(()=>{history.pushState(null,"","#state-"+(++n));history.replaceState(null,"","#replace-"+n);if(target)history.replaceState(null,"",target)},25),150)</script>',
        { headers: { "content-type": "text/html" } },
      );
    }
    return new Response(
      "<!doctype html><title>Sync test</title><p>Workspace Sync test</p>",
      { headers: { "content-type": "text/html" } },
    );
  },
);
const origin = `http://127.0.0.1:${(server.addr as Deno.NetAddr).port}`;
await Deno.writeTextFile(
  join(profile, "user.js"),
  [
    `user_pref("marionette.port", ${port});`,
    'user_pref("floorp.workspaces.sync.manual-test", true);',
    'user_pref("browser.shell.checkDefaultBrowser", false);',
    'user_pref("floorp.browser.welcome.page.shown", true);',
    'user_pref("browser.startup.homepage_override.mstone", "ignore");',
    'user_pref("services.sync.engine.floorp-workspaces-v2", true);',
  ].join("\n"),
);
const child = new Deno.Command(binary, {
  args: [
    "--headless",
    "--no-remote",
    "--remote-allow-system-access",
    "--marionette",
    "--profile",
    realProfile,
    "about:blank",
  ],
  env: {
    MOZ_HEADLESS: "1",
    MOZ_MARIONETTE: "1",
    MOZ_REMOTE_ALLOW_SYSTEM_ACCESS: "1",
  },
  stdout: "null",
  stderr: "null",
}).spawn();
let client: MarionetteClient | undefined;
try {
  let listening = false;
  for (let attempt = 0; attempt < 60; attempt++) {
    try {
      const connection = await Deno.connect({ hostname: "127.0.0.1", port });
      connection.close();
      listening = true;
      break;
    } catch {
      await new Promise((resolve) => setTimeout(resolve, 500));
    }
  }
  if (!listening) {
    throw new Error(
      "The isolated test browser did not start within 30 seconds",
    );
  }
  client = await MarionetteClient.connect(port);
  await client.setContext("chrome");
  await client.send("WebDriver:SetTimeouts", { script: 45_000 });
  await client.executeScript(
    `if(!Services.prefs.getBoolPref("floorp.workspaces.sync.manual-test",false))throw new Error("Dedicated test profile required");if(Services.dirsvc.get("ProfD",Ci.nsIFile).path!==arguments[1])throw new Error("Unexpected browser profile");const file=Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);file.initWithPath(arguments[0]);Services.io.getProtocolHandler("resource").QueryInterface(Ci.nsIResProtocolHandler).setSubstitution("floorp-workspace-sync-validation",Services.io.newFileURI(file));`,
    [scratch, realProfile],
  );
  const results = [];
  const scenarios = [
    `async function runScenario(){const suites=["model-test","record-test","container-model-test","container-bindings-test","container-migration-test"];for(const name of suites){await ChromeUtils.importESModule("resource://floorp-workspace-sync-validation/"+name+".sys.mjs").runAllTests();}return {passed:true,modelSuites:suites};}`,
    ...(options["model-only"] ? [] : [
      await Deno.readTextFile(
        join(
          root,
          "browser-features/modules/test/workspaces/manual/engine-scenario.js",
        ),
      ),
      (await Deno.readTextFile(
        join(
          root,
          "browser-features/modules/test/workspaces/manual/adapter-scenario.js",
        ),
      )).replaceAll("__WORKSPACE_SYNC_TEST_ORIGIN__", origin),
    ]),
  ];
  for (const [index, script] of scenarios.entries()) {
    const scenario = ["models", "engine", "adapter"][index];
    console.log(`[workspace-sync QA] Running ${scenario}`);
    const result = await client.sendWithTimeout(
      "WebDriver:ExecuteAsyncScript",
      {
        script:
          `const done=arguments[arguments.length-1];${script}\nrunScenario().then(done,error=>done({error:String(error),stack:error.stack}));`,
        args: [],
      },
      50_000,
    );
    if (!result?.value?.passed) throw new Error(JSON.stringify(result?.value));
    results.push(result.value);
    await Deno.writeTextFile(
      join(scratch, "results.json"),
      JSON.stringify({ complete: false, completed: results }, null, 2),
    );
  }
  await Deno.writeTextFile(
    join(scratch, "results.json"),
    JSON.stringify(results, null, 2),
  );
  console.log(JSON.stringify({ passed: true, results, scratch }, null, 2));
} finally {
  await client?.close();
  try {
    child.kill("SIGTERM");
  } catch { /* Already exited. */ }
  let stopped = false;
  await Promise.race([
    child.status.then(() => {
      stopped = true;
    }),
    new Promise((resolve) => setTimeout(resolve, 5000)),
  ]);
  if (!stopped) {
    try {
      child.kill("SIGKILL");
    } catch { /* Already exited. */ }
    await child.status;
  }
  await server.shutdown();
}

// The shared Marionette client uses bounded timer races; all owned resources
// have been closed above, so do not wait for unused timeout promises.
Deno.exit(0);
