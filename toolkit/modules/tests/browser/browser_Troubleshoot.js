/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Ideally this would be an xpcshell test, but Troubleshoot relies on things
// that aren't initialized outside of a XUL app environment like AddonManager
// and the "@mozilla.org/xre/app-info;1" component.

const { Troubleshoot } = ChromeUtils.importESModule(
  "resource://gre/modules/Troubleshoot.sys.mjs"
);
const { sinon } = ChromeUtils.importESModule(
  "resource://testing-common/Sinon.sys.mjs"
);

const { FeatureGate } = ChromeUtils.importESModule(
  "resource://featuregates/FeatureGate.sys.mjs"
);
const { PreferenceExperiments } = ChromeUtils.importESModule(
  "resource://normandy/lib/PreferenceExperiments.sys.mjs"
);
const { PreferenceRollouts } = ChromeUtils.importESModule(
  "resource://normandy/lib/PreferenceRollouts.sys.mjs"
);
const { AddonStudies } = ChromeUtils.importESModule(
  "resource://normandy/lib/AddonStudies.sys.mjs"
);
const { NormandyTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/NormandyTestUtils.sys.mjs"
);

NormandyTestUtils.init({ Assert });

add_task(async function snapshotSchema() {
  let snapshot = await Troubleshoot.snapshot();
  try {
    validateObject(snapshot, SNAPSHOT_SCHEMA);
    ok(true, "The snapshot should conform to the schema.");
  } catch (err) {
    ok(false, "Schema mismatch, " + err);
  }
});

add_task(async function experimentalFeatures() {
  let featureGates = await FeatureGate.all();
  ok(featureGates.length, "Should be at least one FeatureGate");

  let snapshot = await Troubleshoot.snapshot();
  for (let i = 0; i < snapshot.experimentalFeatures.length; i++) {
    let experimentalFeature = snapshot.experimentalFeatures[i];
    is(
      experimentalFeature[0],
      featureGates[i].title,
      "The first item in the array should be the title's l10n-id of the FeatureGate"
    );
    is(
      experimentalFeature[1],
      featureGates[i].preference,
      "The second item in the array should be the preference name for the FeatureGate"
    );
    is(
      experimentalFeature[2],
      Services.prefs.getBoolPref(featureGates[i].preference),
      "The third item in the array should be the preference value of the FeatureGate"
    );
  }
});

add_task(async function modifiedPreferences() {
  let prefs = [
    "javascript.troubleshoot",
    "troubleshoot.foo",
    "network.proxy.troubleshoot",
    "print.print_to_filename",
  ];
  prefs.forEach(function (p) {
    Services.prefs.setBoolPref(p, true);
    is(Services.prefs.getBoolPref(p), true, "The pref should be set: " + p);
  });
  Services.prefs.setCharPref("dom.push.userAgentID", "testvalue");
  let snapshot = await Troubleshoot.snapshot();
  let p = snapshot.modifiedPreferences;
  is(
    p["javascript.troubleshoot"],
    true,
    "The pref should be present because it's in the allowed prefs " +
      "and not in the pref regexes that are disallowed."
  );
  ok(
    !("troubleshoot.foo" in p),
    "The pref should be absent because it's not in the allowed prefs."
  );
  ok(
    !("network.proxy.troubleshoot" in p),
    "The pref should be absent because it's in the pref regexes " +
      "that are disallowed."
  );
  ok(
    !("dom.push.userAgentID" in p),
    "The pref should be absent because it's in the pref regexes " +
      "that are disallowed."
  );
  ok(
    !("print.print_to_filename" in p),
    "The pref should be absent because it's not in the allowed prefs."
  );
  prefs.forEach(p => Services.prefs.deleteBranch(p));
  Services.prefs.clearUserPref("dom.push.userAgentID");
});

