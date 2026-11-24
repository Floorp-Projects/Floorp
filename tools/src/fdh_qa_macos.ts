// SPDX-License-Identifier: MPL-2.0

/**
 * fdh macOS QA target (KAN-142 Agentic QA, KAN-144 step 1).
 *
 * Drives one long-lived macOS guest (a tart VM on a Mac host) for
 * cross-platform Agentic QA: preflight, exclusive per-target lock, staging of
 * a Floorp build, launch in the test user's GUI session with a fresh profile
 * and Marionette, chrome-context probes and screenshots, and cleanup that
 * removes everything the task created.
 *
 * Reset is application-level only: every task gets its own directory under
 * `qaRoot` (app copy + profile) and cleanup removes it and every process that
 * references it. System-level state (TCC grants, OS settings) is NOT reset.
 *
 * Credentials never leave the Mac host: the config names the guest key and
 * known-hosts files by their names inside `vmDir` on the host, and every guest
 * command runs as `ssh <host> "cd <vmDir> && ssh -i <key> <guest> ..."`. The
 * operator machine needs only its existing SSH access to the host.
 *
 * This module is not QA qualification, release evidence or a QA3 replacement.
 */

import { encodeBase64 } from "@std/encoding/base64";
import { validateTaskId } from "./fdh_task.ts";
import type {
  GuestRunOutput,
  MacosPayloadKind,
  MacosQaCleanup,
  MacosQaPreflight,
  MacosQaTargetConfig,
  MacosQaTransport,
  MarionetteRequest,
} from "./fdh_qa_macos_types.ts";

export type {
  GuestRunOutput,
  MacosPayloadKind,
  MacosQaCleanup,
  MacosQaPreflight,
  MacosQaTargetConfig,
  MacosQaTransport,
  MarionetteRequest,
} from "./fdh_qa_macos_types.ts";

const ACCOUNT = /^[a-z_][a-z0-9_-]{0,31}$/;
const HOST = /^[A-Za-z0-9][A-Za-z0-9.-]{0,252}$/;
const IPV4 =
  /^(25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)(\.(25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)){3}$/;
const ABS_PATH = /^\/[A-Za-z0-9._/-]{1,511}$/;
const FILE_NAME = /^[A-Za-z0-9._-]{1,128}$/;
const CONFIG_KEYS = [
  "hostSsh",
  "vmDir",
  "guestKeyFile",
  "guestKnownHostsFile",
  "guestUser",
  "guestAddress",
  "testUser",
  "qaRoot",
  "marionettePort",
  "minFreeBytes",
] as const;

function configError(detail: string): Error {
  return new Error(`invalid fdh macOS QA target config: ${detail}`);
}

function safeAbsPath(value: unknown, name: string): string {
  if (typeof value !== "string" || !ABS_PATH.test(value)) {
    throw configError(`${name} must be an absolute path of [A-Za-z0-9._/-]`);
  }
  const parts = value.split("/").slice(1);
  if (parts.some((part) => part === "" || part === "." || part === "..")) {
    throw configError(`${name} must not contain empty, . or .. segments`);
  }
  return value;
}

