// SPDX-License-Identifier: MPL-2.0
// Explicit live run of the Linux container QA target (KAN-144): preflight,
// image (built only when missing and --build-image is given), container,
// stage, launch + probe + screenshot, and container removal always. Exits 1
// unless every step passed and cleanup removed the container.
// Run:
//   deno run --allow-read --allow-write --allow-run=ssh \
//     tools/integration/fdh-qa-linux-live.ts --config <target.json> \
//     --task-id <id> --payload <floorp-linux.tar.xz> --out <dir> [--build-image]
import { encodeHex } from "@std/encoding/hex";
import { decodeBase64 } from "@std/encoding/base64";
import * as path from "@std/path";
import {
  createLinuxTransport,
  LinuxQaError,
  LinuxQaTarget,
  parseLinuxQaTargetConfig,
} from "../src/fdh_qa_linux.ts";
import type { LinuxPayloadKind } from "../src/fdh_qa_linux_types.ts";
import { validateTaskId } from "../src/fdh_task.ts";

function arg(name: string): string {
  const index = Deno.args.indexOf(`--${name}`);
  const value = index >= 0 ? Deno.args[index + 1] : undefined;
  if (value === undefined || value.startsWith("--")) {
    throw new Error(`--${name} is required`);
  }
  return value;
}

const config = parseLinuxQaTargetConfig(
  JSON.parse(await Deno.readTextFile(arg("config"))),
);
const taskId = validateTaskId(arg("task-id"));
const payload = arg("payload");
const out = arg("out");
const buildImage = Deno.args.includes("--build-image");
const kind: LinuxPayloadKind | null = payload.endsWith(".tar.xz")
  ? "tar.xz"
  : payload.endsWith(".tar.bz2")
  ? "tar.bz2"
  : payload.endsWith(".tar.gz")
  ? "tar.gz"
  : null;
if (kind === null) {
  throw new Error("--payload must end in .tar.xz, .tar.bz2 or .tar.gz");
}
await Deno.mkdir(out, { recursive: true });

const sha256 = encodeHex(
  await crypto.subtle.digest("SHA-256", await Deno.readFile(payload)),
);
const target = await LinuxQaTarget.create(config, createLinuxTransport(config));
const report: Record<string, unknown> = {
  taskId,
  image: target.image,
  container: {
    memoryMb: config.memoryMb,
    cpus: config.cpus,
    network: config.network,
  },
  payload: { name: path.basename(payload), kind, sha256 },
  startedAt: new Date().toISOString(),
};
const step = async (name: string, fn: () => Promise<unknown>) => {
  try {
    report[name] = await fn();
    return true;
  } catch (error) {
    report[name] = error instanceof LinuxQaError
      ? { failed: true, ...error.result }
      : { failed: true, error: String(error) };
    return false;
  }
};

let acquireAttempted = false;
let passed = false;
try {
  if (!await step("preflight", () => target.preflight())) {
    throw new Error("preflight failed");
  }
  let preflight = report.preflight as { ready: boolean; blockers: string[] };
  if (preflight.blockers.includes("image-missing") && buildImage) {
    if (!await step("imageBuild", () => target.ensureImage())) {
      throw new Error("image build failed");
    }
    if (!await step("preflightAfterImage", () => target.preflight())) {
      throw new Error("preflight failed");
    }
    preflight = report.preflightAfterImage as typeof preflight;
  }
  if (!preflight.ready) {
    throw new Error(`target not ready: ${preflight.blockers.join(", ")}`);
  }
  acquireAttempted = true;
  passed = await step("acquire", () => target.acquire(taskId)) &&
    await step(
      "stage",
      async () =>
        target.stage(taskId, kind, (await Deno.open(payload)).readable, sha256),
    ) &&
    await step("launch", () => target.launch(taskId)) &&
    await step("probe", () =>
      target.evaluate(
        taskId,
        "const w = Services.wm.getMostRecentWindow('navigator:browser');" +
          " return { app: Services.appinfo.name, version: Services.appinfo.version," +
          " os: Services.appinfo.OS, windows: [...Services.wm.getEnumerator('navigator:browser')].length," +
          " url: w?.gBrowser?.currentURI?.spec ?? null," +
          " contentSandboxLevel: Services.prefs.getIntPref('security.sandbox.content.level', -1) };",
      )) &&
    await step("screenshot", async () => {
      const png = await target.screenshot(taskId);
      const file = path.join(out, `${taskId}-linux.png`);
      await Deno.writeFile(file, decodeBase64(png));
      const digest = encodeHex(
        await crypto.subtle.digest("SHA-256", await Deno.readFile(file)),
      );
      return { file: path.basename(file), sha256: digest };
    });
} catch (error) {
  report.refused = String(error);
} finally {
  if (acquireAttempted) {
    const cleaned = await step("cleanup", () => target.cleanup(taskId));
    const cleanup = report.cleanup as { clean?: boolean };
    passed = passed && cleaned && cleanup.clean === true;
  }
  await step("after", () => target.preflight());
  report.passed = passed;
  report.finishedAt = new Date().toISOString();
  const file = path.join(out, `${taskId}-report.json`);
  await Deno.writeTextFile(file, JSON.stringify(report, null, 2) + "\n");
  console.log(JSON.stringify(report, null, 2));
}
Deno.exit(passed ? 0 : 1);
