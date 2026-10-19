// SPDX-License-Identifier: MPL-2.0

import { assert, assertEquals } from "@std/assert";

const SOURCE_PATCH = new URL(
  "../../.github/patches/floorp-runtime/common/workspace-external-containers.patch",
  import.meta.url,
);
const DEFAULT_ID = "01234567-89ab-cdef-0123-456789abcdef";

type TargetWindow = {
  isPrivate?: boolean;
  workspacesFuncs?: { getCurrentWorkspaceUserContextId(): unknown };
};
type ResolveContext = (
  win?: TargetWindow | null,
  forcePrivate?: boolean,
) => number;

async function fixture() {
  const patch = await Deno.readTextFile(SOURCE_PATCH);
  // Execute the actual Runtime helper carried by the shipping patch. Keeping
  // a separate TypeScript copy would allow the policy tests to drift from it.
  const match = patch.match(
    /^\+export function getWorkspaceUserContextIdForExternalOpen\([\s\S]*?^\+\}/m,
  );
  assert(match, "the Runtime patch exports the tested pre-creation helper");
  const source = match[0].replace(/^\+/gm, "").replace("export ", "");
  const prefs = new Map<string, boolean | string>([
    ["privacy.userContext.enabled", true],
    [
      "floorp.workspaces.v4.store",
      JSON.stringify({
        defaultID: DEFAULT_ID,
        data: [[DEFAULT_ID, { userContextId: 7 }]],
        order: [DEFAULT_ID],
      }),
    ],
  ]);
  const identities = new Set([7, 9]);
  const privateBrowsing = {
    permanentPrivateBrowsing: false,
    isWindowPrivate: (win: TargetWindow) => Boolean(win.isPrivate),
  };
  const resolve = new Function(
    "Services",
    "lazy",
    `${source}\nreturn getWorkspaceUserContextIdForExternalOpen;`,
  )({
    prefs: {
      getBoolPref: (name: string, fallback: boolean) =>
        prefs.get(name) ?? fallback,
      getStringPref: (name: string, fallback: string) =>
        prefs.get(name) ?? fallback,
    },
  }, {
    PrivateBrowsingUtils: privateBrowsing,
    ContextualIdentityService: {
      getPublicIdentityFromId: (id: number) =>
        identities.has(id) ? { userContextId: id, public: true } : undefined,
    },
  }) as ResolveContext;
  return { resolve, prefs, identities, privateBrowsing };
}

Deno.test("external workspace Runtime patches agree for development and packaging", async () => {
  const source = await Deno.readTextFile(SOURCE_PATCH);
  const local = await Deno.readTextFile(
    new URL("../patches/workspace-external-containers.patch", import.meta.url),
  );
  assertEquals(
    local,
    source.replaceAll(
      "browser/components/BrowserContentHandler.sys.mjs",
      "browser/modules/BrowserContentHandler.sys.mjs",
    ),
  );
  assertEquals(
    [...source.matchAll(/^\+\+\+ b\/(.+)$/gm)].map((match) => match[1]),
    [
      "browser/components/BrowserContentHandler.sys.mjs",
      "browser/modules/BrowserDOMWindow.sys.mjs",
    ],
    "only existing JavaScript modules change; no native rebuild is needed",
  );
});

Deno.test("Firefox's valid default-container guess stays distinct from no host guess", async () => {
  const patch = await Deno.readTextFile(SOURCE_PATCH);
  const match = patch.match(
    /^\+ {4}var openingUserContextId = guessUserContextIdEnabled\n(?:\+.*\n)*?\+ {6}: Ci\.nsIScriptSecurityManager\.DEFAULT_USER_CONTEXT_ID;/m,
  );
  assert(match, "the actual Runtime guess expression is tested");
  const resolveGuess = new Function(
    "guessUserContextIdEnabled",
    "lazy",
    "Ci",
    "aURI",
    `${match[0].replace(/^\+/gm, "")}\nreturn openingUserContextId;`,
  ) as (
    enabled: boolean,
    lazy: { URILoadingHelper: { guessUserContextId(): number | null } },
    constants: {
      nsIScriptSecurityManager: { DEFAULT_USER_CONTEXT_ID: number };
    },
    uri: null,
  ) => number | null;
  for (const guess of [0, 9, null]) {
    const lazy = { URILoadingHelper: { guessUserContextId: () => guess } };
    const constants = {
      nsIScriptSecurityManager: { DEFAULT_USER_CONTEXT_ID: 0 },
    };
    assertEquals(resolveGuess(true, lazy, constants, null), guess);
    assertEquals(
      resolveGuess(false, lazy, constants, null),
      0,
      "forced default and non-external opens never select the workspace fallback",
    );
  }
});

