#!/usr/bin/env node
// SPDX-License-Identifier: MPL-2.0
import { createServer } from "node:http";
import { createHash } from "node:crypto";
import { spawn } from "node:child_process";
import {
  mkdirSync,
  mkdtempSync,
  readdirSync,
  readFileSync,
  writeFileSync,
} from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { ChromePipe } from "./chrome-cdp-pipe.mjs";
export async function runContractComparison(
  fixtureName = "debugger-fixture",
  outputPrefix = "debugger-comparison-",
) {
  const tools = dirname(fileURLToPath(import.meta.url)),
    root = resolve(tools, "../..");
  /** @param {string} name @param {string} fallback */
  const option = (name, fallback) => {
    const index = process.argv.indexOf(name);
    const argument = process.argv[index + 1];
    if (index >= 0 && !argument) throw Error("Missing value for " + name);
    return index < 0 ? fallback : resolve(argument ?? fallback);
  };
  const chrome = option(
    "--chrome",
    join(root, "_dist/chrome-reference/extracted/opt/google/chrome/chrome"),
  );
  const floorp = option("--floorp", join(root, "_dist/bin/floorp/floorp"));
  const outputParent = option(
    "--output",
    join(root, "_dist/chrome-runtime-tests"),
  );
  mkdirSync(outputParent, { recursive: true });
  const output = mkdtempSync(join(outputParent, outputPrefix));
  const baseline = JSON.parse(
    readFileSync(join(tools, "chrome-reference.json"), "utf8"),
  );
  /** @type {import("./debugger-reference-types.ts").DebuggerComparisonReport} */
  const report = {
    passed: false,
    scope: `One self-authored ${fixtureName} contract; no real-extension compatibility percentage.`,
    reference: baseline,
    output,
  };
  const html =
    '<!doctype html><title>Native CDP contract target</title><style>#item{padding:7px;border:3px solid;margin:5px}</style><div id="item">native</div><input id="editor"><input type="file" id="upload">';
  const server = createServer(async (request, response) => {
    if (request.url === "/debugger-echo") {
      /** @type {Uint8Array[]} */
      const chunks = [];
      for await (const chunk of request) chunks.push(chunk);
      response.writeHead(200, {
        "Content-Type": "application/json",
        "Cache-Control": "no-store",
      });
      response.end(
        JSON.stringify({
          userAgent: request.headers["user-agent"],
          body: Buffer.concat(chunks).toString("utf8"),
        }),
      );
    } else {
      response.writeHead(200, {
        "Content-Type": "text/html",
        ...((request.url ?? "").startsWith("/csp-target")
          ? {
              "Content-Security-Policy":
                "script-src 'none'; style-src 'none'; object-src 'none'",
            }
          : {}),
      });
      response.end(html);
    }
  });
  await new Promise((resolve) =>
    server.listen(0, "127.0.0.1", () => resolve(undefined)),
  );
  const address = server.address();
  if (!address || typeof address === "string")
    throw Error("Authored local debugger listener missing");
  const original = join(tools, fixtureName),
    fixture = join(output, "fixture");
  mkdirSync(fixture);
  const files = Object.fromEntries(
    readdirSync(original)
      .sort()
      .map((name) => [name, readFileSync(join(original, name), "utf8")]),
  );
  files["config.js"] = `globalThis.floorpDebuggerConfig=${JSON.stringify({
    url: `http://127.0.0.1:${address.port}/target`,
    upload: join(original, "upload.txt"),
    uploadSize: Buffer.byteLength(files["upload.txt"]),
    nativeOverrides: process.argv.includes("--native-overrides"),
  })};`;
  for (const [name, content] of Object.entries(files)) {
    writeFileSync(join(fixture, name), content);
  }
  const manifest = JSON.parse(files["manifest.json"]);
  const id = [
    ...createHash("sha256")
      .update(Buffer.from(manifest.key, "base64"))
      .digest()
      .subarray(0, 16),
  ]
    .flatMap((byte) => [byte >> 4, byte & 15])
    .map((n) => String.fromCharCode(97 + n))
    .join("");
  report.fixture = {
    name: manifest.name,
    version: manifest.version,
    id,
    sha256: createHash("sha256").update(JSON.stringify(files)).digest("hex"),
  };
  const browser = new ChromePipe(chrome, output);
  try {
    report.chromeVersion = await browser.call("Browser.getVersion");
    report.chromeBinarySha256 = createHash("sha256")
      .update(readFileSync(chrome))
      .digest("hex");
    if (
      report.chromeVersion.product !== `Chrome/${baseline.browserVersion}` ||
      report.chromeBinarySha256 !== baseline.binary.sha256
    )
      throw new Error("Chrome differs from frozen official reference");
    const extension = await browser.call("Extensions.loadUnpacked", {
      path: fixture,
    });
    if (extension.id !== id) throw new Error("Fixture key ID mismatch");
    const { targetId } = await browser.call("Target.createTarget", {
      url: `chrome-extension://${id}/probe.html`,
    });
    const { sessionId } = await browser.call("Target.attachToTarget", {
      targetId,
      flatten: true,
    });
    const deadline = Date.now() + 45000;
    while (Date.now() < deadline) {
      const value = await browser.call(
        "Runtime.evaluate",
        {
          expression: "globalThis.floorpReferenceReport??null",
          returnByValue: true,
        },
        sessionId,
      );
      if (value.result.value) {
        report.chrome =
          /** @type {import("./debugger-reference-types.ts").ContractProbeReport} */ (
            value.result.value
          );
        break;
      }
      await new Promise((resolve) => setTimeout(resolve, 30));
    }
    if (!report.chrome?.passed) {
      throw new Error(
        `Chrome debugger contract failed: ${JSON.stringify(report.chrome)}`,
      );
    }
    await browser.close();
    const floorpOutput = join(output, "floorp-result.json"),
      suite = join(output, "floorp-debugger.mts");
    writeFileSync(
      suite,
      `import {runComparison} from ${JSON.stringify(
        join(tools, "comparison-floorp.mjs"),
      )};\nexport async function runAllTests(){await runComparison(${JSON.stringify(
        files,
      )},${JSON.stringify(id)},${JSON.stringify(floorpOutput)});}\n`,
    );
    /** @type {import("./debugger-reference-types.ts").DebuggerRunnerResult} */
    const tested = await new Promise((resolve, reject) => {
      const child = spawn(
        "python3",
        [
          join(tools, "run-browser-tests.py"),
          "--browser",
          floorp,
          "--test",
          suite,
          "--output",
          output,
          "--timeout",
          "60",
        ],
        { cwd: root },
      );
      let stdout = "",
        stderr = "";
      child.stdout.on("data", (chunk) => (stdout += chunk));
      child.stderr.on("data", (chunk) => (stderr += chunk));
      child.on("error", reject);
      child.on("exit", (status) => resolve({ status, stdout, stderr }));
    });
    writeFileSync(
      join(output, "floorp-runner.log"),
      (tested.stdout || "") + (tested.stderr || ""),
    );
    if (tested.error) throw tested.error;
    if (tested.status !== 0) {
      throw new Error(
        `Floorp runner failed: ${tested.stdout} ${tested.stderr}`,
      );
    }
    report.floorp = JSON.parse(readFileSync(floorpOutput, "utf8"));
    if (!report.chrome || !report.floorp)
      throw Error("Actual debugger reports missing");
    report.matches =
      JSON.stringify(report.chrome.cases) ===
      JSON.stringify(report.floorp.cases);
    if (!report.matches) {
      throw new Error("Chrome and Floorp debugger contract cases differ");
    }
    report.passed = true;
  } catch (error) {
    report.error = String(error);
    report.stack = error instanceof Error ? error.stack : undefined;
  } finally {
    await browser.close();
    server.close();
    writeFileSync(
      join(output, "report.json"),
      JSON.stringify(report, null, 2) + "\n",
    );
    console.log(
      JSON.stringify(
        {
          passed: report.passed,
          error: report.error,
          report: join(output, "report.json"),
        },
        null,
        2,
      ),
    );
  }
  if (!report.passed) process.exitCode = 1;
  return report;
}
if (
  process.argv[1] &&
  resolve(process.argv[1]) === fileURLToPath(import.meta.url)
) {
  await runContractComparison();
}
