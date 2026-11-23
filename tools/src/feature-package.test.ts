// SPDX-License-Identifier: MPL-2.0

import { assert, assertEquals } from "@std/assert";
import { executeFeaturePackageCli } from "../feature-package.ts";

function encodeBase64Url(bytes: Uint8Array): string {
  let binary = "";
  for (const byte of bytes) binary += String.fromCharCode(byte);
  return btoa(binary).replaceAll("+", "-").replaceAll("/", "_").replace(
    /=+$/,
    "",
  );
}

Deno.test("feature package CLI packs and verifies without exposing private key or source", async () => {
  const root = await Deno.makeTempDir({ prefix: "feature-package-cli-" });
  let privateBytes: Uint8Array | undefined;
  try {
    const pair = await crypto.subtle.generateKey({ name: "Ed25519" }, true, [
      "sign",
      "verify",
    ]) as CryptoKeyPair;
    privateBytes = new Uint8Array(
      await crypto.subtle.exportKey("pkcs8", pair.privateKey),
    );
    const publicBytes = new Uint8Array(
      await crypto.subtle.exportKey("raw", pair.publicKey),
    );
    const metadataPath = `${root}/metadata.json`;
    const sourcePath = `${root}/source.js`;
    const keyPath = `${root}/private.pk8`;
    const outputPath = `${root}/package.json`;
    const policyPath = `${root}/policy.json`;
    const source = "globalThis.__neverRun = true;\r\n";
    const metadata = {
      featureId: "hub-panel-menu",
      version: "v1",
      releaseSequence: 4,
      compatibility: {
        hostSchema: 2,
        hostBuildId: "host-1",
        runtimeBuildIds: ["runtime-1"],
        capabilities: ["hub-panel"],
      },
    };
    const policy = {
      featureId: "hub-panel-menu",
      hostSchema: 2,
      hostBuildId: "host-1",
      runtimeBuildId: "runtime-1",
      allowedCapabilities: ["hub-panel"],
      minimumReleaseSequence: 1,
      trustedKeys: [{
        keyId: "cli-key",
        publicKey: encodeBase64Url(publicBytes),
      }],
    };
    await Deno.writeTextFile(metadataPath, JSON.stringify(metadata));
    await Deno.writeTextFile(sourcePath, source);
    await Deno.writeTextFile(policyPath, JSON.stringify(policy));
    await Deno.writeFile(keyPath, privateBytes, {
      createNew: true,
      mode: 0o600,
    });

    const packed = await executeFeaturePackageCli([
      "pack",
      "--metadata",
      metadataPath,
      "--source",
      sourcePath,
      "--key-id",
      "cli-key",
      "--key-pkcs8",
      keyPath,
      "--out",
      outputPath,
    ]);
    assertEquals(packed.exitCode, 0);
    assert(packed.stdout.includes('"written":true'));
    assert(!packed.stdout.includes(source));
    assert(!packed.stdout.includes(encodeBase64Url(privateBytes)));

    const verified = await executeFeaturePackageCli([
      "verify",
      "--package",
      outputPath,
      "--policy",
      policyPath,
    ]);
    assertEquals(verified.exitCode, 0);
    assert(verified.stdout.includes('"valid":true'));
    assert(verified.stdout.includes('"featureId":"hub-panel-menu"'));
    assert(!verified.stdout.includes(source));
    assert(!verified.stdout.includes(encodeBase64Url(privateBytes)));

    const refused = await executeFeaturePackageCli([
      "pack",
      "--metadata",
      metadataPath,
      "--source",
      sourcePath,
      "--key-id",
      "cli-key",
      "--key-pkcs8",
      keyPath,
      "--out",
      outputPath,
    ]);
    assertEquals(refused.exitCode, 1);
    assert(refused.stderr.includes("output-exists"));
    assert(!refused.stderr.includes(encodeBase64Url(privateBytes)));

    await Deno.writeTextFile(sourcePath, "x".repeat(131_073));
    const oversized = await executeFeaturePackageCli([
      "pack",
      "--metadata",
      metadataPath,
      "--source",
      sourcePath,
      "--key-id",
      "cli-key",
      "--key-pkcs8",
      keyPath,
      "--out",
      `${root}/too-large.json`,
    ]);
    assertEquals(oversized.exitCode, 1);
    assert(oversized.stderr.includes("input-too-large"));

    await Deno.writeFile(keyPath, new Uint8Array([1, 2, 3]));
    await Deno.writeTextFile(`${root}/small-source.js`, "small source");
    const badKey = await executeFeaturePackageCli([
      "pack",
      "--metadata",
      metadataPath,
      "--source",
      `${root}/small-source.js`,
      "--key-id",
      "cli-key",
      "--key-pkcs8",
      keyPath,
      "--out",
      `${root}/bad-key.json`,
    ]);
    assertEquals(badKey.exitCode, 1);
    assert(badKey.stderr.includes("invalid-key"));
    assert(!badKey.stderr.includes(encodeBase64Url(privateBytes)));
  } finally {
    privateBytes?.fill(0);
    await Deno.remove(root, { recursive: true });
  }
});