Deno.test("external opens before Workspace initialization use the persisted default workspace", async () => {
  const { resolve, prefs } = await fixture();
  const previous = [...prefs];
  assertEquals(resolve(), 7, "cold startup uses the default workspace");
  assertEquals(resolve({}), 7, "an initializing window uses the same fallback");
  assertEquals([...prefs], previous, "resolution must not migrate/write prefs");
});

Deno.test("the target window's selected workspace overrides the profile default", async () => {
  const { resolve } = await fixture();
  assertEquals(
    resolve({
      workspacesFuncs: { getCurrentWorkspaceUserContextId: () => 9 },
    }),
    9,
  );
  assertEquals(
    resolve({
      workspacesFuncs: { getCurrentWorkspaceUserContextId: () => 0 },
    }),
    0,
    "a selected workspace without a container must stay in context 0",
  );
});

Deno.test("external workspace fallback respects disabled workspaces, disabled containers and forced default", async () => {
  for (
    const [name, value] of [
      ["floorp.workspaces.enabled", false],
      ["privacy.userContext.enabled", false],
      ["browser.link.force_default_user_context_id_for_external_opens", true],
    ] as const
  ) {
    const { resolve, prefs } = await fixture();
    prefs.set(name, value);
    assertEquals(resolve(), 0, name);
    assertEquals(
      resolve({
        workspacesFuncs: { getCurrentWorkspaceUserContextId: () => 9 },
      }),
      0,
      name,
    );
  }
});

Deno.test("external workspace fallback never introduces a container into private browsing", async () => {
  const { resolve, privateBrowsing } = await fixture();
  assertEquals(resolve(null, true), 0, "a command-line private window");
  assertEquals(resolve({ isPrivate: true }), 0, "an existing private window");
  privateBrowsing.permanentPrivateBrowsing = true;
  assertEquals(resolve(), 0, "permanent and temporary autostart private mode");
});

Deno.test("external workspace fallback rejects missing, deleted and malformed identities", async () => {
  const { resolve, identities, prefs } = await fixture();
  identities.delete(7);
  assertEquals(resolve(), 0, "a deleted public identity");
  identities.add(7);
  for (
    const userContextId of [undefined, null, -1, 1.5, "7", 0x100000000, 99]
  ) {
    prefs.set(
      "floorp.workspaces.v4.store",
      JSON.stringify({
        defaultID: DEFAULT_ID,
        data: [[DEFAULT_ID, { userContextId }]],
      }),
    );
    assertEquals(resolve(), 0, `invalid default identity ${userContextId}`);
    assertEquals(
      resolve({
        workspacesFuncs: {
          getCurrentWorkspaceUserContextId: () => userContextId,
        },
      }),
      0,
      `invalid live identity ${userContextId}`,
    );
  }
});

Deno.test("external workspace fallback tolerates unavailable or malformed persisted state", async () => {
  const { resolve, prefs } = await fixture();
  for (
    const serialized of [
      "",
      "not JSON",
      "null",
      "[]",
      "{}",
      JSON.stringify({
        defaultID: "invalid",
        data: [["invalid", { userContextId: 7 }]],
      }),
      JSON.stringify({ defaultID: DEFAULT_ID, data: {} }),
      JSON.stringify({ defaultID: DEFAULT_ID, data: [null] }),
      JSON.stringify({ defaultID: DEFAULT_ID, data: [[DEFAULT_ID]] }),
      JSON.stringify({ defaultID: DEFAULT_ID, data: [] }),
    ]
  ) {
    prefs.set("floorp.workspaces.v4.store", serialized);
    assertEquals(resolve(), 0, serialized);
  }
  assertEquals(
    resolve({
      workspacesFuncs: {
        getCurrentWorkspaceUserContextId() {
          throw new Error("window unloading");
        },
      },
    }),
    0,
    "a failing getter cannot prevent an external URL from opening",
  );
});