/** Strict parse: exact key set, no extra keys, no shell metacharacters. */
export function parseMacosQaTargetConfig(value: unknown): MacosQaTargetConfig {
  if (typeof value !== "object" || value === null || Array.isArray(value)) {
    throw configError("expected a JSON object");
  }
  const record = value as Record<string, unknown>;
  const keys = Object.keys(record);
  for (const key of keys) {
    if (!(CONFIG_KEYS as readonly string[]).includes(key)) {
      throw configError(`unknown key ${JSON.stringify(key)}`);
    }
  }
  for (const key of CONFIG_KEYS) {
    if (!(key in record)) throw configError(`missing ${key}`);
  }
  const hostSsh = record.hostSsh;
  if (typeof hostSsh !== "string") {
    throw configError("hostSsh must be a string");
  }
  const at = hostSsh.split("@");
  if (at.length !== 2 || !ACCOUNT.test(at[0]) || !HOST.test(at[1])) {
    throw configError("hostSsh must be user@host");
  }
  const vmDir = safeAbsPath(record.vmDir, "vmDir");
  for (const name of ["guestKeyFile", "guestKnownHostsFile"] as const) {
    const file = record[name];
    if (
      typeof file !== "string" || !FILE_NAME.test(file) || /^\.+$/.test(file)
    ) {
      throw configError(`${name} must be a file name inside vmDir`);
    }
  }
  for (const name of ["guestUser", "testUser"] as const) {
    const account = record[name];
    if (typeof account !== "string" || !ACCOUNT.test(account)) {
      throw configError(`${name} must be a local account name`);
    }
  }
  if (
    typeof record.guestAddress !== "string" || !IPV4.test(record.guestAddress)
  ) {
    throw configError("guestAddress must be an IPv4 address");
  }
  const testUser = record.testUser as string;
  const qaRoot = safeAbsPath(record.qaRoot, "qaRoot");
  if (!qaRoot.startsWith(`/Users/${testUser}/`)) {
    throw configError("qaRoot must be inside /Users/<testUser>/");
  }
  const port = record.marionettePort;
  if (
    !Number.isInteger(port) || (port as number) < 1024 ||
    (port as number) > 65535
  ) {
    throw configError("marionettePort must be an integer in 1024..65535");
  }
  const minFree = record.minFreeBytes;
  if (!Number.isSafeInteger(minFree) || (minFree as number) < 0) {
    throw configError("minFreeBytes must be a non-negative integer");
  }
  return Object.freeze({
    hostSsh,
    vmDir,
    guestKeyFile: record.guestKeyFile as string,
    guestKnownHostsFile: record.guestKnownHostsFile as string,
    guestUser: record.guestUser as string,
    guestAddress: record.guestAddress,
    testUser,
    qaRoot,
    marionettePort: port as number,
    minFreeBytes: minFree as number,
  });
}

/** POSIX single-quote; safe for sh, bash and zsh. */
export function shellQuote(value: string): string {
  if (value.includes("\0")) throw new Error("shell argument contains NUL");
  return "'" + value.replaceAll("'", `'"'"'`) + "'";
}

/** SHA-256 hex of a staged payload. */
const SHA256 = /^[0-9a-f]{64}$/;

/**
 * Local ssh argv that runs `script` with bash inside the guest. The script is
 * base64-encoded into the command line, so the process stdin stays free for an
 * upload stream. Both remote shells (host and guest) only ever parse quoted
 * literals and a fixed `bash -c "$(... | base64 -D)"` wrapper.
 */
export function buildGuestSshArgs(
  config: MacosQaTargetConfig,
  script: string,
): string[] {
  const encoded = encodeBase64(new TextEncoder().encode(script));
  const guestCommand = `bash -c "$(printf %s ${
    shellQuote(encoded)
  } | base64 -D)"`;
  const hostCommand = [
    "cd",
    shellQuote(config.vmDir),
    "&&",
    "exec",
    "ssh",
    "-i",
    shellQuote(config.guestKeyFile),
    "-o",
    shellQuote(`UserKnownHostsFile=${config.guestKnownHostsFile}`),
    "-o",
    "StrictHostKeyChecking=yes",
    "-o",
    "BatchMode=yes",
    "-o",
    "ConnectTimeout=15",
    shellQuote(`${config.guestUser}@${config.guestAddress}`),
    shellQuote(guestCommand),
  ].join(" ");
  return [
    "-o",
    "BatchMode=yes",
    "-o",
    "ConnectTimeout=15",
    "-o",
    "ServerAliveInterval=15",
    config.hostSsh,
    hostCommand,
  ];
}

/**
 * Shared prologue. ROOT and TASK are kept apart and joined only at run time, so
 * the literal task directory never appears in this script's own argv (cleanup
 * matches processes by that path and must not match itself).
 */
function prologue(config: MacosQaTargetConfig, taskId?: string): string {
  const lines = [
    "set -euo pipefail",
    "export LC_ALL=C",
    `ROOT=${shellQuote(config.qaRoot)}`,
    `TUSER=${shellQuote(config.testUser)}`,
    `MPORT=${config.marionettePort}`,
    "result() { printf 'FDH_RESULT %s\\n' \"$1\"; }",
    'fail() { printf \'FDH_RESULT {"ok":false,"error":"%s"}\\n\' "$1"; exit 3; }',
    "sudo -n true 2>/dev/null || fail sudo-unavailable",
    'TUID=$(id -u "$TUSER" 2>/dev/null) || fail test-user-missing',
    'gui_session() { sudo -n launchctl print "gui/$TUID" >/dev/null 2>&1; }',
  ];
  if (taskId !== undefined) {
    validateTaskId(taskId);
    lines.push(
      `TASK=${shellQuote(taskId)}`,
      'TASKDIR="$ROOT""/""$TASK"',
      'LOCK="$ROOT/.lock"',
    );
  }
  return lines.join("\n") + "\n";
}

