/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Cu.import("resource://gre/modules/TelemetryModules.jsm");
Cu.import("resource://gre/modules/AppConstants.jsm");
Cu.import("resource://gre/modules/Preferences.jsm");
Cu.import("resource://gre/modules/ctypes.jsm");
Cu.import("resource://gre/modules/osfile.jsm");

const MAX_NAME_LENGTH = 64;

// The following libraries (except libxul) are all built from the
// toolkit/components/telemetry/tests/modules-test.cpp file, which contains
// instructions on how to build them.
const libModules = ctypes.libraryName("modules-test");
const libUnicode = ctypes.libraryName("modμles-test");
const libLongName = "lorem_ipsum_dolor_sit_amet_consectetur_adipiscing_elit_Fusce_sit_amet_tellus_non_magna_euismod_vestibulum_Vivamus_turpis_duis.dll"
const libUnicodePDB = Services.appinfo.is64Bit ? "testUnicodePDB64.dll" : "testUnicodePDB32.dll";
const libNoPDB = Services.appinfo.is64Bit ? "testNoPDB64.dll" : "testNoPDB32.dll";
const libxul = OS.Path.basename(OS.Constants.Path.libxul);

const libModulesFile = do_get_file(libModules).path;
const libUnicodeFile = OS.Path.join(OS.Path.dirname(libModulesFile), libUnicode);
const libLongNameFile = OS.Path.join(OS.Path.dirname(libModulesFile), libLongName);
const libUnicodePDBFile = do_get_file(libUnicodePDB).path;
const libNoPDBFile = do_get_file(libNoPDB).path;

let libModulesHandle, libUnicodeHandle, libLongNameHandle, libUnicodePDBHandle, libNoPDBHandle;

let expectedLibs;
if (AppConstants.platform === "win") {
  const version = AppConstants.MOZ_APP_VERSION.substring(0, AppConstants.MOZ_APP_VERSION.indexOf(".") + 2);

  expectedLibs = [
    {
      name: libxul,
      debugName: libxul.replace(".dll", ".pdb"),
      version,
    },
    {
      name: libModules,
      debugName: libModules.replace(".dll", ".pdb"),
      version,
    },
    {
      name: libUnicode,
      debugName: libModules.replace(".dll", ".pdb"),
      version,
    },
    {
      name: libLongName.substring(0, MAX_NAME_LENGTH - 1) + "…",
      debugName: libModules.replace(".dll", ".pdb"),
      version,
    },
    {
      name: libUnicodePDB,
      debugName: "libmodμles.pdb",
      version: null,
    },
    {
      name: libNoPDB,
      debugName: null,
      version: null,
    },
  ];
} else if (AppConstants.platform === "android") {
  // Listing shared libraries doesn't work in Android xpcshell tests.
  // https://hg.mozilla.org/mozilla-central/file/0eef1d5a39366059677c6d7944cfe8a97265a011/tools/profiler/core/shared-libraries-linux.cc#l95
  expectedLibs = [];
} else {
  expectedLibs = [
    {
      name: libxul,
      debugName: libxul,
      version: null,
    },
    {
      name: libModules,
      debugName: libModules,
      version: null,
    },
    {
      name: libUnicode,
      debugName: libUnicode,
      version: null,
    },
    {
      name: libLongName.substring(0, MAX_NAME_LENGTH - 1) + "…",
      debugName: libLongName.substring(0, MAX_NAME_LENGTH - 1) + "…",
      version: null,
    },
  ];
}

add_task(async function setup() {
  do_get_profile();

  await OS.File.copy(libModulesFile, libUnicodeFile);
  await OS.File.copy(libModulesFile, libLongName);

  if (AppConstants.platform !== "android") {
    libModulesHandle = ctypes.open(libModulesFile);
    libUnicodeHandle = ctypes.open(libUnicodeFile);
    libLongNameHandle = ctypes.open(libLongNameFile);
    if (AppConstants.platform === "win") {
      libUnicodePDBHandle = ctypes.open(libUnicodePDBFile);
      libNoPDBHandle = ctypes.open(libNoPDBFile);
    }
  }

  // Force the timer to fire (using a small interval).
  Cc["@mozilla.org/updates/timer-manager;1"].getService(Ci.nsIObserver).observe(null, "utm-test-init", "");
  Preferences.set("toolkit.telemetry.modulesPing.interval", 0);
  Preferences.set("app.update.url", "http:/localhost");

  // Start the local ping server and setup Telemetry to use it during the tests.
  PingServer.start();
  Preferences.set(TelemetryUtils.Preferences.Server, "http://localhost:" + PingServer.port);
});

do_register_cleanup(function() {
  if (libModulesHandle) {
    libModulesHandle.close();
  }
  if (libUnicodeHandle) {
    libUnicodeHandle.close();
  }
  if (libLongNameHandle) {
    libLongNameHandle.close();
  }
  if (libUnicodePDBHandle) {
    libUnicodePDBHandle.close();
  }
  if (libNoPDBHandle) {
    libNoPDBHandle.close();
  }

  return OS.File.remove(libUnicodeFile)
  .then(() => OS.File.remove(libLongNameFile))
  .then(() => PingServer.stop());
});

add_task({
  skip_if: () => !AppConstants.MOZ_GECKO_PROFILER,
}, async function test_send_ping() {
  await TelemetryController.testSetup();

  let found = await PingServer.promiseNextPing();
  Assert.ok(!!found, "Telemetry ping submitted.");
  Assert.strictEqual(found.type, "modules", "Ping type is 'modules'");
  Assert.ok(found.environment, "'modules' ping has an environment.");
  Assert.ok(!!found.clientId, "'modules' ping has a client ID.");
  Assert.ok(!!found.payload.modules, "Telemetry ping payload contains the 'modules' array.");

  for (let lib of expectedLibs) {
    let test_lib = found.payload.modules.find(module => module.name === lib.name);

    Assert.ok(!!test_lib, "There is a '" + lib.name + "' module.");

    if (lib.version !== null) {
      Assert.ok(test_lib.version.startsWith(lib.version), "The version of the " + lib.name + " module (" + test_lib.version + ") is correct (it starts with '" + lib.version + "').");
    } else {
      Assert.strictEqual(test_lib.version, null, "The version of the " + lib.name + " module is null.");
    }

    Assert.strictEqual(test_lib.debugName, lib.debugName, "The " + lib.name + " module has the correct debug name.");

    if (lib.debugName === null) {
      Assert.strictEqual(test_lib.debugID, null, "The " + lib.name + " module doesn't have a debug ID.");
    } else {
      Assert.greater(test_lib.debugID.length, 0, "The " + lib.name + " module has a debug ID.");
    }
  }

  let test_lib = found.payload.modules.find(module => module.name === libLongName);
  Assert.ok(!test_lib, "There isn't a '" + libLongName + "' module.");
});
