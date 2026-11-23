// SPDX-License-Identifier: MPL-2.0

import type {
  M3dPackageHttpsCleanupEvidence,
  M3dPackageHttpsFixture,
  M3dPackageHttpsMode,
  M3dPackageHttpsOriginKind,
  M3dPackageHttpsRequestObservation,
} from "./m3d_package_https_fixture.types.ts";
import type {
  Identity,
  ParsedRequest,
  RequestHandler,
  Stall,
} from "./m3d_package_https_fixture.internal.types.ts";

const MAX_HEADER_BYTES = 16 * 1024;
const MAX_OBSERVATIONS = 64;
const MAX_PACKAGE_BYTES = 1024 * 1024;
const READ_TIMEOUT_MS = 5_000;
const WRITE_TIMEOUT_MS = 5_000;
const CLEANUP_TIMEOUT_MS = 5_000;
const OPENSSL_TIMEOUT_MS = 15_000;
const MAX_ACTIVE_CONNECTIONS = 16;
const MAX_ACCEPT_FAILURES = 8;
const SEED_COOKIE_MARKER = "m3d_seed_cookie=seed-marker";
const ACQUISITION_COOKIE_MARKER = "m3d_acquisition_cookie=acquisition-marker";
const AUTH_MARKER = "Bearer m3d-fixture-marker";
const REFERRER_MARKER = "https://m3d-fixture.invalid/marker";
const PACKAGE_PATH_PATTERN =
  /^\/floorp-feature-packages\/v1\/([a-z][a-z0-9-]{0,47})\/([0-9a-f]{64})\.json$/;
const COOKIE_SEED_PATH = "/__m3d/cookie/seed";
const COOKIE_CONTROL_PATH = "/__m3d/cookie/control";
const REDIRECT_TARGET_PATH = "/__m3d/redirect-target";
const MODES = new Set<M3dPackageHttpsMode>([
  "valid-streaming",
  "redirect",
  "status-rejected",
  "content-type-rejected",
  "content-encoding-rejected",
  "oversized-declared",
  "oversized-streamed",
  "truncated-length",
  "wrong-package",
  "stalled-body",
  "read-failure",
  "equal-content-length",
  "conflicting-content-length",
  "cookie-seed",
  "cookie-control",
]);
const encoder = new TextEncoder();
const decoder = new TextDecoder("ascii", { fatal: true });

function fixedError(): Error {
  return new Error("M3d HTTPS fixture operation failed");
}

async function sha256(bytes: Uint8Array): Promise<string> {
  const input = bytes.slice();
  const result = new Uint8Array(await crypto.subtle.digest("SHA-256", input));
  return Array.from(result, (byte) => byte.toString(16).padStart(2, "0"))
    .join("");
}

function packageFeatureId(bytes: Uint8Array): string {
  try {
    const parsed: unknown = JSON.parse(
      new TextDecoder("utf-8", { fatal: true }).decode(bytes),
    );
    if (
      typeof parsed !== "object" || parsed === null || Array.isArray(parsed)
    ) {
      throw fixedError();
    }
    const manifest = Reflect.get(parsed, "manifest") as unknown;
    if (
      typeof manifest !== "object" || manifest === null ||
      Array.isArray(manifest)
    ) {
      throw fixedError();
    }
    const featureId = Reflect.get(manifest, "featureId") as unknown;
    if (
      typeof featureId !== "string" || featureId.length > 48 ||
      !/^[a-z][a-z0-9-]*$/.test(featureId)
    ) throw fixedError();
    return featureId;
  } catch {
    throw fixedError();
  }
}

function withTimeout<T>(promise: Promise<T>, milliseconds: number): Promise<T> {
  return new Promise<T>((resolve, reject) => {
    const timer = setTimeout(() => reject(fixedError()), milliseconds);
    promise.then(
      (value) => {
        clearTimeout(timer);
        resolve(value);
      },
      (error: unknown) => {
        clearTimeout(timer);
        reject(error);
      },
    );
  });
}

