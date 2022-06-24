/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * vim: sw=4 ts=4 sts=4 et
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* This test records code loaded during a dummy background task.
 *
 * To run this test similar to try server, you need to run:
 *   ./mach package
 *   ./mach test --appname=dist <path to test>
 *
 * If you made changes that cause this test to fail, it's likely
 * because you are changing the application startup process.  In
 * general, you should prefer to defer loading code as long as you
 * can, especially if it's not going to be used in background tasks.
 */

"use strict";

const { OS } = ChromeUtils.import("resource://gre/modules/osfile.jsm");

const Cm = Components.manager.QueryInterface(Ci.nsIComponentRegistrar);

// Shortcuts for conditions.
const LINUX = AppConstants.platform == "linux";
const WIN = AppConstants.platform == "win";
const MAC = AppConstants.platform == "macosx";

const backgroundtaskPhases = {
  AfterRunBackgroundTaskNamed: {
    allowlist: {
      modules: [
        "resource://gre/modules/AppConstants.jsm",
        "resource://gre/modules/AsyncShutdown.jsm",
        "resource://gre/modules/BackgroundTasksManager.jsm",
        "resource://gre/modules/Console.jsm",
        "resource://gre/modules/EnterprisePolicies.jsm",
        "resource://gre/modules/EnterprisePoliciesParent.jsm",
        "resource://gre/modules/PromiseUtils.jsm",
        "resource://gre/modules/Services.jsm",
        "resource://gre/modules/XPCOMUtils.jsm",
        "resource://gre/modules/nsAsyncShutdown.jsm",
      ],
      // Human-readable contract IDs are many-to-one mapped to CIDs, so this
      // list is a little misleading.  For example, all of
      // "@mozilla.org/xre/app-info;1", "@mozilla.org/xre/runtime;1", and
      // "@mozilla.org/toolkit/crash-reporter;1", map to the CID
      // {95d89e3e-a169-41a3-8e56-719978e15b12}, but only one is listed here.
      // We could be more precise by listing CIDs, but that's a good deal harder
      // to read and modify.
      services: [
        "@mozilla.org/async-shutdown-service;1",
        "@mozilla.org/backgroundtasks;1",
        "@mozilla.org/backgroundtasksmanager;1",
        "@mozilla.org/base/telemetry;1",
        "@mozilla.org/categorymanager;1",
        "@mozilla.org/chrome/chrome-registry;1",
        "@mozilla.org/cookieService;1",
        "@mozilla.org/docloaderservice;1",
        "@mozilla.org/embedcomp/window-watcher;1",
        "@mozilla.org/enterprisepolicies;1",
        "@mozilla.org/file/directory_service;1",
        "@mozilla.org/intl/stringbundle;1",
        "@mozilla.org/layout/content-policy;1",
        "@mozilla.org/memory-reporter-manager;1",
        "@mozilla.org/network/captive-portal-service;1",
        "@mozilla.org/network/effective-tld-service;1",
        "@mozilla.org/network/idn-service;1",
        "@mozilla.org/network/io-service;1",
        "@mozilla.org/network/network-link-service;1",
        "@mozilla.org/network/protocol;1?name=chrome",
        "@mozilla.org/network/protocol;1?name=file",
        "@mozilla.org/network/protocol;1?name=jar",
        "@mozilla.org/network/protocol;1?name=resource",
        "@mozilla.org/network/socket-transport-service;1",
        "@mozilla.org/network/stream-transport-service;1",
        "@mozilla.org/network/url-parser;1?auth=maybe",
        "@mozilla.org/network/url-parser;1?auth=no",
        "@mozilla.org/network/url-parser;1?auth=yes",
        "@mozilla.org/observer-service;1",
        "@mozilla.org/power/powermanagerservice;1",
        "@mozilla.org/preferences-service;1",
        "@mozilla.org/process/environment;1",
        "@mozilla.org/storage/service;1",
        "@mozilla.org/thirdpartyutil;1",
        "@mozilla.org/toolkit/app-startup;1",
        {
          name: "@mozilla.org/widget/appshell/mac;1",
          condition: MAC,
        },
        {
          name: "@mozilla.org/widget/appshell/gtk;1",
          condition: LINUX,
        },
        {
          name: "@mozilla.org/widget/appshell/win;1",
          condition: WIN,
        },
        "@mozilla.org/xpcom/debug;1",
        "@mozilla.org/xre/app-info;1",
        "@mozilla.org/mime;1",
        {
          name: "@mozilla.org/gfx/info;1",
          condition: WIN,
        },
        {
          name: "@mozilla.org/image/tools;1",
          condition: WIN,
        },
        {
          name: "@mozilla.org/gfx/screenmanager;1",
          condition: WIN,
        },
      ],
    },
  },
  AfterFindRunBackgroundTask: {
    allowlist: {
      modules: [
        // We have a profile marker for this, even though it failed to load!
        "resource:///modules/backgroundtasks/BackgroundTask_wait.jsm",
        "resource://gre/modules/ConsoleAPIStorage.jsm",
        "resource://gre/modules/Timer.jsm",
        // We have a profile marker for this, even though it failed to load!
        "resource://gre/modules/backgroundtasks/BackgroundTask_wait.jsm",
        "resource://testing-common/backgroundtasks/BackgroundTask_wait.jsm",
      ],
      services: ["@mozilla.org/consoleAPI-storage;1"],
    },
  },
  AfterAwaitRunBackgroundTask: {
    allowlist: {
      modules: [],
      services: [],
    },
  },
};

