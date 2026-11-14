// SPDX-License-Identifier: MPL-2.0
import type {
  QaGhEndpoint,
  QaGhErrorCode,
  QaGhOperation,
  QaGhResult,
  QaGhStatusClass,
  QaGhTestChild,
  QaGhTestDependencies,
  QaGhTransport,
} from "./fdh_qa_gh_transport_types.ts";
export type {
  QaGhOperation,
  QaGhResult,
  QaGhTransport,
} from "./fdh_qa_gh_transport_types.ts";

const MIB = 1024 * 1024;
/** Wire byte ceilings, not total heap bounds (owned copies/JSON have overhead).
 * Headers count against stdout budgets, including failed responses. */
export const QA_GH_LIMITS = Object.freeze({
  metadataBytes: 2 * MIB,
  metadataSessionBytes: 16 * MIB,
  zipBytes: 8 * MIB,
  zipSessionBytes: 16 * MIB,
  stderrBytes: 16 * 1024,
  headerBytes: 16 * 1024,
  headerLines: 128,
  metadataMs: 30_000,
  zipMs: 60_000,
  sessionMs: 300_000,
  jsonDepth: 64,
  pages: 10,
});
const REPO = "repos/Floorp-Projects/Floorp";
const TA = Object.getPrototypeOf(Uint8Array.prototype);
const KIND = Object.getOwnPropertyDescriptor(TA, Symbol.toStringTag)!.get!;
const LENGTH = Object.getOwnPropertyDescriptor(TA, "length")!.get!;
const BUFFER = Object.getOwnPropertyDescriptor(TA, "buffer")!.get!;
const OFFSET = Object.getOwnPropertyDescriptor(TA, "byteOffset")!.get!;
const RESIZABLE = Object.getOwnPropertyDescriptor(
  ArrayBuffer.prototype,
  "resizable",
)!.get!;
const SET = Uint8Array.prototype.set;
const PARSE = JSON.parse;
function requireValue(value: unknown): asserts value {
  if (!value) throw new Error("invalid transport input");
}
function fields(
  value: unknown,
  keys: string[],
  kind: PropertyDescriptor,
): Record<string, unknown> {
  requireValue(typeof value === "object" && value !== null);
  requireValue(Reflect.ownKeys(value).length === keys.length);
  const result: Record<string, unknown> = Object.create(null);
  for (const key of keys) {
    const d = key === "kind"
      ? kind
      : Object.getOwnPropertyDescriptor(value, key);
    requireValue(d && "value" in d);
    result[key] = d.value;
  }
  return result;
}
function positive(value: unknown): number {
  requireValue(
    typeof value === "number" && Number.isSafeInteger(value) && value > 0,
  );
  return value;
}
function page(value: unknown): number {
  const n = positive(value);
  requireValue(n <= QA_GH_LIMITS.pages);
  return n;
}
/** Synchronous, one-read primitive snapshot before any process/await. */
function endpoint(operation: QaGhOperation): QaGhEndpoint {
  const d = Object.getOwnPropertyDescriptor(operation, "kind");
  requireValue(d && "value" in d && typeof d.value === "string");
  const kind: unknown = d.value;
  let p = "", zip = false;
  const read = (...keys: string[]) => {
    const f = fields(operation, ["kind", ...keys], d);
    requireValue(f.kind === kind);
    return f;
  };
  switch (kind) {
    case "repository":
      read();
      break;
    case "workflow": {
      const f = read("workflow");
      requireValue(f.workflow === "source" || f.workflow === "qa");
      p = "/actions/workflows/" +
        (f.workflow === "source"
          ? "package_and_publish_release.yml"
          : "validate-final-package-qa.yml");
      break;
    }
    case "currentRun":
      p = "/actions/runs/" + positive(read("runId").runId);
      break;
    case "attemptRun": {
      const f = read("runId", "attempt");
      p = "/actions/runs/" + positive(f.runId) + "/attempts/" +
        positive(f.attempt);
      break;
    }
    case "jobsPage": {
      const f = read("runId", "attempt", "page");
      p = "/actions/runs/" + positive(f.runId) + "/attempts/" +
        positive(f.attempt) + "/jobs?per_page=100&page=" + page(f.page);
      break;
    }
    case "artifactsPage": {
      const f = read("runId", "page");
      p = "/actions/runs/" + positive(f.runId) +
        "/artifacts?per_page=100&page=" + page(f.page);
      break;
    }
    case "artifactMetadata":
    case "selectedZip": {
      p = "/actions/artifacts/" + positive(read("artifactId").artifactId);
      zip = kind === "selectedZip";
      if (zip) p += "/zip";
      break;
    }
    case "pinnedContents": {
      const f = read("file", "head");
      requireValue(f.file === "package" || f.file === "runtimeLock");
      requireValue(typeof f.head === "string" && /^[a-f0-9]{40}$/.test(f.head));
      p = "/contents/" +
        (f.file === "package" ? "package.json" : "floorp-runtime.lock.json") +
        "?ref=" + f.head;
      break;
    }
    case "pendingDeployments":
      p = "/actions/runs/" + positive(read("sourceRunId").sourceRunId) +
        "/pending_deployments";
      break;
    case "releaseTag": {
      const f = read("version");
      requireValue(
        typeof f.version === "string" && f.version.length <= 128 &&
          /^[0-9]+\.[0-9]+\.[0-9]+(?:-[0-9A-Za-z]+(?:[.-][0-9A-Za-z]+)*)?$/
            .test(f.version),
      );
      p = "/releases/tags/v" + f.version;
      break;
    }
    default:
      throw new Error("invalid transport input");
  }
  return { path: REPO + p, zip };
}
function argsFor(e: QaGhEndpoint): string[] {
  return [
    "api",
    "--hostname",
    "github.com",
    "--method",
    "GET",
    "--include",
    "--header",
    "Accept: application/vnd.github+json",
    "--header",
    "X-GitHub-Api-Version: 2022-11-28",
    e.path,
  ];
}
/** Reject Proxy/fake-size/kind spoofs before any property read/copy. */
function nativeChunk(
  value: unknown,
): { view: Uint8Array<ArrayBuffer>; size: number } {
  requireValue(ArrayBuffer.isView(value));
  requireValue(KIND.call(value) === "Uint8Array");
  const buffer: unknown = BUFFER.call(value);
  requireValue(
    Object.getPrototypeOf(buffer) === ArrayBuffer.prototype &&
      !RESIZABLE.call(buffer),
  );
  const size: number = LENGTH.call(value), offset: number = OFFSET.call(value);
  return { view: new Uint8Array(buffer as ArrayBuffer, offset, size), size };
}
function statusClass(status: number): QaGhStatusClass {
  if (status >= 200 && status < 300) return "success";
  if (status === 401) return "unauthorized";
  if (status === 403) return "forbidden";
  if (status === 404) return "notFound";
  if (status >= 400 && status < 500) return "clientError";
  if (status >= 500 && status < 600) return "serverError";
  return "other";
}
/** gh --include prints the FINAL response header, not redirect intermediates.
 * Only the first bounded block is consumed: body bytes can never become headers.
 * Unexpected redirect/multi-response output fails closed, never chases Location. */
