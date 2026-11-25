// SPDX-License-Identifier: MPL-2.0
/// <reference lib="deno.ns" />

import { assertEquals, assertRejects } from "@std/assert";
import {
  m3dAcceptanceVerdict,
  snapshotProfileTree,
} from "./m3d_package_acquisition_acceptance_helpers.ts";
import {
  M3D_REQUIRED_CASE_IDS,
  type M3dAcceptanceCaseResult,
  type M3dAcceptanceVerdictInput,
} from "./m3d_package_acquisition_acceptance.types.ts";

const profileSnapshot = {
  exists: true,
  entries: 4,
  byteLength: 256,
  sha256: "a".repeat(64),
};

function passedCases(): M3dAcceptanceCaseResult[] {
  return M3D_REQUIRED_CASE_IDS.map((id) => ({
    id,
    status: "passed",
    observations: {},
  }));
}

function passingVerdictInput(): M3dAcceptanceVerdictInput {
  return {
    complete: true,
    cases: passedCases(),
    launchFailure: null,
    runFailure: null,
    launchAttempted: true,
    launchSucceeded: true,
    browserCleanupProven: true,
    sharedProfileBefore: profileSnapshot,
    sharedProfileAfter: profileSnapshot,
    cleanup: { fixture: true, browser: true, profile: true },
  };
}

Deno.test("profile snapshot hashes content without exposing paths", async () => {
  const root = await Deno.makeTempDir({ prefix: "m3d-profile-snapshot-" });
  try {
    const missing = await snapshotProfileTree(`${root}/missing`);
    assertEquals(missing.exists, false);
    await Deno.mkdir(`${root}/profile`);
    await Deno.writeTextFile(`${root}/profile/user.js`, "public fixture");
    const first = await snapshotProfileTree(`${root}/profile`);
    const same = await snapshotProfileTree(`${root}/profile`);
    assertEquals(first, same);
    await Deno.writeTextFile(`${root}/profile/user.js`, "changed");
    const changed = await snapshotProfileTree(`${root}/profile`);
    assertEquals(changed.sha256 === first.sha256, false);
    assertEquals(JSON.stringify(changed).includes("user.js"), false);
    await assertRejects(() => snapshotProfileTree(`${root}/profile/user.js`));
  } finally {
    await Deno.remove(root, { recursive: true });
  }
});

Deno.test("acceptance verdict requires every unique required case to pass", () => {
  const passing = passingVerdictInput();
  assertEquals(m3dAcceptanceVerdict(passing), true);
  assertEquals(m3dAcceptanceVerdict({ ...passing, cases: [] }), false);
  assertEquals(
    m3dAcceptanceVerdict({ ...passing, cases: passing.cases.slice(1) }),
    false,
  );
  assertEquals(
    m3dAcceptanceVerdict({
      ...passing,
      cases: [...passing.cases, passing.cases[0]],
    }),
    false,
  );
  assertEquals(
    m3dAcceptanceVerdict({
      ...passing,
      cases: passing.cases.map((entry, index) =>
        index === 0 ? { ...entry, status: "failed" } : entry
      ),
    }),
    false,
  );
  assertEquals(
    m3dAcceptanceVerdict({
      ...passing,
      cases: passing.cases.map((entry, index) =>
        index === 0 ? { ...entry, status: "not-run" } : entry
      ),
    }),
    false,
  );
});

Deno.test("acceptance verdict requires clean launch, profile and cleanup proof", () => {
  const passing = passingVerdictInput();
  assertEquals(m3dAcceptanceVerdict({ ...passing, complete: false }), false);
  assertEquals(
    m3dAcceptanceVerdict({ ...passing, launchFailure: "launch-failed" }),
    false,
  );
  assertEquals(
    m3dAcceptanceVerdict({ ...passing, runFailure: "run-failed" }),
    false,
  );
  assertEquals(
    m3dAcceptanceVerdict({ ...passing, launchAttempted: false }),
    false,
  );
  assertEquals(
    m3dAcceptanceVerdict({ ...passing, launchSucceeded: false }),
    false,
  );
  assertEquals(
    m3dAcceptanceVerdict({ ...passing, browserCleanupProven: false }),
    false,
  );
  assertEquals(
    m3dAcceptanceVerdict({ ...passing, cleanup: { fixture: false } }),
    false,
  );
  assertEquals(m3dAcceptanceVerdict({ ...passing, cleanup: {} }), false);
  assertEquals(
    m3dAcceptanceVerdict({
      ...passing,
      sharedProfileAfter: { ...profileSnapshot, entries: 5 },
    }),
    false,
  );
  assertEquals(
    m3dAcceptanceVerdict({
      ...passing,
      sharedProfileAfter: { ...profileSnapshot, byteLength: 257 },
    }),
    false,
  );
  assertEquals(
    m3dAcceptanceVerdict({
      ...passing,
      sharedProfileAfter: { ...profileSnapshot, sha256: "b".repeat(64) },
    }),
    false,
  );
  assertEquals(
    m3dAcceptanceVerdict({ ...passing, sharedProfileAfter: null }),
    false,
  );
});