function getStackFromProfile(profile, stack, libs) {
  const stackPrefixCol = profile.stackTable.schema.prefix;
  const stackFrameCol = profile.stackTable.schema.frame;
  const frameLocationCol = profile.frameTable.schema.location;

  let index = 1;
  let result = [];
  while (stack) {
    let sp = profile.stackTable.data[stack];
    let frame = profile.frameTable.data[sp[stackFrameCol]];
    stack = sp[stackPrefixCol];
    frame = profile.stringTable[frame[frameLocationCol]];

    if (frame.startsWith("0x")) {
      try {
        let addr = frame.slice("0x".length);
        addr = Number.parseInt(addr, 16);
        for (let lib of libs) {
          if (lib.start <= addr && addr <= lib.end) {
            // Only handle two digits for now.
            let indexString = index.toString(10);
            if (indexString.length == 1) {
              indexString = "0" + indexString;
            }
            let offset = addr - lib.start;
            frame = `#${indexString}: ???[${lib.debugPath} ${"+0x" +
              offset.toString(16)}]`;
            break;
          }
        }
      } catch (e) {
        // Fall through.
      }
    }

    if (frame != "js::RunScript" && !frame.startsWith("next (self-hosted:")) {
      result.push(frame);
      index += 1;
    }
  }
  return result;
}