add_task(async function unicodePreferences() {
  let name = "font.name.sans-serif.x-western";
  let utf8Value = "\xc4\x8capk\xc5\xafv Krasopis";
  let unicodeValue = "\u010Capk\u016Fv Krasopis";

  // set/getCharPref work with 8bit strings (utf8)
  Services.prefs.setCharPref(name, utf8Value);

  let snapshot = await Troubleshoot.snapshot();
  let p = snapshot.modifiedPreferences;
  is(p[name], unicodeValue, "The pref should have correct Unicode value.");
  Services.prefs.deleteBranch(name);
});

add_task(async function printingPreferences() {
  let prefs = [
    "javascript.print_to_filename",
    "print.print_bgimages",
    "print.print_to_filename",
  ];
  prefs.forEach(function (p) {
    Services.prefs.setBoolPref(p, true);
    is(Services.prefs.getBoolPref(p), true, "The pref should be set: " + p);
  });
  let snapshot = await Troubleshoot.snapshot();
  let p = snapshot.printingPreferences;
  is(p["print.print_bgimages"], true, "The pref should be present");
  ok(
    !("print.print_to_filename" in p),
    "The pref should not be present (sensitive)"
  );
  ok(
    !("javascript.print_to_filename" in p),
    "The pref should be absent because it's not a print pref."
  );
  prefs.forEach(p => Services.prefs.deleteBranch(p));
});

add_task(function normandy() {
  const {
    preferenceStudyFactory,
    branchedAddonStudyFactory,
    preferenceRolloutFactory,
  } = NormandyTestUtils.factories;

  return NormandyTestUtils.decorate(
    PreferenceExperiments.withMockExperiments([
      preferenceStudyFactory({
        userFacingName: "Test Pref Study B",
        branch: "test-branch-pref",
      }),
      preferenceStudyFactory({
        userFacingName: "Test Pref Study A",
        branch: "test-branch-pref",
      }),
    ]),
    AddonStudies.withStudies([
      branchedAddonStudyFactory({
        userFacingName: "Test Addon Study B",
        branch: "test-branch-addon",
      }),
      branchedAddonStudyFactory({
        userFacingName: "Test Addon Study A",
        branch: "test-branch-addon",
      }),
    ]),
    PreferenceRollouts.withTestMock({
      rollouts: [
        preferenceRolloutFactory({
          statue: "ACTIVE",
          slug: "test-pref-rollout-b",
        }),
        preferenceRolloutFactory({
          statue: "ACTIVE",
          slug: "test-pref-rollout-a",
        }),
      ],
    }),
    async function testNormandyInfoInTroubleshooting({
      prefExperiments,
      addonStudies,
      prefRollouts,
    }) {
      let snapshot = await Troubleshoot.snapshot();
      let info = snapshot.normandy;
      // The order should be flipped, since each category is sorted by slug.
      Assert.deepEqual(
        info.prefStudies,
        [prefExperiments[1], prefExperiments[0]],
        "prefs studies should exist in the right order"
      );
      Assert.deepEqual(
        info.addonStudies,
        [addonStudies[1], addonStudies[0]],
        "addon studies should exist in the right order"
      );
      Assert.deepEqual(
        info.prefRollouts,
        [prefRollouts[1], prefRollouts[0]],
        "pref rollouts should exist in the right order"
      );
    }
  )();
});

add_task(function normandyErrorHandling() {
  return NormandyTestUtils.decorate(
    NormandyTestUtils.withStub(PreferenceExperiments, "getAllActive", {
      returnValue: Promise.reject("Expected error - PreferenceExperiments"),
    }),
    NormandyTestUtils.withStub(AddonStudies, "getAllActive", {
      returnValue: Promise.reject("Expected error - AddonStudies"),
    }),
    NormandyTestUtils.withStub(PreferenceRollouts, "getAllActive", {
      returnValue: Promise.reject("Expected error - PreferenceRollouts"),
    }),
    async function testNormandyErrorHandling() {
      let consoleEndFn = TestUtils.listenForConsoleMessages();
      let snapshot = await Troubleshoot.snapshot();
      let info = snapshot.normandy;
      Assert.deepEqual(
        info.prefStudies,
        [],
        "prefs studies should be an empty list if there is an error"
      );
      Assert.deepEqual(
        info.addonStudies,
        [],
        "addon studies should be an empty list if there is an error"
      );
      Assert.deepEqual(
        info.prefRollouts,
        [],
        "pref rollouts should be an empty list if there is an error"
      );
      let msgs = await consoleEndFn();
      let expectedSet = new Set([
        /Expected error - PreferenceExperiments/,
        /Expected error - AddonStudies/,
        /Expected error - PreferenceRollouts/,
      ]);

      for (let msg of msgs) {
        msg = msg.wrappedJSObject;
        if (msg.level != "error") {
          continue;
        }

        let msgContents = msg.arguments[0];
        for (let expected of expectedSet) {
          if (expected.test(msgContents)) {
            expectedSet.delete(expected);
            break;
          }
        }
      }

      Assert.equal(
        expectedSet.size,
        0,
        "Should have no messages left in the expected set"
      );
    }
  )();
});

