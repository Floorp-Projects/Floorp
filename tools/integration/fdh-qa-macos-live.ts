// SPDX-License-Identifier: MPL-2.0
// Explicit live run of the macOS QA target (KAN-144): preflight, lock, stage,
// launch + probe + screenshot when a GUI session exists, and cleanup always.
// Run:
//   deno run --allow-read --allow-write --allow-run=ssh \
//     tools/integration/fdh-qa-macos-live.ts --config <target.json> \
//     --task-id <id> --payload <Floorp.dmg|.tar.gz> --out <dir>
// The config holds host/guest names and key FILE NAMES on the Mac host only;
// no credential is read or written on this machine.
import { encodeHex } from "@std/encoding/hex";
import { decodeBase64 } from "@std/encoding/base64";
import * as path from "@std/path";
import {
  createSshTransport,
  MacosQaError,
  MacosQaTarget,
  parseMacosQaTargetConfig,
} from "../src/fdh_qa_macos.ts";
import type { MacosPayloadKind } from "../src/fdh_qa_macos_types.ts";
import { validateTaskId } from "../src/fdh_task.ts";

function arg(name: string): string {
  const index = Deno.args.indexOf(`--${name}`);
  const value = index >= 0 ? Deno.args[index + 1] : undefined;
  if (value === undefined || value.startsWith("--")) {
    throw new Error(`--${name} is required`);
  }
  return value;
}

const config = parseMacosQaTargetConfig(
  JSON.parse(await Deno.readTextFile(arg("config"))),
);
const taskId = validateTaskId(arg("task-id"));
const payload = arg("payload");
const out = arg("out");
const kind: MacosPayloadKind = payload.endsWith(".dmg") ? "dmg" : "tar.gz";
await Deno.mkdir(out, { recursive: true });

const sha256 = encodeHex(
  await crypto.subtle.digest("SHA-256", await Deno.readFile(payload)),
);
const target = new MacosQaTarget(config, createSshTransport(config));
const report: Record<string, unknown> = {
  taskId,
  payload: { name: path.basename(payload), kind, sha256 },
  startedAt: new Date().toISOString(),
};
const step = async (name: string, fn: () => Promise<unknown>) => {
  try {
    report[name] = await fn();
    return true;
  } catch (error) {
    report[name] = error instanceof MacosQaError
      ? { failed: true, ...error.result }
      : { failed: true, error: String(error) };
    return false;
  }
};

const preflight = await target.preflight();
report.preflight = preflight;
let locked = false;
try {
  if (!preflight.ready) {
    throw new Error(`target not ready: ${preflight.blockers.join(", ")}`);
  }
  locked = await step("lock", () => target.acquire(taskId));
  if (
    locked &&
    await step(
      "stage",
      async () =>
        target.stage(taskId, kind, (await Deno.open(payload)).readable, sha256),
    )
  ) {
    if (
      await step("launch", () => target.launch(taskId)) &&
      await step("probe", () =>
        target.evaluate(
          taskId,
          "const w = Services.wm.getMostRecentWindow('navigator:browser');" +
            " return { app: Services.appinfo.name, version: Services.appinfo.version," +
            " os: Services.appinfo.OS, windows: [...Services.wm.getEnumerator('navigator:browser')].length," +
            " url: w?.gBrowser?.currentURI?.spec ?? null };",
        ))
    ) {
      await step("screenshot", async () => {
        const png = await target.screenshot(taskId);
        const file = path.join(out, `${taskId}-macos.png`);
        await Deno.writeFile(file, decodeBase64(png));
        const digest = encodeHex(
          await crypto.subtle.digest("SHA-256", await Deno.readFile(file)),
        );
        return { file: path.basename(file), sha256: digest };
      });
    }
  }
} catch (error) {
  report.refused = String(error);
} finally {
  if (locked) await step("cleanup", () => target.cleanup(taskId));
  report.after = await target.preflight();
  report.finishedAt = new Date().toISOString();
  const file = path.join(out, `${taskId}-report.json`);
  await Deno.writeTextFile(file, JSON.stringify(report, null, 2) + "\n");
  console.log(JSON.stringify(report, null, 2));
}
