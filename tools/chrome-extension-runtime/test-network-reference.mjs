// SPDX-License-Identifier: MPL-2.0
// Authored DNR vectors; records real Chrome155 results without a corpus-score claim.
import { spawn } from "node:child_process";
import { createHash } from "node:crypto";
import { mkdirSync, mkdtempSync, writeFileSync, readFileSync } from "node:fs";
import { join, resolve } from "node:path";
const root = resolve(".");
const chrome = join(
  root,
  "_dist/chrome-reference/extracted/opt/google/chrome/chrome"
);
const output = mkdtempSync(
  join(root, "_dist/chrome-runtime-tests/network-reference-")
);
const fixture = join(output, "fixture");
mkdirSync(fixture);
writeFileSync(
  join(fixture, "manifest.json"),
  JSON.stringify({
    manifest_version: 3,
    name: "Floorp authored DNR reference",
    version: "1",
    permissions: ["declarativeNetRequest", "declarativeNetRequestFeedback"],
    host_permissions: ["<all_urls>"],
  })
);
writeFileSync(
  join(fixture, "probe.html"),
  "<!doctype html><title>DNR probe</title>"
);
class ChromePipe {
  constructor() {
    this.next = 0;
    this.pending = new Map();
    this.buffer = "";
    this.stderr = "";
    this.process = spawn(
      chrome,
      [
        "--headless",
        "--disable-gpu",
        "--disable-background-networking",
        "--disable-component-update",
        "--disable-sync",
        "--disable-default-apps",
        "--no-first-run",
        "--no-default-browser-check",
        "--remote-debugging-pipe",
        "--enable-unsafe-extension-debugging",
        `--user-data-dir=${join(output, "chrome-profile")}`,
        "about:blank",
      ],
      { stdio: ["ignore", "ignore", "pipe", "pipe", "pipe"] }
    );
    this.process.stderr.on("data", chunk => {
      this.stderr += chunk;
    });
    this.process.stdio[4].on("data", chunk => {
      this.buffer += chunk;
      let index;
      while ((index = this.buffer.indexOf("\0")) >= 0) {
        const text = this.buffer.slice(0, index);
        this.buffer = this.buffer.slice(index + 1);
        if (!text) continue;
        const message = JSON.parse(text);
        const pending = this.pending.get(message.id);
        if (pending) {
          this.pending.delete(message.id);
          if (message.error) {
            pending.reject(new Error(JSON.stringify(message.error)));
          } else pending.resolve(message.result);
        }
      }
    });
    this.process.on("error", error => this.fail(error));
    this.process.on("exit", code =>
      this.fail(new Error(`Chrome exited: ${code}`))
    );
  }
  fail(error) {
    for (const pending of this.pending.values()) pending.reject(error);
    this.pending.clear();
  }
  call(method, params = {}, sessionId) {
    const id = ++this.next;
    return new Promise((resolve, reject) => {
      const timer = setTimeout(() => {
        this.pending.delete(id);
        reject(new Error(`CDP ${method} timed out`));
      }, 8000);
      this.pending.set(id, {
        resolve(value) {
          clearTimeout(timer);
          resolve(value);
        },
        reject(error) {
          clearTimeout(timer);
          reject(error);
        },
      });
      this.process.stdio[3].write(
        JSON.stringify({
          id,
          method,
          params,
          ...(sessionId ? { sessionId } : {}),
        }) + "\0"
      );
    });
  }
  async close() {
    try {
      await this.call("Browser.close");
    } catch {
      /* Process may already have exited. */
    }
    this.process.kill();
    writeFileSync(join(output, "chrome.log"), this.stderr);
  }
}

