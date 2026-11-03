/// <reference path="../../libs/@types/gecko/index.d.ts" />
// SPDX-License-Identifier: MPL-2.0
// Authored installed callers and the official legacy companion; zero frozen-corpus score.
const { AddonManager } = ChromeUtils.importESModule(
  "resource://gre/modules/AddonManager.sys.mjs",
);
const { ExtensionParent } = ChromeUtils.importESModule(
  "resource://gre/modules/ExtensionParent.sys.mjs",
);
const { FileUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/FileUtils.sys.mjs",
);
const { NativeManifests } = ChromeUtils.importESModule(
  "resource://gre/modules/NativeManifests.sys.mjs",
);
const { setTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs",
);
const ID = "lmjnegcaeklhafolokijcfjliaokphfk";
const BINARY_SHA =
  "069d434b094a30fad059bde6b44dc50310ff7c4771ce7d5aaf29fed3b3c7e0b1";
/** @param {Uint8Array} bytes */
const digest = async (bytes) =>
  [
    ...new Uint8Array(
      await crypto.subtle.digest("SHA-256", new Uint8Array(bytes)),
    ),
  ].map(
    (byte) => byte.toString(16).padStart(2, "0"),
  ).join("");

// This entire function runs as an installed extension's background script.
/** @param {import("./crx-test-types.ts").CompanionOptions} options @returns {Promise<import("./crx-test-types.ts").CompanionResult>} */
async function probe(options) {
  const chrome =
    /** @type {{chrome:import("./crx-test-types.ts").ChromeNativeAPI}} */ (/** @type {unknown} */ (globalThis))
      .chrome;
  if (typeof chrome.runtime.connectNative !== "function") {
    throw Error(
      "nativeMessaging API is unavailable without its declared permission",
    );
  }
  const port = chrome.runtime.connectNative("net.downloadhelper.coapp");
  /** @type {Map<number,import("./crx-test-types.ts").PendingRPC>} */
  const pending = new Map();
  let serial = 0;
  port.onMessage.addListener((message) => {
    const task = pending.get(message._reply);
    if (!task) return;
    pending.delete(message._reply);
    clearTimeout(task.timer);
    if (message._error) task.reject(Error(message._error));
    else task.resolve(message._result);
  });
  port.onDisconnect.addListener(() => {
    const error = chrome.runtime.lastError?.message ||
      "Official host disconnected";
    for (const task of pending.values()) {
      clearTimeout(task.timer);
      task.reject(Error(error));
    }
    pending.clear();
  });
  /** @param {string} method @param {...unknown} args @returns {Promise<unknown>} */
  const rpc = (method, ...args) =>
    new Promise((resolve, reject) => {
      const key = ++serial;
      const timer = setTimeout(() => {
        pending.delete(key);
        reject(Error("Official native RPC timed out: " + method));
      }, 10000);
      pending.set(key, { resolve, reject, timer });
      port.postMessage({
        type: "weh#rpc",
        _request: key,
        _method: method,
        _args: args,
      });
    });
  try {
    const info =
      /** @type {import("./crx-test-types.ts").NativeHostInfo} */ (await rpc(
        "info",
      ));
    if (info.id !== "net.downloadhelper.coapp" || info.version !== "2.0.19") {
      throw Error("Unexpected official companion identity/version");
    }
    /** @type {import("./crx-test-types.ts").CompanionResult} */
    const result = {
      id: info.id,
      version: info.version,
      binary: info.binary,
      runtimeId: chrome.runtime.id,
      nativeRequestCount: serial,
    };
    if (options.media) {
      const bytes = Uint8Array.from(
        atob(options.media),
        (character) => character.charCodeAt(0),
      );
      const descriptor = await rpc("fs.open", options.path, "w");
      let position = 0;
      try {
        while (position < bytes.length) {
          const chunk = bytes.subarray(position, position + 32768);
          const written = /** @type {number} */ (await rpc(
            "fs.write",
            descriptor,
            [...chunk].join(","),
            0,
            chunk.length,
            position,
          ));
          if (
            !Number.isInteger(written) || written <= 0 || written > chunk.length
          ) throw Error("Invalid actual native write count");
          position += written;
        }
      } finally {
        await rpc("fs.close", descriptor);
      }
      result.bytes = position;
      result.nativeRequestCount = serial;
    }
    return result;
  } finally {
    port.disconnect();
  }
}

