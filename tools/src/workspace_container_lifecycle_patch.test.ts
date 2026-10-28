// SPDX-License-Identifier: MPL-2.0

import { assert, assertEquals, assertRejects } from "@std/assert";

const SOURCE_PATCH = new URL(
  "../../.github/patches/floorp-runtime/common/workspace-container-lifecycle.patch",
  import.meta.url,
);
const SERVICE =
  "toolkit/components/contextualidentity/ContextualIdentityService.sys.mjs";
const PREFS = "browser/components/preferences/config/containers.mjs";
const EXTENSION =
  "toolkit/components/extensions/parent/ext-contextualIdentities.js";
const PREPARING = "floorp-container-removal-preparing";
const FINISHED = "floorp-container-removal-finished";
const GUARD = "floorp-container-removal-guard";

interface Operation {
  id: string;
  userContextId: number;
  userContextIds: readonly number[];
  reason: "delete" | "disable";
}

interface Notification {
  operationId: string | null;
  userContextId: number;
  userContextIds: readonly number[];
  reason: "delete" | "disable";
  outcome?: "completed" | "cancelled";
  blockers?: Promise<void>[];
  prepared?: boolean;
  blocked?: boolean;
}

interface Service {
  prepareRemoval(id: number, reason: "delete" | "disable"): Promise<Operation>;
  finishRemoval(
    op: Operation,
    outcome: "completed" | "cancelled",
  ): Promise<void>;
  disableContainersForFloorpRemoval(): Promise<void>;
  requestDisableForFloorpRemoval(): Promise<void>;
  remove(id: number, op?: Operation): boolean;
  ensureDataReady(): void;
  getPublicIdentityFromId(id: number): { userContextId: number } | undefined;
  getPublicUserContextIds(): number[];
  closeContainerTabs(id?: number): Promise<void>;
  countContainerTabs(id?: number): number;
  notifyAllContainersCleared(): void;
  resetDefault(): void;
  recordRemoval(id: number): void;
}

// Execute the actual lifecycle carried by the production patch. The surrounding
// native identities/prefs/browser calls are test doubles, not a second copy of
// the lifecycle policy. Real Gecko/HTTP acceptance remains a separate gate.
function patchedExcerpt(patch: string, path: string): string {
  const header = `diff --git a/${path} b/${path}\n`;
  const start = patch.indexOf(header);
  assert(start >= 0, path);
  const next = patch.indexOf("\ndiff --git ", start + header.length);
  return patch.slice(start, next < 0 ? undefined : next).split("\n")
    .filter((line) =>
      (line.startsWith("+") && !line.startsWith("+++")) ||
      line.startsWith(" ")
    )
    .map((line) => line.slice(1)).join("\n");
}

function between(source: string, start: string, end: string): string {
  const first = source.indexOf(start);
  assert(first >= 0, start);
  const last = source.indexOf(end, first + start.length);
  assert(last >= 0, end);
  return source.slice(first, last);
}

async function fixture() {
  const patch = await Deno.readTextFile(SOURCE_PATCH);
  const source = patchedExcerpt(patch, SERVICE);
  const preamble = between(
    source,
    "const floorpRemovalOperations",
    "// Canonical container colors",
  );
  const methods = between(source, "  async prepareRemoval(", "  remove(");
  const guard = between(
    source,
    "    const state = operation &&",
    "    Services.clearData.deleteDataFromOriginAttributesPattern",
  );
  const listeners = new Map<string, (detail: Notification) => void>();
  const notifications: { topic: string; detail: Notification }[] = [];
  const actions: string[] = [];
  const identities = new Set([7, 9]);
  let enabled = false;
  let nextId = 0;
  const services = {
    obs: {
      notifyObservers(
        subject: { wrappedJSObject: Notification },
        topic: string,
      ) {
        const detail = subject.wrappedJSObject;
        notifications.push({ topic, detail });
        listeners.get(topic)?.(detail);
      },
    },
    uuid: { generateUUID: () => `{operation-${++nextId}}` },
    prefs: {
      getBoolPref: () => enabled,
      setBoolPref: (_name: string, value: boolean) => {
        enabled = value;
      },
    },
  };
  const service = new Function(
    "Services",
    "console",
    `const MAX_USER_CONTEXT_ID = 0xffffffff;
     const CONTEXTUAL_IDENTITY_ENABLED_PREF = "privacy.userContext.enabled";
     ${preamble}
     return {${methods}
       remove(userContextId, operation = null) {
         ${guard}
         this.recordRemoval(userContextId);
         return true;
       }
     };`,
  )(services, { error() {} }) as Service;
  Object.assign(service, {
    ensureDataReady() {},
    getPublicIdentityFromId(id: number) {
      return identities.has(id) ? { userContextId: id } : undefined;
    },
    getPublicUserContextIds: () => [...identities],
    closeContainerTabs(id = 0) {
      actions.push(`close:${id}`);
      return Promise.resolve();
    },
    countContainerTabs: () => 0,
    notifyAllContainersCleared() {
      actions.push("clear-data");
    },
    resetDefault() {
      actions.push("reset-identities");
    },
    recordRemoval(id: number) {
      actions.push(`remove:${id}`);
      identities.delete(id);
    },
  });
  const settingsBody = between(
    patchedExcerpt(patch, PREFS),
    "  const operation =",
    "\n}\n\nPreferences",
  );
  const removeInSettings = new Function(
    "lazy",
    `return async function(userContextId) {${settingsBody}\n};`,
  )({ ContextualIdentityService: service }) as (id: number) => Promise<void>;
  const extensionBody = between(
    patchedExcerpt(patch, EXTENSION),
    "          const operation =",
    "          return convertedIdentity;",
  );
  const removeInExtension = new Function(
    "ContextualIdentityService",
    "ExtensionError",
    `return async function(id) {
      const identity = {userContextId: id};
      const cookieStoreId = "firefox-container-" + id;
      ${extensionBody}
    };`,
  )(service, Error) as (id: number) => Promise<void>;
  return {
    patch,
    service,
    listeners,
    notifications,
    actions,
    identities,
    removeInSettings,
    removeInExtension,
    setEnabled: (value: boolean) => enabled = value,
    isEnabled: () => enabled,
  };
}