add_task(async function test_xpcom_graph_wait() {
  {
    let omniJa = Services.dirsvc.get("XCurProcD", Ci.nsIFile);
    omniJa.append("omni.ja");
    if (!omniJa.exists()) {
      ok(
        false,
        "This test requires a packaged build, " +
          "run 'mach package' and then use --appname=dist"
      );
      return;
    }
  }

  let profilePath = Cc["@mozilla.org/process/environment;1"]
    .getService(Ci.nsIEnvironment)
    .get("MOZ_UPLOAD_DIR");
  profilePath =
    profilePath ||
    FileUtils.getDir("ProfD", [`testBackgroundTask-${Math.random()}`], true)
      .path;

  profilePath = OS.Path.join(profilePath, "profile_backgroundtask_wait.json");
  await IOUtils.remove(profilePath, { ignoreAbsent: true });

  let extraEnv = {
    MOZ_PROFILER_STARTUP: "1",
    MOZ_PROFILER_SHUTDOWN: profilePath,
  };

  let exitCode = await do_backgroundtask("wait", { extraEnv });
  Assert.equal(0, exitCode);

  let fileContents = await IOUtils.readUTF8(profilePath);
  let rootProfile = JSON.parse(fileContents);
  let profile = rootProfile.threads[0];

  const nameCol = profile.markers.schema.name;
  const dataCol = profile.markers.schema.data;

  function newMarkers() {
    return {
      modules: [], // The equivalent of `Cu.loadedModules`.
      services: [],
    };
  }

  let phases = {};
  let markersForCurrentPhase = newMarkers();

  // If a subsequent phase loads an already loaded resource, that's
  // fine.  Track all loaded resources to ignore such repeated loads.
  let markersForAllPhases = newMarkers();

  for (let m of profile.markers.data) {
    let markerName = profile.stringTable[m[nameCol]];
    if (markerName.startsWith("BackgroundTasksManager:")) {
      phases[
        markerName.split("BackgroundTasksManager:")[1]
      ] = markersForCurrentPhase;
      markersForCurrentPhase = newMarkers();
      continue;
    }

    if (
      ![
        "ChromeUtils.import", // JSMs.
        "GetService", // XPCOM services.
      ].includes(markerName)
    ) {
      continue;
    }

    let markerData = m[dataCol];
    if (markerName == "ChromeUtils.import") {
      let module = markerData.name;
      if (!markersForAllPhases.modules.includes(module)) {
        markersForAllPhases.modules.push(module);
        markersForCurrentPhase.modules.push(module);
      }
    }

    if (markerName == "GetService") {
      // We get a CID from the marker itself, but not a human-readable contract
      // ID.  Now, most of the time, the stack will contain a label like
      // `GetServiceByContractID @...;1`, and we could extract the contract ID
      // from that.  But there are multiple ways to instantiate services, and
      // not all of them are annotated in this manner.  Therefore we "go the
      // other way" and use the component manager's mapping from contract IDs to
      // CIDs.  This opens up the possibility for that mapping to be different
      // between `--backgroundtask` and `xpcshell`, but that's not an issue
      // right at this moment.  It's worth noting that one CID can (and
      // sometimes does) correspond to more than one contract ID.
      let cid = markerData.name;

      if (!markersForAllPhases.services.includes(cid)) {
        markersForAllPhases.services.push(cid);
        markersForCurrentPhase.services.push(cid);
      }
    }
  }

  // Turn `["1", {name: "2", condition: false}, {name: "3", condition: true}]`
  // into `new Set(["1", "3"])`.
  function filterConditions(l) {
    let set = new Set([]);
    for (let entry of l) {
      if (typeof entry == "object") {
        if ("condition" in entry && !entry.condition) {
          continue;
        }
        entry = entry.name;
      }
      set.add(entry);
    }
    return set;
  }

  for (let phaseName in backgroundtaskPhases) {
    for (let listName in backgroundtaskPhases[phaseName]) {
      for (let scriptType in backgroundtaskPhases[phaseName][listName]) {
        backgroundtaskPhases[phaseName][listName][
          scriptType
        ] = filterConditions(
          backgroundtaskPhases[phaseName][listName][scriptType]
        );
      }

      // Turn human-readable contract IDs into CIDs.  It's worth noting that one
      // CID can (and sometimes does) correspond to more than one contract ID.
      let services = Array.from(
        backgroundtaskPhases[phaseName][listName].services || new Set([])
      );
      services = services
        .map(contractID => {
          try {
            return Cm.contractIDToCID(contractID).toString();
          } catch (e) {
            return null;
          }
        })
        .filter(cid => cid);
      services.sort();
      backgroundtaskPhases[phaseName][listName].services = new Set(services);
      info(
        `backgroundtaskPhases[${phaseName}][${listName}].services = ${JSON.stringify(
          services.map(c => c.toString())
        )}`
      );
    }
  }

  // Turn `{CID}` into `{CID} (@contractID)` or `{CID} (one of
  // @contractID1, ..., @contractIDn)` as appropriate.
  function renderResource(resource) {
    const UUID_PATTERN = /^\{[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}\}$/i;
    if (UUID_PATTERN.test(resource)) {
      let foundContractIDs = [];
      for (let contractID of Cm.getContractIDs()) {
        try {
          if (resource == Cm.contractIDToCID(contractID).toString()) {
            foundContractIDs.push(contractID);
          }
        } catch (e) {
          // This can throw for contract IDs that are filtered.  The common
          // reason is that they're limited to a particular process.
        }
      }
      if (!foundContractIDs.length) {
        return `${resource} (CID with no human-readable contract IDs)`;
      } else if (foundContractIDs.length == 1) {
        return `${resource} (CID with human-readable contract ID ${foundContractIDs[0]})`;
      }
      foundContractIDs.sort();
      return `${resource} (CID with human-readable contract IDs ${JSON.stringify(
        foundContractIDs
      )})`;
    }

    return resource;
  }

  for (let phase in backgroundtaskPhases) {
    let loadedList = phases[phase];
    let allowlist = backgroundtaskPhases[phase].allowlist || null;
    if (allowlist) {
      for (let scriptType in allowlist) {
        loadedList[scriptType] = loadedList[scriptType].filter(c => {
          if (!allowlist[scriptType].has(c)) {
            return true;
          }
          allowlist[scriptType].delete(c);
          return false;
        });
        Assert.deepEqual(
          loadedList[scriptType],
          [],
          `${phase}: should have no unexpected ${scriptType} loaded`
        );

        // Present errors in deterministic order.
        let unexpected = Array.from(loadedList[scriptType]);
        unexpected.sort();
        for (let script of unexpected) {
          // It would be nice to show stacks here, but that can be follow-up.
          ok(
            false,
            `${phase}: unexpected ${scriptType}: ${renderResource(script)}`
          );
        }
        Assert.deepEqual(
          allowlist[scriptType].size,
          0,
          `${phase}: all ${scriptType} allowlist entries should have been used`
        );
        let unused = Array.from(allowlist[scriptType]);
        unused.sort();
        for (let script of unused) {
          ok(
            false,
            `${phase}: unused ${scriptType} allowlist entry: ${renderResource(
              script
            )}`
          );
        }
      }
    }
    let denylist = backgroundtaskPhases[phase].denylist || null;
    if (denylist) {
      for (let scriptType in denylist) {
        let resources = denylist[scriptType];
        resources.sort();
        for (let resource of resources) {
          let loaded = loadedList[scriptType].includes(resource);
          let message = `${phase}: ${renderResource(resource)} is not allowed`;
          // It would be nice to show stacks here, but that can be follow-up.
          ok(!loaded, message);
        }
      }
    }
  }
});
