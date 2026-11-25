// SPDX-License-Identifier: MPL-2.0

import * as path from "@std/path";
import { PROJECT_ROOT } from "./defines.ts";
import type { MarionetteStage } from "./browser_connector.types.ts";

const MARIONETTE_STATE_FILE = path.join(
  PROJECT_ROOT,
  "_dist",
  "marionette-port.txt",
);

const SESSION_TIMEOUT_MS = 15_000;
const CONNECT_RETRY_DELAY_MS = 2_000;
const CONNECT_MAX_RETRIES = 3;

/**
 * Minimal Marionette protocol client for Firefox/Floorp.
 *
 * Protocol: length-prefixed JSON over TCP.
 *   Send:    [0, msgId, command, params]
 *   Receive: [1, msgId, error, result]
 */
export class MarionetteClient {
  #conn!: Deno.TcpConn;
  #msgId = 0;
  #byteBuffer = new Uint8Array(0);
  #encoder = new TextEncoder();
  #decoder = new TextDecoder();
  #aborted = false;

  private constructor(
    private readonly onStage?: (stage: MarionetteStage) => void,
    private readonly signal?: AbortSignal,
  ) {}

  #stage(stage: MarionetteStage): void {
    try {
      this.onStage?.(stage);
    } catch { /* Diagnostics cannot affect the connection. */ }
  }

  static async connect(
    port?: number,
    timeoutMs = SESSION_TIMEOUT_MS,
    onStage?: (stage: MarionetteStage) => void,
    signal?: AbortSignal,
  ): Promise<MarionetteClient> {
    signal?.throwIfAborted();
    if (!Number.isFinite(timeoutMs) || timeoutMs <= 0) {
      throw new Error("Invalid Marionette timeout");
    }
    const actualPort = port ?? readPort();
    const deadline = Date.now() + timeoutMs;

    for (let attempt = 1; attempt <= CONNECT_MAX_RETRIES; attempt++) {
      try {
        return await MarionetteClient.#tryConnect(
          actualPort,
          deadline,
          onStage,
          signal,
        );
      } catch (e: unknown) {
        signal?.throwIfAborted();
        const msg = e instanceof Error ? e.message : String(e);
        if (
          attempt < CONNECT_MAX_RETRIES &&
          Date.now() + CONNECT_RETRY_DELAY_MS < deadline
        ) {
          console.error(
            `[marionette] Attempt ${attempt} failed: ${msg}. Retrying in ${CONNECT_RETRY_DELAY_MS}ms...`,
          );
          await new Promise<void>((resolve, reject) => {
            const cleanup = () => {
              clearTimeout(timer);
              signal?.removeEventListener("abort", cancel);
            };
            const cancel = () => {
              cleanup();
              reject(signal?.reason);
            };
            const timer = setTimeout(() => {
              cleanup();
              resolve();
            }, CONNECT_RETRY_DELAY_MS);
            signal?.addEventListener("abort", cancel, { once: true });
            if (signal?.aborted) cancel();
          });
        } else {
          throw new Error(
            `Failed to connect to Marionette after ${CONNECT_MAX_RETRIES} attempts: ${msg}`,
          );
        }
      }
    }
    throw new Error("Unreachable");
  }

  static async #tryConnect(
    port: number,
    deadline: number,
    onStage?: (stage: MarionetteStage) => void,
    signal?: AbortSignal,
  ): Promise<MarionetteClient> {
    const client = new MarionetteClient(onStage, signal);
    try {
      return await client.#withDeadline(async () => {
        console.error(`[marionette] Connecting to port ${port}`);
        client.#stage("connect");
        client.#conn = await Deno.connect({
          hostname: "127.0.0.1",
          port,
        }).then((conn) => {
          if (client.#aborted) {
            conn.close();
            throw new Error("Marionette connection aborted");
          }
          return conn;
        });

        // Read handshake
        client.#stage("handshake");
        const handshake = await client.#readPacket();
        if (handshake?.marionetteProtocol !== 3) {
          client.#conn.close();
          throw new Error("Unexpected Marionette protocol");
        }
        console.error("[marionette] Connected (protocol v3)");

        // Create session with timeout
        let session: unknown;
        try {
          session = await client.sendWithTimeout(
            "WebDriver:NewSession",
            {},
            Math.max(1, deadline - Date.now()),
          );
        } catch {
          client.#conn.close();
          throw new Error(
            "NewSession timed out — browser may be busy or BiDi conflict",
          );
        }

        if (
          typeof session !== "object" || session === null ||
          Array.isArray(session)
        ) {
          throw new Error("Invalid Marionette NewSession result");
        }
        const result = session as Record<string, unknown>;
        if (
          typeof result.sessionId !== "string" || !result.sessionId.trim() ||
          typeof result.capabilities !== "object" ||
          result.capabilities === null ||
          Array.isArray(result.capabilities)
        ) {
          throw new Error("Invalid Marionette NewSession result");
        }
        console.error("[marionette] Session established");

        return client;
      }, Math.max(1, deadline - Date.now()));
    } catch (error) {
      client.abort();
      throw error;
    }
  }

  send(
    command: string,
    params: Record<string, unknown> = {},
    // deno-lint-ignore no-explicit-any
  ): Promise<any> {
    return this.sendWithTimeout(command, params, 30_000);
  }

  async sendWithTimeout(
    command: string,
    params: Record<string, unknown>,
    timeoutMs: number,
    // deno-lint-ignore no-explicit-any
  ): Promise<any> {
    this.#stage("command");
    const response = await this.#withDeadline(async () => {
      const id = this.#msgId++;
      const msg = JSON.stringify([0, id, command, params]);
      // Marionette uses byte-length prefix, not character-length
      const msgBytes = this.#encoder.encode(msg);
      const prefixBytes = this.#encoder.encode(`${msgBytes.length}:`);
      const packet = new Uint8Array(prefixBytes.length + msgBytes.length);
      packet.set(prefixBytes, 0);
      packet.set(msgBytes, prefixBytes.length);
      let written = 0;
      while (written < packet.length) {
        const count = await this.#conn.write(packet.subarray(written));
        if (count <= 0) throw new Error("Marionette write closed");
        written += count;
      }

      const resp = await this.#readPacket();
      // Response: [1, msgId, error, result]
      if (
        !Array.isArray(resp) || resp.length !== 4 || resp[0] !== 1 ||
        resp[1] !== id ||
        (resp[2] !== null &&
          (typeof resp[2] !== "object" || typeof resp[2]?.error !== "string" ||
            typeof resp[2]?.message !== "string"))
      ) {
        throw new Error("Invalid Marionette response");
      }
      return resp;
    }, timeoutMs);
    if (response[2]) {
      const err = response[2];
      throw new Error(
        `Marionette error (${err.error}): ${
          err.message ?? JSON.stringify(err)
        }`,
        {
          cause: {
            name: String(err.error),
            message: String(err.message ?? ""),
          },
        },
      );
    }
    return response[3];
  }

  async setContext(context: "chrome" | "content"): Promise<void> {
    await this.send("Marionette:SetContext", { value: context });
  }

  // deno-lint-ignore no-explicit-any
  async executeScript(script: string, args: unknown[] = []): Promise<any> {
    const result = await this.send("WebDriver:ExecuteScript", {
      script,
      args,
    });
    return result?.value;
  }

  async takeScreenshot(opts?: {
    full?: boolean;
    element?: string;
  }): Promise<Uint8Array> {
    const params: Record<string, unknown> = {
      full: opts?.full ?? true,
      hash: false,
    };
    if (opts?.element) {
      params.id = opts.element;
    }
    const result = await this.send("WebDriver:TakeScreenshot", params);
    const b64 = result?.value;
    if (!b64) throw new Error("No screenshot data returned");
    return Uint8Array.from(atob(b64), (c) => c.charCodeAt(0));
  }

  async findElements(
    selector: string,
    strategy = "css selector",
  ): Promise<string[]> {
    const result = await this.send("WebDriver:FindElements", {
      using: strategy,
      value: selector,
    });
    return (result ?? []).map(
      (el: Record<string, string>) =>
        el["element-6066-11e4-a52e-4f735466cecf"] ?? el.ELEMENT,
    );
  }

  async getElementAttribute(
    elementId: string,
    name: string,
  ): Promise<string | null> {
    const result = await this.send("WebDriver:GetElementAttribute", {
      id: elementId,
      name,
    });
    return result?.value ?? null;
  }

  async getElementText(elementId: string): Promise<string> {
    const result = await this.send("WebDriver:GetElementText", {
      id: elementId,
    });
    return result?.value ?? "";
  }

  async navigate(url: string): Promise<void> {
    await this.send("WebDriver:Navigate", { url });
  }

  async getTitle(): Promise<string> {
    const result = await this.send("WebDriver:GetTitle", {});
    return result?.value ?? "";
  }

  async getUrl(): Promise<string> {
    const result = await this.send("WebDriver:GetCurrentUrl", {});
    return result?.value ?? "";
  }

  async close(): Promise<void> {
    try {
      await this.sendWithTimeout("WebDriver:DeleteSession", {}, 5_000);
    } catch {
      // ignore
    }
    try {
      this.#conn.close();
    } catch {
      // ignore
    }
  }

  abort(): void {
    this.#aborted = true;
    try {
      this.#conn.close();
    } catch {
      // ignore
    }
  }

  async #withDeadline<T>(
    operation: () => Promise<T>,
    timeoutMs: number,
  ): Promise<T> {
    let timer: ReturnType<typeof setTimeout> | undefined;
    const cancelled = Promise.withResolvers<never>();
    const cancel = () => {
      this.abort();
      cancelled.reject(this.signal?.reason);
    };
    this.signal?.addEventListener("abort", cancel, { once: true });
    try {
      this.signal?.throwIfAborted();
      if (this.#aborted || !Number.isFinite(timeoutMs) || timeoutMs <= 0) {
        throw new Error("Marionette connection unavailable");
      }
      return await Promise.race([
        cancelled.promise,
        operation(),
        new Promise<never>((_, reject) => {
          timer = setTimeout(
            () => {
              this.#stage("timeout");
              reject(new Error("Marionette operation timed out"));
            },
            timeoutMs,
          );
        }),
      ]);
    } catch (error) {
      this.abort();
      throw error;
    } finally {
      clearTimeout(timer);
      this.signal?.removeEventListener("abort", cancel);
    }
  }

  // deno-lint-ignore no-explicit-any
  async #readPacket(): Promise<any> {
    // Marionette protocol: length-prefixed by BYTE count, not character count.
    while (true) {
      const colonIdx = this.#byteBuffer.indexOf(0x3a); // ':'
      if (colonIdx > 0) {
        const lenStr = this.#decoder.decode(
          this.#byteBuffer.subarray(0, colonIdx),
        );
        const len = parseInt(lenStr, 10);
        if (!/^\d{1,8}$/.test(lenStr) || len > 16 * 1024 * 1024) {
          throw new Error("Invalid Marionette frame length");
        }
        const jsonStart = colonIdx + 1;
        if (!isNaN(len) && this.#byteBuffer.length >= jsonStart + len) {
          const jsonBytes = this.#byteBuffer.subarray(
            jsonStart,
            jsonStart + len,
          );
          const jsonStr = this.#decoder.decode(jsonBytes);
          this.#byteBuffer = this.#byteBuffer.slice(jsonStart + len);
          return JSON.parse(jsonStr);
        }
      }

      if (colonIdx === 0 || (colonIdx < 0 && this.#byteBuffer.length > 8)) {
        throw new Error("Invalid Marionette frame prefix");
      }
      const buf = new Uint8Array(65536);
      const n = await this.#conn.read(buf);
      if (!n) throw new Error("Marionette connection closed");
      const newBuf = new Uint8Array(this.#byteBuffer.length + n);
      newBuf.set(this.#byteBuffer);
      newBuf.set(buf.subarray(0, n), this.#byteBuffer.length);
      this.#byteBuffer = newBuf;
    }
  }
}

function readPort(): number {
  try {
    const content = Deno.readTextFileSync(MARIONETTE_STATE_FILE).trim();
    const port = parseInt(content, 10);
    if (isNaN(port)) throw new Error("Invalid port");
    return port;
  } catch {
    throw new Error(
      `Marionette port not found at ${MARIONETTE_STATE_FILE}\n` +
        `Is the browser running? Start it with: deno task dev-tool start`,
    );
  }
}

export async function withBrowser<T>(
  fn: (client: MarionetteClient) => Promise<T>,
  onStage?: (stage: MarionetteStage) => void,
): Promise<T> {
  const client = await MarionetteClient.connect(undefined, undefined, onStage);
  try {
    return await fn(client);
  } finally {
    await client.close();
  }
}