/** Guest state for scheduling: free space, GUI session, lock holder. */
export function buildPreflightScript(config: MacosQaTargetConfig): string {
  return prologue(config) + `
FREE=$(df -k /System/Volumes/Data | awk 'NR==2 {print $4 * 1024}')
GUI=false; gui_session && GUI=true
HOLDER=null
if sudo -n test -f "$ROOT/.lock/owner"; then
  H=$(sudo -n cat "$ROOT/.lock/owner" | tr -cd 'a-z0-9-')
  HOLDER="\\"$H\\""
fi
PORTBUSY=false
if sudo -n lsof -nP -iTCP:"$MPORT" -sTCP:LISTEN >/dev/null 2>&1; then PORTBUSY=true; fi
FOREIGN=$(sudo -n ps -axo user=,command= | R="$ROOT/" U="$TUSER" awk '$1 == ENVIRON["U"] && index($0, "/Contents/MacOS/floorp") && !index($0, ENVIRON["R"]) {n++} END {print n + 0}')
OSV=$(sw_vers -productVersion | tr -cd '0-9.')
result "{\\"ok\\":true,\\"freeBytes\\":$FREE,\\"minFreeBytes\\":${config.minFreeBytes},\\"guiSession\\":$GUI,\\"lockHolder\\":$HOLDER,\\"marionettePortBusy\\":$PORTBUSY,\\"foreignFloorp\\":$FOREIGN,\\"osVersion\\":\\"$OSV\\"}"
`;
}

/** Atomic per-target lock (mkdir). Re-acquiring by the same task is a no-op. */
export function buildLockScript(
  config: MacosQaTargetConfig,
  taskId: string,
): string {
  return prologue(config, taskId) + `
sudo -n mkdir -p "$ROOT"
sudo -n chown "$TUSER" "$ROOT"
sudo -n chmod 700 "$ROOT"
if sudo -n mkdir "$LOCK" 2>/dev/null; then
  printf %s "$TASK" | sudo -n tee "$LOCK/owner" >/dev/null
  result '{"ok":true,"acquired":true}'
  exit 0
fi
OWNER=$(sudo -n cat "$LOCK/owner" 2>/dev/null || true)
if [ "$OWNER" = "$TASK" ]; then result '{"ok":true,"acquired":false}'; exit 0; fi
fail lock-held
`;
}

/** Receives the payload on stdin into the task directory. */
export function buildUploadScript(
  config: MacosQaTargetConfig,
  taskId: string,
  kind: MacosPayloadKind,
): string {
  return prologue(config, taskId) + `
[ "$(sudo -n cat "$LOCK/owner" 2>/dev/null || true)" = "$TASK" ] || fail lock-not-held
sudo -n mkdir -p "$TASKDIR"
sudo -n chmod 700 "$TASKDIR"
sudo -n sh -c 'umask 077; cat > "$1"' sh "$TASKDIR/payload.${kind}"
BYTES=$(sudo -n stat -f %z "$TASKDIR/payload.${kind}")
result "{\\"ok\\":true,\\"bytes\\":$BYTES}"
`;
}

