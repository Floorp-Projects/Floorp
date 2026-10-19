/// <reference path="../../libs/@types/gecko/index.d.ts" />
// SPDX-License-Identifier: MPL-2.0
// Supplemental real vendor-host qualification; never part of the frozen50 score.
const { NativeManifests } = ChromeUtils.importESModule(
  "resource://gre/modules/NativeManifests.sys.mjs",
);
const { NativeApp } = ChromeUtils.importESModule(
  "resource://gre/modules/NativeMessaging.sys.mjs",
);
const { ExtensionCommon } = ChromeUtils.importESModule(
  "resource://gre/modules/ExtensionCommon.sys.mjs",
);
const { setTimeout, clearTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs",
);
export async function runAllTests() {
  const output = Services.env.get("FLOORP_COAPP_OUTPUT");
  if (!output) throw Error("Private vendor-host evidence directory required");
  await IOUtils.makeDirectory(output);
  const id = "lmjnegcaeklhafolokijcfjliaokphfk";
  const extension = {
    id: `${id}@chromium-extension`,
    manifestVersion: 3,
    manifest: {
      browser_specific_settings: {
        gecko: {
          floorp_chrome_extension: { schema_version: 1, original_id: id },
        },
      },
    },
  };
  const context = {
    extension,
    manifestVersion: 3,
    envType: "addon_parent",
    url: null,
    jsonStringify: JSON.stringify,
    cloneScope: globalThis,
    logError() {},
    preprocessors: {},
  };
  /** @param {Uint8Array} bytes */
  const digest = async (bytes) =>
    [
      ...new Uint8Array(
        await crypto.subtle.digest("SHA-256", new Uint8Array(bytes)),
      ),
    ].map((byte) => byte.toString(16).padStart(2, "0")).join("");
  /** @type {import("./crx-test-types.ts").CompanionReport} */
  const report = {
    stage: "supplemental-official-vendor-native-host",
    frozenCorpusScoreContribution: 0,
    caller: "authored explicit Chrome-mode parent context; not frozen VDH v10",
    coverage:
      "Production host lookup, authorization, launch and stdio transport; does not assert extension API binding or permission-prompt behavior",
    runtime: {
      version: Services.appinfo.version,
      buildId: Services.appinfo.appBuildID,
    },
    passed: false,
  };
  /** @type {import("./crx-test-types.ts").NativeClient<import("./crx-test-types.ts").HostReply>|undefined} */
  let app;
  try {
    const host = await NativeManifests.lookupManifest(
      "stdio",
      "net.downloadhelper.coapp",
      context,
    );
    report.lookup = host &&
      {
        path: host.path,
        chromeOrigin: host.chromeOrigin,
        executable: host.manifest.path,
      };
    if (host?.chromeOrigin !== `chrome-extension://${id}/`) {
      throw Error(
        "Official vendor Chrome registration was blocked by the unrelated Firefox allowlist",
      );
    }
    const binarySha256 = await digest(await IOUtils.read(host.manifest.path));
    if (
      binarySha256 !==
        "069d434b094a30fad059bde6b44dc50310ff7c4771ce7d5aaf29fed3b3c7e0b1"
    ) {
      throw Error(
        "Official companion executable digest differs from the authenticated release archive",
      );
    }
    report.vendorProvenance = {
      archiveSource:
        "https://github.com/aclap-dev/vdhcoapp/releases/tag/v2.0.19",
      archiveSha256:
        "97c43b20fa36dcfbf468c1b652071bc200bd676339b8375065b2d7f93fc958bd",
      binarySha256,
      registrationSha256: await digest(await IOUtils.read(host.path)),
      linuxSignature:
        "Vendor publishes a SHA256 checksum; no detached Linux signature is provided",
    };
    class NativeContext extends ExtensionCommon.BaseContext {
      constructor() {
        super("addon_parent", extension);
        this.sandbox = Cu.Sandbox(Cu.getObjectPrincipal(globalThis), {});
      }
      get cloneScope() {
        return globalThis;
      }
      get principal() {
        return Cu.getObjectPrincipal(this.sandbox);
      }
    }
    const nativeContext = new NativeContext();
    app =
      /** @type {import("./crx-test-types.ts").NativeClient<import("./crx-test-types.ts").HostReply>} */ (new NativeApp(
        nativeContext,
        "net.downloadhelper.coapp",
      ));
    const connectedApp = app;
    /** @type {Map<number,import("./crx-test-types.ts").PendingRPC>} */
    const pending = new Map();
    let request = 0;
    app.on("message", (_event, message) => {
      const task = pending.get(message._reply);
      if (!task) return;
      pending.delete(message._reply);
      clearTimeout(task.timer);
      if (message._error) task.reject(Error(message._error));
      else task.resolve(message._result);
    });
    app.on("disconnect", (_event, error) => {
      for (const task of pending.values()) {
        clearTimeout(task.timer);
        task.reject(error || Error("Official native host disconnected"));
      }
      pending.clear();
    });
    /** @param {string} method @param {...unknown} args @returns {Promise<unknown>} */
    const rpc = (method, ...args) =>
      new Promise((resolve, reject) => {
        const key = ++request;
        const timer = setTimeout(() => {
          pending.delete(key);
          reject(Error("Official native host RPC timed out: " + method));
        }, 10000);
        pending.set(key, { resolve, reject, timer });
        const message = {
          type: "weh#rpc",
          _request: key,
          _method: method,
          _args: args,
        };
        connectedApp.send(
          new StructuredCloneHolder(
            "",
            null,
            NativeApp.encodeMessage(nativeContext, message),
          ),
        );
      });
    const info =
      /** @type {import("./crx-test-types.ts").NativeHostInfo} */ (await rpc(
        "info",
      ));
    if (
      info.id !== "net.downloadhelper.coapp" || info.version !== "2.0.19" ||
      info.binary !== host.manifest.path
    ) throw Error("Unexpected official host identity/version/executable");
    report.vendor = {
      id: info.id,
      version: info.version,
      target: info.target,
      converterBase: info.converterBase,
      converterBaseVersion: info.converterBaseVersion,
    };
    const media = Services.env.get("FLOORP_COAPP_MEDIA");
    const bytes = media ? await IOUtils.read(media) : new TextEncoder().encode(
      "Authored Floorp official native-host protocol probe\n",
    );
    const destination = PathUtils.join(
      output,
      media ? "official-host-saved.mp4" : "official-host-saved.txt",
    );
    const fd = await rpc("fs.open", destination, "w");
    let position = 0;
    try {
      while (position < bytes.length) {
        const chunk = bytes.subarray(position, position + 32768);
        const written = /** @type {number} */ (await rpc(
          "fs.write",
          fd,
          [...chunk].join(","),
          0,
          chunk.length,
          position,
        ));
        if (
          !Number.isInteger(written) || written <= 0 || written > chunk.length
        ) throw Error("Official native host returned an invalid write count");
        position += written;
      }
    } finally {
      await rpc("fs.close", fd);
    }
    const actual = await IOUtils.read(destination);
    if (
      actual.length !== bytes.length ||
      actual.some((byte, index) => byte !== bytes[index])
    ) throw Error("Actual official-host output differs from authored bytes");
    report.savedFile = {
      path: destination,
      bytes: actual.length,
      sha256: await digest(actual),
    };
    report.nativeRequestCount = request;
    report.passed = true;
  } catch (error) {
    report.error = String(error);
    throw error;
  } finally {
    await app?._cleanup();
    await IOUtils.writeJSON(PathUtils.join(output, "report.json"), report);
    console.log("REPORT " + PathUtils.join(output, "report.json"));
  }
}
