/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { RemoteSettings } = ChromeUtils.importESModule(
  "resource://services-settings/remote-settings.sys.mjs"
);

const COLLECTION_NAME = "fingerprinting-protection-overrides";

// The javascript bitwise operator only support 32bits. So, we only test
// RFPTargets that is under low 32 bits.
const TARGET_DEFAULT = extractLow32Bits(
  Services.rfp.enabledFingerprintingProtections
);
const TARGET_PointerEvents = 0x00000002;
const TARGET_CanvasRandomization = 0x000000100;
const TARGET_WindowOuterSize = 0x002000000;
const TARGET_Gamepad = 0x00800000;

// A helper function to filter high 32 bits.
function extractLow32Bits(value) {
  return value & 0xffffffff;
}

const TEST_CASES = [
  // Test simple addition.
  {
    entires: [
      {
        id: "1",
        last_modified: 1000000000000001,
        overrides: "+WindowOuterSize",
        firstPartyDomain: "example.org",
      },
    ],
    expects: [
      {
        domain: "example.org",
        overrides: TARGET_DEFAULT | TARGET_WindowOuterSize,
      },
      { domain: "example.com", noEntry: true },
    ],
  },
  // Test simple subtraction
  {
    entires: [
      {
        id: "1",
        last_modified: 1000000000000001,
        overrides: "-WindowOuterSize",
        firstPartyDomain: "example.org",
      },
    ],
    expects: [
      {
        domain: "example.org",
        overrides: TARGET_DEFAULT & ~TARGET_WindowOuterSize,
      },
      { domain: "example.com", noEntry: true },
    ],
  },
  // Test simple subtraction for default targets.
  {
    entires: [
      {
        id: "1",
        last_modified: 1000000000000001,
        overrides: "-CanvasRandomization",
        firstPartyDomain: "example.org",
      },
    ],
    expects: [
      {
        domain: "example.org",
        overrides: TARGET_DEFAULT & ~TARGET_CanvasRandomization,
      },
      { domain: "example.com", noEntry: true },
    ],
  },
  // Test multiple targets
  {
    entires: [
      {
        id: "1",
        last_modified: 1000000000000001,
        overrides: "+WindowOuterSize,+PointerEvents,-Gamepad",
        firstPartyDomain: "example.org",
      },
    ],
    expects: [
      {
        domain: "example.org",
        overrides:
          (TARGET_DEFAULT | TARGET_WindowOuterSize | TARGET_PointerEvents) &
          ~TARGET_Gamepad,
      },
      { domain: "example.com", noEntry: true },
    ],
  },
  // Test multiple entries
  {
    entires: [
      {
        id: "1",
        last_modified: 1000000000000001,
        overrides: "+WindowOuterSize,+PointerEvents,-Gamepad",
        firstPartyDomain: "example.org",
      },
      {
        id: "2",
        last_modified: 1000000000000001,
        overrides: "+Gamepad",
        firstPartyDomain: "example.com",
      },
    ],
    expects: [
      {
        domain: "example.org",
        overrides:
          (TARGET_DEFAULT | TARGET_WindowOuterSize | TARGET_PointerEvents) &
          ~TARGET_Gamepad,
      },
      {
        domain: "example.com",
        overrides: TARGET_DEFAULT | TARGET_Gamepad,
      },
    ],
  },
  // Test an entry with both first-party and third-party domains.
  {
    entires: [
      {
        id: "1",
        last_modified: 1000000000000001,
        overrides: "+WindowOuterSize,+PointerEvents,-Gamepad",
        firstPartyDomain: "example.org",
        thirdPartyDomain: "example.com",
      },
    ],
    expects: [
      {
        domain: "example.org,example.com",
        overrides:
          (TARGET_DEFAULT | TARGET_WindowOuterSize | TARGET_PointerEvents) &
          ~TARGET_Gamepad,
      },
    ],
  },
  // Test an entry with the first-party set to a wildcard.
  {
    entires: [
      {
        id: "1",
        last_modified: 1000000000000001,
        overrides: "+WindowOuterSize,+PointerEvents,-Gamepad",
        firstPartyDomain: "*",
      },
    ],
    expects: [
      {
        domain: "*",
        overrides:
          (TARGET_DEFAULT | TARGET_WindowOuterSize | TARGET_PointerEvents) &
          ~TARGET_Gamepad,
      },
    ],
  },
  // Test a entry with the first-party set to a wildcard and the third-party set
  // to a domain.
  {
    entires: [
      {
        id: "1",
        last_modified: 1000000000000001,
        overrides: "+WindowOuterSize,+PointerEvents,-Gamepad",
        firstPartyDomain: "*",
        thirdPartyDomain: "example.com",
      },
    ],
    expects: [
      {
        domain: "*,example.com",
        overrides:
          (TARGET_DEFAULT | TARGET_WindowOuterSize | TARGET_PointerEvents) &
          ~TARGET_Gamepad,
      },
    ],
  },
  // Test an entry with all targets disabled using '-AllTargets' but only enable one target.
  {
    entires: [
      {
        id: "1",
        last_modified: 1000000000000001,
        overrides: "-AllTargets,+WindowOuterSize",
        firstPartyDomain: "*",
        thirdPartyDomain: "example.com",
      },
    ],
    expects: [
      {
        domain: "*,example.com",
        overrides: TARGET_WindowOuterSize,
      },
    ],
  },
  // Test an entry with all targets disabled using '-AllTargets' but enable some targets.
  {
    entires: [
      {
        id: "1",
        last_modified: 1000000000000001,
        overrides: "-AllTargets,+WindowOuterSize,+Gamepad",
        firstPartyDomain: "*",
        thirdPartyDomain: "example.com",
      },
    ],
    expects: [
      {
        domain: "*,example.com",
        overrides: TARGET_WindowOuterSize | TARGET_Gamepad,
      },
    ],
  },
  // Test an invalid entry with only third party domain.
  {
    entires: [
      {
        id: "1",
        last_modified: 1000000000000001,
        overrides: "+WindowOuterSize,+PointerEvents,-Gamepad",
        thirdPartyDomain: "example.com",
      },
    ],
    expects: [
      {
        domain: "example.com",
        noEntry: true,
      },
    ],
  },
  // Test an invalid entry with both wildcards.
  {
    entires: [
      {
        id: "1",
        last_modified: 1000000000000001,
        overrides: "+WindowOuterSize,+PointerEvents,-Gamepad",
        firstPartyDomain: "*",
        thirdPartyDomain: "*",
      },
    ],
    expects: [
      {
        domain: "example.org",
        noEntry: true,
      },
    ],
  },
];

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [["privacy.fingerprintingProtection.remoteOverrides.testing", true]],
  });

  registerCleanupFunction(() => {
    Services.rfp.cleanAllOverrides();
  });
});