/** Verifies the payload hash and installs Floorp.app into the task directory. */
export function buildInstallScript(
  config: MacosQaTargetConfig,
  taskId: string,
  kind: MacosPayloadKind,
  sha256: string,
): string {
  if (!SHA256.test(sha256)) throw new Error("sha256 must be 64 lowercase hex");
  const extract = kind === "dmg"
    ? `MNT=$(mktemp -d /tmp/fdh-qa-mnt.XXXXXX)
sudo -n hdiutil attach -nobrowse -readonly -noautoopen -mountpoint "$MNT" "$P" >/dev/null || fail dmg-attach-failed
APP_SRC=$(find "$MNT" -maxdepth 1 -name '*.app' -type d | head -n 1)
if [ -z "$APP_SRC" ]; then sudo -n hdiutil detach "$MNT" >/dev/null 2>&1 || true; fail app-missing; fi
sudo -n ditto "$APP_SRC" "$TASKDIR/app/Floorp.app" || { sudo -n hdiutil detach "$MNT" >/dev/null 2>&1 || true; fail copy-failed; }
sudo -n hdiutil detach "$MNT" >/dev/null || sudo -n hdiutil detach -force "$MNT" >/dev/null
rmdir "$MNT" 2>/dev/null || true`
    : `sudo -n mkdir -p "$TASKDIR/extract"
sudo -n tar -xzf "$P" -C "$TASKDIR/extract" || fail untar-failed
APP_SRC=$(sudo -n find "$TASKDIR/extract" -maxdepth 2 -name '*.app' -type d | head -n 1)
[ -n "$APP_SRC" ] || fail app-missing
sudo -n mv "$APP_SRC" "$TASKDIR/app/Floorp.app"
sudo -n rm -rf "$TASKDIR/extract"`;
  return prologue(config, taskId) + `
[ "$(sudo -n cat "$LOCK/owner" 2>/dev/null || true)" = "$TASK" ] || fail lock-not-held
P="$TASKDIR/payload.${kind}"
sudo -n test -f "$P" || fail payload-missing
GOT=$(sudo -n shasum -a 256 "$P" | awk '{print $1}')
[ "$GOT" = ${shellQuote(sha256)} ] || fail sha256-mismatch
sudo -n rm -rf "$TASKDIR/app"
sudo -n mkdir -p "$TASKDIR/app"
${extract}
sudo -n rm -f "$P"
EXE="$TASKDIR/app/Floorp.app/Contents/MacOS/floorp"
sudo -n test -x "$EXE" || fail executable-missing
sudo -n xattr -dr com.apple.quarantine "$TASKDIR/app/Floorp.app" 2>/dev/null || true
sudo -n chown -R "$TUSER" "$TASKDIR"
VER=$(sudo -n defaults read "$TASKDIR/app/Floorp.app/Contents/Info.plist" CFBundleShortVersionString 2>/dev/null | tr -cd 'A-Za-z0-9.@+-')
result "{\\"ok\\":true,\\"version\\":\\"$VER\\"}"
`;
}

/** Marionette client run with the guest's python3; reads a request from env. */
const MARIONETTE_CLIENT = String.raw`
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
    elif op == "quit":
        try:
            call("Marionette:Quit", {"flags": ["eForceQuit"]})
        except Exception:
            pass
    print("FDH_RESULT " + json.dumps(out))
except Exception as e:
    print("FDH_RESULT " + json.dumps({"ok": False, "error": "marionette-error", "detail": str(e)[:400]}))
    sys.exit(3)
`;

function marionetteSnippet(request: MarionetteRequest): string {
  const req = encodeBase64(new TextEncoder().encode(JSON.stringify(request)));
  const client = encodeBase64(new TextEncoder().encode(MARIONETTE_CLIENT));
  return `PY=/usr/local/bin/python3
[ -x "$PY" ] || PY=/Library/Frameworks/Python.framework/Versions/Current/bin/python3
[ -x "$PY" ] || fail python-missing
printf %s ${shellQuote(client)} | base64 -D > "$CLIENT"
FDH_MPORT="$MPORT" FDH_REQ=${shellQuote(req)} "$PY" "$CLIENT"
`;
}

/**
 * Launches the staged Floorp in the test user's GUI session with a fresh
 * profile and waits until Marionette answers.
 */
