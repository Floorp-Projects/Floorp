// SPDX-License-Identifier: MPL-2.0
// Exercises the native Sync transport/crypto against an isolated loopback server.
import { parseArgs } from "@std/cli";
import { dirname, fromFileUrl, join, toFileUrl } from "@std/path";
import { MarionetteClient } from "../src/browser_connector.ts";

const root = fromFileUrl(new URL("../../", import.meta.url));
const { binary: binaryOption } = parseArgs(Deno.args, { string: ["binary"] });
if (!binaryOption) {
  throw new Error("--binary /absolute/path/to/floorp required");
}
const binary = await Deno.realPath(binaryOption);
const base = join(root, "_dist", "workspace-migration-native");
await Deno.mkdir(base, { recursive: true });
const scratch = await Deno.makeTempDir({ dir: base, prefix: "run-" });
const profile = join(scratch, "profile");
await Deno.mkdir(profile);
const executable = await Deno.realPath(
  join(root, "node_modules", ".bin", "esbuild"),
);
const esbuild = await import(
  toFileUrl(join(dirname(executable), "..", "lib", "main.js")).href
);
for (
  const [entry, name] of [
    [
      "browser-features/modules/modules/workspaces/WorkspaceContainerMigration.sys.mts",
      "migration",
    ],
    ["browser-features/modules/common/workspace-sync.ts", "model"],
    ["browser-features/modules/common/workspace-sync-record.ts", "record"],
    [
      "browser-features/modules/test/workspaces/workspaceContainerMigration.test.mts",
      "migration-test",
    ],
  ]
) {
  await esbuild.build({
    entryPoints: [join(root, entry)],
    bundle: true,
    format: "esm",
    platform: "neutral",
    outfile: join(scratch, `${name}.sys.mjs`),
  });
}
// A separate module instance represents process restart with the same durable journal.
await Deno.copyFile(
  join(scratch, "migration.sys.mjs"),
  join(scratch, "restart.sys.mjs"),
);

interface StoredRecord {
  id: string;
  payload: string;
  modified?: number;
}
interface CollectionState {
  modified: number;
  records: Map<string, StoredRecord>;
}
const collections = new Map<string, CollectionState>();
const events: {
  method: string;
  path: string;
  conditional: string | null;
  status: number;
}[] = [];
const server = Deno.serve({
  hostname: "127.0.0.1",
  port: 0,
  onListen: () => {},
}, async (request) => {
  const url = new URL(request.url);
  const path = url.pathname;
  const conditional = request.headers.get("x-if-unmodified-since");
  const collectionPath = path.endsWith("/meta/global")
    ? path.slice(0, -7)
    : path;
  let state = collections.get(collectionPath);
  if (!state && path.endsWith("/meta/global")) {
    state = {
      modified: 1,
      records: new Map([["global", {
        id: "global",
        modified: 1,
        payload: JSON.stringify({
          storageVersion: 5,
          syncID: "test-global",
          engines: { bookmarks: { version: 1, syncID: "retain-bookmarks" } },
        }),
      }]]),
    };
    collections.set(collectionPath, state);
  }
  const respond = (
    body: unknown,
    status: number,
    headers: Record<string, string> = {},
  ) => {
    events.push({ method: request.method, path, conditional, status });
    return new Response(JSON.stringify(body), {
      status,
      headers: {
        "content-type": "application/json",
        "x-last-modified": String(state?.modified ?? 0),
        ...headers,
      },
    });
  };
  if (conditional !== null && Number(conditional) !== (state?.modified ?? 0)) {
    return respond({}, 412);
  }
  if (request.method === "GET") {
    if (!state) return respond({}, 404);
    if (path.endsWith("/meta/global")) {
      return respond(state.records.get("global"), 200);
    }
    const all = [...state.records.values()];
    const offset = Number(url.searchParams.get("offset") ?? "0");
    // Force multiple pages even for a small fixture to prove full backfill.
    const page = all.slice(offset, offset + 2);
    return respond(
      page,
      200,
      offset + 2 < all.length
        ? { "x-weave-next-offset": String(offset + 2) }
        : {},
    );
  }
  if (request.method !== "POST" && request.method !== "PUT") {
    return respond({}, 405);
  }
  state ??= { modified: 0, records: new Map() };
  collections.set(collectionPath, state);
  const body = await request.json();
  const records: StoredRecord[] = Array.isArray(body) ? body : [body];
  state.modified += 1;
  for (const record of records) {
    state.records.set(record.id, { ...record, modified: state.modified });
  }
  return respond(
    request.method === "PUT"
      ? state.modified
      : { success: records.map((record) => record.id), failed: {} },
    200,
  );
});
const origin = `http://127.0.0.1:${(server.addr as Deno.NetAddr).port}`;
const listener = Deno.listen({ hostname: "127.0.0.1", port: 0 });
const port = (listener.addr as Deno.NetAddr).port;
listener.close();
await Deno.writeTextFile(
  join(profile, "user.js"),
  [
    `user_pref("marionette.port", ${port});`,
    'user_pref("floorp.workspaces.sync.manual-test", true);',
    'user_pref("browser.shell.checkDefaultBrowser", false);',
    'user_pref("floorp.browser.welcome.page.shown", true);',
    'user_pref("browser.startup.homepage_override.mstone", "ignore");',
  ].join("\n"),
);
const child = new Deno.Command(binary, {
  args: [
    "--headless",
    "--no-remote",
    "--remote-allow-system-access",
    "--marionette",
    "--profile",
    profile,
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
    throw new Error("Isolated browser did not start in 30 seconds");
  }
  client = await MarionetteClient.connect(port);
  await client.setContext("chrome");
  await client.send("WebDriver:SetTimeouts", { script: 45_000 });
  await client.executeScript(
    'if(!Services.prefs.getBoolPref("floorp.workspaces.sync.manual-test",false)||Services.dirsvc.get("ProfD",Ci.nsIFile).path!==arguments[1])throw new Error("Unexpected profile");const file=Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);file.initWithPath(arguments[0]);Services.io.getProtocolHandler("resource").QueryInterface(Ci.nsIResProtocolHandler).setSubstitution("floorp-migration-validation",Services.io.newFileURI(file));',
    [scratch, profile],
  );
  const scenario = (await Deno.readTextFile(
    join(
      root,
      "browser-features/modules/test/workspaces/manual/container-migration-scenario.js",
    ),
  )).replaceAll("__MIGRATION_TEST_ORIGIN__", origin);
  const result = await client.sendWithTimeout("WebDriver:ExecuteAsyncScript", {
    script:
      `const done=arguments[arguments.length-1];${scenario}\nrunScenario().then(done,error=>done({error:String(error),stack:error.stack}));`,
    args: [],
  }, 50_000);
  if (!result?.value?.passed) throw new Error(JSON.stringify(result?.value));
  const legacyWrites = events.filter((event) =>
    event.path === "/main/storage/floorp-workspaces" && event.method !== "GET"
  );
  if (legacyWrites.length !== 1) {
    throw new Error("Migration wrote the legacy collection");
  }
  if (
    !events.some((event) =>
      event.path.endsWith("/floorp-workspaces-migration") &&
      event.status === 412 && event.conditional === "0"
    )
  ) throw new Error("Native CAS race was not observed");
  const report = {
    passed: true,
    result: result.value,
    requests: events,
    scratch,
  };
  await Deno.writeTextFile(
    join(scratch, "results.json"),
    JSON.stringify(report, null, 2),
  );
  console.log(JSON.stringify(report, null, 2));
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
Deno.exit(0);
