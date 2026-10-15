// SPDX-License-Identifier: MPL-2.0

import * as path from "@std/path";
import { connectLoopbackOnce } from "../final-workspace-qa/marionette.ts";
import { PROJECT_ROOT } from "./defines.ts";
import type {
  BrowserOperationFailure,
  MarionetteBrowserConnectionOptions,
  MarionetteConnectionOptions,
  MarionetteScreenshotOptions,
} from "./browser_connector_types.ts";

const MARIONETTE_STATE_FILE = path.join(
  PROJECT_ROOT,
  "_dist",
  "marionette-port.txt",
);
const STARTUP_TIMEOUT_MS = 15_000;
const MAX_COMMAND_TIMEOUT_MS = 30_000;
const MAX_COMMAND_BYTES = 1024 * 1024;
// Includes base64 screenshots. This development transport issues no QA verdict.
const MAX_RESPONSE_BYTES = 16 * 1024 * 1024;
const MAX_PREFIX_DIGITS = String(MAX_RESPONSE_BYTES).length;

function record(value: unknown): value is Record<string, unknown> {
  return typeof value === "object" && value !== null && !Array.isArray(value);
}

function valueOf(result: unknown): unknown {
  if (!record(result) || !Object.hasOwn(result, "value")) {
    throw new Error("Marionette result is missing its value");
  }
  return result.value;
}

function timeoutWithin(value: number, maximum: number): void {
  if (!Number.isSafeInteger(value) || value < 1 || value > maximum) {
    throw new Error("Marionette deadline is outside its fixed bound");
  }
}

export function parseMarionettePort(value: unknown): number {
  if (typeof value === "string" && /^[1-9][0-9]{0,4}$/.test(value)) {
    value = Number(value);
  }
  if (
    typeof value !== "number" || !Number.isSafeInteger(value) || value < 1 ||
    value > 65535
  ) {
    throw new Error("Invalid Marionette port; expected an integer in 1..65535");
  }
  return value;
}

/** One session, ordered commands, bounded framing and no reconnect/replay. */
export class MarionetteClient {
  #conn: Deno.TcpConn;
  #msgId = 0;
  #encoder = new TextEncoder();
  #decoder = new TextDecoder("utf-8", { fatal: true });
  #sessionId: string | null = null;
  #closed = false;
  #busy = false;
  #inFlight?: Promise<unknown>;
  #closing?: Promise<void>;
  #closeFailed = false;
  #closeFailure: unknown;
  #now: () => number;

  private constructor(conn: Deno.TcpConn, now: () => number) {
    this.#conn = conn;
    this.#now = now;
  }