export function buildLaunchScript(
  config: MacosQaTargetConfig,
  taskId: string,
  startUrl = "about:blank",
): string {
  if (!/^(about:[a-z-]+|https?:\/\/[A-Za-z0-9.:/_-]{1,200})$/.test(startUrl)) {
    throw new Error("startUrl must be about:<page> or a plain http(s) URL");
  }
  return prologue(config, taskId) + `
[ "$(sudo -n cat "$LOCK/owner" 2>/dev/null || true)" = "$TASK" ] || fail lock-not-held
gui_session || fail gui-session-absent
APP="$TASKDIR/app/Floorp.app"
sudo -n test -x "$APP/Contents/MacOS/floorp" || fail not-installed
if sudo -n lsof -nP -iTCP:"$MPORT" -sTCP:LISTEN >/dev/null 2>&1; then fail marionette-port-busy; fi
PROFILE="$TASKDIR/profile"
sudo -n rm -rf "$PROFILE"
sudo -n -u "$TUSER" mkdir -p "$PROFILE"
cat <<EOF | sudo -n -u "$TUSER" tee "$PROFILE/user.js" >/dev/null
user_pref("marionette.port", $MPORT);
user_pref("browser.shell.checkDefaultBrowser", false);
user_pref("browser.startup.page", 0);
user_pref("browser.aboutwelcome.enabled", false);
user_pref("datareporting.policy.dataSubmissionEnabled", false);
user_pref("app.update.disabledForTesting", true);
user_pref("browser.sessionstore.resume_from_crash", false);
EOF
sudo -n launchctl asuser "$TUID" sudo -n -u "$TUSER" open -n -a "$APP" --env MOZ_REMOTE_ALLOW_SYSTEM_ACCESS=1 --args -no-remote -profile "$PROFILE" -marionette ${
    shellQuote(startUrl)
  } || fail open-failed
CLIENT=$(mktemp /tmp/fdh-qa-mc.XXXXXX)
trap 'rm -f "$CLIENT"' EXIT
${marionetteSnippet({ op: "ready", connectTimeout: 90 })}`;
}

/** Runs one Marionette request against the task's running Floorp. */
export function buildMarionetteScript(
  config: MacosQaTargetConfig,
  taskId: string,
  request: MarionetteRequest,
): string {
  return prologue(config, taskId) + `
[ "$(sudo -n cat "$LOCK/owner" 2>/dev/null || true)" = "$TASK" ] || fail lock-not-held
CLIENT=$(mktemp /tmp/fdh-qa-mc.XXXXXX)
trap 'rm -f "$CLIENT"' EXIT
${marionetteSnippet(request)}`;
}

/**
 * Quits Floorp, stops every process that references the task directory,
 * removes the directory and releases the lock if this task holds it. Reports
 * what remained so the caller can refuse a dirty target.
 */
export function buildCleanupScript(
  config: MacosQaTargetConfig,
  taskId: string,
): string {
  return prologue(config, taskId) + `
OWNER=$(sudo -n cat "$LOCK/owner" 2>/dev/null || true)
if [ -n "$OWNER" ] && [ "$OWNER" != "$TASK" ]; then fail lock-held; fi
CLIENT=$(mktemp /tmp/fdh-qa-mc.XXXXXX)
trap 'rm -f "$CLIENT"' EXIT
if sudo -n lsof -nP -iTCP:"$MPORT" -sTCP:LISTEN >/dev/null 2>&1; then
  ( ${
    marionetteSnippet({ op: "quit" }).trim().replaceAll("\n", "\n    ")
  } ) >/dev/null 2>&1 || true
fi
pids() { sudo -n ps -axo pid=,command= | D="$TASKDIR" awk -v self="$$" 'index($0, ENVIRON["D"]) && $1 != self {print $1}'; }
for _ in 1 2 3 4 5 6 7 8 9 10; do [ -z "$(pids)" ] && break; sleep 1; done
P=$(pids); [ -n "$P" ] && sudo -n kill -TERM $P 2>/dev/null || true
for _ in 1 2 3 4 5; do [ -z "$(pids)" ] && break; sleep 1; done
P=$(pids); [ -n "$P" ] && sudo -n kill -KILL $P 2>/dev/null || true
sleep 1
LEFT=$(pids | wc -l | tr -d ' ')
sudo -n rm -rf "$TASKDIR"
GONE=true; sudo -n test -e "$TASKDIR" && GONE=false
RELEASED=false
if [ "$OWNER" = "$TASK" ] && [ "$LEFT" = 0 ] && [ "$GONE" = true ]; then
  sudo -n rm -rf "$LOCK"; RELEASED=true
fi
result "{\\"ok\\":true,\\"residualProcesses\\":$LEFT,\\"taskDirRemoved\\":$GONE,\\"lockReleased\\":$RELEASED}"
`;
}

