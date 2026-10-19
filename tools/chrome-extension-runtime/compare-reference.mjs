#!/usr/bin/env node
// SPDX-License-Identifier: MPL-2.0
// A self-authored fixture comparison. This is not a real-extension compatibility score.
import { spawnSync } from "node:child_process";
import { ChromePipe } from "./chrome-cdp-pipe.mjs";
import { createHash } from "node:crypto";
import {
  mkdirSync,
  mkdtempSync,
  readdirSync,
  readFileSync,
  writeFileSync,
} from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const tools = dirname(fileURLToPath(import.meta.url));
const root = resolve(tools, "../..");
const option = (name, fallback) => {
  const index = process.argv.indexOf(name);
  return index < 0 ? fallback : resolve(process.argv[index + 1]);
};
const chrome = option(
  "--chrome",
  join(root, "_dist/chrome-reference/extracted/opt/google/chrome/chrome"),
);
const floorp = option("--floorp", join(root, "_dist/bin/floorp/floorp"));
const baseline = JSON.parse(
  readFileSync(join(tools, "chrome-reference.json"), "utf8"),
);
const outputParent = option(
  "--output",
  join(root, "_dist/chrome-runtime-tests"),
);
mkdirSync(outputParent, { recursive: true });
const output = mkdtempSync(join(outputParent, "comparison-"));
const fixture = join(tools, "comparison-fixture");
const files = Object.fromEntries(
  readdirSync(fixture).sort().map(
    (name) => [name, readFileSync(join(fixture, name), "utf8")],
  ),
);
const manifest = JSON.parse(files["manifest.json"]);
const id = [
  ...createHash("sha256").update(Buffer.from(manifest.key, "base64")).digest()
    .subarray(0, 16),
]
  .flatMap((byte) => [byte >> 4, byte & 15]).map((value) =>
    String.fromCharCode(97 + value)
  ).join("");
const report = {
  passed: false,
  fixture: {
    name: manifest.name,
    version: manifest.version,
    originalId: id,
    sha256: createHash("sha256").update(JSON.stringify(files)).digest("hex"),
  },
  scope:
    "One self-authored API fixture; no Store extensions or compatibility-rate claim.",
  output,
  reference: baseline,
  lockedRuntimeSourceCommit:
    JSON.parse(readFileSync(join(root, "floorp-runtime.lock.json"), "utf8"))
      .source.commit,
};

async function runChrome() {
  const browser = new ChromePipe(chrome, output);
  let extensionId;
  try {
    report.chromeVersion = await browser.call("Browser.getVersion");
    report.chromeBinarySha256 = createHash("sha256").update(
      readFileSync(chrome),
    ).digest("hex");
    if (
      report.chromeVersion.product !== `Chrome/${baseline.browserVersion}` ||
      report.chromeBinarySha256 !== baseline.binary.sha256
    ) {
      throw new Error(
        "Chrome differs from the frozen official 155.0.8059.84 Linux reference",
      );
    }
    ({ id: extensionId } = await browser.call("Extensions.loadUnpacked", {
      path: fixture,
    }));
    if (extensionId !== id) {
      throw new Error("Chrome ID differs from the fixture key");
    }
    const { targetId } = await browser.call("Target.createTarget", {
      url: `chrome-extension://${id}/probe.html`,
    });
    const { sessionId } = await browser.call("Target.attachToTarget", {
      targetId,
      flatten: true,
    });
    const deadline = Date.now() + 25000;
    while (Date.now() < deadline) {
      const evaluated = await browser.call("Runtime.evaluate", {
        expression: "globalThis.floorpReferenceReport ?? null",
        returnByValue: true,
      }, sessionId);
      if (evaluated.exceptionDetails) {
        throw new Error(JSON.stringify(evaluated.exceptionDetails));
      }
      const value = evaluated.result.value;
      if (value) return value;
      await new Promise((resolve) => setTimeout(resolve, 50));
    }
    throw new Error("Chrome fixture timed out");
  } finally {
    if (extensionId) {
      try {
        await browser.call("Extensions.uninstall", { id: extensionId });
      } catch { /* Closing process cleans its private profile. */ }
    }
    await browser.close();
  }
}

