// SPDX-License-Identifier: MPL-2.0
import assert from "node:assert/strict";
import { execFileSync } from "node:child_process";
import { readFileSync } from "node:fs";
import { fileURLToPath } from "node:url";
import { Script } from "node:vm";
import { test } from "node:test";

/** @typedef {import('./types.ts').Tab} Tab */
/** @typedef {import('./types.ts').RestoreState} RestoreState */
/** @typedef {import('./types.ts').SharedOracle} SharedOracle */
/** @typedef {import('./types.ts').PhaseResult} PhaseResult */

const directory = fileURLToPath(new URL(".", import.meta.url));
const factory = readFileSync(
  new URL("restore-oracle.js.txt", import.meta.url),
  "utf8",
);
/** @type {SharedOracle} */
const oracle = new Script(factory).runInNewContext({}, { timeout: 1000 });
// Use the actual Python assembler, including its unchanged factory bytes.
const script = execFileSync("python3", [
  "-B",
  "-c",
  "import run, sys; sys.stdout.buffer.write(run.browser_script().encode('utf-8'))",
], { cwd: directory, encoding: "utf8" });
const C = "http://localhost:5186/";
const B = "about:robots#qa3-synthetic-B1";
const A = "about:robots#qa3-synthetic-A1";
const H = "https://synthetic-home.invalid/";
const expected = {
  state: { configuredNewTabURL: C },
  retainedURL: B,
  closedURL: A,
  b: "workspace-b",
};
/** @param {string} url @param {string | null} [workspace] @param {boolean} [selected] @returns {Tab} */
const tab = (url, workspace = "workspace-a", selected = false) => ({
  url,
  workspace,
  hidden: workspace === "workspace-b",
  selected,
});
/** @param {Tab[]} tabs @returns {RestoreState} */
const state = (tabs) => ({ configuredNewTabURL: C, pendingExit: false, tabs });

