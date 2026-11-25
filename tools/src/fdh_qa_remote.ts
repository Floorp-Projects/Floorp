// SPDX-License-Identifier: MPL-2.0

/**
 * Shared pieces of the FDH Agentic QA targets (KAN-142): shell quoting, the
 * `FDH_RESULT` protocol, the guest-side Marionette client and an ssh-based
 * transport that feeds a script (or an upload) on stdin.
 */

import { encodeBase64 } from "@std/encoding/base64";
import type {
  GuestRunOutput,
  MarionetteRequest,
  QaTransport,
} from "./fdh_qa_remote_types.ts";

export type {
  GuestRunOutput,
  MarionetteRequest,
  QaTransport,
} from "./fdh_qa_remote_types.ts";

/** POSIX single-quote; safe for sh, bash and zsh. */
export function shellQuote(value: string): string {
  if (value.includes("\0")) throw new Error("shell argument contains NUL");
  return "'" + value.replaceAll("'", `'"'"'`) + "'";
}

/**
 * Script text for `bash -s`. The body is one brace group, so bash parses all
 * of it before running anything and a child command that reads stdin (a
 * prompting `hdiutil`, say) cannot swallow the rest of the script.
 */
export function wrapForStdin(script: string): string {
  return `{\n${script}\n}\n`;
}

/** Marionette client run with the guest's python3; reads a request from env. */
export const MARIONETTE_CLIENT = String.raw`
import base64, json, os, socket, sys, time
port = int(os.environ["FDH_MPORT"])
req = json.loads(base64.b64decode(os.environ["FDH_REQ"]))
deadline = time.time() + req.get("connectTimeout", 5)
sock = None
while True:
    try:
        sock = socket.create_connection(("127.0.0.1", port), timeout=2)
        break
    except OSError:
        if time.time() > deadline:
            print("FDH_RESULT " + json.dumps({"ok": False, "error": "marionette-unavailable"}))
            sys.exit(3)
        time.sleep(1)
sock.settimeout(60)
def recv():
    head = b""
    while not head.endswith(b":"):
        b = sock.recv(1)
        if not b:
            raise RuntimeError("connection closed")
        head += b
    n = int(head[:-1]); data = b""
    while len(data) < n:
        chunk = sock.recv(n - len(data))
        if not chunk:
            raise RuntimeError("connection closed")
        data += chunk
    return json.loads(data)
seq = [0]
def call(name, args):
    seq[0] += 1
    data = json.dumps([0, seq[0], name, args]).encode()
    sock.sendall(str(len(data)).encode() + b":" + data)
    res = recv()
    if res[2] is not None:
        raise RuntimeError(json.dumps(res[2])[:400])
    return res[3]
try:
    recv()
    call("WebDriver:NewSession", {"capabilities": {}})
    call("Marionette:SetContext", {"value": "chrome"})
    out = {"ok": True}
    op = req["op"]
    if op == "ready":
        out["value"] = call("WebDriver:ExecuteScript", {"script": "return Services.appinfo.version", "args": [], "newSandbox": True}).get("value")
    elif op == "eval":
        out["value"] = call("WebDriver:ExecuteScript", {"script": req["script"], "args": [], "newSandbox": True}).get("value")
    elif op == "screenshot":
        out["png"] = call("WebDriver:TakeScreenshot", {"full": False, "hash": False}).get("value")
    print("FDH_RESULT " + json.dumps(out))
except Exception as e:
    print("FDH_RESULT " + json.dumps({"ok": False, "error": "marionette-error", "detail": str(e)[:400]}))
    sys.exit(3)
`;

/** base64 of the Marionette client and of one request, for a guest script. */
export function marionettePayload(
  request: MarionetteRequest,
): { client: string; request: string } {
  return {
    client: encodeBase64(new TextEncoder().encode(MARIONETTE_CLIENT)),
    request: encodeBase64(new TextEncoder().encode(JSON.stringify(request))),
  };
}

/** Last `FDH_RESULT <json>` line of a run, or a transport error. */
export function parseGuestResult(
  output: GuestRunOutput,
): Record<string, unknown> {
  if (output.timedOut === true) {
    return { ok: false, error: "timeout", exitCode: output.code };
  }
  const lines = output.stdout.split("\n").filter((line) =>
    line.startsWith("FDH_RESULT ")
  );
  const last = lines.at(-1);
  if (last === undefined) {
    return {
      ok: false,
      error: "no-result",
      exitCode: output.code,
      stderr: output.stderr.slice(-400),
    };
  }
  let parsed: unknown;
  try {
    parsed = JSON.parse(last.slice("FDH_RESULT ".length));
  } catch {
    return { ok: false, error: "malformed-result", exitCode: output.code };
  }
  if (typeof parsed !== "object" || parsed === null || Array.isArray(parsed)) {
    return { ok: false, error: "malformed-result", exitCode: output.code };
  }
  const record = parsed as Record<string, unknown>;
  if (record.ok === true && output.code !== 0) {
    return { ok: false, error: "nonzero-exit", exitCode: output.code };
  }
  return record;
}

/** How a transport reaches the remote shell for one run. */
export type RemoteArgs = (script: string, mode: "stdin" | "argv") => string[];

/**
 * Real transport over the local `ssh` binary, no shell on this side.
 * Without an upload the script goes to stdin (`bash -s`, one brace group);
 * with one, `args` must carry the script in argv and stdin carries the
 * upload. The stdin feed and the output collection run concurrently, so an
 * early remote exit still returns the remote result instead of a broken-pipe
 * error. On timeout the local process is killed; the remote side is not
 * guaranteed to stop.
 */
export function createRemoteTransport(
  args: RemoteArgs,
  options: {
    command?: [string, ...string[]];
    timeoutMs?: number;
    uploadTimeoutMs?: number;
  } = {},
): QaTransport {
  const [program, ...prefix] = options.command ?? ["ssh"];
  return async (script, upload) => {
    const timeoutMs = upload
      ? options.uploadTimeoutMs ?? 1_800_000
      : options.timeoutMs ?? 300_000;
    const child = new Deno.Command(program, {
      args: [...prefix, ...args(script, upload ? "argv" : "stdin")],
      stdin: "piped",
      stdout: "piped",
      stderr: "piped",
    }).spawn();
    let timedOut = false;
    const abort = new AbortController();
    const timer = setTimeout(() => {
      timedOut = true;
      // Stop feeding too, so a stalled upload source cannot hold us here.
      abort.abort();
      try {
        child.kill("SIGKILL");
      } catch {
        // already exited
      }
    }, timeoutMs);
    const source = upload ?? new Blob([wrapForStdin(script)]).stream();
    const feeding = source.pipeTo(child.stdin, { signal: abort.signal })
      .catch(() => {
        // The remote side closed early or we timed out; any result is still
        // on stdout.
      });
    try {
      const out = await child.output();
      // The child is gone: a source that is still mid-read is abandoned.
      abort.abort();
      await feeding;
      return {
        code: out.code,
        stdout: new TextDecoder().decode(out.stdout),
        stderr: new TextDecoder().decode(out.stderr),
        timedOut,
      };
    } finally {
      clearTimeout(timer);
    }
  };
}

export class QaTargetError extends Error {
  constructor(
    readonly target: string,
    readonly step: string,
    readonly result: Record<string, unknown>,
  ) {
    super(`${target} QA ${step} failed: ${String(result.error ?? "unknown")}`);
  }
}
