// SPDX-License-Identifier: MPL-2.0
/** Fixed GitHub QA dispatch/ref wire. No Runner protocol, workflow change, or publication authority. */
import type {
  QaDispatchHttp,
  QaDispatchRequest,
} from "./fdh_qa_dispatch_types.ts";
import { planQaDispatch } from "./fdh_qa_dispatch.ts";
import type {
  QaDispatchWireExitStatus,
  QaDispatchWireHeaderResult,
  QaDispatchWireTestChild,
  QaDispatchWireTestDependencies,
  QaDispatchWireTransport,
} from "./fdh_qa_dispatch_gh_types.ts";

const REPOSITORY = "Floorp-Projects/Floorp" as const;
const WORKFLOW = "validate-final-package-qa.yml" as const;
const MAIN_REF_ENDPOINT = "repos/Floorp-Projects/Floorp/git/ref/heads/main";
const DISPATCH_ENDPOINT =
  "repos/Floorp-Projects/Floorp/actions/workflows/validate-final-package-qa.yml/dispatches";
export const QA_DISPATCH_WIRE_LIMITS = Object.freeze({
  timeoutMs: 30_000,
  stdoutBytes: 128 * 1024,
  stderrBytes: 16 * 1024,
  headerBytes: 16 * 1024,
  headerLines: 128,
});

function argsFor(method: "GET" | "POST", endpoint: string): string[] {
  const args = [
    "api",
    "--hostname",
    "github.com",
    "--include",
    "--method",
    method,
    "-H",
    "Accept: application/vnd.github+json",
    "-H",
    "X-GitHub-Api-Version: 2026-03-10",
  ];
  if (method === "POST") args.push("--input", "-");
  args.push(endpoint);
  return args;
}

function response(bytes: Uint8Array): QaDispatchWireHeaderResult {
  let end = -1;
  let lineStart = 0;
  for (
    let i = 0;
    i < Math.min(bytes.length, QA_DISPATCH_WIRE_LIMITS.headerBytes);
    i++
  ) {
    if (bytes[i] !== 10) continue;
    if (i === lineStart || (i === lineStart + 1 && bytes[lineStart] === 13)) {
      end = i + 1;
      break;
    }
    lineStart = i + 1;
  }
  if (end <= 0) return { status: 0, body: new Uint8Array() };
  let header: string;
  try {
    header = new TextDecoder("utf-8", { fatal: true }).decode(
      bytes.subarray(0, end),
    );
  } catch {
    return { status: 0, body: new Uint8Array() };
  }
  const lines = header.replaceAll("\r\n", "\n").split("\n");
  if (lines.length > QA_DISPATCH_WIRE_LIMITS.headerLines) {
    return { status: 0, body: new Uint8Array() };
  }
  const match =
    /^HTTP\/(?:1\.[01]|2(?:\.0)?|3(?:\.0)?) ([1-5][0-9]{2})(?: [^\r\n]*)?$/
      .exec(lines[0]);
  if (!match) return { status: 0, body: new Uint8Array() };
  return { status: Number(match[1]), body: bytes.slice(end) };
}

function parseBody(bytes: Uint8Array): unknown {
  if (bytes.length === 0) return null;
  try {
    const text = new TextDecoder("utf-8", { fatal: true }).decode(bytes);
    return JSON.parse(text);
  } catch {
    return null;
  }
}