Deno.test("container lifecycle patches agree for packaging and extracted artifacts", async () => {
  const source = await Deno.readTextFile(SOURCE_PATCH);
  const mappings = [
    [SERVICE, `moz-src/${SERVICE}`],
    [
      PREFS,
      "browser/chrome/browser/content/browser/preferences/config/containers.mjs",
    ],
    [
      "browser/components/preferences/config/tabs-browsing.mjs",
      "browser/chrome/browser/content/browser/preferences/config/tabs-browsing.mjs",
    ],
    [
      EXTENSION,
      "chrome/toolkit/content/extensions/parent/ext-contextualIdentities.js",
    ],
  ];
  let expected = source;
  for (const [from, to] of mappings) expected = expected.replaceAll(from, to);
  assertEquals(
    await Deno.readTextFile(
      new URL(
        "../patches/workspace-container-lifecycle.patch",
        import.meta.url,
      ),
    ),
    expected,
  );
  assertEquals(
    [...source.matchAll(/^\+\+\+ b\/(.+)$/gm)].map((match) => match[1]).sort(),
    mappings.map(([from]) => from).sort(),
    "only the four existing JavaScript resources change",
  );
});

Deno.test("settings removal waits for persisted protection before closing any tab", async () => {
  const f = await fixture();
  const saved = Promise.withResolvers<void>();
  f.listeners.set(PREPARING, (detail) => detail.blockers?.push(saved.promise));
  const pending = f.removeInSettings(7);
  assertEquals(f.actions, []);
  const preparing = f.notifications[0].detail;
  assertEquals(preparing.userContextIds, [7]);
  saved.resolve();
  await pending;
  assertEquals(f.actions, ["close:7", "remove:7"]);
  assertEquals(f.notifications.at(-1)?.detail.outcome, "completed");
});

Deno.test("extension removal waits too without adding an unsolicited close-all", async () => {
  const f = await fixture();
  const saved = Promise.withResolvers<void>();
  f.listeners.set(PREPARING, (detail) => detail.blockers?.push(saved.promise));
  const pending = f.removeInExtension(7);
  assertEquals(f.actions, []);
  saved.resolve();
  await pending;
  assertEquals(f.actions, ["remove:7"]);
  assertEquals(f.notifications.at(-1)?.detail.outcome, "completed");
});

Deno.test("failed durable preparation waits for siblings before cancelling and never destroys", async () => {
  const f = await fixture();
  const lateSave = Promise.withResolvers<void>();
  f.listeners.set(PREPARING, (detail) => {
    detail.blockers?.push(
      Promise.reject(new Error("disk full")),
      lateSave.promise,
    );
  });
  const pending = f.removeInSettings(7);
  await Promise.resolve();
  await Promise.resolve();
  assertEquals(f.notifications.filter((n) => n.topic === FINISHED).length, 0);
  lateSave.resolve();
  await assertRejects(() => pending, AggregateError, "could not be persisted");
  assertEquals(f.actions, []);
  assertEquals(f.notifications.at(-1)?.detail.outcome, "cancelled");
});

Deno.test("cancelled settings tab closure keeps the identity and finishes as cancelled", async () => {
  const f = await fixture();
  f.service.countContainerTabs = () => 1;
  await f.removeInSettings(7);
  assertEquals(f.actions, ["close:7"]);
  assert(f.identities.has(7));
  assertEquals(f.notifications.at(-1)?.detail.outcome, "cancelled");
});