add_task(async function themes() {
  let snapshot = await Troubleshoot.snapshot();
  let foundTheme = false;
  for (let addon of snapshot.addons) {
    if (addon.type == "theme") {
      foundTheme = true;
      break;
    }
  }
  ok(foundTheme, "found a theme in the addons list");
});

// This is inspired by JSON Schema, or by the example on its Wikipedia page
// anyway.
const SNAPSHOT_SCHEMA = {
  type: "object",
  required: true,
  properties: {
    application: {
      required: true,
      type: "object",
      properties: {
        name: {
          required: true,
          type: "string",
        },
        version: {
          required: true,
          type: "string",
        },
        buildID: {
          required: true,
          type: "string",
        },
        distributionID: {
          required: true,
          type: "string",
        },
        userAgent: {
          required: true,
          type: "string",
        },
        osVersion: {
          required: true,
          type: "string",
        },
        osTheme: {
          type: "string",
        },
        rosetta: {
          required: false,
          type: "boolean",
        },
        vendor: {
          type: "string",
        },
        updateChannel: {
          type: "string",
        },
        supportURL: {
          type: "string",
        },
        launcherProcessState: {
          type: "number",
        },
        remoteAutoStart: {
          type: "boolean",
          required: true,
        },
        fissionAutoStart: {
          type: "boolean",
        },
        fissionDecisionStatus: {
          type: "string",
        },
        numTotalWindows: {
          type: "number",
        },
        numFissionWindows: {
          type: "number",
        },
        numRemoteWindows: {
          type: "number",
        },
        policiesStatus: {
          type: "number",
        },
        keyLocationServiceGoogleFound: {
          type: "boolean",
        },
        keySafebrowsingGoogleFound: {
          type: "boolean",
        },
        keyMozillaFound: {
          type: "boolean",
        },
        safeMode: {
          type: "boolean",
        },
        memorySizeBytes: {
          type: "number",
        },
        diskAvailableBytes: {
          type: "number",
        },
        pointingDevices: {
          required: false,
          type: "array",
        },
      },
    },
    crashes: {
      required: false,
      type: "object",
      properties: {
        pending: {
          required: true,
          type: "number",
        },
        submitted: {
          required: true,
          type: "array",
          items: {
            type: "object",
            properties: {
              id: {
                required: true,
                type: "string",
              },
              date: {
                required: true,
                type: "number",
              },
              pending: {
                required: true,
                type: "boolean",
              },
            },
          },
        },
      },
    },
    addons: {
      required: true,
      type: "array",
      items: {
        type: "object",
        properties: {
          name: {
            required: true,
            type: "string",
          },
          type: {
            required: true,
            type: "string",
          },
          version: {
            required: true,
            type: "string",
          },
          id: {
            required: true,
            type: "string",
          },
          isActive: {
            required: true,
            type: "boolean",
          },
        },
      },
    },
    securitySoftware: {
      required: false,
      type: "object",
      properties: {
        registeredAntiVirus: {
          required: true,
          type: "string",
        },
        registeredAntiSpyware: {
          required: true,
          type: "string",
        },
        registeredFirewall: {
          required: true,
          type: "string",
        },
      },
    },
    features: {
      required: true,
      type: "array",
      items: {
        type: "object",
        properties: {
          name: {
            required: true,
            type: "string",
          },
          version: {
            required: true,
            type: "string",
          },
          id: {
            required: true,
            type: "string",
          },
        },
      },
    },
    processes: {
      required: true,
      type: "object",
      properties: {
        maxWebContentProcesses: {
          required: true,
          type: "number",
        },
        remoteTypes: {
          required: true,
          type: "object",
        },
      },
    },
    experimentalFeatures: {
      required: true,
      type: "array",
    },
    environmentVariables: {
      required: true,
      type: "object",
    },
    modifiedPreferences: {
      required: true,
      type: "object",
    },
    printingPreferences: {
      required: true,
      type: "object",
    },
    lockedPreferences: {
      required: true,
      type: "object",
      properties: {
        "fission.autostart": {
          required: false,
          type: "boolean",
        },
        "fission.autostart.session": {
          required: false,
          type: "boolean",
        },
        "media.utility-process.enabled": {
          required: false,
          type: "boolean",
        },
        "media.utility-ffmpeg.enabled": {
          required: false,
          type: "boolean",
        },
        "media.utility-ffvpx.enabled": {
          required: false,
          type: "boolean",
        },
        "media.utility-wmf.enabled": {
          required: false,
          type: "boolean",
        },
        "media.utility-applemedia.enabled": {
          required: false,
          type: "boolean",
        },
        "media.utility-vorbis.enabled": {
          required: false,
          type: "boolean",
        },
        "media.utility-wav.enabled": {
          required: false,
          type: "boolean",
        },
        "media.utility-opus.enabled": {
          required: false,
          type: "boolean",
        },
      },
    },
    places: {
      required: true,
      type: "array",
      items: {
        type: "object",
        items: {
          entity: {
            required: true,
            type: "string",
          },
          count: {
            required: true,
            type: "number",
          },
          sizeBytes: {
            required: true,
            type: "number",
          },
          sizePerc: {
            required: true,
            type: "number",
          },
          efficiencyPerc: {
            required: true,
            type: "number",
          },
          sequentialityPerc: {
            required: true,
            type: "number",
          },
        },
      },
    },
    graphics: {
      required: true,
      type: "object",
      properties: {
        numTotalWindows: {
          required: true,
          type: "number",
        },
        numAcceleratedWindows: {
          required: true,
          type: "number",
        },
        graphicsDevicePixelRatios: {
          type: "array",
          items: {
            type: "number",
          },
        },
        windowLayerManagerType: {
          type: "string",
        },
        windowLayerManagerRemote: {
          type: "boolean",
        },
        numAcceleratedWindowsMessage: {
          type: "object",
          properties: {
            key: {
              required: true,
              type: "string",
            },
            args: {
              required: false,
              type: "object",
            },
          },
        },
        adapterDescription: {
          type: "string",
        },
        adapterVendorID: {
          type: "string",
        },
        adapterDeviceID: {
          type: "string",
        },
        adapterSubsysID: {
          type: "string",
        },
        adapterRAM: {
          type: "number",
        },
        adapterDrivers: {
          type: "string",
        },
        driverVendor: {
          type: "string",
        },
        driverVersion: {
          type: "string",
        },
        driverDate: {
          type: "string",
        },
        adapterDescription2: {
          type: "string",
        },
        adapterVendorID2: {
          type: "string",
        },
        adapterDeviceID2: {
          type: "string",
        },
        adapterSubsysID2: {
          type: "string",
        },
        adapterRAM2: {
          type: "number",
        },
        adapterDrivers2: {
          type: "string",
        },
        driverVendor2: {
          type: "string",
        },
        driverVersion2: {
          type: "string",
        },
        driverDate2: {
          type: "string",
        },
        isGPU2Active: {
          type: "boolean",
        },
        direct2DEnabled: {
          type: "boolean",
        },
        directWriteEnabled: {
          type: "boolean",
        },
        directWriteVersion: {
          type: "string",
        },
        clearTypeParameters: {
          type: "string",
        },
        webgl1Renderer: {
          type: "string",
        },
        webgl1Version: {
          type: "string",
        },
        webgl1DriverExtensions: {
          type: "string",
        },
        webgl1Extensions: {
          type: "string",
        },
        webgl1WSIInfo: {
          type: "string",
        },
        webgl2Renderer: {
          type: "string",
        },
        webgl2Version: {
          type: "string",
        },
        webgl2DriverExtensions: {
          type: "string",
        },
        webgl2Extensions: {
          type: "string",
        },
        webgl2WSIInfo: {
          type: "string",
        },
        webgpuDefaultAdapter: {
          type: "object",
        },
        webgpuFallbackAdapter: {
          type: "object",
        },
        info: {
          type: "object",
        },
        failures: {
          type: "object",
          properties: {
            key: {
              required: true,
              type: "string",
            },
            args: {
              required: false,
              type: "object",
            },
          },
        },
        indices: {
          type: "array",
          items: {
            type: "number",
          },
        },
        featureLog: {
          type: "object",
        },
        crashGuards: {
          type: "array",
        },
        direct2DEnabledMessage: {
          type: "object",
          properties: {
            key: {
              required: true,
              type: "string",
            },
            args: {
              required: false,
              type: "object",
            },
          },
        },
        targetFrameRate: {
          type: "number",
        },
        windowProtocol: {
          type: "string",
        },
        desktopEnvironment: {
          type: "string",
        },
      },
    },
    media: {
      required: true,
      type: "object",
      properties: {
        currentAudioBackend: {
          required: true,
          type: "string",
        },
        currentMaxAudioChannels: {
          required: true,
          type: "number",
        },
        currentPreferredSampleRate: {
          required: true,
          type: "number",
        },
        audioOutputDevices: {
          required: true,
          type: "array",
          items: {
            type: "object",
            properties: {
              name: {
                required: true,
                type: "string",
              },
              groupId: {
                required: true,
                type: "string",
              },
              vendor: {
                required: true,
                type: "string",
              },
              type: {
                required: true,
                type: "number",
              },
              state: {
                required: true,
                type: "number",
              },
              preferred: {
                required: true,
                type: "number",
              },
              supportedFormat: {
                required: true,
                type: "number",
              },
              defaultFormat: {
                required: true,
                type: "number",
              },
              maxChannels: {
                required: true,
                type: "number",
              },
              defaultRate: {
                required: true,
                type: "number",
              },
              maxRate: {
                required: true,
                type: "number",
              },
              minRate: {
                required: true,
                type: "number",
              },
              maxLatency: {
                required: true,
                type: "number",
              },
              minLatency: {
                required: true,
                type: "number",
              },
            },
          },
        },
        audioInputDevices: {
          required: true,
          type: "array",
          items: {
            type: "object",
            properties: {
              name: {
                required: true,
                type: "string",
              },
              groupId: {
                required: true,
                type: "string",
              },
              vendor: {
                required: true,
                type: "string",
              },
              type: {
                required: true,
                type: "number",
              },
              state: {
                required: true,
                type: "number",
              },
              preferred: {
                required: true,
                type: "number",
              },
              supportedFormat: {
                required: true,
                type: "number",
              },
              defaultFormat: {
                required: true,
                type: "number",
              },
              maxChannels: {
                required: true,
                type: "number",
              },
              defaultRate: {
                required: true,
                type: "number",
              },
              maxRate: {
                required: true,
                type: "number",
              },
              minRate: {
                required: true,
                type: "number",
              },
              maxLatency: {
                required: true,
                type: "number",
              },
              minLatency: {
                required: true,
                type: "number",
              },
            },
          },
        },
        codecSupportInfo: {
          required: false,
          type: "string",
        },
      },
    },
    accessibility: {
      required: true,
      type: "object",
      properties: {
        isActive: {
          required: true,
          type: "boolean",
        },
        forceDisabled: {
          type: "number",
        },
        handlerUsed: {
          type: "boolean",
        },
        instantiator: {
          type: "string",
        },
      },
    },
    libraryVersions: {
      required: true,
      type: "object",
      properties: {
        NSPR: {
          required: true,
          type: "object",
          properties: {
            minVersion: {
              required: true,
              type: "string",
            },
            version: {
              required: true,
              type: "string",
            },
          },
        },
        NSS: {
          required: true,
          type: "object",
          properties: {
            minVersion: {
              required: true,
              type: "string",
            },
            version: {
              required: true,
              type: "string",
            },
          },
        },
        NSSUTIL: {
          required: true,
          type: "object",
          properties: {
            minVersion: {
              required: true,
              type: "string",
            },
            version: {
              required: true,
              type: "string",
            },
          },
        },
        NSSSSL: {
          required: true,
          type: "object",
          properties: {
            minVersion: {
              required: true,
              type: "string",
            },
            version: {
              required: true,
              type: "string",
            },
          },
        },
        NSSSMIME: {
          required: true,
          type: "object",
          properties: {
            minVersion: {
              required: true,
              type: "string",
            },
            version: {
              required: true,
              type: "string",
            },
          },
        },
      },
    },
    userJS: {
      required: true,
      type: "object",
      properties: {
        exists: {
          required: true,
          type: "boolean",
        },
      },
    },
    sandbox: {
      required: false,
      type: "object",
      properties: {
        hasSeccompBPF: {
          required: AppConstants.platform == "linux",
          type: "boolean",
        },
        hasSeccompTSync: {
          required: AppConstants.platform == "linux",
          type: "boolean",
        },
        hasUserNamespaces: {
          required: AppConstants.platform == "linux",
          type: "boolean",
        },
        hasPrivilegedUserNamespaces: {
          required: AppConstants.platform == "linux",
          type: "boolean",
        },
        canSandboxContent: {
          required: false,
          type: "boolean",
        },
        canSandboxMedia: {
          required: false,
          type: "boolean",
        },
        contentSandboxLevel: {
          required: AppConstants.MOZ_SANDBOX,
          type: "number",
        },
        effectiveContentSandboxLevel: {
          required: AppConstants.MOZ_SANDBOX,
          type: "number",
        },
        contentWin32kLockdownState: {
          required: AppConstants.MOZ_SANDBOX && AppConstants.platform == "win",
          type: "string",
        },
        supportSandboxGpuLevel: {
          required: AppConstants.MOZ_SANDBOX && AppConstants.platform == "win",
          type: "number",
        },
        syscallLog: {
          required: AppConstants.platform == "linux",
          type: "array",
          items: {
            type: "object",
            properties: {
              index: {
                required: true,
                type: "number",
              },
              pid: {
                required: true,
                type: "number",
              },
              tid: {
                required: true,
                type: "number",
              },
              procType: {
                required: true,
                type: "string",
              },
              syscall: {
                required: true,
                type: "number",
              },
              args: {
                required: true,
                type: "array",
                items: {
                  type: "string",
                },
              },
            },
          },
        },
      },
    },
    startupCache: {
      required: false,
      type: "object",
      properties: {
        DiskCachePath: {
          required: true,
          type: "string",
        },
        IgnoreDiskCache: {
          required: true,
          type: "boolean",
        },
        FoundDiskCacheOnInit: {
          required: true,
          type: "boolean",
        },
        WroteToDiskCache: {
          required: true,
          type: "boolean",
        },
      },
    },
    intl: {
      required: true,
      type: "object",
      properties: {
        localeService: {
          required: true,
          type: "object",
          properties: {
            requested: {
              required: true,
              type: "array",
            },
            available: {
              required: true,
              type: "array",
            },
            supported: {
              required: true,
              type: "array",
            },
            regionalPrefs: {
              required: true,
              type: "array",
            },
            defaultLocale: {
              required: true,
              type: "string",
            },
          },
        },
        osPrefs: {
          required: true,
          type: "object",
          properties: {
            systemLocales: {
              required: true,
              type: "array",
            },
            regionalPrefsLocales: {
              required: true,
              type: "array",
            },
          },
        },
      },
    },
    remoteAgent: {
      type: "object",
      properties: {
        running: {
          required: true,
          type: "boolean",
        },
        url: {
          required: true,
          type: "string",
        },
      },
    },
    normandy: {
      type: "object",
      required: AppConstants.MOZ_NORMANDY,
      properties: {
        addonStudies: {
          type: "array",
          items: {
            type: "object",
            properties: {
              userFacingName: { type: "string", required: true },
              branch: { type: "string", required: true },
            },
          },
          required: true,
        },
        prefRollouts: {
          type: "array",
          items: {
            type: "object",
            properties: {
              slug: { type: "string", required: true },
              state: { type: "string", required: true },
            },
          },
          required: true,
        },
        prefStudies: {
          type: "array",
          items: {
            type: "object",
            properties: {
              userFacingName: { type: "string", required: true },
              branch: { type: "string", required: true },
            },
          },
          required: true,
        },
        nimbusExperiments: {
          type: "array",
          items: {
            type: "object",
            properties: {
              userFacingName: { type: "string", required: true },
              branch: {
                type: "object",
                properties: {
                  slug: { type: "string", required: true },
                },
              },
            },
          },
          required: true,
        },
        nimbusRollouts: {
          type: "array",
          items: {
            type: "object",
            properties: {
              featureId: { type: "string", required: true },
              slug: { type: "string", required: true },
            },
          },
        },
      },
    },
    legacyUserStylesheets: {
      type: "object",
      properties: {
        active: {
          required: true,
          type: "boolean",
        },
        types: {
          required: true,
          type: "array",
        },
      },
    },
  },
};

