// SPDX-License-Identifier: MPL-2.0

import * as path from "@std/path";
import { PROJECT_ROOT } from "./defines.ts";

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
  #sessionId: string | null = null;

  private constructor() {}

  static async connect(port?: number): Promise<MarionetteClient> {
    const actualPort = port ?? readPort();

    for (let attempt = 1; attempt <= CONNECT_MAX_RETRIES; attempt++) {
      try {
        return await MarionetteClient.#tryConnect(actualPort);
      } catch (e: unknown) {
        const msg = e instanceof Error ? e.message : String(e);
        if (attempt < CONNECT_MAX_RETRIES) {
          console.error(
            `[marionette] Attempt ${attempt} failed: ${msg}. Retrying in ${CONNECT_RETRY_DELAY_MS}ms...`,
          );
          await new Promise((r) => setTimeout(r, CONNECT_RETRY_DELAY_MS));
        } else {
          throw new Error(
            `Failed to connect to Marionette after ${CONNECT_MAX_RETRIES} attempts: ${msg}`,
          );
        }
      }
    }
    throw new Error("Unreachable");
  }

  static async #tryConnect(port: number): Promise<MarionetteClient> {
    const client = new MarionetteClient();

    console.error(`[marionette] Connecting to port ${port}`);
    client.#conn = await Deno.connect({
      hostname: "127.0.0.1",
      port,
    });

    // Read handshake
    const handshake = await client.#readPacketWithTimeout(SESSION_TIMEOUT_MS);
    if (handshake?.marionetteProtocol !== 3) {
      client.#conn.close();
      throw new Error(
        `Unexpected Marionette protocol: ${JSON.stringify(handshake)}`,
      );
    }
    console.error("[marionette] Connected (protocol v3)");

    // Create session with timeout
    try {
      const session = await client.sendWithTimeout(
        "WebDriver:NewSession",
        {},
        SESSION_TIMEOUT_MS,
      );
      client.#sessionId = session?.sessionId;
      console.error(`[marionette] Session: ${client.#sessionId}`);
    } catch {
      client.#conn.close();
      throw new Error(
        "NewSession timed out — browser may be busy or BiDi conflict",
      );
    }

    return client;
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
    const id = this.#msgId++;
    const msg = JSON.stringify([0, id, command, params]);
    // Marionette uses byte-length prefix, not character-length
    const msgBytes = this.#encoder.encode(msg);
    const prefixBytes = this.#encoder.encode(`${msgBytes.length}:`);
    const packet = new Uint8Array(prefixBytes.length + msgBytes.length);
    packet.set(prefixBytes, 0);
    packet.set(msgBytes, prefixBytes.length);
    await this.#conn.write(packet);

    const resp = await this.#readPacketWithTimeout(timeoutMs);
    // Response: [1, msgId, error, result]
    if (!Array.isArray(resp) || resp[0] !== 1) {
      throw new Error(`Invalid response: ${JSON.stringify(resp)}`);
    }
    if (resp[2]) {
      const err = resp[2];
      throw new Error(
        `Marionette error (${err.error}): ${err.message ?? JSON.stringify(err)}`,
      );
    }
    return resp[3];
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

  // deno-lint-ignore no-explicit-any
  async #readPacketWithTimeout(timeoutMs: number): Promise<any> {
    const result = await Promise.race([
      this.#readPacket(),
      new Promise<never>((_, reject) =>
        setTimeout(
          () => reject(new Error(`Read timeout (${timeoutMs}ms)`)),
          timeoutMs,
        ),
      ),
    ]);
    return result;
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
): Promise<T> {
  const client = await MarionetteClient.connect();
  try {
    return await fn(client);
  } finally {
    await client.close();
  }
}