/** Last `FDH_RESULT <json>` line of a run, or a transport error. */
export function parseGuestResult(
  output: GuestRunOutput,
): Record<string, unknown> {
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

/** Real transport: the local `ssh` binary, two hops, no shell on this side. */
export function createSshTransport(
  config: MacosQaTargetConfig,
  options: { ssh?: string; timeoutMs?: number } = {},
): MacosQaTransport {
  const ssh = options.ssh ?? "ssh";
  const timeoutMs = options.timeoutMs ?? 300_000;
  return async (script, stdin) => {
    const child = new Deno.Command(ssh, {
      args: buildGuestSshArgs(config, script),
      stdin: stdin ? "piped" : "null",
      stdout: "piped",
      stderr: "piped",
    }).spawn();
    const timer = setTimeout(() => {
      try {
        child.kill("SIGKILL");
      } catch {
        // already exited
      }
    }, timeoutMs);
    try {
      if (stdin) await stdin.pipeTo(child.stdin);
      const out = await child.output();
      return {
        code: out.code,
        stdout: new TextDecoder().decode(out.stdout),
        stderr: new TextDecoder().decode(out.stderr),
      };
    } finally {
      clearTimeout(timer);
    }
  };
}

export class MacosQaError extends Error {
  constructor(readonly step: string, readonly result: Record<string, unknown>) {
    super(`macOS QA ${step} failed: ${String(result.error ?? "unknown")}`);
  }
}

/** Typed step wrapper over a transport. */
export class MacosQaTarget {
  constructor(
    readonly config: MacosQaTargetConfig,
    private readonly transport: MacosQaTransport,
  ) {}

  private async step(
    name: string,
    script: string,
    stdin?: ReadableStream<Uint8Array>,
  ): Promise<Record<string, unknown>> {
    const result = parseGuestResult(await this.transport(script, stdin));
    if (result.ok !== true) throw new MacosQaError(name, result);
    return result;
  }

  async preflight(): Promise<MacosQaPreflight> {
    const r = await this.step("preflight", buildPreflightScript(this.config));
    const blockers: string[] = [];
    if (
      typeof r.freeBytes !== "number" || r.freeBytes < this.config.minFreeBytes
    ) {
      blockers.push("low-disk");
    }
    if (r.guiSession !== true) blockers.push("gui-session-absent");
    if (r.lockHolder !== null) blockers.push("lock-held");
    if (r.marionettePortBusy === true) blockers.push("marionette-port-busy");
    const foreignFloorp = Number(r.foreignFloorp ?? 0);
    if (foreignFloorp > 0) blockers.push("foreign-floorp-running");
    return {
      freeBytes: Number(r.freeBytes),
      guiSession: r.guiSession === true,
      lockHolder: typeof r.lockHolder === "string" ? r.lockHolder : null,
      marionettePortBusy: r.marionettePortBusy === true,
      foreignFloorp,
      osVersion: String(r.osVersion ?? ""),
      ready: blockers.length === 0,
      blockers,
    };
  }

  acquire(taskId: string) {
    return this.step("lock", buildLockScript(this.config, taskId));
  }

  async stage(
    taskId: string,
    kind: MacosPayloadKind,
    payload: ReadableStream<Uint8Array>,
    sha256: string,
  ): Promise<{ bytes: number; version: string }> {
    const up = await this.step(
      "upload",
      buildUploadScript(this.config, taskId, kind),
      payload,
    );
    const inst = await this.step(
      "install",
      buildInstallScript(this.config, taskId, kind, sha256),
    );
    return { bytes: Number(up.bytes), version: String(inst.version ?? "") };
  }

  async launch(
    taskId: string,
    startUrl?: string,
  ): Promise<{ version: string }> {
    const r = await this.step(
      "launch",
      buildLaunchScript(this.config, taskId, startUrl),
    );
    return { version: String(r.value ?? "") };
  }

  async evaluate(taskId: string, script: string): Promise<unknown> {
    const r = await this.step(
      "eval",
      buildMarionetteScript(this.config, taskId, { op: "eval", script }),
    );
    return r.value;
  }

  async screenshot(taskId: string): Promise<string> {
    const r = await this.step(
      "screenshot",
      buildMarionetteScript(this.config, taskId, { op: "screenshot" }),
    );
    if (typeof r.png !== "string") {
      throw new MacosQaError("screenshot", { error: "no-image" });
    }
    return r.png;
  }

  async cleanup(taskId: string): Promise<MacosQaCleanup> {
    const r = await this.step(
      "cleanup",
      buildCleanupScript(this.config, taskId),
    );
    const residualProcesses = Number(r.residualProcesses);
    const taskDirRemoved = r.taskDirRemoved === true;
    const lockReleased = r.lockReleased === true;
    return {
      residualProcesses,
      taskDirRemoved,
      lockReleased,
      clean: residualProcesses === 0 && taskDirRemoved && lockReleased,
    };
  }
}
