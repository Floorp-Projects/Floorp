// SPDX-License-Identifier: MPL-2.0

import { assert, assertEquals } from "@std/assert";

async function fixture() {
  const source = await Deno.readTextFile(
    new URL(
      "../../browser-features/chrome/common/ui-custom/layout/dom-manipulator.ts",
      import.meta.url,
    ),
  );
  const match = source.match(
    /^function getBookmarkWorkspaceUserContextId\([\s\S]*?^\}/m,
  );
  assert(
    match,
    "test the bookmark creation policy from the actual implementation",
  );
  const helper = match[0].replace("win: Window", "win").replace(
    "): number {",
    ") {",
  );
  const state = {
    context: 7 as unknown,
    privateWindow: false,
    permanentPrivateBrowsing: false,
    workspacesEnabled: true,
    containersEnabled: true,
    getterThrows: false,
  };
  const identities = new Set([7]);
  const resolve = new Function(
    "Workspaces",
    "ChromeUtils",
    "Services",
    `${helper}\nreturn getBookmarkWorkspaceUserContextId;`,
  )(
    {
      getCtx: () =>
        state.workspacesEnabled
          ? {
            getCurrentWorkspaceUserContextId: () => {
              if (state.getterThrows) throw new Error("workspace unloading");
              return state.context;
            },
          }
          : null,
    },
    {
      importESModule: (path: string) =>
        path.includes("PrivateBrowsingUtils")
          ? {
            PrivateBrowsingUtils: {
              get permanentPrivateBrowsing() {
                return state.permanentPrivateBrowsing;
              },
              isWindowPrivate: () => state.privateWindow,
            },
          }
          : {
            ContextualIdentityService: {
              getPublicIdentityFromId: (id: number) =>
                identities.has(id) ? { userContextId: id } : null,
            },
          },
    },
    { prefs: { getBoolPref: () => state.containersEnabled } },
  ) as (win: object) => number;
  return { resolve: () => resolve({}), state, identities };
}

Deno.test("bookmark container creation preserves privacy and disabled-feature policies", async () => {
  const { resolve, state } = await fixture();
  assertEquals(resolve(), 7, "a live public selected-workspace identity");
  for (
    const property of ["privateWindow", "permanentPrivateBrowsing"] as const
  ) {
    state[property] = true;
    assertEquals(resolve(), 0, property);
    state[property] = false;
  }
  for (const property of ["workspacesEnabled", "containersEnabled"] as const) {
    state[property] = false;
    assertEquals(resolve(), 0, property);
    state[property] = true;
  }
  state.getterThrows = true;
  assertEquals(
    resolve(),
    0,
    "an unavailable workspace cannot prevent bookmark navigation",
  );
});

Deno.test("bookmark container creation rejects deleted, internal and malformed identities", async () => {
  const { resolve, state, identities } = await fixture();
  identities.delete(7);
  assertEquals(resolve(), 0, "a deleted public identity");
  identities.add(7);
  for (const context of [0, -1, 1.5, "7", undefined, null, 0x100000000, 8]) {
    state.context = context;
    assertEquals(resolve(), 0, `invalid or non-public identity: ${context}`);
  }
});
