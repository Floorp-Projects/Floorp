// SPDX-License-Identifier: MPL-2.0

import type { TcpDial } from "./types.ts";

const MAX_PACKET = 1024 * 1024;

export async function connectLoopbackOnce(
  port: number,
  timeoutMilliseconds = 15_000,
  dial: TcpDial = Deno.connect,
): Promise<Deno.TcpConn> {
  if (
    !Number.isSafeInteger(timeoutMilliseconds) || timeoutMilliseconds < 1 ||
    timeoutMilliseconds > 15_000
  ) {
    throw new Error("Initial TCP deadline is outside its fixed bound");
  }
  const controller = new AbortController();
  const timeout = new Error("Original TCP connection timed out");
  const deadline = performance.now() + timeoutMilliseconds;
  let sealedFailure = false;
  let timer: ReturnType<typeof setTimeout> | undefined;
  const expired = new Promise<never>((_, reject) => {
    timer = setTimeout(() => {
      sealedFailure = true;
      controller.abort(timeout);
      reject(timeout);
    }, timeoutMilliseconds);
  });
  try {
    const connected = dial({
      hostname: "127.0.0.1",
      port,
      signal: controller.signal,
      autoSelectFamily: false,
    }).then((connection) => {
      if (sealedFailure || performance.now() >= deadline) {
        sealedFailure = true;
        controller.abort(timeout);
        try {
          connection.close();
        } catch { /* Native abort can already close it. */ }
        throw timeout;
      }
      return connection;
    });
    return await Promise.race([connected, expired]);
  } finally {
    clearTimeout(timer);
  }
}

export class SinglePassMarionette {
  #conn: Deno.TcpConn;
  #audit: (event: { direction: string; prefix: string; json: string }) => void;
  #buffer = new Uint8Array(0);
  #id = 0;
  #encoder = new TextEncoder();
  #decoder = new TextDecoder("utf-8", { fatal: true });

  private constructor(
    conn: Deno.TcpConn,
    audit: (event: { direction: string; prefix: string; json: string }) => void,
  ) {
    this.#conn = conn;
    this.#audit = audit;
  }

  static async connect(
    port: number,
    audit: (
      event: { direction: string; prefix: string; json: string },
    ) => void = () => {},
  ): Promise<SinglePassMarionette> {
    const conn = await connectLoopbackOnce(port);
    const client = new SinglePassMarionette(conn, audit);
    try {
      const hello = await client.#deadline(() => client.#read(), 15_000);
      if (
        typeof hello !== "object" || hello === null ||
        (hello as Record<string, unknown>).marionetteProtocol !== 3
      ) throw new Error("Original Marionette handshake rejected");
      const session = await client.command("WebDriver:NewSession", {}, 15_000);
      if (
        typeof session !== "object" || session === null ||
        typeof (session as Record<string, unknown>).sessionId !== "string" ||
        !(session as Record<string, unknown>).sessionId
      ) throw new Error("Original session identity missing");
      return client;
    } catch (error) {
      client.close();
      throw error;
    }
  }

  close(): void {
    try {
      this.#conn.close();
    } catch { /* Already closed by the deadline. */ }
  }

  async #deadline<T>(operation: () => Promise<T>, ms: number): Promise<T> {
    let timer: ReturnType<typeof setTimeout> | undefined;
    try {
      return await Promise.race([
        operation(),
        new Promise<never>((_, reject) => {
          timer = setTimeout(() => {
            this.close();
            reject(new Error("Original Marionette operation timed out"));
          }, ms);
        }),
      ]);
    } finally {
      clearTimeout(timer);
    }
  }

  async #read(): Promise<unknown> {
    while (true) {
      const colon = this.#buffer.indexOf(58);
      if (colon >= 0) {
        const prefix = this.#decoder.decode(this.#buffer.subarray(0, colon));
        if (!/^[1-9][0-9]{0,6}$/.test(prefix)) {
          throw new Error("Invalid packet length");
        }
        const size = Number(prefix);
        if (size > MAX_PACKET) {
          throw new Error("Marionette packet exceeds bound");
        }
        if (this.#buffer.length >= colon + 1 + size) {
          const json = this.#decoder.decode(
            this.#buffer.subarray(colon + 1, colon + 1 + size),
          );
          this.#audit({ direction: "receive", prefix, json });
          this.#buffer = this.#buffer.slice(colon + 1 + size);
          return JSON.parse(json);
        }
      } else if (this.#buffer.length > 7) {
        throw new Error("Unbounded packet prefix");
      }
      if (this.#buffer.length > MAX_PACKET + 8) {
        throw new Error("Unbounded packet buffer");
      }
      const chunk = new Uint8Array(65536);
      const count = await this.#conn.read(chunk);
      if (count === null) {
        throw new Error("Original Marionette connection ended");
      }
      const joined = new Uint8Array(this.#buffer.length + count);
      joined.set(this.#buffer);
      joined.set(chunk.subarray(0, count), this.#buffer.length);
      this.#buffer = joined;
    }
  }

  async command(
    name: string,
    params: Record<string, unknown>,
    ms = 30_000,
  ): Promise<unknown> {
    return await this.#deadline(async () => {
      const id = this.#id++;
      const json = this.#encoder.encode(JSON.stringify([0, id, name, params]));
      if (json.length > MAX_PACKET) {
        throw new Error("Command exceeds packet bound");
      }
      const prefix = this.#encoder.encode(`${json.length}:`);
      this.#audit({
        direction: "send",
        prefix: String(json.length),
        json: this.#decoder.decode(json),
      });
      const packet = new Uint8Array(prefix.length + json.length);
      packet.set(prefix);
      packet.set(json, prefix.length);
      let written = 0;
      while (written < packet.length) {
        written += await this.#conn.write(packet.subarray(written));
      }
      const response = await this.#read();
      if (
        !Array.isArray(response) || response.length !== 4 ||
        response[0] !== 1 || response[1] !== id
      ) {
        throw new Error("Original response message identity mismatch");
      }
      if (response[2] !== null) {
        throw new Error(
          `Original Marionette rejection: ${
            JSON.stringify(response[2]).slice(0, 1024)
          }`,
        );
      }
      return response[3];
    }, ms);
  }
}
