/// <reference path="../../libs/@types/gecko/index.d.ts" />
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Runs inside a private browser profile against an isolated, patched source
// fixture. Every native process and manifest belongs to the test directory.
const { NativeManifests } = ChromeUtils.importESModule(
  "resource://floorp-native-fixture/NativeManifests.sys.mjs",
);
const { NativeApp } = ChromeUtils.importESModule(
  "resource://floorp-native-fixture/NativeMessaging.sys.mjs",
);
const { Schemas, SchemaRoot } = ChromeUtils.importESModule(
  "resource://gre/modules/Schemas.sys.mjs",
);
const { ExtensionCommon } = ChromeUtils.importESModule(
  "resource://gre/modules/ExtensionCommon.sys.mjs",
);
const { AppConstants } = ChromeUtils.importESModule(
  "resource://gre/modules/AppConstants.sys.mjs",
);

/** @param {unknown} condition @param {string} message */
function assert(condition, message) {
  if (!condition) {
    throw new Error(message);
  }
}

/** @param {unknown} actual @param {unknown} expected @param {string} message */
function equal(actual, expected, message) {
  assert(
    JSON.stringify(actual) === JSON.stringify(expected),
    `${message}: expected ${JSON.stringify(expected)}, got ${
      JSON.stringify(actual)
    }`,
  );
}