function normalized(value) {
  const tab = value.contextsBefore.find((context) =>
    context.contextType === "TAB"
  );
  const offscreen = value.offscreenContext;
  return {
    runtimeId: value.runtimeId,
    resource: value.resource,
    selfMessage: value.selfMessage,
    selfPort: value.selfPort,
    contextTypes: value.contextsBefore.map((context) => context.contextType)
      .sort(),
    tabHasDocumentId: typeof tab?.documentId === "string",
    offscreen: {
      exists: value.offscreenExists,
      type: offscreen.contextType,
      hasDocumentId: typeof offscreen.documentId === "string",
      frameId: offscreen.frameId,
      tabId: offscreen.tabId,
      windowId: offscreen.windowId,
      contextIdFilterCount: value.contextIdFilterCount,
      documentIdFilterCount: value.documentIdFilterCount,
      message: value.offscreenMessage,
      existsAfterClose: value.offscreenExistsAfterClose,
      contextsAfterClose: value.contextsAfterClose,
      replacementDocumentIdChanged: value.replacementDocumentIdChanged,
    },
  };
}

try {
  report.chrome = await runChrome();
  writeFileSync(
    join(output, "chrome-result.json"),
    JSON.stringify(report.chrome, null, 2) + "\n",
  );
  if (!report.chrome.passed) {
    throw new Error(`Chrome fixture failed: ${report.chrome.error}`);
  }
  const suite = join(output, "floorp-comparison.mts");
  const floorpOutput = join(output, "floorp-result.json");
  writeFileSync(
    suite,
    `import {runComparison} from ${
      JSON.stringify(join(tools, "comparison-floorp.mjs"))
    };\nexport async function runAllTests(){await runComparison(${
      JSON.stringify(files)
    },${JSON.stringify(id)},${JSON.stringify(floorpOutput)});}\n`,
  );
  const tested = spawnSync("python3", [
    join(tools, "run-browser-tests.py"),
    "--browser",
    floorp,
    "--test",
    suite,
    "--output",
    output,
    "--timeout",
    "45",
  ], { cwd: root, encoding: "utf8", timeout: 65000 });
  writeFileSync(
    join(output, "floorp-runner.log"),
    (tested.stdout || "") + (tested.stderr || ""),
  );
  if (tested.error) throw tested.error;
  if (tested.status !== 0) {
    throw new Error(`Floorp runner failed: ${tested.stdout}\n${tested.stderr}`);
  }
  report.floorp = JSON.parse(readFileSync(floorpOutput, "utf8"));
  if (!report.floorp.passed) {
    throw new Error(`Floorp fixture failed: ${report.floorp.error}`);
  }
  report.normalized = {
    chrome: normalized(report.chrome),
    floorp: normalized(report.floorp),
  };
  report.matches = JSON.stringify(report.normalized.chrome) ===
    JSON.stringify(report.normalized.floorp);
  const floorpUsesNativeWorker =
    typeof report.floorp.transformedBackground?.service_worker === "string";
  report.expectedDifferences = {
    extensionURLScheme: {
      chrome: report.chrome.urlScheme,
      floorp: report.floorp.urlScheme,
    },
    backgroundExecution: {
      chrome: report.chrome.background,
      floorp: report.floorp.background,
    },
    floorpTransformedBackground: report.floorp.transformedBackground,
    explanation: floorpUsesNativeWorker
      ? "Both fixtures use an MV3 Service Worker; this records only the authored API contract exercised here."
      : "Chrome uses an MV3 Service Worker. This Floorp artifact uses the converter's classic event-page fallback.",
  };
  if (!report.matches) throw new Error("Normalized fixture results differ");
  if (
    report.chrome.background.hasDocument !== false ||
    report.floorp.background.hasDocument !== !floorpUsesNativeWorker
  ) {
    throw new Error(
      "The actual background global does not match its installed manifest backend",
    );
  }
  report.passed = true;
} catch (error) {
  report.error = String(error);
  report.stack = error.stack;
} finally {
  writeFileSync(
    join(output, "report.json"),
    JSON.stringify(report, null, 2) + "\n",
  );
  console.log(
    JSON.stringify(
      {
        passed: report.passed,
        error: report.error,
        chromeVersion: report.chromeVersion?.product,
        report: join(output, "report.json"),
      },
      null,
      2,
    ),
  );
}
if (!report.passed) process.exitCode = 1;
