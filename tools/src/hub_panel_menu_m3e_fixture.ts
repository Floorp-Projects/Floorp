// SPDX-License-Identifier: MPL-2.0

import { join } from "@std/path";
import { signFeaturePackage } from "../../browser-features/modules/modules/feature-host/package/FeaturePackageVerifier.ts";
import type {
  FeaturePackageAuthoringMetadata,
  FeaturePackagePolicy,
} from "../../browser-features/modules/modules/feature-host/package/types.ts";
import { HUB_PANEL_MENU_FIXTURES } from "../../browser-features/modules/modules/feature-host/HubPanelMenuFixtures.sys.mts";
import { startM3dPackageHttpsFixture } from "./m3d_package_https_fixture.ts";
import type { M3dPackageHttpsFixture } from "./m3d_package_https_fixture.types.ts";
import type {
  HubPanelMenuM3eFixtureBundle,
  HubPanelMenuM3eSignedPackage,
} from "./hub_panel_menu_m3e_acceptance.types.ts";
import { runHubPanelMenuM3eOwnedCommand } from "./hub_panel_menu_m3e_process.ts";

const KEY_ID = "hub-panel-menu-m3e-ephemeral";
const FEATURE_ID = "hub-panel-menu";

function base64Url(bytes: Uint8Array): string {
  let binary = "";
  for (const byte of bytes) binary += String.fromCharCode(byte);
  return btoa(binary).replace(/\+/g, "-").replace(/\//g, "_")
    .replace(/=+$/g, "");
}

async function sha256(bytes: Uint8Array): Promise<string> {
  const copy = bytes.slice();
  const hash = new Uint8Array(await crypto.subtle.digest("SHA-256", copy));
  return Array.from(hash, (byte) => byte.toString(16).padStart(2, "0"))
    .join("");
}

export async function createHubPanelMenuM3eFixtureBundle(
  binding: Readonly<{
    hostSchema: number;
    hostBuildId: string;
    runtimeBuildId: string;
    capabilities: readonly string[];
  }>,
): Promise<HubPanelMenuM3eFixtureBundle> {
  if (!binding.capabilities.includes("app-menu")) {
    throw new Error("core policy binding does not grant app-menu");
  }
  const keyPair = await crypto.subtle.generateKey(
    { name: "Ed25519" },
    false,
    ["sign", "verify"],
  ) as CryptoKeyPair;
  const publicKey = new Uint8Array(
    await crypto.subtle.exportKey("raw", keyPair.publicKey),
  );
  const policy: FeaturePackagePolicy = {
    featureId: FEATURE_ID,
    hostSchema: binding.hostSchema,
    hostBuildId: binding.hostBuildId,
    runtimeBuildId: binding.runtimeBuildId,
    allowedCapabilities: ["app-menu"],
    minimumReleaseSequence: 1,
    trustedKeys: [{ keyId: KEY_ID, publicKey: base64Url(publicKey) }],
  };
  const fixtures = [
    { fixture: HUB_PANEL_MENU_FIXTURES["hub-menu-v1"], sequence: 1 },
    { fixture: HUB_PANEL_MENU_FIXTURES["hub-menu-v2"], sequence: 2 },
    { fixture: HUB_PANEL_MENU_FIXTURES["hub-menu-broken"], sequence: 3 },
  ] as const;
  const packages: Record<string, HubPanelMenuM3eSignedPackage> = {};
  for (const { fixture, sequence } of fixtures) {
    const metadata: FeaturePackageAuthoringMetadata = {
      featureId: FEATURE_ID,
      version: fixture.version,
      releaseSequence: sequence,
      compatibility: {
        hostSchema: binding.hostSchema,
        hostBuildId: binding.hostBuildId,
        runtimeBuildIds: [binding.runtimeBuildId],
        capabilities: ["app-menu"],
      },
    };
    const bytes = await signFeaturePackage(
      metadata,
      fixture.source,
      KEY_ID,
      keyPair.privateKey,
    );
    packages[fixture.version] = {
      version: fixture.version,
      releaseSequence: sequence,
      packageSha256: await sha256(bytes),
      byteLength: bytes.length,
      bytes,
    };
  }
  return { policy, packages };
}

export async function startHubPanelMenuM3eHttpsFixture(
  bundle: HubPanelMenuM3eFixtureBundle,
): Promise<M3dPackageHttpsFixture> {
  const packages = new Map(
    Object.values(bundle.packages).map((
      item,
    ) => [item.packageSha256, item.bytes]),
  );
  return await startM3dPackageHttpsFixture(packages);
}

export async function waitForHubPanelMenuM3eFixtureIdle(
  fixture: M3dPackageHttpsFixture,
  timeoutMs: number,
): Promise<ReturnType<M3dPackageHttpsFixture["snapshot"]>> {
  const deadline = performance.now() + timeoutMs;
  let snapshot = fixture.snapshot();
  while (snapshot.activeStreams !== 0 || snapshot.activeConnections !== 0) {
    if (performance.now() >= deadline) {
      throw new Error(
        "M3e HTTPS fixture did not become idle within its deadline",
      );
    }
    await new Promise<void>((resolve) => setTimeout(resolve, 20));
    snapshot = fixture.snapshot();
  }
  return snapshot;
}

export async function installHubPanelMenuM3ePublicCa(
  profilePath: string,
  certificatePem: string,
  runDirectory: string,
): Promise<void> {
  const certificatePath = join(runDirectory, "m3e-public-test-ca.pem");
  await Deno.writeTextFile(certificatePath, certificatePem, {
    createNew: true,
    mode: 0o600,
  });
  const commands = [
    {
      args: ["-N", "-d", `sql:${profilePath}`, "--empty-password"],
      stdout: "m3e-certutil-init.stdout.log",
      stderr: "m3e-certutil-init.stderr.log",
    },
    {
      args: [
        "-A",
        "-d",
        `sql:${profilePath}`,
        "-n",
        "M3e owned test CA",
        "-t",
        "C,,",
        "-i",
        certificatePath,
      ],
      stdout: "m3e-certutil-add.stdout.log",
      stderr: "m3e-certutil-add.stderr.log",
    },
  ];
  for (const item of commands) {
    const result = await runHubPanelMenuM3eOwnedCommand(
      "certutil",
      item.args,
      profilePath,
      join(runDirectory, item.stdout),
      join(runDirectory, item.stderr),
    );
    if (!result.success || !result.cleanupProven) {
      throw new Error("M3e public CA installation failed");
    }
  }
  const listing = await runHubPanelMenuM3eOwnedCommand(
    "certutil",
    ["-L", "-d", `sql:${profilePath}`],
    profilePath,
    join(runDirectory, "m3e-certutil-list.stdout.log"),
    join(runDirectory, "m3e-certutil-list.stderr.log"),
  );
  if (
    !listing.success || !listing.cleanupProven ||
    !/M3e owned test CA\s+C,,/.test(listing.stdout)
  ) throw new Error("M3e public CA trust was not verified");
}