function response(
  bytes: Uint8Array,
): { body: Uint8Array<ArrayBuffer>; status: number } {
  let end = -1, lineStart = 0;
  for (let i = 0; i < Math.min(bytes.length, QA_GH_LIMITS.headerBytes); i++) {
    if (bytes[i] !== 10) continue;
    if (i === lineStart || (i === lineStart + 1 && bytes[lineStart] === 13)) {
      end = i + 1;
      break;
    }
    lineStart = i + 1;
  }
  requireValue(end > 0 && end <= QA_GH_LIMITS.headerBytes);
  const header = new TextDecoder("utf-8", { fatal: true }).decode(
    bytes.subarray(0, end),
  );
  const lines = header.replaceAll("\r\n", "\n").split("\n");
  requireValue(lines.length <= QA_GH_LIMITS.headerLines);
  const match =
    /^HTTP\/(?:1\.[01]|2(?:\.0)?|3(?:\.0)?) ([1-5][0-9]{2})(?: [^\r\n]*)?$/
      .exec(lines[0]);
  requireValue(match);
  for (const line of lines.slice(1, -2)) {
    requireValue(/^[!#$%&'*+.^_`0-9A-Za-z|~-]+:[^\r\n]*$/.test(line));
  }
  return { body: bytes.slice(end), status: Number(match[1]) };
}
function metadata(bytes: Uint8Array): string {
  const text = new TextDecoder("utf-8", { fatal: true }).decode(bytes);
  let depth = 0, quoted = false, escaped = false;
  for (const c of text) {
    if (quoted) {
      if (escaped) escaped = false;
      else if (c === "\\") escaped = true;
      else if (c === '"') quoted = false;
    } else if (c === '"') quoted = true;
    else if (c === "{" || c === "[") {
      requireValue(++depth <= QA_GH_LIMITS.jsonDepth);
    } else if (c === "}" || c === "]") requireValue(--depth >= 0);
  }
  requireValue(!quoted && depth === 0);
  PARSE(text);
  return text;
}
function failure(
  code: QaGhErrorCode,
  classification: QaGhStatusClass = "none",
): QaGhResult {
  return { ok: false, code, statusClass: classification };
}
function createTransport(deps: QaGhTestDependencies): QaGhTransport {
  let last = deps.now();
  const started = last;
  let totalMetadata = 0, totalZip = 0, busy = false;
  function now(): number {
    const n = deps.now();
    requireValue(Number.isFinite(n) && n >= last && Number.isFinite(started));
    last = n;
    return n;
  }
  return {
    async request(operation, signal) {
      let e: QaGhEndpoint;
      try {
        e = endpoint(operation);
      } catch {
        return failure("invalidOperation");
      }
      if (busy) return failure("busy");
      if (signal?.aborted) return failure("aborted");
      let commandStart: number;
      try {
        commandStart = now();
      } catch {
        return failure("clock");
      }
      if (commandStart - started >= QA_GH_LIMITS.sessionMs) {
        return failure("sessionTimeout");
      }
      if (
        (e.zip ? totalZip : totalMetadata) >=
          (e.zip
            ? QA_GH_LIMITS.zipSessionBytes
            : QA_GH_LIMITS.metadataSessionBytes)
      ) return failure("sessionLimit");
      busy = true;
      let child: QaGhTestChild | undefined;
      let outStream: ReadableStream<Uint8Array> | undefined;
      let errStream: ReadableStream<Uint8Array> | undefined;
      let outReader: ReadableStreamDefaultReader<Uint8Array> | undefined;
      let errReader: ReadableStreamDefaultReader<Uint8Array> | undefined;
      let cause: QaGhErrorCode | undefined;
      let killed = false;
      let childWorkSettled = false;
      let timer: unknown;
      let hasTimer = false;
      const cancels: Promise<unknown>[] = [];
      // One fixed-cap accumulator avoids per-chunk object growth on tiny chunks.
      let output: Uint8Array<ArrayBuffer> | undefined;
      let outSize = 0, errSize = 0;
      const stop = (code: QaGhErrorCode) => {
        cause ??= code;
        // After status + both drains settled, no process/pipe work remains to cancel.
        if (childWorkSettled) return;
        for (
          const [reader, stream] of [[outReader, outStream], [
            errReader,
            errStream,
          ]] as const
        ) {
          try {
            const cancellation = reader ? reader.cancel() : stream?.cancel();
            if (cancellation) cancels.push(cancellation.catch(() => {}));
          } catch { /* sanitized */ }
        }
        if (child && !killed) {
          killed = true;
          try {
            child.kill("SIGKILL");
          } catch { /* already exited or host failure */ }
        }
      };
      const check = () => {
        if (cause) return false;
        try {
          const n = now();
          if (signal?.aborted) stop("aborted");
          else if (n - started >= QA_GH_LIMITS.sessionMs) {
            stop("sessionTimeout");
          } else if (
            n - commandStart >=
              (e.zip ? QA_GH_LIMITS.zipMs : QA_GH_LIMITS.metadataMs)
          ) stop("timeout");
        } catch {
          stop("clock");
        }
        return !cause;
      };
      const onAbort = () => stop("aborted");
      let statusPromise:
        | Promise<{ readonly success: boolean; readonly code: number }>
        | undefined;
      const drains: Promise<void>[] = [];
      try {
        // Only installed gh and owner-managed auth/config/TLS are trusted host TCB.
        child = deps.spawn(Object.freeze(argsFor(e)));
        statusPromise = child.status.then((s) => s, () => {
          stop("process");
          return { success: false, code: -1 };
        });
        // Register both readers before starting either drain; failures clean owned child.
        outStream = child.stdout;
        outReader = outStream.getReader();
        errStream = child.stderr;
        errReader = errStream.getReader();
        signal?.addEventListener("abort", onAbort, { once: true });
        if (check()) {
          const sessionDeadline = started + QA_GH_LIMITS.sessionMs;
          const commandDeadline = commandStart +
            (e.zip ? QA_GH_LIMITS.zipMs : QA_GH_LIMITS.metadataMs);
          const remaining = Math.min(sessionDeadline, commandDeadline) - now();
          timer = deps.setTimer(
            () =>
              stop(
                sessionDeadline <= commandDeadline
                  ? "sessionTimeout"
                  : "timeout",
              ),
            Math.max(0, remaining),
          );
          hasTimer = true;
          const drain = async (
            reader: ReadableStreamDefaultReader<Uint8Array>,
            stderr: boolean,
          ) => {
            try {
              while (check()) {
                const item = await reader.read();
                if (!check()) break;
                if (item.done) break;
                const { view, size } = nativeChunk(item.value);
                if (stderr) {
                  errSize += size;
                  if (errSize > QA_GH_LIMITS.stderrBytes) {
                    stop("stderrLimit");
                    break;
                  }
                  // Never retain or decode raw stderr.
                } else {
                  outSize += size;
                  if (e.zip) totalZip += size;
                  else totalMetadata += size;
                  if (
                    outSize >
                      (e.zip
                        ? QA_GH_LIMITS.zipBytes
                        : QA_GH_LIMITS.metadataBytes)
                  ) {
                    stop("stdoutLimit");
                    break;
                  }
                  if (
                    (e.zip ? totalZip : totalMetadata) >
                      (e.zip
                        ? QA_GH_LIMITS.zipSessionBytes
                        : QA_GH_LIMITS.metadataSessionBytes)
                  ) {
                    stop("sessionLimit");
                    break;
                  }
                  if (size) {
                    output ??= new Uint8Array(
                      e.zip
                        ? QA_GH_LIMITS.zipBytes
                        : QA_GH_LIMITS.metadataBytes,
                    );
                    SET.call(output, view, outSize - size);
                  }
                }
              }
            } catch {
              stop("stream");
            } finally {
              try {
                reader.releaseLock();
              } catch { /* sanitized */ }
            }
          };
          drains.push(drain(outReader, false), drain(errReader, true));
        }
      } catch {
        stop(child ? "stream" : "spawn");
      }
      // No early return after spawn: status + BOTH drains + all cancels settle.
      try {
        const settled = await Promise.allSettled([statusPromise, ...drains]);
        childWorkSettled = true;
        check();
        await Promise.allSettled(cancels);
        if (cause) return failure(cause);
        const status = settled[0];
        if (status.status !== "fulfilled" || !status.value) {
          return failure("process");
        }
        const bytes = output ? output.subarray(0, outSize) : new Uint8Array();
        let r: { body: Uint8Array<ArrayBuffer>; status: number };
        try {
          r = response(bytes);
        } catch {
          return failure("protocol");
        }
        const classification = statusClass(r.status);
        if (classification !== "success") {
          return failure("http", classification);
        }
        if (!status.value.success || status.value.code !== 0) {
          return failure("process", classification);
        }
        if (!check()) return failure(cause!);
        if (e.zip) {
          return {
            ok: true,
            kind: "zip",
            bytes: r.body,
            publicationAuthorized: false,
          };
        }
        try {
          const text = metadata(r.body);
          if (!check()) return failure(cause!);
          return {
            ok: true,
            kind: "metadata",
            jsonText: text,
            publicationAuthorized: false,
          };
        } catch {
          return failure("metadata");
        }
      } finally {
        signal?.removeEventListener("abort", onAbort);
        if (hasTimer) deps.clearTimer(timer);
        for (const reader of [outReader, errReader]) {
          try {
            reader?.releaseLock();
          } catch { /* sanitized */ }
        }
        busy = false;
      }
    },
  };
}
/** No subprocess/credential/environment access until request(). Host installed gh
 * config/auth/redirect/TLS behavior is TCB; this is not API-origin attestation,
 * process-tree termination, protected approval or publication authorization. */
export function createQaGhTransport(): QaGhTransport {
  return createTransport({
    spawn: (args) =>
      new Deno.Command("gh", {
        args: [...args],
        stdin: "null",
        stdout: "piped",
        stderr: "piped",
      }).spawn(),
    now: () => performance.now(),
    setTimer: (callback, ms) => setTimeout(callback, ms),
    clearTimer: (handle) => clearTimeout(handle as number),
  });
}
/** Synthetic tests only. Never wire this seam to CLI/model-controlled options. */
export function createQaGhTransportForTest(
  deps: QaGhTestDependencies,
): QaGhTransport {
  return createTransport(deps);
}