Deno.test("direct mapped removal is guarded and tokens cannot authorize another identity", async () => {
  const f = await fixture();
  f.listeners.set(GUARD, (detail) => {
    if (detail.userContextId === 7 && !detail.prepared) detail.blocked = true;
  });
  assertEquals(f.service.remove(7), false);
  const operation = await f.service.prepareRemoval(7, "delete");
  assertEquals(f.service.remove(9, operation), false, "wrong identity token");
  assertEquals(f.service.remove(7, operation), true);
  assertEquals(f.service.remove(7, operation), false, "already consumed token");
  await f.service.finishRemoval(operation, "completed");
  assertEquals(f.service.remove(7, operation), false, "finished token");
  assertEquals(
    f.service.remove(9),
    true,
    "unmapped native caller remains supported",
  );
});

Deno.test("disable snapshots all identities and protects before close/clear/reset", async () => {
  const f = await fixture();
  const saved = Promise.withResolvers<void>();
  f.listeners.set(PREPARING, (detail) => detail.blockers?.push(saved.promise));
  const pending = f.service.disableContainersForFloorpRemoval();
  assertEquals(f.actions, []);
  assertEquals(f.notifications[0].detail.userContextIds, [7, 9]);
  assert(Object.isFrozen(f.notifications[0].detail.userContextIds));
  saved.resolve();
  await pending;
  assertEquals(f.actions, ["close:0", "clear-data", "reset-identities"]);
  assertEquals(f.notifications.at(-1)?.detail.reason, "disable");
  assertEquals(f.notifications.at(-1)?.detail.outcome, "completed");
});

Deno.test("cancellation disarms a token before completion persistence finishes", async () => {
  const f = await fixture();
  const operation = await f.service.prepareRemoval(7, "delete");
  const saved = Promise.withResolvers<void>();
  f.listeners.set(FINISHED, (detail) => detail.blockers?.push(saved.promise));
  const pending = f.service.finishRemoval(operation, "cancelled");
  assertEquals(f.service.remove(7, operation), false);
  assertEquals(f.actions, []);
  saved.resolve();
  await pending;
});

Deno.test("failed or refused disable restores enabled pref without clearing identity data", async () => {
  for (const failure of ["persist", "close"]) {
    const f = await fixture();
    if (failure === "persist") {
      f.listeners.set(PREPARING, (detail) => {
        detail.blockers?.push(Promise.reject(new Error("disk full")));
      });
    } else f.service.countContainerTabs = () => 1;
    await f.service.disableContainersForFloorpRemoval();
    assertEquals(f.actions, failure === "persist" ? [] : ["close:0"]);
    assert(f.isEnabled());
    assertEquals(f.notifications.at(-1)?.detail.outcome, "cancelled");
  }
});

Deno.test("reenabling during persistence cancels disable before the first closure", async () => {
  const f = await fixture();
  const saved = Promise.withResolvers<void>();
  f.listeners.set(PREPARING, (detail) => detail.blockers?.push(saved.promise));
  const pending = f.service.disableContainersForFloorpRemoval();
  f.setEnabled(true);
  saved.resolve();
  await pending;
  assertEquals(f.actions, []);
  assertEquals(f.notifications.at(-1)?.detail.outcome, "cancelled");
});

Deno.test("ordinary close-all does not acquire a removal lifecycle", async () => {
  const f = await fixture();
  const source = patchedExcerpt(f.patch, SERVICE);
  const closeHunk = source.slice(source.indexOf("      let remoteTabIds"));
  assert(!closeHunk.includes("prepareRemoval"));
  assert(!closeHunk.includes("floorp-container-removal"));
});

Deno.test("a disable requested during cancellation persistence receives a new preflight", async () => {
  const f = await fixture();
  const prepared = Promise.withResolvers<void>();
  const releasePreparation = Promise.withResolvers<void>();
  const cancelling = Promise.withResolvers<void>();
  const releaseCancellation = Promise.withResolvers<void>();
  let preparations = 0;
  f.listeners.set(PREPARING, (detail) => {
    if (++preparations === 1) {
      detail.blockers?.push(releasePreparation.promise);
      prepared.resolve();
    }
  });
  f.listeners.set(FINISHED, (detail) => {
    if (detail.outcome === "cancelled") {
      detail.blockers?.push(releaseCancellation.promise);
      cancelling.resolve();
    }
  });
  const first = f.service.requestDisableForFloorpRemoval();
  await prepared.promise;
  f.setEnabled(true);
  releasePreparation.resolve();
  await cancelling.promise;
  f.setEnabled(false);
  const second = f.service.requestDisableForFloorpRemoval();
  assertEquals(
    f.actions,
    [],
    "no destruction before cancellation is persisted",
  );
  releaseCancellation.resolve();
  await Promise.all([first, second]);
  assertEquals(preparations, 2);
  assertEquals(
    f.notifications.filter(({ topic }) => topic === FINISHED).map((
      { detail },
    ) => detail.outcome),
    ["cancelled", "completed"],
  );
  assertEquals(f.actions, ["close:0", "clear-data", "reset-identities"]);
});