test("native assembly uses the identical pure factory and compiles", () => {
  assert.ok(script.startsWith(`const restartOracle = ${factory};\n`));
  new Script(`(function(){${script}})`);
  assert.ok(Object.isFrozen(oracle));
});
for (const startup of [C, "about:blank", "about:home", "about:newtab", H]) {
  test(`positive original B plus one known startup URI: ${startup}`, () => {
    oracle.workspaceExit(
      state([tab(B, "workspace-b"), tab(startup, "workspace-a", true)]),
      expected,
      H,
    );
  });
}
test("positive B alone permits zero startup tabs", () => {
  oracle.workspaceExit(
    state([{ ...tab(B, "workspace-b", true), hidden: false }]),
    expected,
    H,
  );
});
test("positive known startup tab may have no workspace attribution", () => {
  oracle.workspaceExit(
    state([tab(B, "workspace-b"), tab(C, null, true)]),
    expected,
    H,
  );
});
/** @type {Array<[string, Tab[], RegExp]>} */
const negative = [
  ["B+C+C", [tab(B, "workspace-b"), tab(C), tab(C)], /duplicate startup/],
  [
    "B+C+about:blank",
    [tab(B, "workspace-b"), tab(C), tab("about:blank")],
    /duplicate startup/,
  ],
  [
    "B+U",
    [tab(B, "workspace-b"), tab("https://unknown.invalid/")],
    /unexpected restored/,
  ],
  [
    "B+B+C",
    [tab(B, "workspace-b"), tab(B, "workspace-b"), tab(C)],
    /exactly once/,
  ],
  [
    "closed A1 is restored",
    [tab(B, "workspace-b"), tab(C), tab(A)],
    /closed A tab/,
  ],
  ["exact configured URI with an unknown suffix", [
    tab(B, "workspace-b"),
    tab(`${C}?unexpected=1`),
  ], /unexpected restored/],
  ["wrong B workspace", [tab(B, "workspace-a"), tab(C)], /membership/],
  ["missing B", [tab(C)], /exactly once/],
];
for (const [name, tabs, error] of negative) {
  test(`negative ${name}`, () =>
    assert.throws(() => oracle.workspaceExit(state(tabs), expected, H), error));
}
test("negative configured URI changes between native boots", () => {
  assert.throws(
    () =>
      oracle.workspaceExit(
        {
          ...state([tab(B, "workspace-b"), tab(C)]),
          configuredNewTabURL: "about:newtab",
        },
        expected,
        H,
      ),
    /unchanged across restart/,
  );
});
test("negative missing configured URI fails closed", () => {
  assert.throws(
    () => oracle.configuration({ configuredNewTabURL: "" }, expected.state),
    /URI is available/,
  );
});
test("negative pending exit is not consumed", () => {
  assert.throws(
    () =>
      oracle.workspaceExit(
        { ...state([tab(B, "workspace-b"), tab(C)]), pendingExit: true },
        expected,
        H,
      ),
    /consumed pending exit/,
  );
});
test("positive selecting B preserves count and selects its visible tab", () => {
  const before = state([tab(B, "workspace-b"), tab(C, "workspace-a", true)]);
  const after = state([
    { ...tab(B, "workspace-b", true), hidden: false },
    tab(C),
  ]);
  oracle.selectedWorkspace(after, before, expected, H);
});
/** @type {Array<[string, RestoreState, RegExp]>} */
const afterNegatives = [
  [
    "new replacement",
    state([{ ...tab(B, "workspace-b", true), hidden: false }, tab(C)]),
    /replacement tab/,
  ],
  ["hidden B", state([tab(B, "workspace-b", true), tab(C)]), /visible in B/],
  [
    "unselected B",
    state([
      { ...tab(B, "workspace-b"), hidden: false },
      tab(C, "workspace-a", true),
    ]),
    /B tab is selected/,
  ],
  [
    "two selected tabs",
    state([
      { ...tab(B, "workspace-b", true), hidden: false },
      tab(C, "workspace-a", true),
    ]),
    /B tab is selected/,
  ],
];
for (const [name, after, message] of afterNegatives) {
  test(`negative after selecting B: ${name}`, () => {
    const before = name === "new replacement"
      ? state([{ ...tab(B, "workspace-b", true), hidden: false }])
      : state([tab(B, "workspace-b"), tab(C, "workspace-a", true)]);
    assert.throws(
      () => oracle.selectedWorkspace(after, before, expected, H),
      message,
    );
  });
}

