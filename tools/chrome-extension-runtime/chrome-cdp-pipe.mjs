// SPDX-License-Identifier: MPL-2.0
import { spawn } from "node:child_process";
import { writeFileSync } from "node:fs";
import { join } from "node:path";
export class ChromePipe {
  constructor(chrome, output, extraArguments = [], { headed = false } = {}) {
    this.output = output;
    this.next = 0;
    this.pending = new Map();
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
    this.process.stdio[4].setEncoding("utf8");
    this.process.stdio[3].on("error", (error) => this.fail(error));
    this.process.stderr.on("data", (chunk) => {
      this.stderr += chunk;
    });
    this.process.stdio[4].on("data", (chunk) => {
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
    this.process.on("error", (error) => this.fail(error));
    this.process.on(
      "exit",
      (code) => this.fail(new Error(`Chrome exited: ${code}`)),
    );
  }
  fail(error) {
    for (const pending of this.pending.values()) pending.reject(error);
    this.pending.clear();
  }
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
