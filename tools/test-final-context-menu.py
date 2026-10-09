#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Exercise shipped Fluent/catalog and Flasco gating with normal sandboxing."""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import platform
import plistlib
import tempfile


# Chrome-context WebDriver payload, following the existing final-UA probe.
PROBE_SOURCE = r"""
// SPDX-License-Identifier: MPL-2.0
// Executed by Marionette inside the unmodified shipped browser, in chrome context.
const done = arguments[arguments.length - 1];
(async () => {
  const win = globalThis.window.wrappedJSObject;
  const { Experiments } = ChromeUtils.importESModule(
    "resource://noraneko/modules/experiments/Experiments.sys.mjs",
  );
  const { ContextMenuCatalogService: catalog } = ChromeUtils.importESModule(
    "resource://noraneko/modules/context-menu/ContextMenuCatalogService.sys.mjs",
  );
  const { NoranekoConstants } = ChromeUtils.importESModule(
    "resource://noraneko/modules/NoranekoConstants.sys.mjs",
  );
  const identity = {
    version: Services.appinfo.version,
    appBuildID: Services.appinfo.appBuildID,
    platformBuildID: Services.appinfo.platformBuildID,
    floorpVersion: NoranekoConstants.version2,
    buildID2: NoranekoConstants.buildID2,
    startupMode: Services.prefs.getStringPref("nora.startup.mode", ""),
    httpLoaderAllowed: Services.prefs.getBoolPref(
      "nora.dev.allow_http_loader",
      false,
    ),
    sandboxLevel: Services.prefs.getIntPref("security.sandbox.content.level"),
  };
  const checks = [];
  const sleep = (ms) => new Promise((resolve) => win.setTimeout(resolve, ms));
  const assert = (condition, message) => {
    if (!condition) throw new Error(message);
  };
  const wait = async (condition, description) => {
    const deadline = Date.now() + 10000;
    while (!condition() && Date.now() < deadline) await sleep(25);
    assert(condition(), description);
  };
  const enabled = () => catalog.getSnapshot().surfaces.length > 0;
  const checkState = async (name, expected) => {
    await wait(() => enabled() === expected, name);
    await sleep(100);
    assert(enabled() === expected, `${name}: stable catalog owner state`);
    checks.push({
      name,
      customizerActive: enabled(),
      catalogRevision: catalog.getRevision(),
    });
  };
  const id = "context_menu_customization";
  const policy = "floorp.experiments.participationPolicy";
  const master = "floorp.contextMenu.enabled";
  const configPref = "floorp.contextMenu.config";
  const savedConfig = Services.prefs.getStringPref(configPref);
  const source = "floorp.experiments.manifestUrl";
  const entry = (rollout = 100, extra = {}) => ({
    id,
    rollout,
    salt: "floorp-final-package-context-menu-qa",
    ...extra,
    variants: [{ id: "control", weight: 0 }, { id: "enabled", weight: 1 }],
  });
  const refresh = async (
    entries,
    participation = "default",
    installId = "final-context-menu",
  ) => {
    Experiments.clearCache();
    Services.prefs.setStringPref(policy, participation);
    Services.prefs.setStringPref(
      source,
      "data:application/json," +
        encodeURIComponent(JSON.stringify({ experiments: entries })),
    );
    await Experiments.init({ installId });
  };
  await wait(
    () => !Experiments.initializing,
    "initial shipped Flasco initialization",
  );
  await checkState("missing experiment leaves no customization catalog", false);

  const popup = win.document.getElementById("tabContextMenu");
  assert(popup, "native tab context menu exists");
  const missingKey = "floorp-nonexistent-final-package-diagnostic-key";
  const fixtures = [["floorp-final-qa-missing", missingKey], [
    "floorp-final-qa-valid",
    "reload-tab",
  ]]
    .map(([itemId, key]) => {
      const node = win.document.createXULElement("menuitem");
      node.id = itemId;
      node.setAttribute("data-lazy-l10n-id", key);
      popup.appendChild(node);
      return node;
    });
  const before = fixtures.map((node) => node.outerHTML);
  const l10n = new win.Localization(["browser/tabContextMenu.ftl"], true);
  const translated = l10n.formatMessagesSync([{ id: "reload-tab" }])[0]
    ?.attributes?.find((attribute) => attribute.name === "label")?.value;
  assert(translated, "native Fluent reload-tab label resolves");

  await refresh([entry()]);
  await checkState(
    "default enrolled enabled variant activates shipped controller",
    true,
  );
  const labels = () =>
    catalog.getSnapshot().surfaces.flatMap((surface) =>
      surface.profiles.flatMap((profile) =>
        profile.containers.flatMap((container) =>
          container.items.map((item) => item.label)
        )
      )
    );
  assert(
    labels().includes(missingKey),
    "missing Fluent key retains fallback row",
  );
  assert(
    labels().includes(translated),
    "valid Fluent label resolves after missing key",
  );
  assert(
    fixtures.every((node, i) => node.outerHTML === before[i]),
    "catalog leaves lazy native DOM intact",
  );
  checks.push({
    name:
      "shipped catalog bounds missing Fluent keys and continues native translation",
    fallback: missingKey,
    translatedLabel: translated,
  });

  // Exercise the real native popup as well as its startup seeding path.
  win.TabContextMenu.updateContextMenu(win.gBrowser.selectedTab);
  popup.openPopupAtScreen(240, 160, true);
  await wait(() => popup.state === "open", "native tab popup opens");
  popup.hidePopup();
  await wait(() => popup.state === "closed", "native tab popup closes");
  checks.push({
    name: "native tab popup remains responsive",
    state: popup.state,
  });

  Services.prefs.setBoolPref(master, false);
  await checkState("master off removes catalog owners", false);
  Services.prefs.setBoolPref(master, true);
  await checkState("master on resumes eligible saved layout", true);
  Services.prefs.setStringPref(policy, "never");
  await checkState(
    "never policy immediately destroys shipped controller",
    false,
  );
  await refresh([entry()], "never");
  await checkState(
    "never policy remains excluded after manifest refresh",
    false,
  );
  await refresh([entry(0)]);
  await checkState("default outside rollout remains excluded", false);
  assert(
    Experiments.forceEnrollExperiment(id).success,
    "normal per-experiment force enrollment succeeds",
  );
  await checkState("force enrollment enables excluded rollout", true);
  assert(
    Experiments.disableExperiment(id).success,
    "normal per-experiment opt-out succeeds",
  );
  await checkState("per-experiment opt-out removes catalog owners", false);
  await refresh([entry(0)], "always");
  await checkState("always participation enables experimental variant", true);

  const cohorts = new Set();
  for (let i = 0; i < 64 && cohorts.size < 2; i++) {
    await refresh([entry(50)], "default", `final-context-menu-cohort-${i}`);
    const experiment = Experiments.getAllExperiments().find((candidate) =>
      candidate.id === id
    );
    const expected = experiment?.currentVariantId === "enabled" &&
      experiment.enrollmentStatus === "enrolled";
    await checkState(`real 50 percent assignment ${i}`, expected);
    cohorts.add(expected);
  }
  assert(
    cohorts.size === 2,
    "real client's 50 percent rollout exercises both cohorts",
  );
  await refresh([]);
  await checkState("removed experiment stops customization", false);
  Services.prefs.setStringPref(source, "data:application/json,not-json");
  await Experiments.init({ installId: "final-context-menu" });
  await checkState(
    "failed manifest retrieval keeps customization disabled",
    false,
  );
  await refresh([entry()]);
  await checkState("successful manifest restores customization", true);
  Experiments.clearCache();
  await checkState("cache clear disables stale manifest immediately", false);
  await refresh([
    entry(100, {
      start: new Date(Date.now() + 1500).toISOString(),
      end: new Date(Date.now() + 4500).toISOString(),
    }),
  ]);
  await checkState("future start creates no customization controller", false);
  await checkState(
    "UTC start activates fetched manifest without refetch",
    true,
  );
  await checkState("UTC end removes customization controller", false);

  await refresh([entry()]);
  const memory = Cc["@mozilla.org/memory-reporter-manager;1"].getService(
    Ci.nsIMemoryReporterManager,
  );
  const initialResident = memory.resident;
  const residentSamples = [];
  for (let i = 0; i < 12; i++) {
    Services.prefs.setBoolPref(master, false);
    await wait(() => !enabled(), `repeat ${i} disable`);
    Services.prefs.setBoolPref(master, true);
    await wait(enabled, `repeat ${i} missing-key catalog rebuild`);
    assert(
      labels().includes(missingKey) && labels().includes(translated),
      "rebuild preserves both diagnostic rows",
    );
    residentSamples.push(memory.resident);
  }
  assert(
    Math.max(...residentSamples) - initialResident < 512 * 1024 * 1024,
    "repeated missing Fluent lookups remain below the 512 MiB growth ceiling",
  );
  assert(
    Services.prefs.getStringPref(configPref) === savedConfig,
    "gating never overwrites saved layout",
  );
  checks.push({
    name: "repeated missing-key rebuilds stay responsive and preserve layout",
    repetitions: 12,
    initialResident,
    residentSamples,
    growthCeilingBytes: 512 * 1024 * 1024,
  });
  fixtures.forEach((node) => node.remove());
  Services.prefs.setStringPref(policy, "never");
  await checkState("final cleanup releases shipped catalog owners", false);
  return {
    status: "passed",
    identity,
    checks,
    fixtureManifestScope: "isolated test profile only",
  };
})().then(
  done,
  (error) =>
    done({ status: "failed", error: String(error), stack: error?.stack }),
);
"""


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--browser", type=Path, required=True)
    parser.add_argument("--native-record", type=Path, required=True)
    parser.add_argument("--expected-floorp-version", required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    record = json.loads(args.native_record.read_text())
    require(record["schema_version"] == 2 and record["verification"]["status"] == "verified",
            "Expected the authenticated final native record")
    require(not record["floorp_package"]["unsigned"], "Requires normal production package")
    require(not any(os.environ.get(key) for key in (
        "MOZ_DISABLE_CONTENT_SANDBOX", "MOZ_DISABLE_GMP_SANDBOX",
        "MOZ_DISABLE_RDD_SANDBOX", "MOZ_DISABLE_GPU_SANDBOX")),
        "Refusing a sandbox-disabled test environment")
    spec = importlib.util.spec_from_file_location("native_runtime_helpers",
        Path(__file__).parent / "app-shim/test-runtime.py")
    helper = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(helper)
    browser = args.browser.resolve(strict=True)
    binary = browser
    if browser.is_dir():
        info = plistlib.loads((browser / "Contents/Info.plist").read_bytes())
        binary = browser / "Contents/MacOS" / info["CFBundleExecutable"]
    args.output.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix="run-", dir=args.output.resolve()))
    profile = output / "profile"
    profile.mkdir(mode=0o700)
    port = helper.unused_port()
    preferences = {
        "marionette.port": port, "browser.shell.checkDefaultBrowser": False,
        "browser.aboutwelcome.enabled": False, "browser.startup.page": 0,
        "browser.startup.homepage": "about:blank", "browser.startup.homepage_override.mstone": "ignore",
        "browser.sessionstore.resume_from_crash": False, "app.update.enabled": False,
        "datareporting.policy.dataSubmissionEnabled": False,
        "floorp.experiments.manifestUrl": 'data:application/json,%7B%22experiments%22%3A%5B%5D%7D',
        "floorp.experiments.participationPolicy": "default", "floorp.contextMenu.enabled": True,
        "floorp.contextMenu.config": json.dumps({"schemaVersion": 1, "surfaces": {
            "browser.tabs": {"base": {"root": {"order": ["tab.reload", "tab.close"],
                "hidden": ["tab.close"]}}, "profiles": {}}
        }}, separators=(",", ":")),
    }
    (profile / "user.js").write_text("\n".join(
        f"user_pref({json.dumps(key)}, {json.dumps(value)});" for key, value in preferences.items()) + "\n")
    probe = PROBE_SOURCE
    report = {"status": "failed", "nativeRecord": record, "profilePreferences": preferences,
              "probeSha256": hashlib.sha256(probe.encode()).hexdigest(),
              "method": "shipped modules, native popup, catalog and real Flasco client"}
    host = client = None
    try:
        with (output / "browser.log").open("w") as log:
            host = helper.HostProcess(browser, binary, profile, output, log,
                                      launch_services=platform.system() == "Darwin")

        def connect():
            require(host.poll() is None, "Browser exited before Marionette")
            try:
                return helper.Marionette(port)
            except OSError:
                return None

        client = helper.wait_for("Marionette", connect, 90)
        host.verify_session(client.session)
        report["hostProcessIdentity"] = {"pid": host.pid, "argv": host.command, "verified": host.verified}
        client.context("chrome")
        helper.wait_for("bundled sidebar", lambda: client.script(
            "return !!window.document.getElementById('panel-sidebar-select-box');"), 90)
        client.command("WebDriver:SetTimeouts", {"script": 180000})
        result = client.command("WebDriver:ExecuteAsyncScript", {"script": probe, "args": []})
        result = result.get("value", result)
        report["browserRegression"] = result
        require(result.get("status") == "passed", f"Shipped context-menu regression: {result.get('error')}")
        identity = result["identity"]
        require(identity["version"] == record["firefox_version"] == "157.0.1" and
                identity["appBuildID"] == identity["platformBuildID"] == record["runtime"]["expected_build_id"],
                "Executing browser differs from the authenticated native Runtime")
        require(identity["floorpVersion"] == args.expected_floorp_version and
                identity["buildID2"] == record["build_id2"], "Floorp package identity mismatch")
        require(identity["startupMode"] not in ("dev", "test") and not identity["httpLoaderAllowed"] and
                identity["sandboxLevel"] > 0, "Expected shipped modules and enabled content sandbox")
        require(host.poll() is None and host._matching_identity(host.pid) == host.identity,
                "Browser PID/executable changed during regressions")
        report["status"] = "passed"
    except Exception as error:
        report["error"] = str(error)
    finally:
        if client is not None:
            try:
                client.close()
            except (OSError, RuntimeError) as error:
                report["clientCleanupError"] = str(error)
                report["status"] = "failed"
        if host is not None:
            if host.poll() is not None:
                report["unexpectedHostExit"] = host.returncode
                report["status"] = "failed"
            try:
                host.close()
            except Exception as error:
                report["hostCleanupError"] = str(error)
                report["status"] = "failed"
            report["hostExitCode"] = host.returncode
            report["cleanupRequestedSignal"] = host.requested_signal
            report["ownedHostPid"] = host.pid
            if host.returncode not in (0, -int(host.requested_signal or 0)):
                report["hostCleanupError"] = f"Missing or abnormal browser exit: {host.returncode}"
                report["status"] = "failed"
        (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
        print(json.dumps({"status": report["status"], "report": str(output / "report.json"),
                          "error": report.get("error")}))
    return 0 if report["status"] == "passed" else 1


if __name__ == "__main__":
    raise SystemExit(main())