add_task(async function test_remote_settings() {
  // Add initial empty record.
  let db = RemoteSettings(COLLECTION_NAME).db;
  await db.importChanges({}, Date.now(), []);

  for (let test of TEST_CASES) {
    info(`Testing with entry ${JSON.stringify(test.entires)}`);

    // Create a promise for waiting the overrides get updated.
    let promise = promiseObserver("fpp-test:set-overrides-finishes");

    // Trigger the fingerprinting overrides update by a remote settings sync.
    await RemoteSettings(COLLECTION_NAME).emit("sync", {
      data: {
        current: test.entires,
      },
    });
    await promise;

    ok(true, "Got overrides update");

    for (let expect of test.expects) {
      // Get the addition and subtraction flags for the domain.
      try {
        let overrides = extractLow32Bits(
          Services.rfp.getFingerprintingOverrides(expect.domain)
        );

        // Verify if the flags are matching to expected values.
        is(overrides, expect.overrides, "The override value is correct.");
      } catch (e) {
        ok(expect.noEntry, "The override entry doesn't exist.");
      }
    }
  }

  db.clear();
});

add_task(async function test_pref() {
  for (let test of TEST_CASES) {
    info(`Testing with entry ${JSON.stringify(test.entires)}`);

    // Create a promise for waiting the overrides get updated.
    let promise = promiseObserver("fpp-test:set-overrides-finishes");

    // Trigger the fingerprinting overrides update by setting the pref.
    await SpecialPowers.pushPrefEnv({
      set: [
        [
          "privacy.fingerprintingProtection.granularOverrides",
          JSON.stringify(test.entires),
        ],
      ],
    });

    await promise;
    ok(true, "Got overrides update");

    for (let expect of test.expects) {
      try {
        // Get the addition and subtraction flags for the domain.
        let overrides = extractLow32Bits(
          Services.rfp.getFingerprintingOverrides(expect.domain)
        );

        // Verify if the flags are matching to expected values.
        is(overrides, expect.overrides, "The override value is correct.");
      } catch (e) {
        ok(expect.noEntry, "The override entry doesn't exist.");
      }
    }
  }
});

// Verify if the pref overrides the remote settings.
add_task(async function test_pref_override_remote_settings() {
  // Add initial empty record.
  let db = RemoteSettings(COLLECTION_NAME).db;
  await db.importChanges({}, Date.now(), []);

  // Trigger a remote settings sync.
  let promise = promiseObserver("fpp-test:set-overrides-finishes");
  await RemoteSettings(COLLECTION_NAME).emit("sync", {
    data: {
      current: [
        {
          id: "1",
          last_modified: 1000000000000001,
          overrides: "+WindowOuterSize",
          firstPartyDomain: "example.org",
        },
      ],
    },
  });
  await promise;

  // Then, setting the pref.
  promise = promiseObserver("fpp-test:set-overrides-finishes");
  await SpecialPowers.pushPrefEnv({
    set: [
      [
        "privacy.fingerprintingProtection.granularOverrides",
        JSON.stringify([
          {
            id: "1",
            last_modified: 1000000000000001,
            overrides: "+PointerEvents,-WindowOuterSize,-Gamepad",
            firstPartyDomain: "example.org",
          },
        ]),
      ],
    ],
  });
  await promise;

  // Get the addition and subtraction flags for the domain.
  let overrides = extractLow32Bits(
    Services.rfp.getFingerprintingOverrides("example.org")
  );

  // Verify if the flags are matching to the pref settings.
  is(
    overrides,
    (TARGET_DEFAULT | TARGET_PointerEvents) &
      ~TARGET_Gamepad &
      ~TARGET_WindowOuterSize,
    "The override addition value is correct."
  );

  db.clear();
});