/**
 * Throws an Error if obj doesn't conform to schema.  That way you get a nice
 * error message and a stack to help you figure out what went wrong, which you
 * wouldn't get if this just returned true or false instead.  There's still
 * room for improvement in communicating validation failures, however.
 *
 * @param obj    The object to validate.
 * @param schema The schema that obj should conform to.
 */
function validateObject(obj, schema) {
  if (obj === undefined && !schema.required) {
    return;
  }
  if (typeof schema.type != "string") {
    throw schemaErr("'type' must be a string", schema);
  }
  if (objType(obj) != schema.type) {
    throw validationErr("Object is not of the expected type", obj, schema);
  }
  let validatorFnName = "validateObject_" + schema.type;
  if (!(validatorFnName in this)) {
    throw schemaErr("Validator function not defined for type", schema);
  }
  this[validatorFnName](obj, schema);
}

function validateObject_object(obj, schema) {
  if (typeof schema.properties != "object") {
    // Don't care what obj's properties are.
    return;
  }
  // First check that all the schema's properties match the object.
  for (let prop in schema.properties) {
    validateObject(obj[prop], schema.properties[prop]);
  }
  // Now check that the object doesn't have any properties not in the schema.
  for (let prop in obj) {
    if (!(prop in schema.properties)) {
      throw validationErr(
        "Object has property " + prop + " not in schema",
        obj,
        schema
      );
    }
  }
}

function validateObject_array(array, schema) {
  if (typeof schema.items != "object") {
    // Don't care what the array's elements are.
    return;
  }
  array.forEach(elt => validateObject(elt, schema.items));
}

function validateObject_string(str, schema) {}
function validateObject_boolean(bool, schema) {}
function validateObject_number(num, schema) {}

function validationErr(msg, obj, schema) {
  return new Error(
    "Validation error: " +
      msg +
      ": object=" +
      JSON.stringify(obj) +
      ", schema=" +
      JSON.stringify(schema)
  );
}

function schemaErr(msg, schema) {
  return new Error("Schema error: " + msg + ": " + JSON.stringify(schema));
}

function objType(obj) {
  let type = typeof obj;
  if (type != "object") {
    return type;
  }
  if (Array.isArray(obj)) {
    return "array";
  }
  if (obj === null) {
    return "null";
  }
  return type;
}