async function browserPhase(
  {
    wrongProfile = false,
    wrongPid = false,
    unknown = false,
    changedConfiguration = false,
  } = {},
) {
  const tabs = [
    tab(B, "workspace-b"),
    tab(
      unknown ? "https://synthetic-unexpected.invalid/" : C,
      "workspace-a",
      true,
    ),
  ].map((value) => ({
    ...value,
    /** @param {string} name */
    getAttribute(name) {
      return name === "floorpWorkspaceId" ? this.workspace : null;
    },
    /** @param {string} name */
    hasAttribute(name) {
      return name === "hidden" && this.hidden;
    },
  }));
  const profile = "/owned-synthetic-profile";
  const nativePid = 123;
  let tabReads = 0;
  let preferenceWrites = 0;
  const control = {
    runId: "synthetic-boot2",
    expiresAtMs: Date.now() + 240000,
    filter: [
      "tools/workspace-restart-development/no-autodiscovered-suites.test.ts",
    ],
  };
  const bootstrap = {
    runId: control.runId,
    status: "done",
    results: [],
    discoveredFiles: [],
    aborted: false,
  };
  const browser = {
    selectedTab: tabs[1],
    /** @param {Tab} value */
    getBrowserForTab: (value) => ({ currentURI: { spec: value.url } }),
  };
  Object.defineProperty(browser, "tabs", {
    get() {
      tabReads++;
      return tabs;
    },
  });
  const win = {
    gBrowser: browser,
    BROWSER_NEW_TAB_URL: changedConfiguration ? "about:newtab" : C,
    gBrowserInit: { delayedStartupFinished: true },
    SessionStore: {
      promiseInitialized: Promise.resolve(),
      promiseAllWindowsRestored: Promise.resolve(),
      /** @param {Tab} value */
      getTabState: (value) =>
        JSON.stringify({ entries: [{ url: value.url }], index: 1 }),
    },
    workspacesFuncs: {
      getSelectedWorkspaceID: () => "workspace-a",
      changeWorkspace: () => {
        tabs[0].hidden = false;
        tabs[0].selected = true;
        tabs[1].selected = false;
        browser.selectedTab = tabs[0];
      },
    },
  };
  const Services = {
    wm: { getMostRecentWindow: () => win, getEnumerator: () => [win] },
    dirsvc: {
      get: () => ({ path: wrongProfile ? "/foreign-profile" : profile }),
    },
    appinfo: {
      processID: wrongPid ? 999 : nativePid,
      version: "157.0.1",
      appBuildID: "20261007125927",
    },
    prefs: {
      /** @param {string} key */
      getStringPref: (key) =>
        key === "nora.tests.state"
          ? JSON.stringify(bootstrap)
          : key === "nora.startup.test"
          ? "loaded"
          : key === "browser.startup.homepage"
          ? H
          : "",
      getBoolPref: () => false,
      setBoolPref: () => {
        preferenceWrites++;
      },
      setStringPref: () => {
        preferenceWrites++;
      },
    },
  };
  const cfg = {
    action: wrongProfile || wrongPid ? "prepare" : "verify",
    case: "workspace-exit",
    nonce: "synthetic",
    control,
    syntheticProfile: { realpath: profile },
    nativePid,
    expected: { ...expected, case: "workspace-exit", nonce: "synthetic" },
  };
  /** @type {Promise<PhaseResult>} */
  const reply = new Promise((resolve) => {
    new Script(`(function(){${script}})(cfg, done)`).runInNewContext({
      Services,
      Components: { interfaces: { nsIFile: {} } },
      cfg,
      done: resolve,
      /** @param {() => void} fn @param {number} ms */
      setTimeout: (fn, ms) => {
        if (ms < 1000) queueMicrotask(fn);
        return 0;
      },
    }, { timeout: 1000 });
  });
  const result = await reply;
  return { result, tabReads, preferenceWrites };
}
test("actual assembled browser QA accepts B+C through the shared oracle", async () => {
  const { result } = await browserPhase();
  assert.equal(result.ok, true);
  assert.equal(result.syntheticProfileVerified, true);
});
test("actual assembled browser QA rejects unknown URI and captures its actual synthetic state", async () => {
  const { result } = await browserPhase({ unknown: true });
  assert.equal(result.ok, false);
  assert.match(result.error ?? "", /unexpected restored/);
  assert.ok(result.observedState);
  assert.equal(result.observedState.configuredNewTabURL, C);
  assert.equal(
    result.observedState.tabs[1].currentURI,
    "https://synthetic-unexpected.invalid/",
  );
  assert.equal(result.observedState.tabs[1].workspace, "workspace-a");
  assert.equal(result.observedState.tabs[1].selected, true);
});
test("actual assembled browser QA rejects changed native configuration", async () => {
  const { result } = await browserPhase({ changedConfiguration: true });
  assert.equal(result.ok, false);
  assert.match(result.error ?? "", /unchanged across restart/);
});
for (const kind of ["wrongProfile", "wrongPid"]) {
  test(`synthetic guard rejects ${kind} before tab reads or prepare mutations`, async () => {
    const { result, tabReads, preferenceWrites } = await browserPhase({
      [kind]: true,
    });
    assert.equal(result.ok, false);
    assert.equal(result.syntheticProfileVerified, false);
    assert.equal(result.observedState, undefined);
    assert.equal(tabReads, 0);
    assert.equal(preferenceWrites, 0);
  });
}