async function runOpenSsl(
  args: readonly string[],
  stdin?: string,
): Promise<Uint8Array> {
  const child = new Deno.Command("openssl", {
    args: [...args],
    stdin: stdin === undefined ? "null" : "piped",
    stdout: "piped",
    stderr: "piped",
  }).spawn();
  const deadline = performance.now() + OPENSSL_TIMEOUT_MS;
  const output = child.output().then((result) => result, () => null);
  let writer: WritableStreamDefaultWriter<Uint8Array> | null = null;
  let result: Deno.CommandOutput | null = null;
  const wait = <T>(promise: Promise<T>): Promise<T> => {
    const remaining = deadline - performance.now();
    if (remaining <= 0) throw fixedError();
    return withTimeout(promise, remaining);
  };
  try {
    if (stdin !== undefined) {
      if (!child.stdin) throw fixedError();
      writer = child.stdin.getWriter();
      void writer.closed.catch(() => undefined);
      await wait(writer.write(encoder.encode(stdin)));
      await wait(writer.close());
    }
    result = await wait(output);
    if (!result?.success) throw fixedError();
  } catch {
    killOpenSslChild(child);
    return await cleanupOpenSsl(writer, output);
  }
  if (writer && !releaseOpenSslWriter(writer)) throw fixedError();
  return result.stdout;
}

function killOpenSslChild(child: Deno.ChildProcess): void {
  try {
    child.kill("SIGKILL");
  } catch {
    return;
  }
}

function releaseOpenSslWriter(
  writer: WritableStreamDefaultWriter<Uint8Array>,
): boolean {
  try {
    writer.releaseLock();
    return true;
  } catch {
    return false;
  }
}

async function cleanupOpenSsl(
  writer: WritableStreamDefaultWriter<Uint8Array> | null,
  output: Promise<Deno.CommandOutput | null>,
): Promise<never> {
  if (writer) {
    await withTimeout(writer.abort(fixedError()), CLEANUP_TIMEOUT_MS).catch(
      () => undefined,
    );
  }
  await withTimeout(output, CLEANUP_TIMEOUT_MS).catch(() => undefined);
  if (writer) {
    await withTimeout(writer.closed, CLEANUP_TIMEOUT_MS).catch(() => undefined);
    releaseOpenSslWriter(writer);
  }
  throw fixedError();
}

async function assertLinuxOpenSsl3(): Promise<void> {
  if (Deno.build.os !== "linux") throw fixedError();
  const version = decoder.decode(await runOpenSsl(["version"]));
  if (!/^OpenSSL 3\./.test(version)) throw fixedError();
}

async function privateKeyPem(): Promise<string> {
  return decoder.decode(
    await runOpenSsl([
      "genpkey",
      "-algorithm",
      "RSA",
      "-pkeyopt",
      "rsa_keygen_bits:2048",
    ]),
  );
}

function identityFrom(
  keyPem: string,
  certPem: string,
): Identity {
  return { keyPem, certPem };
}

async function rootIdentity(): Promise<Identity> {
  const key = await privateKeyPem();
  const certificate = decoder.decode(
    await runOpenSsl([
      "req",
      "-new",
      "-x509",
      "-key",
      "/dev/stdin",
      "-subj",
      "/CN=M3d Ephemeral Loopback Root CA",
      "-days",
      "2",
      "-sha256",
      "-addext",
      "basicConstraints=critical,CA:TRUE,pathlen:0",
      "-addext",
      "keyUsage=critical,keyCertSign,cRLSign",
      "-addext",
      "subjectKeyIdentifier=hash",
      "-outform",
      "PEM",
    ], key),
  );
  const extensions = decoder.decode(
    await runOpenSsl([
      "x509",
      "-in",
      "/dev/stdin",
      "-noout",
      "-ext",
      "basicConstraints,keyUsage",
    ], certificate),
  );
  if (
    !extensions.includes("CA:TRUE") ||
    !extensions.includes("Certificate Sign") ||
    !extensions.includes("CRL Sign")
  ) throw fixedError();
  return identityFrom(key, certificate);
}