Deno.test("feature package CLI rejects duplicate, unknown, positional and oversized inputs", async () => {
  const duplicate = await executeFeaturePackageCli([
    "verify",
    "--package",
    "one",
    "--package",
    "two",
    "--policy",
    "policy",
  ]);
  assertEquals(duplicate.exitCode, 1);
  assert(duplicate.stderr.includes("invalid-arguments"));

  const unknown = await executeFeaturePackageCli([
    "verify",
    "--package",
    "one",
    "--policy",
    "two",
    "--unknown",
    "three",
  ]);
  assertEquals(unknown.exitCode, 1);
  assert(unknown.stderr.includes("invalid-arguments"));

  const missing = await executeFeaturePackageCli([
    "verify",
    "--package",
    "one",
  ]);
  assertEquals(missing.exitCode, 1);
  assert(missing.stderr.includes("invalid-arguments"));

  const positional = await executeFeaturePackageCli([
    "verify",
    "--package",
    "one",
    "--policy",
    "two",
    "three",
  ]);
  assertEquals(positional.exitCode, 1);
  assert(positional.stderr.includes("invalid-arguments"));

  const root = await Deno.makeTempDir({
    prefix: "feature-package-cli-bounds-",
  });
  try {
    const packagePath = `${root}/package.json`;
    const policyPath = `${root}/policy.json`;
    await Deno.writeFile(packagePath, new Uint8Array(1_048_577));
    await Deno.writeTextFile(policyPath, "{}");
    const result = await executeFeaturePackageCli([
      "verify",
      "--package",
      packagePath,
      "--policy",
      policyPath,
    ]);
    assertEquals(result.exitCode, 1);
    assert(result.stderr.includes("input-too-large"));
  } finally {
    await Deno.remove(root, { recursive: true });
  }
});

Deno.test("feature package CLI bounds metadata, policy and PKCS#8 file reads", async () => {
  const root = await Deno.makeTempDir({
    prefix: "feature-package-cli-file-limits-",
  });
  try {
    const metadataPath = `${root}/metadata.json`;
    const sourcePath = `${root}/source.js`;
    const keyPath = `${root}/key.pk8`;
    const outputPath = `${root}/package.json`;
    const packagePath = `${root}/input-package.json`;
    const policyPath = `${root}/policy.json`;
    const packArgs = [
      "pack",
      "--metadata",
      metadataPath,
      "--source",
      sourcePath,
      "--key-id",
      "test-key",
      "--key-pkcs8",
      keyPath,
      "--out",
      outputPath,
    ];
    await Deno.writeTextFile(sourcePath, "x");
    await Deno.writeFile(keyPath, new Uint8Array());
    await Deno.writeFile(metadataPath, new Uint8Array(8_193));
    const oversizedMetadata = await executeFeaturePackageCli(packArgs);
    assertEquals(oversizedMetadata.exitCode, 1);
    assert(oversizedMetadata.stderr.includes("input-too-large"));

    await Deno.writeTextFile(
      metadataPath,
      JSON.stringify({
        featureId: "hub-panel-menu",
        version: "v1",
        releaseSequence: 1,
        compatibility: {
          hostSchema: 2,
          hostBuildId: "host-1",
          runtimeBuildIds: ["runtime-1"],
          capabilities: [],
        },
      }),
    );
    await Deno.writeFile(keyPath, new Uint8Array(1_025));
    const oversizedKey = await executeFeaturePackageCli(packArgs);
    assertEquals(oversizedKey.exitCode, 1);
    assert(oversizedKey.stderr.includes("input-too-large"));

    await Deno.writeFile(packagePath, new Uint8Array([1]));
    await Deno.writeFile(policyPath, new Uint8Array(16_385));
    const oversizedPolicy = await executeFeaturePackageCli([
      "verify",
      "--package",
      packagePath,
      "--policy",
      policyPath,
    ]);
    assertEquals(oversizedPolicy.exitCode, 1);
    assert(oversizedPolicy.stderr.includes("input-too-large"));
  } finally {
    await Deno.remove(root, { recursive: true });
  }
});
