// SPDX-License-Identifier: MPL-2.0
import { parseArgs } from "@std/cli";
import { dirname, fromFileUrl, join } from "@std/path";
import { MarionetteClient } from "../src/browser_connector.ts";
import type {
  LiveClient,
  LiveContainer,
  LiveIdentity,
  LiveSetup,
  LiveSnapshot,
} from "./live-types.ts";

const options = parseArgs(Deno.args, {
  string: ["setup", "phase"],
  default: { phase: "status" },
});
if (
  !options.setup ||
  !["preflight", "status", "acceptance"].includes(options.phase)
) {
  throw new Error(
    "Use --setup <dedicated setup.json> --phase preflight|status|acceptance",
  );
}
const setupFile = await Deno.realPath(options.setup);
const setup = JSON.parse(await Deno.readTextFile(setupFile)) as LiveSetup;
if (
  setup.clients.length !== 2 ||
  setup.clients[0].profile === setup.clients[1].profile
) throw new Error("Two distinct dedicated profiles are required");
const repository = fromFileUrl(new URL("../../", import.meta.url));
const fixture = await Deno.readTextFile(
  join(
    repository,
    "browser-features/modules/test/workspaces/manual/live-two-client.js",
  ),
);
const clients = new Map<string, MarionetteClient>();
const result = {
  passed: false,
  sameAccount: false,
  clientsSignedIn: false,
  clientsSyncReady: false,
  cloudHosts: [] as string[],
  cases: [] as string[],
  sourceBuild: setup.buildCommit,
  sourceTree: setup.buildTree,
  engineSha256: setup.engineSha256,
  buildWorkingTreeDirty: setup.buildWorkingTreeDirty ?? false,
  resourceSha256: setup.resourceSha256 ?? {},
};
let server: Deno.HttpServer | undefined;
async function operation<T>(
  target: LiveClient,
  action: string,
  args: Record<string, unknown> = {},
): Promise<T> {
  const client = clients.get(target.name);
  if (!client) throw new Error("QA client is not connected");
  const response = await client.sendWithTimeout(
    "WebDriver:ExecuteAsyncScript",
    {
      script:
        `const done=arguments[arguments.length-1];${fixture}\nliveOperation(arguments[0],arguments[1]).then(done,()=>done({qaOperationFailed:true}));`,
      args: [action, { ...args, profile: target.profile, client: target.name }],
    },
    125_000,
  );
  const value = response.value as T & { qaOperationFailed?: boolean };
  if (value?.qaOperationFailed) {
    throw new Error(
      `Client ${target.name} failed QA operation ${action}; credentials and raw browser errors are intentionally not collected`,
    );
  }
  return value;
}
function assert(condition: unknown, description: string): asserts condition {
  if (!condition) throw new Error(description);
}
try {
  for (const target of setup.clients) {
    const client = await MarionetteClient.connect(target.marionettePort);
    await client.setContext("chrome");
    await client.send("WebDriver:SetTimeouts", { script: 120_000 });
    clients.set(target.name, client);
  }
  if (options.phase === "preflight") {
    const checks = await Promise.all(
      setup.clients.map((target) => operation(target, "preflight")),
    );
    console.log(JSON.stringify({ passed: true, preflight: checks }, null, 2));
  } else {
    const [a, b] = setup.clients;
    const identities = await Promise.all(
      setup.clients.map((target) =>
        operation<LiveIdentity>(
          target,
          "ready",
        )
      ),
    );
    result.clientsSignedIn = identities.every((identity) =>
      identity.signedIn && identity.verified
    );
    result.clientsSyncReady = identities.every((identity) =>
      identity.syncReady
    );
    result.sameAccount = Boolean(
      identities[0].uid && identities[0].uid === identities[1].uid,
    );
    if (options.phase === "status") {
      console.log(
        JSON.stringify(
          {
            clientsSignedIn: result.clientsSignedIn,
            sameAccount: result.sameAccount,
            clientsSyncReady: result.clientsSyncReady,
          },
          null,
          2,
        ),
      );
    } else {
      assert(
        result.clientsSignedIn && result.sameAccount && result.clientsSyncReady,
        "Sign in to the same dedicated Mozilla Account in both QA apps before acceptance",
      );
      for (const target of setup.clients) {
        await operation(target, "pause-scheduler");
      }
      server = Deno.serve(
        { hostname: "127.0.0.1", port: 0, onListen: () => {} },
        (request) => {
          const title = new URL(request.url).pathname.replace(
            /[^a-zA-Z0-9-]/g,
            " ",
          ).trim();
          return new Response(
            `<!doctype html><title>QA ${title}</title><p>Workspace Sync acceptance page</p>`,
            { headers: { "content-type": "text/html" } },
          );
        },
      );
      const origin = `http://127.0.0.1:${(server.addr as Deno.NetAddr).port}`;
      const run = crypto.randomUUID();
      const url = (name: string) => `${origin}/${run}/${name}`;
      const sync = async (target: LiveClient, allowEmpty = false) => {
        const received = await operation<
          { finished: boolean; host: string; lastSyncPositive: boolean }
        >(target, "sync");
        assert(
          received.finished && (allowEmpty || received.lastSyncPositive),
          "Authenticated workspace engine sync must finish",
        );
        if (!result.cloudHosts.includes(received.host)) {
          result.cloudHosts.push(received.host);
        }
      };
      const initialSettings = await operation<{ fingerprint: string }>(
        a,
        "settings-fingerprint",
      );
      await sync(a, true);
      await sync(b, true);
      const snapshot = (target: LiveClient, id: string) =>
        operation<LiveSnapshot>(target, "snapshot", { workspaceId: id });
      const settled = async (
        target: LiveClient,
        id: string,
        predicate: (value: LiveSnapshot) => boolean,
        description: string,
      ) => {
        for (let attempt = 0; attempt < 200; attempt++) {
          const observed = await snapshot(target, id);
          if (predicate(observed)) return observed;
          await new Promise((resolve) => setTimeout(resolve, 100));
        }
        throw new Error(description);
      };
      const control = await operation<{ workspaceId: string }>(a, "create", {
        name: `QA Local Only ${run}`,
        urls: [url("local-only")],
        selected: false,
      });
      const selected = await operation<{ workspaceId: string }>(a, "create", {
        name: `QA Selected ${run}`,
        urls: [url("first"), url("second")],
        selected: true,
      });
      const ws = selected.workspaceId;
      await sync(a);
      await sync(b);
      const remote = await settled(
        b,
        ws,
        (value) =>
          value.selected && value.tabs.length === 2 && value.tabs.some((tab) =>
            tab.url === url("first")
          ) && value.tabs.some((tab) => tab.url === url("second")),
        "Selected workspace and both native tabs must arrive on client B",
      );
      assert(
        !(await snapshot(b, control.workspaceId)).exists,
        "Unselected workspace must remain local",
      );
      result.cases.push(
        "selected workspace, per-tab cloud transport and local-only exclusion",
      );
      const first = remote.tabs.find((tab) => tab.url === url("first"))!;
      const second = remote.tabs.find((tab) => tab.url === url("second"))!;
      await operation(b, "edit", {
        workspaceId: ws,
        name: `QA 日本語 🍣 ${run}`,
        tabId: second.id,
        url: url("changed-by-b"),
        pinned: true,
      });
      await sync(b);
      await sync(a);
      await settled(
        a,
        ws,
        (value) =>
          value.name === `QA 日本語 🍣 ${run}` &&
          value.tabs.some((tab) =>
            tab.id === second.id && tab.url === url("changed-by-b") &&
            tab.pinned &&
            tab.title === `QA ${run} changed-by-b`
          ),
        "Remote metadata, navigation, title and pin must arrive on client A",
      );
      result.cases.push(
        "bidirectional metadata, navigation, title and pin changes",
      );
      await operation(a, "edit", {
        workspaceId: ws,
        addURL: url("addition-a"),
      });
      await operation(b, "edit", {
        workspaceId: ws,
        addURL: url("addition-b"),
      });
      await sync(a);
      await sync(b);
      await sync(a);
      await sync(b);
      const hasAdditions = (state: LiveSnapshot) =>
        state.tabs.length === 4 && state.tabs.some((tab) =>
          tab.url === url("addition-a")
        ) && state.tabs.some((tab) => tab.url === url("addition-b"));
      await settled(
        a,
        ws,
        hasAdditions,
        "Concurrent additions must converge on A",
      );
      await settled(
        b,
        ws,
        hasAdditions,
        "Concurrent additions must converge on B",
      );
      result.cases.push("concurrent unsynced additions converge");
      await operation(a, "edit", {
        workspaceId: ws,
        tabId: first.id,
        close: true,
      });
      const reordered = await operation<{ reordered: boolean }>(b, "edit", {
        workspaceId: ws,
        tabId: first.id,
        last: true,
      });
      assert(
        reordered.reordered,
        "The stale client must actually reorder its tab",
      );
      await operation(b, "edit", {
        workspaceId: ws,
        addURL: url("addition-while-close"),
      });
      await sync(a);
      await sync(b);
      await sync(a);
      await sync(b);
      assert(
        [await snapshot(a, ws), await snapshot(b, ws)].every((state) =>
          !state.tabs.some((tab) => tab.id === first.id) &&
          state.tabs.some((tab) => tab.url === url("addition-while-close"))
        ),
        "A close must win over stale reordering and insertion on client B",
      );
      result.cases.push(
        "native tombstone wins over stale reorder and insertion",
      );
      const empty = await operation<{ workspaceId: string }>(a, "create", {
        name: `QA Empty ${run}`,
        urls: [],
        selected: true,
      });
      await sync(a);
      await sync(b);
      await settled(
        b,
        empty.workspaceId,
        (value) => value.selected && value.tabs.length === 0,
        "An empty selected workspace must arrive without creating web tabs",
      );
      result.cases.push("empty workspace sync");
      await operation(b, "select", { workspaceId: ws, selected: false });
      await operation(a, "edit", {
        workspaceId: ws,
        addURL: url("excluded-incoming"),
      });
      await operation(b, "edit", {
        workspaceId: ws,
        addURL: url("excluded-local"),
      });
      await sync(a);
      await sync(b);
      assert(
        !(await snapshot(b, ws)).tabs.some((tab) =>
          tab.url === url("excluded-incoming")
        ) && !(await snapshot(a, ws)).tabs.some((tab) =>
          tab.url === url("excluded-local")
        ),
        "Local opt-out must suppress downloads and uploads while preserving tabs",
      );
      await operation(b, "select", { workspaceId: ws, selected: true });
      await sync(b);
      await sync(a);
      await sync(b);
      assert(
        [await snapshot(a, ws), await snapshot(b, ws)].every((state) =>
          state.tabs.some((tab) => tab.url === url("excluded-incoming")) &&
          state.tabs.some((tab) => tab.url === url("excluded-local"))
        ),
        "Re-selection must backfill remote changes and retain local additions",
      );
      result.cases.push(
        "local opt-out and explicit re-selection recover both sides",
      );

      const sameName = `QA Shared Container ${run}`;
      const sourceContainer = await operation<LiveContainer>(
        a,
        "container-create",
        { name: sameName },
      );
      const receiverContainer = await operation<LiveContainer>(
        b,
        "container-create",
        {
          name: sameName,
          color: "red",
          icon: "fingerprint",
          avoidId: sourceContainer.userContextId,
        },
      );
      assert(
        sourceContainer.userContextId !== receiverContainer.userContextId,
        "Container reuse fixture must use different native IDs on each client",
      );
      const mappedWorkspace = await operation<{ workspaceId: string }>(
        a,
        "create",
        {
          name: `QA Mapped Container ${run}`,
          urls: [url("mapped-container"), url("mapped-none")],
          tabUserContextIds: [sourceContainer.userContextId, 0],
          defaultUserContextId: sourceContainer.userContextId,
          selected: true,
        },
      );
      const mappedId = mappedWorkspace.workspaceId;
      const sourceMapped = await snapshot(a, mappedId);
      const mappedRef = sourceMapped.desiredDefaultContainer;
      assert(
        mappedRef?.kind === "container",
        "Source workspace default must have a logical container reference",
      );
      const sourceMappedTab = sourceMapped.tabs.find((tab) =>
        tab.url === url("mapped-container")
      );
      assert(sourceMappedTab, "Source contained tab must exist");
      await sync(a);
      await sync(b);
      await settled(
        b,
        mappedId,
        (value) =>
          value.containerStates.some((binding) =>
            binding.containerId === mappedRef.containerId &&
            binding.reason === "confirmation"
          ) &&
          !value.tabs.some((tab) => tab.url === url("mapped-container")) &&
          value.tabs.some((tab) =>
            tab.url === url("mapped-none") && tab.userContextId === 0
          ),
        "An existing same-name container must wait for confirmation while explicit-none tabs remain usable",
      );
      await operation(b, "container-bind", {
        containerId: mappedRef.containerId,
        userContextId: receiverContainer.userContextId,
      });
      await settled(
        b,
        mappedId,
        (value) =>
          value.defaultUserContextId === receiverContainer.userContextId &&
          value.tabs.some((tab) =>
            tab.id === sourceMappedTab.id &&
            tab.userContextId === receiverContainer.userContextId
          ),
        "Confirmed association must use the receiver's local container number",
      );
      const reusedMetadata = await operation<LiveContainer[]>(
        b,
        "container-info",
        { userContextId: receiverContainer.userContextId },
      );
      assert(
        reusedMetadata[0]?.color === "red" &&
          reusedMetadata[0]?.icon === "fingerprint",
        "Binding an existing container must preserve its local presentation",
      );
      await operation(b, "container-rename", {
        userContextId: receiverContainer.userContextId,
        name: `QA Renamed Locally ${run}`,
      });
      await sync(b);
      await sync(a);
      assert(
        (await snapshot(b, mappedId)).tabs.some((tab) =>
          tab.id === sourceMappedTab.id &&
          tab.userContextId === receiverContainer.userContextId
        ) &&
          (await operation<LiveContainer[]>(a, "container-info", {
              userContextId: sourceContainer.userContextId,
            }))[0]?.name === sameName,
        "Local rename must keep the remembered association without changing the other client's container",
      );
      result.cases.push(
        "same-name confirmation maps different local IDs, preserves metadata and survives local rename",
      );

      const rebound = await operation<LiveContainer>(b, "container-create", {
        name: `QA Rebound Container ${run}`,
      });
      await operation(b, "container-bind", {
        containerId: mappedRef.containerId,
        userContextId: rebound.userContextId,
      });
      await settled(
        b,
        mappedId,
        (value) =>
          value.error === "container-pending" &&
          value.tabs.some((tab) =>
            tab.id === sourceMappedTab.id &&
            tab.userContextId === receiverContainer.userContextId
          ),
        "Rebinding must leave an already-open tab in its existing native context until explicit reopen",
      );
      const reopened = await operation<
        { reopened: boolean; stableUUID: boolean }
      >(b, "container-reopen", {
        workspaceId: mappedId,
        tabId: sourceMappedTab.id,
        userContextId: rebound.userContextId,
      });
      assert(
        reopened.reopened && reopened.stableUUID,
        "Explicit native UI reopen must keep exactly one owner of the stable tab UUID",
      );
      await sync(b);
      await sync(a);
      assert(
        (await snapshot(a, mappedId)).tabs.some((tab) =>
          tab.id === sourceMappedTab.id &&
          tab.userContextId === sourceContainer.userContextId
        ),
        "A receiver-side rebind/reopen must not change the source's native container mapping",
      );
      result.cases.push(
        "explicit container reopen preserves stable UUID and profile-local mapping",
      );

      const automaticName = `QA Automatic Container ${run}`;
      const automaticSource = await operation<LiveContainer>(
        a,
        "container-create",
        { name: automaticName },
      );
      const automaticWorkspace = await operation<{ workspaceId: string }>(
        a,
        "create",
        {
          name: `QA Automatic Workspace ${run}`,
          urls: [url("automatic-default"), url("automatic-none")],
          tabUserContextIds: [null, 0],
          defaultUserContextId: automaticSource.userContextId,
          selected: true,
        },
      );
      const automaticId = automaticWorkspace.workspaceId;
      const automaticLocal = await snapshot(a, automaticId);
      const automaticRef = automaticLocal.desiredDefaultContainer;
      const inheritedTab = automaticLocal.tabs.find((tab) =>
        tab.url === url("automatic-default")
      );
      assert(
        automaticRef?.kind === "container" &&
          inheritedTab?.userContextId === automaticSource.userContextId &&
          automaticLocal.tabs.some((tab) =>
            tab.url === url("automatic-none") && tab.userContextId === 0
          ),
        "Native opening must inherit the workspace default while preserving explicit zero",
      );
      await sync(a);
      await sync(b);
      const automaticRemote = await settled(
        b,
        automaticId,
        (value) =>
          value.tabs.length === 2 &&
          value.tabs.some((tab) =>
            tab.id === inheritedTab.id && tab.userContextId > 0
          ) &&
          value.tabs.some((tab) =>
            tab.url === url("automatic-none") && tab.userContextId === 0
          ),
        "No-match definitions must auto-create a native container and materialize both contained and explicit-none tabs",
      );
      const automaticReceiverId = automaticRemote.tabs.find((tab) =>
        tab.id === inheritedTab.id
      )!.userContextId;
      assert(
        automaticRemote.defaultUserContextId === automaticReceiverId &&
          (await operation<LiveContainer[]>(b, "container-info", {
              name: automaticName,
            })).length === 1,
        "Automatic receive must create one local identity shared by the workspace default and tab",
      );
      result.cases.push(
        "no-match auto-creation and native workspace-default inheritance preserve explicit none",
      );

      await operation(b, "container-delete", {
        userContextId: automaticReceiverId,
      });
      await settled(
        b,
        automaticId,
        (value) =>
          value.containerStates.some((binding) =>
            binding.containerId === automaticRef.containerId &&
            binding.reason === "deleted"
          ) &&
          !value.tabs.some((tab) => tab.id === inheritedTab.id) &&
          value.desiredTabs.some((tab) =>
            tab.id === inheritedTab.id && !tab.deleted
          ),
        "Deleting a mapped native container must hold its desired tab without an outgoing close",
      );
      await sync(b);
      await sync(a);
      assert(
        (await snapshot(a, automaticId)).tabs.some((tab) =>
          tab.id === inheritedTab.id
        ) &&
          (await operation<LiveContainer[]>(b, "container-info", {
              name: automaticName,
            })).length === 0,
        "Deletion-induced closure must not remove the other client's tab or silently recreate the deleted container",
      );
      await operation(b, "container-create-binding", {
        containerId: automaticRef.containerId,
      });
      await settled(
        b,
        automaticId,
        (value) =>
          value.tabs.some((tab) =>
            tab.id === inheritedTab.id && tab.userContextId > 0 &&
            tab.userContextId !== automaticReceiverId
          ),
        "Explicit new-container recovery must restore the same desired tab UUID in a new local identity",
      );
      result.cases.push(
        "native container deletion emits no remote tombstone and explicit recovery restores the same UUID",
      );

      const beforeDelete = (await snapshot(b, ws)).tabs.length;
      await operation(a, "delete", { workspaceId: ws });
      await operation(a, "delete", { workspaceId: empty.workspaceId });
      await operation(a, "delete", { workspaceId: mappedId });
      await operation(a, "delete", { workspaceId: automaticId });
      await sync(a);
      await sync(b);
      await settled(
        b,
        ws,
        (value) => !value.selected && value.tabs.length === beforeDelete,
        "Remote parent deletion must stop sync and preserve local browser tabs",
      );
      result.cases.push(
        "test record cleanup and remote parent deletion preserve local tabs",
      );
      const finalSettings = await operation<{ fingerprint: string }>(
        a,
        "settings-fingerprint",
      );
      assert(
        initialSettings.fingerprint === finalSettings.fingerprint,
        "Other account engines and declined settings must remain unchanged",
      );
      result.cases.push(
        "other account engine metadata and declined settings remain unchanged",
      );
      result.passed = true;
      const report = join(dirname(setupFile), `acceptance-${run}.json`);
      await Deno.writeTextFile(report, JSON.stringify(result, null, 2));
      console.log(JSON.stringify({ ...result, report }, null, 2));
    }
  }
} finally {
  if (options.phase === "acceptance") {
    for (const target of setup.clients) {
      if (clients.has(target.name)) {
        try {
          await operation(target, "restore-scheduler");
        } catch { /* Leave the dedicated QA profile for inspection. */ }
      }
    }
  }
  for (const client of clients.values()) await client.close();
  await server?.shutdown();
}
Deno.exit(0);