function createTransport(
  deps: QaDispatchWireTestDependencies,
): QaDispatchWireTransport {
  async function run(
    method: "GET" | "POST",
    endpoint: string,
    stdinBody: string | null,
  ): Promise<QaDispatchHttp> {
    const started = deps.now();
    let child: QaDispatchWireTestChild | undefined;
    let outStream: ReadableStream<Uint8Array> | undefined;
    let errStream: ReadableStream<Uint8Array> | undefined;
    let outReader: ReadableStreamDefaultReader<Uint8Array> | undefined;
    let errReader: ReadableStreamDefaultReader<Uint8Array> | undefined;
    let statusPromise: Promise<QaDispatchWireExitStatus> | undefined;
    let cause = false;
    let killed = false;
    let childWorkSettled = false;
    let timer: unknown;
    let hasTimer = false;
    const cancels: Promise<unknown>[] = [];
    const output = new Uint8Array(QA_DISPATCH_WIRE_LIMITS.stdoutBytes);
    let outSize = 0, errSize = 0;
    const stop = () => {
      cause = true;
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
        } catch { /* already exited */ }
      }
    };
    const check = () => {
      if (cause) return false;
      try {
        if (deps.now() - started >= QA_DISPATCH_WIRE_LIMITS.timeoutMs) {
          stop();
        }
      } catch {
        stop();
      }
      return !cause;
    };
    const drains: Promise<void>[] = [];
    try {
      child = deps.spawn(Object.freeze(argsFor(method, endpoint)), stdinBody);
      statusPromise = child.status.then((s) => s, () => {
        stop();
        return { success: false, code: -1 };
      });
      outStream = child.stdout;
      outReader = outStream.getReader();
      errStream = child.stderr;
      errReader = errStream.getReader();
      if (check()) {
        timer = deps.setTimer(() => stop(), QA_DISPATCH_WIRE_LIMITS.timeoutMs);
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
              const chunk = item.value;
              if (!(chunk instanceof Uint8Array)) {
                stop();
                break;
              }
              const size = chunk.length;
              if (stderr) {
                errSize += size;
                if (errSize > QA_DISPATCH_WIRE_LIMITS.stderrBytes) {
                  stop();
                  break;
                }
              } else {
                outSize += size;
                if (outSize > QA_DISPATCH_WIRE_LIMITS.stdoutBytes) {
                  stop();
                  break;
                }
                if (size) output.set(chunk, outSize - size);
              }
            }
          } catch {
            stop();
          } finally {
            try {
              reader.releaseLock();
            } catch { /* sanitized */ }
          }
        };
        drains.push(drain(outReader, false), drain(errReader, true));
        if (stdinBody !== null && child.stdin) {
          const writer = child.stdin.getWriter();
          try {
            const bytes = new TextEncoder().encode(stdinBody);
            await writer.write(bytes);
            await writer.close();
          } catch {
            stop();
            try {
              writer.releaseLock();
            } catch { /* sanitized */ }
          }
        }
      }
    } catch {
      stop();
    }
    try {
      const settled = await Promise.allSettled([statusPromise, ...drains]);
      childWorkSettled = true;
      check();
      await Promise.allSettled(cancels);
      if (cause) return { status: 0, body: null };
      const status = settled[0];
      if (status.status !== "fulfilled" || !status.value) {
        return { status: 0, body: null };
      }
      const bytes = output.subarray(0, outSize);
      const parsed = response(bytes);
      if (parsed.status === 0) return { status: 0, body: null };
      const body = parseBody(parsed.body);
      if (parsed.status >= 200 && parsed.status < 300) {
        if (!status.value.success || status.value.code !== 0) {
          return { status: 0, body: null };
        }
      }
      return { status: parsed.status, body };
    } finally {
      if (hasTimer) deps.clearTimer(timer);
      for (const reader of [outReader, errReader]) {
        try {
          reader?.releaseLock();
        } catch { /* sanitized */ }
      }
    }
  }
  return {
    async mainRef() {
      return await run("GET", MAIN_REF_ENDPOINT, null);
    },
    async dispatch(request: QaDispatchRequest) {
      const plan = await planQaDispatch(request);
      if (
        plan.repository !== REPOSITORY ||
        plan.workflow !== ".github/workflows/" + WORKFLOW
      ) {
        throw new Error("Invalid QA dispatch plan");
      }
      const stdinBody = JSON.stringify({ ref: plan.ref, inputs: plan.inputs });
      return await run("POST", DISPATCH_ENDPOINT, stdinBody);
    },
  };
}

function spawnProduction(
  args: readonly string[],
  stdinBody: string | null,
): QaDispatchWireTestChild {
  return new Deno.Command("gh", {
    args: [...args],
    stdin: stdinBody === null ? "null" : "piped",
    stdout: "piped",
    stderr: "piped",
    env: { GH_PROMPT_DISABLED: "1", GIT_TERMINAL_PROMPT: "0" },
  }).spawn();
}

/** Production factory; installed gh config/auth/TLS are host TCB, not API attestation. */
export function createQaDispatchWireTransport(): QaDispatchWireTransport {
  return createTransport({
    spawn: spawnProduction,
    now: () => performance.now(),
    setTimer: (callback, milliseconds) => setTimeout(callback, milliseconds),
    clearTimer: (handle) => clearTimeout(handle as number),
  });
}

/** Synthetic tests only; no actual gh, browser, network, commit, or push. */
export function createQaDispatchWireTransportForTest(
  deps: QaDispatchWireTestDependencies,
): QaDispatchWireTransport {
  return createTransport(deps);
}