  static async connect(
    port?: number,
    options: MarionetteConnectionOptions = {},
  ): Promise<MarionetteClient> {
    const actualPort = parseMarionettePort(port ?? readPort());
    const startup = options.startupTimeoutMilliseconds ?? STARTUP_TIMEOUT_MS;
    timeoutWithin(startup, STARTUP_TIMEOUT_MS);
    const now = options.now ?? (() => performance.now());
    const deadline = now() + startup;
    const remaining = () => {
      const milliseconds = Math.ceil(deadline - now());
      if (milliseconds < 1) throw new Error("Marionette startup timed out");
      return Math.min(milliseconds, startup);
    };
    // Reuses only the once-only dial primitive, with no qualification authority.
    const connection = await connectLoopbackOnce(
      actualPort,
      remaining(),
      options.dial,
    );
    const client = new MarionetteClient(connection, now);
    try {
      const greeting = await client.#bounded(
        () => client.#readPacket(),
        remaining(),
      );
      if (
        !record(greeting) || greeting.marionetteProtocol !== 3 ||
        greeting.applicationType !== "gecko"
      ) {
        throw new Error("Unexpected Marionette greeting");
      }
      const session = await client.sendWithTimeout(
        "WebDriver:NewSession",
        {},
        remaining(),
      );
      if (
        !record(session) || typeof session.sessionId !== "string" ||
        session.sessionId.length < 1 || session.sessionId.length > 256
      ) {
        throw new Error(
          "Marionette NewSession returned no valid session identity",
        );
      }
      client.#sessionId = session.sessionId;
      return client;
    } catch (error) {
      client.#shutdown();
      throw error;
    }
  }

  send(
    command: string,
    params: Record<string, unknown> = {},
  ): Promise<unknown> {
    return this.sendWithTimeout(command, params, MAX_COMMAND_TIMEOUT_MS);
  }

  async sendWithTimeout(
    command: string,
    params: Record<string, unknown>,
    timeoutMs: number,
  ): Promise<unknown> {
    if (this.#closed) throw new Error("Marionette connection is closed");
    if (this.#busy) throw new Error("A Marionette command is already pending");
    timeoutWithin(timeoutMs, MAX_COMMAND_TIMEOUT_MS);
    if (
      typeof command !== "string" || command.length < 1 ||
      command.length > 128 || !record(params)
    ) {
      throw new Error("Invalid Marionette command declaration");
    }
    this.#busy = true;
    const operation = this.#bounded(async () => {
      if (!Number.isSafeInteger(this.#msgId)) {
        throw new Error("Marionette message identity exhausted");
      }
      const id = this.#msgId++;
      const json = this.#encoder.encode(
        JSON.stringify([0, id, command, params]),
      );
      if (json.length > MAX_COMMAND_BYTES) {
        throw new Error("Marionette command exceeds its byte bound");
      }
      const prefix = this.#encoder.encode(String(json.length) + ":");
      const packet = new Uint8Array(prefix.length + json.length);
      packet.set(prefix);
      packet.set(json, prefix.length);
      let written = 0;
      while (written < packet.length) {
        const count = await this.#conn.write(packet.subarray(written));
        if (
          !Number.isSafeInteger(count) || count < 1 ||
          count > packet.length - written
        ) {
          throw new Error("Marionette write made invalid progress");
        }
        written += count;
      }
      const response = await this.#readPacket();
      if (
        !Array.isArray(response) || response.length !== 4 ||
        response[0] !== 1 ||
        !Number.isSafeInteger(response[1]) || response[1] !== id
      ) {
        throw new Error("Marionette response message identity mismatch");
      }
      if (response[2] !== null) {
        const error: unknown = response[2];
        if (
          !record(error) || typeof error.error !== "string" ||
          error.error.length > 128 ||
          typeof error.message !== "string" || error.message.length > 8192
        ) {
          throw new Error("Invalid Marionette error response");
        }
        throw new Error(
          "Marionette error (" + error.error + "): " + error.message,
        );
      }
      return response[3] as unknown;
    }, timeoutMs);
    this.#inFlight = operation;
    try {
      return await operation;
    } finally {
      this.#busy = false;
      this.#inFlight = undefined;
    }
  }

  #shutdown(): void {
    if (this.#closed) return;
    this.#closed = true;
    this.#sessionId = null;
    try {
      this.#conn.close();
    } catch (error) {
      this.#closeFailed = true;
      this.#closeFailure = error;
    }
  }

  async #bounded<T>(
    operation: () => Promise<T>,
    milliseconds: number,
  ): Promise<T> {
    const deadline = this.#now() + milliseconds;
    let timer: ReturnType<typeof setTimeout> | undefined;
    const pending = Promise.resolve().then(operation);
    const expired = new Promise<never>((_, reject) => {
      timer = setTimeout(() => {
        reject(
          new Error("Marionette operation timed out (" + milliseconds + "ms)"),
        );
        this.#shutdown();
      }, milliseconds);
    });
    try {
      const value = await Promise.race([pending, expired]);
      if (this.#now() >= deadline) {
        throw new Error("Marionette operation completed after its deadline");
      }
      return value;
    } catch (error) {
      this.#shutdown();
      // Deno TCP close aborts pending I/O. Reap it before returning the first error.
      await pending.catch(() => {});
      throw error;
    } finally {
      clearTimeout(timer);
    }
  }

  async #readExactly(bytes: Uint8Array): Promise<void> {
    let read = 0;
    while (read < bytes.length) {
      const count = await this.#conn.read(bytes.subarray(read));
      if (
        count === null || !Number.isSafeInteger(count) || count < 1 ||
        count > bytes.length - read
      ) {
        throw new Error(
          "Marionette packet ended or made invalid read progress",
        );
      }
      read += count;
    }
  }

  async #readPacket(): Promise<unknown> {
    const digit = new Uint8Array(1);
    let prefix = "";
    while (true) {
      await this.#readExactly(digit);
      if (digit[0] === 58) break;
      if (
        digit[0] < 48 || digit[0] > 57 || prefix.length >= MAX_PREFIX_DIGITS ||
        (prefix.length === 0 && digit[0] === 48)
      ) {
        throw new Error("Invalid or unbounded Marionette length prefix");
      }
      prefix += String.fromCharCode(digit[0]);
    }
    const length = Number(prefix);
    if (
      !Number.isSafeInteger(length) || length < 1 || length > MAX_RESPONSE_BYTES
    ) {
      throw new Error("Marionette response exceeds its byte bound");
    }
    const body = new Uint8Array(length);
    await this.#readExactly(body);
    return JSON.parse(this.#decoder.decode(body)) as unknown;
  }

  async setContext(context: "chrome" | "content"): Promise<void> {
    await this.send("Marionette:SetContext", { value: context });
  }

  async executeScript<T = unknown>(
    script: string,
    args: unknown[] = [],
  ): Promise<T> {
    return valueOf(
      await this.send("WebDriver:ExecuteScript", { script, args }),
    ) as T;
  }

  async takeScreenshot(
    opts?: MarionetteScreenshotOptions,
  ): Promise<Uint8Array> {
    const params: Record<string, unknown> = {
      full: opts?.full ?? true,
      hash: false,
    };
    if (opts?.element) params.id = opts.element;
    const value = valueOf(await this.send("WebDriver:TakeScreenshot", params));
    if (typeof value !== "string" || value.length < 1) {
      throw new Error("No valid screenshot data returned");
    }
    return Uint8Array.from(atob(value), (character) => character.charCodeAt(0));
  }

  async findElements(
    selector: string,
    strategy = "css selector",
  ): Promise<string[]> {
    const result = await this.send("WebDriver:FindElements", {
      using: strategy,
      value: selector,
    });
    if (!Array.isArray(result)) {
      throw new Error("Invalid Marionette element list");
    }
    return result.map((element: unknown) => {
      if (!record(element)) {
        throw new Error("Invalid Marionette element reference");
      }
      const id = element["element-6066-11e4-a52e-4f735466cecf"] ??
        element.ELEMENT;
      if (typeof id !== "string" || id.length < 1) {
        throw new Error("Invalid Marionette element identity");
      }
      return id;
    });
  }

  async getElementAttribute(
    elementId: string,
    name: string,
  ): Promise<string | null> {
    const value = valueOf(
      await this.send("WebDriver:GetElementAttribute", { id: elementId, name }),
    );
    if (value !== null && typeof value !== "string") {
      throw new Error("Invalid Marionette attribute value");
    }
    return value;
  }

  async getElementText(elementId: string): Promise<string> {
    const value = valueOf(
      await this.send("WebDriver:GetElementText", { id: elementId }),
    );
    if (typeof value !== "string") {
      throw new Error("Invalid Marionette element text");
    }
    return value;
  }

  async navigate(url: string): Promise<void> {
    await this.send("WebDriver:Navigate", { url });
  }

  async getTitle(): Promise<string> {
    const value = valueOf(await this.send("WebDriver:GetTitle", {}));
    if (typeof value !== "string") throw new Error("Invalid Marionette title");
    return value;
  }

  async getUrl(): Promise<string> {
    const value = valueOf(await this.send("WebDriver:GetCurrentUrl", {}));
    if (typeof value !== "string") throw new Error("Invalid Marionette URL");
    return value;
  }

  close(): Promise<void> {
    return this.#closing ??= this.#close();
  }

  async #close(): Promise<void> {
    if (this.#busy) {
      this.#shutdown();
      await this.#inFlight?.catch(() => {});
      throw new Error("Marionette closed with a command pending");
    }
    try {
      if (!this.#closed && this.#sessionId) {
        await this.sendWithTimeout("WebDriver:DeleteSession", {}, 5_000);
      }
    } finally {
      this.#shutdown();
    }
    if (this.#closeFailed) throw this.#closeFailure;
  }
}

