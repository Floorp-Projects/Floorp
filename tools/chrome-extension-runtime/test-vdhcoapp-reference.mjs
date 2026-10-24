#!/usr/bin/env node
// SPDX-License-Identifier: MPL-2.0
// Authored developer caller + genuine vendor host. Excluded from frozen50 scoring.
import { ChromePipe } from "./chrome-cdp-pipe.mjs";
import { createHash } from "node:crypto";
import { execFileSync } from "node:child_process";
import { homedir } from "node:os";
import { mkdirSync, mkdtempSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
const root = resolve(dirname(fileURLToPath(import.meta.url)), "../..");
/** @param {string} name */
const argument = (name) => {
  const i = process.argv.indexOf(name);
  if (i < 0 || !process.argv[i + 1]) throw Error(name + " is required");
  return resolve(process.argv[i + 1]);
};
const mediaPath = argument("--media");
const bytes = readFileSync(mediaPath),
  sha = /** @param {Uint8Array|string} value */ (value) =>
    createHash("sha256").update(value).digest("hex");
const id = "lmjnegcaeklhafolokijcfjliaokphfk";
const original = JSON.parse(
  readFileSync(
    join(
      root,
      "_dist/chrome-extension-corpus/chrome-unpacked",
      id,
      "manifest.json",
    ),
    "utf8",
  ),
);
const reference = JSON.parse(
  readFileSync(
    join(root, "tools/chrome-extension-runtime/chrome-reference.json"),
    "utf8",
  ),
);
const chrome = join(
  root,
  "_dist/chrome-reference/extracted/opt/google/chrome/chrome",
);
if (sha(readFileSync(chrome)) !== reference.binary.sha256) {
  throw Error("Pinned reference browser digest mismatch");
}
const registration = readFileSync(
  join(
    homedir(),
    ".config/google-chrome/NativeMessagingHosts/net.downloadhelper.coapp.json",
  ),
);
const native = JSON.parse(registration.toString("utf8"));
if (
  native.name !== "net.downloadhelper.coapp" ||
  !native.allowed_origins?.includes(`chrome-extension://${id}/`) ||
  sha(readFileSync(native.path)) !==
    "069d434b094a30fad059bde6b44dc50310ff7c4771ce7d5aaf29fed3b3c7e0b1"
) throw Error("Official host registration or release binary digest mismatch");
const output = mkdtempSync(
  join(root, "_dist/chrome-runtime-tests/vdhcoapp-reference-"),
);
const fixture = join(output, "authored-extension");
mkdirSync(fixture);
// Private-profile registration uses the exact installer-generated JSON bytes.
const hosts = join(output, "chrome-profile/NativeMessagingHosts");
mkdirSync(hosts, { recursive: true });
writeFileSync(join(hosts, "net.downloadhelper.coapp.json"), registration);
writeFileSync(
  join(fixture, "manifest.json"),
  JSON.stringify({
    manifest_version: 3,
    name: "Authored official companion protocol probe",
    version: "1.0",
    key: original.key,
    permissions: ["nativeMessaging"],
  }),
);
writeFileSync(
  join(fixture, "proof.html"),
  '<!doctype html><meta charset="utf-8"><title>Authored official companion probe</title><script src="proof.js"></script>',
);
/** @param {import("./crx-test-types.ts").CompanionOptions & {media:string,path:string}} options */
function probeVendor(options) {
  const chrome =
    /** @type {{chrome:import("./crx-test-types.ts").ChromeNativeAPI}} */ (/** @type {unknown} */ (globalThis))
      .chrome;
  return (async () => {
    const port = chrome.runtime.connectNative("net.downloadhelper.coapp");
    /** @type {Map<number,import("./crx-test-types.ts").PendingRPC>} */
    const pending = new Map();
    let serial = 0;
    port.onMessage.addListener((message) => {
      const request = pending.get(message._reply);
      if (!request) return;
      pending.delete(message._reply);
      clearTimeout(request.timer);
      if (message._error) request.reject(Error(message._error));
      else request.resolve(message._result);
    });
    port.onDisconnect.addListener(() => {
      const error = chrome.runtime.lastError?.message ||
        "Vendor host disconnected";
      for (const request of pending.values()) {
        clearTimeout(request.timer);
        request.reject(Error(error));
      }
      pending.clear();
    });
    /** @param {string} method @param {...unknown} args @returns {Promise<unknown>} */
    const rpc = (method, ...args) =>
      new Promise((resolve, reject) => {
        const key = ++serial,
          timer = setTimeout(() => {
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
      const bytes = Uint8Array.from(
        atob(options.media),
        (c) => c.charCodeAt(0),
      );
      const fd = await rpc("fs.open", options.path, "w");
      let position = 0;
      try {
        while (position < bytes.length) {
          const chunk = bytes.subarray(position, position + 32768),
            written = /** @type {number} */ (await rpc(
              "fs.write",
              fd,
              [...chunk].join(","),
              0,
              chunk.length,
              position,
            ));
          if (
            !Number.isInteger(written) || written <= 0 || written > chunk.length
          ) throw Error("Invalid actual host write count");
          position += written;
        }
      } finally {
        await rpc("fs.close", fd);
      }
      return {
        id: info.id,
        version: info.version,
        target: info.target,
        nativeRequestCount: serial,
        bytes: position,
      };
    } finally {
      port.disconnect();
    }
  })();
}
writeFileSync(
  join(fixture, "proof.js"),
  `window.probeVendor=${probeVendor.toString()};`,
);
/** @type {import("./crx-test-types.ts").CompanionReport} */
const report = {
  stage: "supplemental-official-vendor-native-host",
  frozenCorpusScoreContribution: 0,
  caller:
    "authored unpacked developer extension with original public manifest key; not frozen VDH v10",
  reference,
  output,
  vendor: {
    archiveSource: "https://github.com/aclap-dev/vdhcoapp/releases/tag/v2.0.19",
    archiveSha256:
      "97c43b20fa36dcfbf468c1b652071bc200bd676339b8375065b2d7f93fc958bd",
    binarySha256: sha(readFileSync(native.path)),
    registrationSha256: sha(registration),
    linuxSignature:
      "Vendor publishes a SHA256 checksum; no detached Linux signature is provided",
  },
  passed: false,
};
const browser = new ChromePipe(chrome, output);
try {
  const loaded = await browser.call("Extensions.loadUnpacked", {
    path: fixture,
  });
  if (loaded.id !== id) {
    throw Error("Authored developer fixture original ID mismatch");
  }
  const target = await browser.call("Target.createTarget", {
    url: `chrome-extension://${id}/proof.html`,
  });
  const { sessionId } = await browser.call("Target.attachToTarget", {
    targetId: target.targetId,
    flatten: true,
  });
  await new Promise((resolve) => setTimeout(resolve, 400));
  const path = join(output, "official-host-saved.mp4");
  const value = await browser.call("Runtime.evaluate", {
    expression: `window.probeVendor(${
      JSON.stringify({ media: bytes.toString("base64"), path })
    })`,
    returnByValue: true,
    awaitPromise: true,
  }, sessionId);
  if (value.exceptionDetails) {
    throw Error(JSON.stringify(value.exceptionDetails));
  }
  report.native =
    /** @type {import("./crx-test-types.ts").CompanionResult} */ (value.result
      .value);
  const saved = readFileSync(path);
  if (sha(saved) !== sha(bytes)) {
    throw Error("Actual native companion saved different bytes");
  }
  const probe =
    /** @type {import("./crx-test-types.ts").MediaProbe} */ (JSON.parse(
      execFileSync("ffprobe", [
        "-v",
        "error",
        "-show_streams",
        "-show_format",
        "-of",
        "json",
        path,
      ], { encoding: "utf8" }),
    ));
  if (
    !probe.streams.some((s) => s.codec_type === "video") ||
    !probe.streams.some((s) => s.codec_type === "audio") ||
    Number(probe.format.duration) < 3.5
  ) throw Error("Native saved media is incomplete or unplayable");
  report.savedFile = {
    path,
    bytes: saved.length,
    sha256: sha(saved),
    duration: probe.format.duration,
    streams: probe.streams.map((s) => ({
      type: s.codec_type,
      codec: s.codec_name,
    })),
  };
  report.passed = true;
} catch (error) {
  report.error = String(error);
  process.exitCode = 1;
} finally {
  await browser.close();
  writeFileSync(join(output, "report.json"), JSON.stringify(report, null, 2));
  console.log(
    JSON.stringify({ output, passed: report.passed, error: report.error }),
  );
}