const browser = new ChromePipe();
try {
  const version = await browser.call("Browser.getVersion");
  const baseline = JSON.parse(
    readFileSync(
      join(root, "tools/chrome-extension-runtime/chrome-reference.json"),
      "utf8"
    )
  );
  if (
    version.product !== `Chrome/${baseline.browserVersion}` ||
    createHash("sha256").update(readFileSync(chrome)).digest("hex") !==
      baseline.binary.sha256
  )
    throw Error("Chrome differs from frozen verified reference");
  const { id } = await browser.call("Extensions.loadUnpacked", {
    path: fixture,
  });
  const { targetId } = await browser.call("Target.createTarget", {
    url: `chrome-extension://${id}/probe.html`,
  });
  const { sessionId } = await browser.call("Target.attachToTarget", {
    targetId,
    flatten: true,
  });
  await browser.call("Runtime.enable", {}, sessionId);
  const source = String(async () => {
    const d = chrome.declarativeNetRequest;
    const patterns = [
      "",
      "é",
      "(?u)abc",
      "abc",
      "^https?://([^/]+)/",
      "(?i)HELLO",
      "(?s)a.b",
      "(?U)a+",
      "(?P<foo>abc)",
      "\\Q[a].*\\E",
      "\\123",
      "\\1",
      "a(?=b)",
      "[a&&b]",
      "[a--b]",
      "[[:digit:]]+",
      "\\C",
      "a{1000}",
      "a{1001}",
      "(a+)+$",
      "\\u0061",
      "\\p{Greek}",
      "a".repeat(2000),
      "(?:a|b){500}",
    ];
    const regex = [];
    let ruleId = 0;
    for (const pattern of patterns) {
      ++ruleId;
      const supported = await d.isRegexSupported({
        regex: pattern,
        requireCapturing: true,
      });
      let matches = [];
      if (supported.isSupported && pattern && !/[^\x00-\x7f]/.test(pattern)) {
        await d.updateSessionRules({
          removeRuleIds: (await d.getSessionRules()).map(rule => rule.id),
          addRules: [
            {
              id: ruleId,
              action: { type: "block" },
              condition: {
                regexFilter: pattern,
                isUrlFilterCaseSensitive: true,
                resourceTypes: ["xmlhttprequest"],
              },
            },
          ],
        });
        for (const path of [
          "abc",
          "HELLO",
          "123",
          "S",
          "ab",
          "&",
          "aaab",
          "[a].*",
        ]) {
          const result = await d.testMatchOutcome({
            url: "http://example.test/" + path,
            type: "xmlhttprequest",
          });
          if (result.matchedRules.length) matches.push(path);
        }
      }
      regex.push({
        pattern,
        supported,
        withoutCapturing: await d.isRegexSupported({ regex: pattern }),
        rules: await d.getSessionRules(),
        matches,
      });
    }
    const constants = {};
    for (const key of [
      "MAX_NUMBER_OF_DYNAMIC_RULES",
      "MAX_NUMBER_OF_UNSAFE_DYNAMIC_RULES",
      "MAX_NUMBER_OF_SESSION_RULES",
      "MAX_NUMBER_OF_UNSAFE_SESSION_RULES",
      "MAX_NUMBER_OF_ENABLED_STATIC_RULESETS",
      "GETMATCHEDRULES_QUOTA_INTERVAL",
      "MAX_GETMATCHEDRULES_CALLS_PER_INTERVAL",
    ])
      constants[key] = d[key];
    return { regex, constants };
  });
  const result = await browser.call(
    "Runtime.evaluate",
    { expression: `(${source})()`, awaitPromise: true, returnByValue: true },
    sessionId
  );
  const report = {
    schemaVersion: 1,
    scope:
      "Authored DNR API/regex vectors; not a Store-extension compatibility score",
    version,
    ...result.result.value,
    exception: result.exceptionDetails,
  };
  writeFileSync(join(output, "report.json"), JSON.stringify(report, null, 2));
  console.log(join(output, "report.json"));
  console.log(
    JSON.stringify({
      patterns: report.regex?.length,
      constants: report.constants,
      exception: report.exception,
    })
  );
} finally {
  await browser.close();
}