function readPort(): number {
  let content: string;
  try {
    content = Deno.readTextFileSync(MARIONETTE_STATE_FILE);
  } catch (error) {
    throw new Error("Marionette port not found at " + MARIONETTE_STATE_FILE, {
      cause: error,
    });
  }
  return parseMarionettePort(content.trim());
}

export async function withBrowser<T>(
  fn: (client: MarionetteClient) => Promise<T>,
  options: MarionetteBrowserConnectionOptions = {},
): Promise<T> {
  const client = await MarionetteClient.connect(options.port, options);
  let failure: BrowserOperationFailure = { failed: false };
  try {
    return await fn(client);
  } catch (error) {
    failure = { failed: true, error };
    throw error;
  } finally {
    await finishBrowserCleanup(client, failure);
  }
}

/** Shared by the real gesture callers: cleanup can never replace a first error. */
export async function finishBrowserCleanup(
  client: MarionetteClient,
  failure: BrowserOperationFailure,
  cleanupErrors: unknown[] = [],
): Promise<void> {
  try {
    await client.close();
  } catch (error) {
    cleanupErrors.push(error);
  }
  if (failure.failed) {
    if (cleanupErrors.length > 0) {
      try {
        console.error(
          "[marionette] Secondary cleanup failures:",
          cleanupErrors.length,
        );
      } catch {
        // A broken diagnostic sink must not replace the original exception.
      }
    }
    throw failure.error;
  }
  if (cleanupErrors.length > 0) {
    throw new AggregateError(cleanupErrors, "Browser session cleanup failed");
  }
}
