// SPDX-License-Identifier: MPL-2.0
import { spawn } from "node:child_process";
import { createHash } from "node:crypto";
import { readFileSync, writeFileSync } from "node:fs";
import { join } from "node:path";

/** @param {string} binary */
export function assertFrozenChromeBinary(binary) {
  const reference = /** @type {{binary: {sha256: string}}} */ (JSON.parse(
    readFileSync(new URL("./chrome-reference.json", import.meta.url), "utf8"),
  ));
  const digest = createHash("sha256").update(readFileSync(binary)).digest(
    "hex",
  );
  if (digest !== reference.binary.sha256) {
    throw Error("Frozen Chrome binary hash mismatch");
  }
}

export class ChromePipe {
  /**
   * @param {string} chrome
   * @param {string} output
   * @param {string[]} [extraArguments]
   * @param {{headed?: boolean}} [options]
   */
  constructor(chrome, output, extraArguments = [], { headed = false } = {}) {
    this.output = output;
    this.next = 0;
    /** @type {Map<number, import("./cdp-types.ts").PendingCDPCall>} */
    this.pending = new Map();
    /** @type {import("./cdp-types.ts").CDPEvent[] | undefined} */
    this.events = undefined;
    this.closed = false;
    this.buffer = "";
    this.stderr = "";
    this.process = spawn(chrome, [
      ...(headed ? [] : ["--headless"]),
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
      ...extraArguments,
      "about:blank",
    ], { stdio: ["ignore", "ignore", "pipe", "pipe", "pipe"] });
    const input = this.process.stdio[3];
    const outputPipe = this.process.stdio[4];
    const stderr = this.process.stderr;
    if (
      !input || !("write" in input) || !outputPipe ||
      !("setEncoding" in outputPipe) || !stderr
    ) {
      this.process.kill();
      throw new Error("Chrome debugging pipes were not created");
    }
    this.input = /** @type {import("node:stream").Writable} */ (input);
    outputPipe.setEncoding("utf8");
    this.input.on("error", (error) => this.fail(error));
    stderr.on("data", (chunk) => {
      this.stderr += chunk;
    });
    outputPipe.on("data", (chunk) => {
      this.buffer += chunk;
      let index;
      while ((index = this.buffer.indexOf("\0")) >= 0) {
        const text = this.buffer.slice(0, index);
        this.buffer = this.buffer.slice(index + 1);
        if (!text) continue;
        const message =
          /** @type {import("./cdp-types.ts").CDPMessage} */ (JSON.parse(text));
        const pending = message.id === undefined
          ? undefined
          : this.pending.get(message.id);
        if (pending) {
          this.pending.delete(/** @type {number} */ (message.id));
          if (message.error) {
            pending.reject(new Error(JSON.stringify(message.error)));
          } else pending.resolve(message.result);
        } else if (message.method && this.events) {
          this.events.push({
            method: message.method,
            params: message.params,
            sessionId: message.sessionId,
          });
          if (this.events.length > 2000) this.events.shift();
        }
      }
    });
    this.process.on("error", (error) => this.fail(error));
    this.process.on(
      "exit",
      (code) => this.fail(new Error(`Chrome exited: ${code}`)),
    );
  }
  /** @param {Error} error */
  fail(error) {
    for (const pending of this.pending.values()) pending.reject(error);
    this.pending.clear();
  }
  /**
   * @template {import("./cdp-types.ts").CDPMethod} Method
   * @param {Method} method
   * @param {Record<string, unknown>} [params]
   * @param {string} [sessionId]
   * @returns {Promise<import("./cdp-types.ts").CDPResult<Method>>}
   */
  call(method, params = {}, sessionId) {
    if (this.process.exitCode !== null || this.process.killed) {
      return Promise.reject(new Error("Chrome is no longer running"));
    }
    const id = ++this.next;
    return new Promise((resolve, reject) => {
      const timer = setTimeout(() => {
        this.pending.delete(id);
        reject(new Error(`CDP ${method} timed out`));
      }, 8000);
      this.pending.set(id, {
        resolve(value) {
          clearTimeout(timer);
          resolve(
            /** @type {import("./cdp-types.ts").CDPResult<Method>} */ (value),
          );
        },
        reject(error) {
          clearTimeout(timer);
          reject(error);
        },
      });
      this.input.write(
        JSON.stringify({
          id,
          method,
          params,
          ...(sessionId ? { sessionId } : {}),
        }) + "\0",
      );
    });
  }
  async close() {
    if (this.closed) return;
    this.closed = true;
    try {
      await this.call("Browser.close");
    } catch { /* Process may already have exited. */ }
    this.process.kill();
    writeFileSync(join(this.output, "chrome.log"), this.stderr);
  }
}