/** @template T @param {()=>T|false|null|undefined} read @param {string} label @returns {Promise<T>} */
async function until(read, label) {
  const deadline = Date.now() + 20000;
  while (Date.now() < deadline) {
    const result = read();
    if (result) return result;
    await new Promise((resolve) => setTimeout(resolve, 50));
  }
  throw Error("Timed out waiting for " + label);
}

export async function runAllTests() {
  if (Services.appinfo.OS !== "Linux") {
    throw Error("The authenticated official companion fixture is Linux-only");
  }
  const output = Services.env.get("FLOORP_COAPP_OUTPUT");
  const media = Services.env.get("FLOORP_COAPP_MEDIA");
  if (!output || !media) {
    throw Error(
      "Private companion evidence directory and authored MP4 required",
    );
  }
  await IOUtils.makeDirectory(output);
  /** @type {import("./crx-test-types.ts").CompanionReport & {cases:import("./crx-test-types.ts").CompanionCase[]}} */
  const report = {
    stage: "supplemental-official-native-host-public-api",
    frozenCorpusScoreContribution: 0,
    caller:
      "Explicitly authored temporary developer extensions; not the signed frozen VDH v10 package",
    coverage:
      "Real installed MV2 document and MV3 native Service Worker runtime.connectNative, declared nativeMessaging permission, original Chrome-origin authorization, ordinary Gecko unauthorized-caller denial, real vendor process and framed RPC transport; does not test a persistent-install permission prompt",
    runtime: {
      version: Services.appinfo.version,
      buildId: Services.appinfo.appBuildID,
    },
    cases: [],
    passed: false,
  };
  /** @type {import("./crx-test-types.ts").InstalledAddon[]} */
  const fixtures = [];
  const window =
    /** @type {import("./crx-test-types.ts").BrowserWindow} */ (/** @type {unknown} */ (Services
      .wm.getMostRecentWindow("navigator:browser")));
  /** @type {import("./crx-test-types.ts").NativeTab[]} */
  const controls = [];
  const bytes = await IOUtils.read(media);
  let encoded = "";
  for (let offset = 0; offset < bytes.length; offset += 32768) {
    encoded += String.fromCharCode(...bytes.subarray(offset, offset + 32768));
  }
  const destination = PathUtils.join(
    output,
    "official-host-public-api-saved.mp4",
  );
  if (await IOUtils.exists(destination)) {
    throw Error("Refusing to overwrite previous companion evidence");
  }
  /** @param {string} label @param {string|null} id @param {boolean} compatible @param {boolean} permission @param {import("./crx-test-types.ts").CompanionOptions} [options] @param {boolean} [worker] @returns {Promise<import("./crx-test-types.ts").CompanionReply>} */
  const execute = async (
    label,
    id,
    compatible,
    permission,
    options = {},
    worker = false,
  ) => {
    const directory = PathUtils.join(output, "authored-" + label);
    if (await IOUtils.exists(directory)) {
      throw Error("Refusing to reuse an authored extension fixture");
    }
    await IOUtils.makeDirectory(directory);
    const addonId = compatible
      ? `${id}@chromium-extension`
      : "floorp-coapp-gecko-control@tests";
    const manifest = {
      manifest_version: worker ? 3 : 2,
      name: "Authored official companion " + label,
      version: "1.0",
      permissions: permission ? ["nativeMessaging"] : [],
      background: worker
        ? { service_worker: "background.js", type: "module" }
        : { scripts: ["background.js"] },
      browser_specific_settings: {
        gecko: {
          id: addonId,
          ...(compatible
            ? {
              floorp_chrome_extension: { schema_version: 1, original_id: id },
            }
            : {}),
        },
      },
    };
    const call = `(${probe.toString()})(${JSON.stringify(options)})`;
    const script = worker
      ? `browser.runtime.onMessage.addListener(message => {
          if (message.officialCompanionProbe !== ${
        JSON.stringify(label)
      }) return;
          return ${call}.then(result => ({completed:true,result}), error => ({completed:true,error:String(error)}));
        });`
      : `${call}.then(result => { document.title = JSON.stringify({completed:true,result}); }, error => { document.title = JSON.stringify({completed:true,error:String(error)}); });`;
    await IOUtils.writeJSON(
      PathUtils.join(directory, "manifest.json"),
      manifest,
    );
    await IOUtils.writeUTF8(PathUtils.join(directory, "background.js"), script);
    if (worker) {
      await IOUtils.writeUTF8(
        PathUtils.join(directory, "control.html"),
        '<!doctype html><script src="control.js"></script>',
      );
      await IOUtils.writeUTF8(
        PathUtils.join(directory, "control.js"),
        `
        browser.runtime.sendMessage({officialCompanionProbe:${
          JSON.stringify(label)
        }}).then(
          reply => { document.title = JSON.stringify(reply); },
          error => { document.title = JSON.stringify({completed:true,error:String(error)}); });`,
      );
    }
    const addon = await AddonManager.installTemporaryAddon(
      new FileUtils.File(directory),
    );
    fixtures.push(addon);
    const extension = await until(
      () => ExtensionParent.GlobalManager.extensionMap.get(addonId),
      label + " installed policy",
    );
    /** @type {import("./crx-test-types.ts").NativeTab|undefined} */
    let control;
    if (worker) {
      await until(
        () => extension.backgroundContext?.viewType === "background_worker",
        "actual native extension Service Worker",
      );
      if (!extension.policy.backgroundServiceWorkerAllowed) {
        throw Error("Native Worker compatibility policy is missing");
      }
      control = window.gBrowser.addTab(
        extension.baseURI.resolve("control.html"),
        {
          triggeringPrincipal: Services.scriptSecurityManager
            .getSystemPrincipal(),
        },
      );
      controls.push(control);
    }
    const reply = await until(() => {
      const title = worker
        ? control?.linkedBrowser.contentTitle
        : extension.backgroundContext?.xulBrowser?.contentTitle;
      try {
        const value = JSON.parse(title);
        return value.completed && value;
      } catch {
        return false;
      }
    }, label + " real native API result");
    report.cases.push({
      label,
      addonId,
      chromeMode: compatible,
      nativeMessagingPermission: permission,
      background: worker ? "native-module-service-worker" : "MV2-document",
      ...reply,
    });
    if (control) {
      window.gBrowser.removeTab(control, { skipPermitUnload: true });
      controls.pop();
    }
    await addon.uninstall();
    fixtures.pop();
    return reply;
  };
  try {
    const context = {
      extension: {
        id: `${ID}@chromium-extension`,
        manifestVersion: 2,
        manifest: {
          browser_specific_settings: {
            gecko: {
              floorp_chrome_extension: { schema_version: 1, original_id: ID },
            },
          },
        },
      },
      manifestVersion: 2,
      envType: "addon_parent",
      url: null,
      jsonStringify: JSON.stringify,
      cloneScope: globalThis,
      logError() {},
      preprocessors: {},
    };
    const host = await NativeManifests.lookupManifest(
      "stdio",
      "net.downloadhelper.coapp",
      context,
    );
    if (host?.chromeOrigin !== `chrome-extension://${ID}/`) {
      throw Error(
        "Official Chrome registration does not authorize the expected original ID",
      );
    }
    const binarySha256 = await digest(await IOUtils.read(host.manifest.path));
    if (binarySha256 !== BINARY_SHA) {
      throw Error("Official release executable digest mismatch");
    }
    report.vendorProvenance = {
      release: "https://github.com/aclap-dev/vdhcoapp/releases/tag/v2.0.19",
      archiveSha256:
        "97c43b20fa36dcfbf468c1b652071bc200bd676339b8375065b2d7f93fc958bd",
      binarySha256,
      registrationPath: host.path,
      registrationSha256: await digest(await IOUtils.read(host.path)),
      linuxSignature:
        "Vendor HTTPS SHA256 checksum; no detached Linux signature provided",
    };
    const noPermission = await execute("without-permission", ID, true, false);
    if (
      !/nativeMessaging API is unavailable|connectNative.*(?:not a function|undefined)|does not have.*permission|permission.*native/i
        .test(noPermission.error || "") || noPermission.result
    ) {
      throw Error(
        "Expected nativeMessaging permission denial was not observed: " +
          JSON.stringify(noPermission),
      );
    }
    const wrongOrigin = await execute(
      "unauthorized-chrome-origin",
      "a".repeat(32),
      true,
      true,
    );
    if (
      !/No such native application net\.downloadhelper\.coapp/i.test(
        wrongOrigin.error || "",
      ) || wrongOrigin.result
    ) {
      throw Error(
        "Expected unauthorized-origin rejection was not observed: " +
          JSON.stringify(wrongOrigin),
      );
    }
    const ordinary = await execute("ordinary-gecko", null, false, true);
    if (
      !/No such native application net\.downloadhelper\.coapp/i.test(
        ordinary.error || "",
      ) || ordinary.result
    ) {
      throw Error(
        "Expected ordinary Gecko namespace rejection was not observed: " +
          JSON.stringify(ordinary),
      );
    }
    const authorized = await execute(
      "authorized-chrome-origin",
      ID,
      true,
      true,
      { path: destination, media: btoa(encoded) },
    );
    if (
      authorized.error || authorized.result?.runtimeId !== ID ||
      authorized.result.binary !== host.manifest.path
    ) {
      throw Error(
        "Authorized original-ID public native API failed: " +
          JSON.stringify(authorized),
      );
    }
    const actual = await IOUtils.read(destination);
    if (
      actual.length !== bytes.length ||
      actual.some((byte, index) => byte !== bytes[index])
    ) {
      throw Error(
        "Actual public-API companion output differs from authored media",
      );
    }
    report.savedFile = {
      path: destination,
      bytes: actual.length,
      sha256: await digest(actual),
    };
    const workerNoPermission = await execute(
      "worker-without-permission",
      ID,
      true,
      false,
      {},
      true,
    );
    if (
      !/nativeMessaging API is unavailable|not enough privileges|does not have.*permission|permission.*native/i
        .test(workerNoPermission.error || "") || workerNoPermission.result
    ) {
      throw Error(
        "Expected native Worker permission denial was not observed: " +
          JSON.stringify(workerNoPermission),
      );
    }
    const workerWrongOrigin = await execute(
      "worker-unauthorized-origin",
      "a".repeat(32),
      true,
      true,
      {},
      true,
    );
    if (
      !/No such native application net\.downloadhelper\.coapp/i.test(
        workerWrongOrigin.error || "",
      ) || workerWrongOrigin.result
    ) {
      throw Error(
        "Expected native Worker origin rejection was not observed: " +
          JSON.stringify(workerWrongOrigin),
      );
    }
    const workerPath = PathUtils.join(
      output,
      "official-host-worker-public-api-saved.mp4",
    );
    if (await IOUtils.exists(workerPath)) {
      throw Error("Refusing to overwrite previous Worker companion evidence");
    }
    const worker = await execute("authorized-native-worker", ID, true, true, {
      path: workerPath,
      media: btoa(encoded),
    }, true);
    if (
      worker.error || worker.result?.runtimeId !== ID ||
      worker.result.binary !== host.manifest.path
    ) {
      throw Error(
        "Authorized native Worker public native API failed: " +
          JSON.stringify(worker),
      );
    }
    const workerBytes = await IOUtils.read(workerPath);
    if (
      workerBytes.length !== bytes.length ||
      workerBytes.some((byte, index) => byte !== bytes[index])
    ) {
      throw Error(
        "Actual native Worker companion output differs from authored media",
      );
    }
    report.workerSavedFile = {
      path: workerPath,
      bytes: workerBytes.length,
      sha256: await digest(workerBytes),
    };
    report.passed = true;
  } catch (error) {
    report.error = String(error);
    throw error;
  } finally {
    for (const control of controls) {
      window.gBrowser.removeTab(control, { skipPermitUnload: true });
    }
    for (const addon of fixtures.reverse()) await addon.uninstall();
    await IOUtils.writeJSON(PathUtils.join(output, "report.json"), report);
    console.log("REPORT " + PathUtils.join(output, "report.json"));
  }
}