async function signedLeaf(
  ca: Identity,
  temporaryDirectory: string,
  serial: string,
): Promise<Identity> {
  const key = await privateKeyPem();
  const csr = await runOpenSsl([
    "req",
    "-new",
    "-key",
    "/dev/stdin",
    "-subj",
    "/CN=127.0.0.1",
    "-sha256",
  ], key);
  const caPath = `${temporaryDirectory}/root-public.pem`;
  const csrPath = `${temporaryDirectory}/leaf-${serial}.csr`;
  const extensionPath = `${temporaryDirectory}/leaf-${serial}.cnf`;
  const certificatePath = `${temporaryDirectory}/leaf-${serial}.pem`;
  await Deno.writeTextFile(caPath, ca.certPem, {
    createNew: true,
    mode: 0o600,
  });
  await Deno.writeFile(csrPath, csr, { createNew: true, mode: 0o600 });
  await Deno.writeTextFile(
    extensionPath,
    [
      "[server_cert]",
      "basicConstraints=critical,CA:FALSE",
      "keyUsage=critical,digitalSignature,keyEncipherment",
      "extendedKeyUsage=serverAuth",
      "subjectAltName=IP:127.0.0.1",
      "subjectKeyIdentifier=hash",
      "authorityKeyIdentifier=keyid,issuer",
      "",
    ].join("\n"),
    { createNew: true, mode: 0o600 },
  );
  const certificate = decoder.decode(
    await runOpenSsl([
      "x509",
      "-req",
      "-in",
      csrPath,
      "-CA",
      caPath,
      "-CAkey",
      "/dev/stdin",
      "-set_serial",
      `0x${serial}`,
      "-days",
      "2",
      "-sha256",
      "-extfile",
      extensionPath,
      "-extensions",
      "server_cert",
      "-outform",
      "PEM",
    ], ca.keyPem),
  );
  await Deno.writeTextFile(certificatePath, certificate, {
    createNew: true,
    mode: 0o600,
  });
  const verification = await runOpenSsl([
    "verify",
    "-CAfile",
    caPath,
    "-no-CApath",
    "-no-CAstore",
    "-verify_ip",
    "127.0.0.1",
    "-purpose",
    "sslserver",
    certificatePath,
  ]).catch(() => new Uint8Array());
  if (!decoder.decode(verification).endsWith(`${certificatePath}: OK\n`)) {
    throw fixedError();
  }
  const extensions = decoder.decode(
    await runOpenSsl([
      "x509",
      "-in",
      certificatePath,
      "-noout",
      "-ext",
      "subjectAltName,basicConstraints,keyUsage,extendedKeyUsage",
    ]),
  );
  if (
    !extensions.includes("IP Address:127.0.0.1") ||
    !extensions.includes("CA:FALSE") ||
    !extensions.includes("Digital Signature") ||
    !extensions.includes("Key Encipherment") ||
    !extensions.includes("TLS Web Server Authentication")
  ) throw fixedError();
  return identityFrom(key, certificate);
}

async function selfSignedLeaf(): Promise<Identity> {
  const key = await privateKeyPem();
  const certificate = decoder.decode(
    await runOpenSsl([
      "req",
      "-new",
      "-x509",
      "-key",
      "/dev/stdin",
      "-subj",
      "/CN=127.0.0.1",
      "-days",
      "2",
      "-sha256",
      "-addext",
      "subjectAltName=IP:127.0.0.1",
      "-addext",
      "basicConstraints=critical,CA:FALSE",
      "-addext",
      "keyUsage=critical,digitalSignature,keyEncipherment",
      "-addext",
      "extendedKeyUsage=serverAuth",
      "-outform",
      "PEM",
    ], key),
  );
  return identityFrom(key, certificate);
}

function findHeaderEnd(bytes: Uint8Array): number {
  for (let i = 3; i < bytes.length; i++) {
    if (
      bytes[i - 3] === 13 && bytes[i - 2] === 10 && bytes[i - 1] === 13 &&
      bytes[i] === 10
    ) return i + 1;
  }
  return -1;
}

