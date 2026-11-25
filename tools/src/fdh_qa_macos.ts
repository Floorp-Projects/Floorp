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
import {
  createRemoteTransport,
  marionettePayload,
  parseGuestResult,
  QaTargetError,
  shellQuote,
  wrapForStdin,
} from "./fdh_qa_remote.ts";

export { parseGuestResult, shellQuote, wrapForStdin };

import type {
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
  if (record.guestUser === testUser) {
    throw configError("guestUser and testUser must differ");
  }
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

/** SHA-256 hex of a staged payload. */
const SHA256 = /^[0-9a-f]{64}$/;

/** Longest argv-mode host command; Windows CreateProcess caps at 32767. */
const MAX_ARGV_COMMAND = 24_000;

/**
 * Local ssh argv that runs a guest script with bash. In `stdin` mode the guest
 * runs `bash -s` and the caller writes the script to stdin. In `argv` mode
 * (used when stdin carries an upload) the script is base64-encoded into the
 * command line. Both remote shells (host and guest) only ever parse quoted
 * literals and a fixed wrapper.
 */
export function buildGuestSshArgs(
  config: MacosQaTargetConfig,
  script: string,
  mode: "stdin" | "argv",
): string[] {
  const guestCommand = mode === "stdin"
    ? "bash -s"
    : `bash -c "$(printf %s ${
      shellQuote(encodeBase64(new TextEncoder().encode(script)))
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
  if (hostCommand.length > MAX_ARGV_COMMAND) {
    throw new Error("guest script too long for argv mode");
  }
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
 * Root-owned lock outside the test user's tree, so the browser under test
 * cannot forge or remove it. The lock is a symlink whose target is the owner
 * task id: `ln -s` creates it and records the owner in one atomic step.
 */
const LOCK_DIR = "/private/var/db/fdh-qa";

/**
 * Shared prologue. ROOT and TASK are kept apart and joined only at run time, so
 * the literal task directory never appears in this script's own argv (cleanup
 * matches processes by that path and must not match itself). Commands that
 * touch the task tree run as the test user (`asu`), never as root, so a
 * planted symlink cannot redirect a root write.
 */
function prologue(config: MacosQaTargetConfig, taskId?: string): string {
  const lines = [
    "set -euo pipefail",
    "export LC_ALL=C",
    "cd /",
    `ROOT=${shellQuote(config.qaRoot)}`,
    `TUSER=${shellQuote(config.testUser)}`,
    `MPORT=${config.marionettePort}`,
    `LOCKDIR=${shellQuote(LOCK_DIR)}`,
    'LOCK="$LOCKDIR/lock"',
    "result() { printf 'FDH_RESULT %s\\n' \"$1\"; }",
    'fail() { printf \'FDH_RESULT {"ok":false,"error":"%s"}\\n\' "$1"; exit 3; }',
    "sudo -n true 2>/dev/null || fail sudo-unavailable",
    'TUID=$(id -u "$TUSER" 2>/dev/null) || fail test-user-missing',
    'asu() { sudo -n -u "$TUSER" "$@"; }',
    'gui_session() { sudo -n launchctl print "gui/$TUID" >/dev/null 2>&1; }',
    // Owner task id, "-unowned" for a lock that is not a symlink, or empty.
    'lock_owner() { if sudo -n test -L "$LOCK"; then sudo -n readlink "$LOCK" | tr -cd \'a-z0-9-\'; elif sudo -n test -e "$LOCK"; then printf %s -unowned; fi; }',
    'port_busy() { sudo -n lsof -nP -iTCP:"$MPORT" -sTCP:LISTEN >/dev/null 2>&1; }',
  ];
  if (taskId !== undefined) {
    validateTaskId(taskId);
    lines.push(
      `TASK=${shellQuote(taskId)}`,
      'TASKDIR="$ROOT""/""$TASK"',
      'own_lock() { [ "$(lock_owner)" = "$TASK" ] || fail lock-not-held; }',
      // The test user owns its home; a symlinked root would redirect writes.
      'sudo -n test -L "$ROOT" && fail qa-root-symlink',
    );
  }
  return lines.join("\n") + "\n";
}

/** Guest state for scheduling: free space, GUI session, lock holder. */
export function buildPreflightScript(config: MacosQaTargetConfig): string {
  return prologue(config) + `
FREE=$(df -k /System/Volumes/Data | awk 'NR==2 {print $4 * 1024}')
GUI=false; gui_session && GUI=true
H=$(lock_owner)
HOLDER=null; [ -n "$H" ] && HOLDER="\\"$H\\""
PORTBUSY=false; port_busy && PORTBUSY=true
FOREIGN=$(sudo -n ps -axwwo user=,command= | R="$ROOT/" U="$TUSER" awk '$1 == ENVIRON["U"] && index($0, "/Contents/MacOS/floorp") && !index($0, ENVIRON["R"]) {n++} END {print n + 0}')
OSV=$(sw_vers -productVersion | tr -cd '0-9.')
result "{\\"ok\\":true,\\"freeBytes\\":$FREE,\\"minFreeBytes\\":${config.minFreeBytes},\\"guiSession\\":$GUI,\\"lockHolder\\":$HOLDER,\\"marionettePortBusy\\":$PORTBUSY,\\"foreignFloorp\\":$FOREIGN,\\"osVersion\\":\\"$OSV\\"}"
`;
}

/** Atomic per-target lock. Re-acquiring by the same task is a no-op. */
export function buildLockScript(
  config: MacosQaTargetConfig,
  taskId: string,
): string {
  return prologue(config, taskId) + `
sudo -n mkdir -p "$LOCKDIR"
sudo -n chown root:wheel "$LOCKDIR"
sudo -n chmod 700 "$LOCKDIR"
asu mkdir -p "$ROOT"
asu chmod 700 "$ROOT"
if sudo -n ln -s "$TASK" "$LOCK" 2>/dev/null && [ "$(lock_owner)" = "$TASK" ]; then
  result '{"ok":true,"acquired":true}'
  exit 0
fi
if [ "$(lock_owner)" = "$TASK" ]; then result '{"ok":true,"acquired":false}'; exit 0; fi
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
own_lock
asu mkdir -p "$TASKDIR"
asu chmod 700 "$TASKDIR"
asu sh -c 'umask 077; cat > "$1"' sh "$TASKDIR/payload.${kind}"
BYTES=$(asu stat -f %z "$TASKDIR/payload.${kind}")
result "{\\"ok\\":true,\\"bytes\\":$BYTES}"
`;
}

/**
 * Verifies the payload hash and installs Floorp.app into the task directory.
 * The payload is first copied (read as the test user) into a root-only
 * snapshot; the hash check, the dmg attach and the tar read all use that
 * snapshot, so the test user cannot swap the file after it was checked.
 */
export function buildInstallScript(
  config: MacosQaTargetConfig,
  taskId: string,
  kind: MacosPayloadKind,
  sha256: string,
): string {
  if (!SHA256.test(sha256)) throw new Error("sha256 must be 64 lowercase hex");
  const extract = kind === "dmg"
    ? `MNT=$(mktemp -d /tmp/fdh-qa-mnt.XXXXXX)
sudo -n hdiutil attach -nobrowse -readonly -noautoopen -mountpoint "$MNT" "$SNAP" >/dev/null </dev/null || fail dmg-attach-failed
APP_SRC=$(find "$MNT" -maxdepth 1 -name '*.app' -type d | head -n 1) || true
[ -n "$APP_SRC" ] || fail app-missing
asu ditto "$APP_SRC" "$TASKDIR/app/Floorp.app" || fail copy-failed
detach_mnt`
    : `asu mkdir -p "$TASKDIR/extract"
sudo -n cat "$SNAP" | asu tar -xzf - -C "$TASKDIR/extract" || fail untar-failed
APP_SRC=$(asu find "$TASKDIR/extract" -maxdepth 2 -name '*.app' -type d | head -n 1) || true
[ -n "$APP_SRC" ] || fail app-missing
asu mv "$APP_SRC" "$TASKDIR/app/Floorp.app"
asu rm -rf "$TASKDIR/extract"`;
  return prologue(config, taskId) + `
own_lock
P="$TASKDIR/payload.${kind}"
asu test -f "$P" || fail payload-missing
MNT=""; SNAP=""
detach_mnt() { if [ -n "$MNT" ]; then sudo -n hdiutil detach "$MNT" >/dev/null 2>&1 || sudo -n hdiutil detach -force "$MNT" >/dev/null 2>&1 || true; rmdir "$MNT" 2>/dev/null || true; MNT=""; fi; }
on_exit() { detach_mnt; if [ -n "$SNAP" ]; then sudo -n rm -f "$SNAP"; fi; }
trap on_exit EXIT
SNAP=$(sudo -n mktemp "$LOCKDIR/payload.XXXXXX")
asu cat "$P" | sudo -n tee "$SNAP" >/dev/null || fail snapshot-failed
GOT=$(sudo -n shasum -a 256 "$SNAP" | awk '{print $1}')
[ "$GOT" = ${shellQuote(sha256)} ] || fail sha256-mismatch
asu rm -rf "$TASKDIR/app"
asu mkdir -p "$TASKDIR/app"
${extract}
asu rm -f "$P"
asu test -x "$TASKDIR/app/Floorp.app/Contents/MacOS/floorp" || fail executable-missing
asu xattr -dr com.apple.quarantine "$TASKDIR/app/Floorp.app" 2>/dev/null || true
VER=$(asu defaults read "$TASKDIR/app/Floorp.app/Contents/Info.plist" CFBundleShortVersionString 2>/dev/null | tr -cd 'A-Za-z0-9.@+-') || true
result "{\\"ok\\":true,\\"version\\":\\"$VER\\"}"
`;
}

function marionetteSnippet(request: MarionetteRequest): string {
  const { client, request: req } = marionettePayload(request);
  return `PY=/usr/local/bin/python3
[ -x "$PY" ] || PY=/Library/Frameworks/Python.framework/Versions/Current/bin/python3
[ -x "$PY" ] || fail python-missing
CLIENT=$(mktemp /tmp/fdh-qa-mc.XXXXXX)
trap 'rm -f "$CLIENT"' EXIT
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
own_lock
gui_session || fail gui-session-absent
APP="$TASKDIR/app/Floorp.app"
asu test -x "$APP/Contents/MacOS/floorp" || fail not-installed
port_busy && fail marionette-port-busy
PROFILE="$TASKDIR/profile"
asu rm -rf "$PROFILE"
asu mkdir -p "$PROFILE"
cat <<EOF | asu tee "$PROFILE/user.js" >/dev/null
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
${marionetteSnippet({ op: "ready", connectTimeout: 90 })}`;
}

/** Runs one Marionette request against the task's running Floorp. */
export function buildMarionetteScript(
  config: MacosQaTargetConfig,
  taskId: string,
  request: MarionetteRequest,
): string {
  return prologue(config, taskId) + `
own_lock
${marionetteSnippet(request)}`;
}

/**
 * Stops every process that references the task directory (TERM, then KILL),
 * removes the directory and releases the lock if this task holds it. Nothing
 * outside the task directory is signalled. Reports what remained so the
 * caller can refuse a dirty target.
 */
export function buildCleanupScript(
  config: MacosQaTargetConfig,
  taskId: string,
): string {
  return prologue(config, taskId) + `
OWNER=$(lock_owner)
if [ -n "$OWNER" ] && [ "$OWNER" != "$TASK" ]; then fail lock-held; fi
pids() { sudo -n ps -axwwo pid=,command= | D="$TASKDIR/" awk -v self="$$" 'index($0, ENVIRON["D"]) && $1 != self {print $1}'; }
P=$(pids); [ -n "$P" ] && sudo -n kill -TERM $P 2>/dev/null || true
for _ in 1 2 3 4 5 6 7 8 9 10; do [ -z "$(pids)" ] && break; sleep 1; done
P=$(pids); [ -n "$P" ] && sudo -n kill -KILL $P 2>/dev/null || true
sleep 1
LEFT=$(pids | wc -l | tr -d ' ')
asu rm -rf "$TASKDIR" 2>/dev/null || true
GONE=true; sudo -n test -e "$TASKDIR" && GONE=false
RELEASED=false
if [ "$OWNER" = "$TASK" ] && [ "$LEFT" = 0 ] && [ "$GONE" = true ]; then
  sudo -n rm -f "$LOCK"; RELEASED=true
fi
result "{\\"ok\\":true,\\"residualProcesses\\":$LEFT,\\"taskDirRemoved\\":$GONE,\\"lockReleased\\":$RELEASED}"
`;
}

/** Real transport: the local `ssh` binary, two hops (host, then guest). */
export function createSshTransport(
  config: MacosQaTargetConfig,
  options: Parameters<typeof createRemoteTransport>[1] = {},
): MacosQaTransport {
  return createRemoteTransport(
    (script, mode) => buildGuestSshArgs(config, script, mode),
    options,
  );
}

export class MacosQaError extends QaTargetError {
  constructor(step: string, result: Record<string, unknown>) {
    super("macOS", step, result);
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
    const foreignFloorp = typeof r.foreignFloorp === "number"
      ? r.foreignFloorp
      : Number.NaN;
    if (!(foreignFloorp === 0)) blockers.push("foreign-floorp-running");
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