/** @param {string} directory @param {string} python */
export async function runTests(directory, python) {
  assert(
    AppConstants.platform === "linux",
    "Native host integration requires Linux",
  );
  await Schemas.load("chrome://extensions/content/schemas/manifest.json");
  const metadataSchema = await IOUtils.readJSON(
    PathUtils.join(directory, "metadata-schema.json"),
  );
  const isolatedSchema = new SchemaRoot(
    null,
    new Map([["metadata", metadataSchema]]),
  );
  isolatedSchema.parseSchemas();
  assert(
    metadataSchema[0].namespace === "floorpChromeManifest",
    "Fixture schema is isolated",
  );
  const completed = [];
  const id = "a".repeat(32);
  const origin = `chrome-extension://${id}/`;
  const addonId = `${id}@chromium-extension`;
  const metadata = { schema_version: 1, original_id: id };
  const extension = {
    id: addonId,
    manifestVersion: 3,
    manifest: {
      browser_specific_settings: {
        gecko: { floorp_chrome_extension: metadata },
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
  class NativeContext extends ExtensionCommon.BaseContext {
    /** @param {import("./crx-test-types.ts").NativeCaller} ownedExtension */
    constructor(ownedExtension = extension) {
      super("addon_parent", ownedExtension);
      this.sandbox = Cu.Sandbox(Cu.getObjectPrincipal(globalThis), {});
    }
    get cloneScope() {
      return globalThis;
    }
    get principal() {
      return Cu.getObjectPrincipal(this.sandbox);
    }
  }
  const firefoxDirectory = PathUtils.join(directory, "firefox");
  const chromeDirectory = PathUtils.join(directory, "chrome");
  const fallbackDirectory = PathUtils.join(directory, "fallback");
  for (const dir of [firefoxDirectory, chromeDirectory, fallbackDirectory]) {
    await IOUtils.makeDirectory(dir);
  }
  const oldLookup = NativeManifests._lookup;
  const oldDirectories = NativeManifests._chromeManifestDirectories;
  await NativeManifests.init();
  /** @param {string} type @param {string} name @param {import("./crx-test-types.ts").NativeContextInput} ctx */
  NativeManifests._lookup = (type, name, ctx) =>
    NativeManifests._tryPaths(type, name, [firefoxDirectory], ctx);
  NativeManifests._chromeManifestDirectories = () => [
    chromeDirectory,
    fallbackDirectory,
  ];
  /** @type {import("./crx-test-types.ts").NativeClient|null} */
  let app = null;
  try {
    equal(
      NativeManifests.getChromeExtensionOrigin(context),
      origin,
      "Explicit metadata resolves the original ID",
    );
    for (
      const invalid of [
        {},
        { ...metadata, schema_version: 2 },
        { ...metadata, original_id: "z".repeat(32) },
        { ...metadata, original_id: "A".repeat(32) },
      ]
    ) {
      const value = isolatedSchema.normalize(
        invalid,
        "floorpChromeManifest.Metadata",
        context,
      );
      assert(
        value.error,
        `Schema refuses invalid metadata ${JSON.stringify(invalid)}`,
      );
    }
    assert(
      !isolatedSchema.normalize(
        metadata,
        "floorpChromeManifest.Metadata",
        context,
      ).error,
      "Schema accepts the converter's metadata",
    );
    equal(
      NativeManifests.getChromeExtensionOrigin({
        extension: { ...extension, id: "other@tests.mozilla.org" },
      }),
      null,
      "Metadata cannot opt in a different Gecko ID",
    );
    completed.push("metadata schema and exact Gecko identity");

    const name = "floorp_native_test";
    const hostPath = PathUtils.join(chromeDirectory, `${name}.json`);
    const fallbackPath = PathUtils.join(fallbackDirectory, `${name}.json`);
    const command = PathUtils.join(chromeDirectory, "host.py");
    const program = `#!${python} -u
import json, struct, sys
stdin = getattr(sys.stdin, 'buffer', sys.stdin)
stdout = getattr(sys.stdout, 'buffer', sys.stdout)
while True:
    raw = stdin.read(4)
    if len(raw) != 4:
        break
    message = json.loads(stdin.read(struct.unpack('@I', raw)[0]))
    reply = json.dumps({'args': sys.argv[1:], 'message': message}).encode('utf-8')
    stdout.write(struct.pack('@I', len(reply)))
    stdout.write(reply)
    stdout.flush()
`;
    await IOUtils.writeUTF8(command, program);
    await IOUtils.setPermissions(command, 0o755);
    const host = {
      name,
      description: "Floorp Chrome native messaging fixture",
      type: "stdio",
      path: command,
      allowed_origins: [origin],
    };
    /** @param {string} path @param {import("./crx-test-types.ts").HostManifest} value */
    const writeHost = (path, value) => IOUtils.writeJSON(path, value);
    await writeHost(hostPath, host);
    await writeHost(fallbackPath, host);
    const found = await NativeManifests.lookupManifest("stdio", name, context);
    equal(found.chromeOrigin, origin, "Authorized Chrome host is discovered");
    equal(
      await NativeManifests.lookupManifest("stdio", name, {
        ...context,
        extension: { id: addonId, manifest: {} },
      }),
      null,
      "Firefox mode does not discover Chrome hosts",
    );
    equal(
      await NativeManifests.lookupManifest("storage", name, context),
      null,
      "Managed storage does not search Chrome hosts",
    );
    completed.push("host discovery is scoped to opted-in stdio callers");

    const nativeContext = new NativeContext();
    app =
      /** @type {import("./crx-test-types.ts").NativeClient} */ (new NativeApp(
        nativeContext,
        name,
      ));
    const connectedChromeApp = app;
    const reply = new Promise((resolve, reject) => {
      connectedChromeApp.on("message", (_event, value) => resolve(value));
      connectedChromeApp.on(
        "disconnect",
        (_event, error) =>
          reject(error || new Error("Native host disconnected before reply")),
      );
    });
    app.send(
      new StructuredCloneHolder(
        "",
        null,
        NativeApp.encodeMessage(nativeContext, { hello: "Chrome" }),
      ),
    );
    equal(
      await reply,
      { args: [origin], message: { hello: "Chrome" } },
      "An actual executable receives Chrome argv and exchanges framed JSON",
    );
    await app._cleanup();
    app = null;
    completed.push("executable host Chrome argv and framed JSON round trip");

    for (
      const allowed_origins of [
        ["chrome-extension://" + "b".repeat(32) + "/"],
        ["chrome-extension://*/"],
        [],
      ]
    ) {
      await writeHost(hostPath, { ...host, allowed_origins });
      equal(
        await NativeManifests.lookupManifest("stdio", name, context),
        null,
        "Denied user host cannot fall through to an authorized fallback host",
      );
    }
    await IOUtils.writeUTF8(hostPath, "{invalid json");
    equal(
      await NativeManifests.lookupManifest("stdio", name, context),
      null,
      "Malformed host cannot select another executable",
    );
    await writeHost(hostPath, host);
    completed.push("exact origin authorization and denied host precedence");

    const firefoxHosts = PathUtils.join(
      firefoxDirectory,
      "native-messaging-hosts",
    );
    await IOUtils.makeDirectory(firefoxHosts);
    const firefoxPath = PathUtils.join(firefoxHosts, `${name}.json`);
    /** @type {import("./crx-test-types.ts").HostManifest} */
    const firefoxHost = {
      ...host,
      allowed_extensions: ["other@tests.mozilla.org"],
    };
    delete firefoxHost.allowed_origins;
    await writeHost(firefoxPath, firefoxHost);
    equal(
      (await NativeManifests.lookupManifest("stdio", name, context))
        .chromeOrigin,
      origin,
      "An unrelated Firefox allowlist does not deny an authorized Chrome host",
    );
    const geckoExtension = {
      ...extension,
      id: "firefox@tests.mozilla.org",
      manifest: {},
    };
    const geckoContext = { ...context, extension: geckoExtension };
    await writeHost(firefoxPath, {
      ...firefoxHost,
      allowed_extensions: [geckoExtension.id],
    });
    await writeHost(hostPath, { ...host, allowed_origins: [] });
    equal(
      await NativeManifests.lookupManifest("stdio", name, context),
      null,
      "A denied Chrome host never selects an authorized Firefox registration",
    );
    await writeHost(hostPath, host);
    equal(
      await NativeManifests.lookupManifest("stdio", name, {
        ...context,
        extension: { ...geckoExtension, id: "denied@tests.mozilla.org" },
      }),
      null,
      "An ordinary Firefox caller never selects the authorized Chrome namespace",
    );
    const firefoxFound = await NativeManifests.lookupManifest(
      "stdio",
      name,
      geckoContext,
    );
    assert(
      !firefoxFound.chromeOrigin,
      "Firefox host keeps the Firefox launch protocol",
    );
    equal(
      firefoxFound.path,
      firefoxPath,
      "Gecko mode selects the Firefox registration",
    );
    const firefoxContext = new NativeContext(geckoExtension);
    app =
      /** @type {import("./crx-test-types.ts").NativeClient} */ (new NativeApp(
        firefoxContext,
        name,
      ));
    const connectedFirefoxApp = app;
    const firefoxReply = new Promise((resolve, reject) => {
      connectedFirefoxApp.on("message", (_event, value) => resolve(value));
      connectedFirefoxApp.on("disconnect", (_event, error) =>
        reject(
          error || new Error("Firefox native host disconnected before reply"),
        ));
    });
    app.send(
      new StructuredCloneHolder(
        "",
        null,
        NativeApp.encodeMessage(firefoxContext, "Firefox"),
      ),
    );
    equal(
      await firefoxReply,
      {
        args: [firefoxPath, geckoExtension.id],
        message: "Firefox",
      },
      "Firefox executable receives its original manifest-path and Gecko-ID argv",
    );
    await app._cleanup();
    app = null;
    completed.push(
      "Independent authorization namespaces and unchanged Firefox launch protocol",
    );
    return { passed: true, completed };
  } finally {
    await app?._cleanup();
    /** @param {string} type @param {string} name @param {import("./crx-test-types.ts").NativeContextInput} ctx */
    NativeManifests._lookup = oldLookup;
    NativeManifests._chromeManifestDirectories = oldDirectories;
  }
}