async function readRequest(connection: Deno.TlsConn): Promise<ParsedRequest> {
  let bytes = new Uint8Array();
  const chunk = new Uint8Array(2048);
  const deadline = performance.now() + READ_TIMEOUT_MS;
  while (bytes.length <= MAX_HEADER_BYTES) {
    const remaining = deadline - performance.now();
    if (remaining <= 0) throw fixedError();
    const count = await withTimeout(connection.read(chunk), remaining);
    if (count === null) throw fixedError();
    const next = new Uint8Array(bytes.length + count);
    next.set(bytes);
    next.set(chunk.subarray(0, count), bytes.length);
    bytes = next;
    const end = findHeaderEnd(bytes);
    if (end >= 0) {
      if (end > MAX_HEADER_BYTES) throw fixedError();
      const text = decoder.decode(bytes.subarray(0, end - 4));
      const lines = text.split("\r\n");
      const requestLine = lines.shift()?.split(" ");
      if (
        !requestLine || requestLine.length !== 3 ||
        !/^[!#$%&'*+.^_`|~0-9A-Za-z-]+$/.test(requestLine[0]) ||
        !["HTTP/1.0", "HTTP/1.1"].includes(requestLine[2])
      ) throw fixedError();
      const headers = new Map<string, string[]>();
      for (const line of lines) {
        const colon = line.indexOf(":");
        if (colon <= 0) throw fixedError();
        const name = line.slice(0, colon).trim().toLowerCase();
        const value = line.slice(colon + 1).trim();
        if (!/^[!#$%&'*+.^_`|~0-9a-z-]+$/.test(name)) throw fixedError();
        headers.set(name, [...(headers.get(name) ?? []), value]);
      }
      return { method: requestLine[0], target: requestLine[1], headers };
    }
  }
  throw fixedError();
}

async function writeAll(
  connection: Deno.TlsConn,
  bytes: Uint8Array,
  deadline = performance.now() + WRITE_TIMEOUT_MS,
): Promise<void> {
  let offset = 0;
  while (offset < bytes.length) {
    const remaining = deadline - performance.now();
    if (remaining <= 0) throw fixedError();
    const written = await withTimeout(
      connection.write(bytes.subarray(offset)),
      remaining,
    );
    if (written <= 0) throw fixedError();
    offset += written;
  }
}

function responseHead(
  status: number,
  headers: readonly string[],
): Uint8Array {
  const reasons: Readonly<Record<number, string>> = {
    200: "OK",
    204: "No Content",
    302: "Found",
    400: "Bad Request",
    404: "Not Found",
    405: "Method Not Allowed",
    503: "Service Unavailable",
  };
  const reason = reasons[status] ?? "Internal Server Error";
  return encoder.encode(
    `HTTP/1.1 ${status} ${reason}\r\n${headers.join("\r\n")}\r\n\r\n`,
  );
}

function deferred(): { promise: Promise<void>; resolve(): void } {
  let resolve!: () => void;
  const promise = new Promise<void>((done) => resolve = done);
  return { promise, resolve };
}

function headerFlags(headers: Map<string, string[]>): Omit<
  M3dPackageHttpsRequestObservation,
  "origin" | "mode"
> {
  const cookies = headers.get("cookie") ?? [];
  const authorization = headers.get("authorization") ?? [];
  const referers = headers.get("referer") ?? [];
  return {
    cookiePresent: cookies.length > 0,
    seedCookieMarkerMatch: cookies.some((value) =>
      value.includes(SEED_COOKIE_MARKER)
    ),
    acquisitionCookieMarkerMatch: cookies.some((value) =>
      value.includes(ACQUISITION_COOKIE_MARKER)
    ),
    authorizationPresent: authorization.length > 0,
    authorizationMarkerMatch: authorization.includes(AUTH_MARKER),
    referrerPresent: referers.length > 0,
    referrerMarkerMatch: referers.includes(REFERRER_MARKER),
  };
}

class OwnedTlsServer {
  readonly port: number;
  readonly #listener: Deno.TlsListener;
  readonly #connections = new Set<Deno.TlsConn>();
  readonly #closedConnections = new Set<Deno.TlsConn>();
  readonly #tasks = new Set<Promise<void>>();
  readonly #onRequest: (
    origin: M3dPackageHttpsOriginKind,
    request: ParsedRequest,
    connection: Deno.TlsConn,
  ) => Promise<void>;
  #closing = false;
  #cleanupErrors = 0;
  #listenerClosed = false;
  #acceptLoopSettled = false;
  #acceptFailures = 0;
  readonly #acceptTask: Promise<void>;
  readonly kind: M3dPackageHttpsOriginKind;

  constructor(
    origin: M3dPackageHttpsOriginKind,
    identity: Identity,
    onRequest: RequestHandler,
  ) {
    this.kind = origin;
    this.#listener = Deno.listenTls({
      hostname: "127.0.0.1",
      port: 0,
      cert: identity.certPem,
      key: identity.keyPem,
    });
    this.port = this.#listener.addr.port;
    this.#onRequest = (kind, request, connection) =>
      onRequest(kind, request, connection);
    this.#acceptTask = this.#accept(origin).catch(() => {
      this.#cleanupErrors++;
    });
  }

  get activeConnections(): number {
    return this.#connections.size;
  }

  get cleanupErrorCount(): number {
    return this.#cleanupErrors;
  }

  get listenerClosed(): boolean {
    return this.#listenerClosed;
  }

  get acceptLoopSettled(): boolean {
    return this.#acceptLoopSettled;
  }

  #closeConnection(connection: Deno.TlsConn): void {
    if (this.#closedConnections.has(connection)) return;
    this.#closedConnections.add(connection);
    try {
      connection.close();
    } catch {
      this.#cleanupErrors++;
    }
  }

  async #accept(origin: M3dPackageHttpsOriginKind): Promise<void> {
    while (!this.#closing) {
      let connection: Deno.TlsConn;
      try {
        connection = await this.#listener.accept();
      } catch {
        if (this.#closing) return;
        this.#acceptFailures++;
        if (this.#acceptFailures >= MAX_ACCEPT_FAILURES) {
          this.#cleanupErrors++;
          return;
        }
        continue;
      }
      if (this.#connections.size >= MAX_ACTIVE_CONNECTIONS) {
        this.#closeConnection(connection);
        this.#closedConnections.delete(connection);
        continue;
      }
      if (this.#closing) {
        this.#closeConnection(connection);
        this.#closedConnections.delete(connection);
        return;
      }
      this.#connections.add(connection);
      const task = (async () => {
        try {
          const request = await readRequest(connection);
          await this.#onRequest(origin, request, connection);
        } catch {
          return;
        } finally {
          this.#connections.delete(connection);
          this.#closeConnection(connection);
          this.#closedConnections.delete(connection);
        }
      })();
      this.#tasks.add(task);
      void task.then(
        () => this.#tasks.delete(task),
        () => {
          this.#tasks.delete(task);
          this.#cleanupErrors++;
        },
      );
    }
  }

  async close(): Promise<boolean> {
    this.#closing = true;
    const deadline = performance.now() + CLEANUP_TIMEOUT_MS;
    try {
      this.#listener.close();
      this.#listenerClosed = true;
    } catch {
      this.#cleanupErrors++;
    }
    try {
      const remaining = deadline - performance.now();
      if (remaining <= 0) throw fixedError();
      await withTimeout(this.#acceptTask, remaining);
      this.#acceptLoopSettled = true;
    } catch {
      this.#cleanupErrors++;
    }
    for (const connection of this.#connections) {
      this.#closeConnection(connection);
    }
    try {
      const remaining = deadline - performance.now();
      if (remaining <= 0) throw fixedError();
      const settled = await withTimeout(
        Promise.allSettled([...this.#tasks]),
        remaining,
      );
      return this.#connections.size === 0 && this.#cleanupErrors === 0 &&
        settled.every((result) => result.status === "fulfilled") &&
        this.#listenerClosed && this.#acceptLoopSettled;
    } catch {
      this.#cleanupErrors++;
      return false;
    }
  }
}

export async function startM3dPackageHttpsFixture(
  input: ReadonlyMap<string, Uint8Array>,
): Promise<M3dPackageHttpsFixture> {
  if (input.size === 0 || input.size > 8) throw fixedError();
  const packages = new Map<string, Uint8Array>();
  const packageFeatureIds = new Map<string, string>();
  for (const [hash, bytes] of input) {
    if (
      !/^[0-9a-f]{64}$/.test(hash) || !(bytes instanceof Uint8Array) ||
      bytes.length === 0 || bytes.length > MAX_PACKAGE_BYTES
    ) {
      throw fixedError();
    }
    const snapshot = bytes.slice();
    if (await sha256(snapshot) !== hash) throw fixedError();
    packages.set(hash, snapshot);
    packageFeatureIds.set(hash, packageFeatureId(snapshot));
  }
  try {
    await assertLinuxOpenSsl3();
  } catch {
    throw fixedError();
  }
  let temporaryDirectory: string;
  try {
    temporaryDirectory = await Deno.makeTempDir({
      prefix: "m3d-https-public-",
    });
  } catch {
    throw fixedError();
  }
  let ca: Identity | null = null;
  let trusted: Identity | null = null;
  let untrusted: Identity | null = null;
  const servers: OwnedTlsServer[] = [];
  let nextMode: M3dPackageHttpsMode = "valid-streaming";
  let requests = 0;
  let activeStreams = 0;
  let cancellations = 0;
  let redirectTargets = 0;
  let observationOverflow = false;
  const observations: M3dPackageHttpsRequestObservation[] = [];
  const stalls = new Set<Stall>();
  const backgroundTasks = new Set<Promise<void>>();
  let closing = false;
  let closePromise: Promise<M3dPackageHttpsCleanupEvidence> | null = null;
  let cleanupErrorCount = 0;

  const cleanup = async (): Promise<M3dPackageHttpsCleanupEvidence> => {
    if (closePromise) return await closePromise;
    closePromise = (async () => {
      closing = true;
      await releaseStalls();
      const results = await Promise.allSettled(
        servers.map((server) => server.close()),
      );
      await withTimeout(
        Promise.allSettled([...backgroundTasks]),
        CLEANUP_TIMEOUT_MS,
      ).catch(() => {
        cleanupErrorCount++;
      });
      cleanupErrorCount += servers.reduce(
        (count, server) => count + server.cleanupErrorCount,
        0,
      );
      for (const identity of [ca, trusted, untrusted]) {
        if (identity) identity.keyPem = "";
      }
      await withTimeout(
        Deno.remove(temporaryDirectory, { recursive: true }),
        CLEANUP_TIMEOUT_MS,
      ).catch(() => {
        cleanupErrorCount++;
      });
      const serversClosed = results.every((result) =>
        result.status === "fulfilled" && result.value
      );
      if (!serversClosed) cleanupErrorCount++;
      const listenersClosed = servers.every((server) => server.listenerClosed);
      const activeConnectionsClosed = servers.every((server) =>
        server.activeConnections === 0
      );
      const acceptLoopsSettled = servers.every((server) =>
        server.acceptLoopSettled
      );
      return {
        closed: listenersClosed && activeConnectionsClosed &&
          acceptLoopsSettled && activeStreams === 0 && stalls.size === 0 &&
          backgroundTasks.size === 0 && cleanupErrorCount === 0 &&
          serversClosed,
        listenersClosed,
        activeConnectionsClosed,
        stallsReleased: activeStreams === 0 && stalls.size === 0 &&
          backgroundTasks.size === 0,
        acceptLoopsSettled,
        cleanupErrorCount,
      };
    })();
    return await closePromise;
  };

  const releaseStalls = async (): Promise<void> => {
    const pending = [...stalls];
    for (const stall of pending) {
      stall.releaseRequested = true;
      stall.resolve();
    }
    await withTimeout(
      Promise.allSettled(pending.map((stall) => stall.task)).then(() => {}),
      CLEANUP_TIMEOUT_MS,
    ).catch(() => {
      cleanupErrorCount++;
    });
  };

  const onRequest = async (
    origin: M3dPackageHttpsOriginKind,
    request: ParsedRequest,
    connection: Deno.TlsConn,
  ): Promise<void> => {
    requests++;
    const mode = nextMode;
    nextMode = "valid-streaming";
    const record = (observedMode: M3dPackageHttpsMode | "redirect-target") => {
      if (observations.length < MAX_OBSERVATIONS) {
        observations.push({
          origin,
          mode: observedMode,
          ...headerFlags(request.headers),
        });
      } else observationOverflow = true;
    };

    if (request.method !== "GET") {
      record(mode);
      return await writeAll(
        connection,
        responseHead(405, ["Content-Length: 0", "Connection: close"]),
      );
    }
    let target: URL;
    try {
      if (!request.target.startsWith("/") || request.target.startsWith("//")) {
        throw fixedError();
      }
      target = new URL(request.target, "https://127.0.0.1");
      if (
        target.origin !== "https://127.0.0.1" || target.search !== "" ||
        target.hash !== "" || request.target !== target.pathname
      ) throw fixedError();
    } catch {
      record(mode);
      return await writeAll(
        connection,
        responseHead(404, ["Content-Length: 0", "Connection: close"]),
      );
    }
    if (target.pathname === REDIRECT_TARGET_PATH) {
      record("redirect-target");
      redirectTargets++;
      return await writeAll(
        connection,
        responseHead(204, ["Content-Length: 0", "Connection: close"]),
      );
    }
    record(mode);

    if (target.pathname === COOKIE_SEED_PATH) {
      if (mode !== "cookie-seed") {
        return await writeAll(
          connection,
          responseHead(404, ["Content-Length: 0", "Connection: close"]),
        );
      }
      return await writeAll(
        connection,
        responseHead(204, [
          "Content-Length: 0",
          `Set-Cookie: ${SEED_COOKIE_MARKER}; Secure; Path=/; SameSite=None`,
          "Connection: close",
        ]),
      );
    }
    if (target.pathname === COOKIE_CONTROL_PATH) {
      if (mode !== "cookie-control") {
        return await writeAll(
          connection,
          responseHead(404, ["Content-Length: 0", "Connection: close"]),
        );
      }
      return await writeAll(
        connection,
        responseHead(204, ["Content-Length: 0", "Connection: close"]),
      );
    }

    const match = PACKAGE_PATH_PATTERN.exec(target.pathname);
    if (!match) {
      return await writeAll(
        connection,
        responseHead(404, ["Content-Length: 0", "Connection: close"]),
      );
    }
    const [, featureId, requestedHash] = match;
    let body = packages.get(requestedHash);
    if (!body || packageFeatureIds.get(requestedHash) !== featureId) {
      return await writeAll(
        connection,
        responseHead(404, ["Content-Length: 0", "Connection: close"]),
      );
    }
    if (mode === "cookie-seed" || mode === "cookie-control") {
      return await writeAll(
        connection,
        responseHead(404, ["Content-Length: 0", "Connection: close"]),
      );
    }
    if (mode === "redirect") {
      const server = servers.find((item) => item.kind === origin);
      if (!server) throw fixedError();
      return await writeAll(
        connection,
        responseHead(302, [
          `Location: https://127.0.0.1:${server.port}${REDIRECT_TARGET_PATH}`,
          "Content-Length: 0",
          "Connection: close",
        ]),
      );
    }
    if (mode === "wrong-package") {
      const other = [...packages.entries()].find(([hash]) =>
        hash !== requestedHash
      );
      if (!other) {
        return await writeAll(
          connection,
          responseHead(404, ["Content-Length: 0", "Connection: close"]),
        );
      }
      body = other[1];
    }

    const contentType = mode === "content-type-rejected"
      ? "text/plain"
      : "application/octet-stream";
    const contentEncoding = mode === "content-encoding-rejected"
      ? "gzip"
      : null;
    const cookie =
      `Set-Cookie: ${ACQUISITION_COOKIE_MARKER}; Secure; Path=/; SameSite=None`;
    if (mode === "status-rejected") {
      return await writeAll(
        connection,
        responseHead(503, ["Content-Length: 0", "Connection: close"]),
      );
    }
    if (mode === "oversized-declared") {
      const oversized = new Uint8Array(MAX_PACKAGE_BYTES + 1);
      const deadline = performance.now() + WRITE_TIMEOUT_MS;
      await writeAll(
        connection,
        responseHead(200, [
          "Content-Type: application/octet-stream",
          `Content-Length: ${oversized.length}`,
          cookie,
          "Connection: close",
        ]),
        deadline,
      );
      for (let offset = 0; offset < oversized.length; offset += 16 * 1024) {
        const chunk = oversized.subarray(
          offset,
          Math.min(offset + 16 * 1024, oversized.length),
        );
        await writeAll(connection, chunk, deadline);
      }
      return;
    }
    const headers = [
      `Content-Type: ${contentType}`,
      cookie,
      "Connection: close",
    ];
    if (contentEncoding) headers.push(`Content-Encoding: ${contentEncoding}`);
    if (mode === "oversized-streamed") {
      const oversized = new Uint8Array(MAX_PACKAGE_BYTES + 1);
      headers.push("Transfer-Encoding: chunked");
      const deadline = performance.now() + WRITE_TIMEOUT_MS;
      await writeAll(connection, responseHead(200, headers), deadline);
      for (let offset = 0; offset < oversized.length; offset += 16 * 1024) {
        const chunk = oversized.subarray(
          offset,
          Math.min(offset + 16 * 1024, oversized.length),
        );
        await writeAll(
          connection,
          encoder.encode(`${chunk.length.toString(16)}\r\n`),
          deadline,
        );
        await writeAll(connection, chunk, deadline);
        await writeAll(connection, encoder.encode("\r\n"), deadline);
      }
      return await writeAll(connection, encoder.encode("0\r\n\r\n"), deadline);
    }
    if (
      mode === "equal-content-length" || mode === "conflicting-content-length"
    ) {
      const alternate = mode === "equal-content-length"
        ? body.length
        : body.length + 1;
      headers.push(
        `Content-Length: ${body.length}`,
        `Content-Length: ${alternate}`,
      );
      await writeAll(connection, responseHead(200, headers));
      return await writeAll(connection, body);
    }
    const declared = mode === "truncated-length"
      ? body.length + 1
      : body.length;
    headers.push(`Content-Length: ${declared}`);
    await writeAll(connection, responseHead(200, headers));
    if (mode === "read-failure") {
      await writeAll(
        connection,
        body.subarray(0, Math.max(1, Math.floor(body.length / 3))),
      );
      return;
    }
    if (mode === "stalled-body") {
      const midpoint = Math.max(1, Math.floor(body.length / 2));
      await writeAll(connection, body.subarray(0, midpoint));
      activeStreams++;
      const wait = deferred();
      const peerWait = deferred();
      const stall: Stall = {
        connection,
        remainder: body.subarray(midpoint),
        resolve: wait.resolve,
        peerClosed: peerWait.promise,
        peerTask: Promise.resolve(),
        task: Promise.resolve(),
        releaseRequested: false,
        cancelled: false,
      };
      stall.peerTask = (async () => {
        const chunk = new Uint8Array(1024);
        let received = 0;
        try {
          for (;;) {
            const count = await connection.read(chunk);
            if (count === null) {
              if (!stall.releaseRequested && !closing && !stall.cancelled) {
                stall.cancelled = true;
                cancellations++;
              }
              peerWait.resolve();
              return;
            }
            received += count;
            if (received > MAX_HEADER_BYTES) {
              peerWait.resolve();
              return;
            }
          }
        } catch {
          if (!stall.releaseRequested && !closing && !stall.cancelled) {
            stall.cancelled = true;
            cancellations++;
          }
          peerWait.resolve();
        }
      })();
      backgroundTasks.add(stall.peerTask);
      void stall.peerTask.then(
        () => backgroundTasks.delete(stall.peerTask),
        () => {
          backgroundTasks.delete(stall.peerTask);
          cleanupErrorCount++;
        },
      );
      stall.task = (async () => {
        try {
          await Promise.race([wait.promise, stall.peerClosed]);
          if (stall.releaseRequested && !closing) {
            await writeAll(connection, stall.remainder);
          }
        } catch {
          return;
        } finally {
          activeStreams--;
          stalls.delete(stall);
        }
      })();
      stalls.add(stall);
      return await stall.task;
    }
    return await writeAll(connection, body);
  };

  try {
    ca = await rootIdentity();
    trusted = await signedLeaf(ca, temporaryDirectory, "1025");
    untrusted = await selfSignedLeaf();
    const serverIdentities = [trusted, untrusted, trusted] as const;
    for (const [index, identity] of serverIdentities.entries()) {
      servers.push(
        new OwnedTlsServer(
          index === 0 ? "trusted" : index === 1 ? "untrusted" : "raw",
          identity,
          onRequest,
        ),
      );
    }
    const ports = servers.map((server) => server.port);
    return {
      trustedOrigin: `https://127.0.0.1:${ports[0]}`,
      untrustedOrigin: `https://127.0.0.1:${ports[1]}`,
      rawOrigin: `https://127.0.0.1:${ports[2]}`,
      caCertificatePem: ca.certPem,
      setMode(mode) {
        if (closing || !MODES.has(mode)) throw fixedError();
        if (mode === "wrong-package" && packages.size < 2) throw fixedError();
        nextMode = mode;
      },
      snapshot() {
        return {
          requests,
          activeStreams,
          activeConnections: servers.reduce(
            (count, server) => count + server.activeConnections,
            0,
          ),
          cancellations,
          redirectTargets,
          observations: observations.map((observation) => ({ ...observation })),
          observationOverflow,
          ports: { trusted: ports[0], untrusted: ports[1], raw: ports[2] },
        };
      },
      releaseStalls,
      close: cleanup,
    };
  } catch {
    await cleanup();
    throw fixedError();
  }
}
