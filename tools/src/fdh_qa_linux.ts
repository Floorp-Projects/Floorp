// SPDX-License-Identifier: MPL-2.0

/**
 * fdh Linux QA target (KAN-142 Agentic QA, KAN-144 Linux side).
 *
 * Each task gets one fresh Docker container on a Linux host: Xvfb + openbox
 * as the desktop, a non-root `qa` user, no capabilities, resource limits and
 * (by default) no network. The build is uploaded into the container, checked
 * against its SHA-256, extracted and launched with a fresh profile and
 * Marionette. Cleanup removes the container, so the reset is complete for
 * everything inside it.
 *
 * One container name per target doubles as the lock: `docker run --name` is
 * atomic, and the container carries the owning task id as a label. Commands
 * only ever touch the container with that exact name and the `fdh.qa=1`
 * label, never other containers on the host.
 *
 * The image is defined here (Dockerfile + entrypoint) and tagged by the
 * SHA-256 of that definition, so a changed definition builds a new image.
 *
 * Not covered: real desktop sessions (KDE/GNOME/Wayland), GPU drivers and
 * distro-specific integration. Not QA qualification or a QA3 replacement.
 */

import { encodeBase64 } from "@std/encoding/base64";
import { encodeHex } from "@std/encoding/hex";
import { validateTaskId } from "./fdh_task.ts";
import {
  createRemoteTransport,
  marionettePayload,
  parseGuestResult,
  QaTargetError,
  shellQuote,
} from "./fdh_qa_remote.ts";
import type {
  LinuxPayloadKind,
  LinuxQaCleanup,
  LinuxQaPreflight,
  LinuxQaTargetConfig,
  QaTransport,
} from "./fdh_qa_linux_types.ts";
import type { MarionetteRequest } from "./fdh_qa_remote_types.ts";

export type {
  LinuxPayloadKind,
  LinuxQaCleanup,
  LinuxQaNetwork,
  LinuxQaPreflight,
  LinuxQaTargetConfig,
} from "./fdh_qa_linux_types.ts";

/**
 * Image definition. Changing either text changes the image tag. The base is
 * pinned by digest and the build never pulls, so a build cannot move the
 * host's own `debian:trixie-slim` tag. Packages still come from the Debian
 * archive at build time, so two builds of one tag can differ in package
 * versions.
 */
export const LINUX_QA_DOCKERFILE = `# SPDX-License-Identifier: MPL-2.0
FROM debian:trixie-slim@sha256:8aa5a7bf5a1955bddbe7d74fcdd0b91ab30e74362a8b73af672b0481b35b44a0
RUN apt-get update \\
 && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \\
      xvfb openbox dbus-x11 python3 procps xz-utils bzip2 ca-certificates \\
      libgtk-3-0t64 libasound2t64 libx11-xcb1 libxtst6 libpci3 \\
      libegl1 libgl1 libgl1-mesa-dri fonts-dejavu-core fonts-noto-cjk \\
 && rm -rf /var/lib/apt/lists/* \\
 && useradd --create-home --uid 10001 qa
COPY entrypoint.sh /usr/local/bin/fdh-qa-entrypoint
RUN chmod 0755 /usr/local/bin/fdh-qa-entrypoint
USER qa
WORKDIR /home/qa
ENV DISPLAY=:99
ENTRYPOINT ["/usr/local/bin/fdh-qa-entrypoint"]
`;

export const LINUX_QA_ENTRYPOINT = `#!/bin/sh
set -eu
Xvfb :99 -screen 0 1280x800x24 -nolisten tcp >/tmp/xvfb.log 2>&1 &
i=0
while [ ! -e /tmp/.X11-unix/X99 ] && [ "$i" -lt 100 ]; do i=$((i + 1)); sleep 0.1; done
openbox >/tmp/openbox.log 2>&1 &
exec sleep infinity
`;

/** `fdh-qa-linux:<first 12 hex of SHA-256(Dockerfile \\0 entrypoint)>`. */
export async function linuxQaImageTag(): Promise<string> {
  const bytes = new TextEncoder().encode(
    LINUX_QA_DOCKERFILE + "\0" + LINUX_QA_ENTRYPOINT,
  );
  const digest = encodeHex(await crypto.subtle.digest("SHA-256", bytes));
  return `fdh-qa-linux:${digest.slice(0, 12)}`;
}

const ACCOUNT = /^[a-z_][a-z0-9_-]{0,31}$/;
const HOST = /^[A-Za-z0-9][A-Za-z0-9.-]{0,252}$/;
const CONTAINER = /^fdh-qa-[a-z0-9-]{1,40}$/;
const IMAGE_TAG = /^fdh-qa-linux:[0-9a-f]{12}$/;
const SHA256 = /^[0-9a-f]{64}$/;
const CONFIG_KEYS = [
  "hostSsh",
  "containerName",
  "memoryMb",
  "cpus",
  "network",
  "marionettePort",
  "minFreeBytes",
  "minAvailableMemoryMb",
] as const;

function configError(detail: string): Error {
  return new Error(`invalid fdh Linux QA target config: ${detail}`);
}

/** Strict parse: exact key set, no shell metacharacters, bounded limits. */
export function parseLinuxQaTargetConfig(value: unknown): LinuxQaTargetConfig {
  if (typeof value !== "object" || value === null || Array.isArray(value)) {
    throw configError("expected a JSON object");
  }
  const record = value as Record<string, unknown>;
  for (const key of Object.keys(record)) {
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
  if (
    typeof record.containerName !== "string" ||
    !CONTAINER.test(record.containerName)
  ) {
    throw configError("containerName must match fdh-qa-[a-z0-9-]+");
  }
  const memoryMb = record.memoryMb;
  if (
    !Number.isInteger(memoryMb) || (memoryMb as number) < 512 ||
    (memoryMb as number) > 16384
  ) {
    throw configError("memoryMb must be an integer in 512..16384");
  }
  const cpus = record.cpus;
  if (
    typeof cpus !== "number" || !Number.isFinite(cpus) || cpus < 0.5 ||
    cpus > 16 || !/^\d+(\.\d{1,2})?$/.test(String(cpus))
  ) {
    throw configError("cpus must be a number in 0.5..16 (two decimals)");
  }
  if (record.network !== "none" && record.network !== "bridge") {
    throw configError('network must be "none" or "bridge"');
  }
  const port = record.marionettePort;
  if (
    !Number.isInteger(port) || (port as number) < 1024 ||
    (port as number) > 65535
  ) {
    throw configError("marionettePort must be an integer in 1024..65535");
  }
  for (const name of ["minFreeBytes", "minAvailableMemoryMb"] as const) {
    const n = record[name];
    if (!Number.isSafeInteger(n) || (n as number) < 0) {
      throw configError(`${name} must be a non-negative integer`);
    }
  }
  return Object.freeze({
    hostSsh,
    containerName: record.containerName,
    memoryMb: memoryMb as number,
    cpus,
    network: record.network,
    marionettePort: port as number,
    minFreeBytes: record.minFreeBytes as number,
    minAvailableMemoryMb: record.minAvailableMemoryMb as number,
  });
}

/** Size of the container's writable home (a tmpfs, counted as memory). */
const HOME_TMPFS_MB = 1024;

/** Longest argv-mode remote command; Windows CreateProcess caps at 32767. */
const MAX_ARGV_COMMAND = 24_000;

/**
 * Local ssh argv for one host script. In `stdin` mode the host runs
 * `bash -s` and the script arrives on stdin; in `argv` mode (stdin carries an
 * upload) the script is base64-encoded into the command line.
 */
export function buildHostSshArgs(
  config: LinuxQaTargetConfig,
  script: string,
  mode: "stdin" | "argv",
): string[] {
  const command = mode === "stdin"
    ? "bash -s"
    : `bash -c "$(printf %s ${
      shellQuote(encodeBase64(new TextEncoder().encode(script)))
    } | base64 -d)"`;
  if (command.length > MAX_ARGV_COMMAND) {
    throw new Error("host script too long for argv mode");
  }
  return [
    "-o",
    "BatchMode=yes",
    "-o",
    "ConnectTimeout=15",
    "-o",
    "ServerAliveInterval=15",
    config.hostSsh,
    command,
  ];
}

function prologue(config: LinuxQaTargetConfig, image: string): string {
  if (!IMAGE_TAG.test(image)) throw new Error("invalid image tag");
  return [
    "set -euo pipefail",
    "export LC_ALL=C",
    "cd /",
    `NAME=${shellQuote(config.containerName)}`,
    `IMAGE=${shellQuote(image)}`,
    `MPORT=${config.marionettePort}`,
    "result() { printf 'FDH_RESULT %s\\n' \"$1\"; }",
    'fail() { printf \'FDH_RESULT {"ok":false,"error":"%s"}\\n\' "$1"; exit 3; }',
    // Failure with a sanitised detail (e.g. Docker's own error text).
    'failx() { D=$(printf %s "$2" | tr -cd \'A-Za-z0-9 ._:/=-\' | tail -c 300); printf \'FDH_RESULT {"ok":false,"error":"%s","detail":"%s"}\\n\' "$1" "$D"; exit 3; }',
    "command -v docker >/dev/null 2>&1 || fail docker-missing",
    "docker info >/dev/null 2>&1 || fail docker-unavailable",
    // Task id of our container, "-foreign" for a same-named container we did
    // not create, or empty when there is none.
    'holder() { local L; L=$(docker container inspect -f \'{{index .Config.Labels "fdh.qa"}}|{{index .Config.Labels "fdh.qa.task"}}\' "$NAME" 2>/dev/null) || return 0; ' +
    'case "$L" in 1\\|*) printf %s "${L#1|}" | tr -cd \'a-z0-9-\' ;; *) printf %s -foreign ;; esac; }',
  ].join("\n") + "\n";
}

function taskPrologue(
  config: LinuxQaTargetConfig,
  image: string,
  taskId: string,
): string {
  validateTaskId(taskId);
  return prologue(config, image) + [
    `TASK=${shellQuote(taskId)}`,
    // Resolve the container ID once, after the ownership check, and use it
    // for every later command: a script that outlives a timeout can then
    // never reach a newer container that reuses the name.
    "label_task() { docker container inspect -f '{{index .Config.Labels \"fdh.qa.task\"}}' \"$1\" 2>/dev/null | tr -cd 'a-z0-9-'; }",
    'own() { [ "$(holder)" = "$TASK" ] || fail container-not-held; CID=$(docker container inspect -f \'{{.Id}}\' "$NAME" 2>/dev/null | tr -cd \'0-9a-f\'); [ -n "$CID" ] && [ "$(label_task "$CID")" = "$TASK" ] || fail container-not-held; [ "$(docker container inspect -f \'{{.State.Running}}\' "$CID")" = true ] || fail container-not-running; }',
    // docker exec as the unprivileged qa user, never root.
    'qx() { docker exec -u qa "$CID" "$@"; }',
  ].join("\n") + "\n";
}

/** Host state for scheduling: disk, memory, image, current container. */
export function buildPreflightScript(
  config: LinuxQaTargetConfig,
  image: string,
): string {
  return prologue(config, image) + `
ROOTDIR=$(docker info -f '{{.DockerRootDir}}')
FREE=$(df -Pk "$ROOTDIR" | awk 'NR==2 {print $4 * 1024}'); [ -n "$FREE" ] || FREE=0
AVAIL=$(awk '/^MemAvailable:/ {print int($2 / 1024)}' /proc/meminfo); [ -n "$AVAIL" ] || AVAIL=0
IMG=false; docker image inspect "$IMAGE" >/dev/null 2>&1 && IMG=true
H=$(holder)
HOLDER=null; [ -n "$H" ] && HOLDER="\\"$H\\""
result "{\\"ok\\":true,\\"freeBytes\\":$FREE,\\"availableMemoryMb\\":$AVAIL,\\"imagePresent\\":$IMG,\\"containerHolder\\":$HOLDER}"
`;
}

/** Builds the image when it is missing; a present image is left alone. */
export function buildImageScript(
  config: LinuxQaTargetConfig,
  image: string,
): string {
  const dockerfile = encodeBase64(
    new TextEncoder().encode(LINUX_QA_DOCKERFILE),
  );
  const entrypoint = encodeBase64(
    new TextEncoder().encode(LINUX_QA_ENTRYPOINT),
  );
  return prologue(config, image) + `
if docker image inspect "$IMAGE" >/dev/null 2>&1; then result '{"ok":true,"built":false}'; exit 0; fi
CTX=$(mktemp -d /tmp/fdh-qa-image.XXXXXX)
trap 'rm -rf "$CTX"' EXIT
printf %s ${shellQuote(dockerfile)} | base64 -d > "$CTX/Dockerfile"
printf %s ${shellQuote(entrypoint)} | base64 -d > "$CTX/entrypoint.sh"
if ! docker build -q -t "$IMAGE" --label fdh.qa.image=1 "$CTX" > "$CTX/build.log" 2>&1; then
  failx image-build-failed "$(tail -c 600 "$CTX/build.log")"
fi
result '{"ok":true,"built":true}'
`;
}

/** Creates the task's container; the container name is the lock. */
export function buildAcquireScript(
  config: LinuxQaTargetConfig,
  image: string,
  taskId: string,
): string {
  return taskPrologue(config, image, taskId) + `
docker image inspect "$IMAGE" >/dev/null 2>&1 || fail image-missing
H=$(holder)
if [ "$H" = "$TASK" ]; then result '{"ok":true,"acquired":false}'; exit 0; fi
[ -z "$H" ] || fail container-held
if ! ERR=$(docker run -d --init --name "$NAME" \\
    --label fdh.qa=1 --label "fdh.qa.task=$TASK" \\
    --memory ${config.memoryMb}m --memory-swap ${config.memoryMb}m \\
    --cpus ${config.cpus} --pids-limit 2048 --shm-size 512m \\
    --read-only \\
    --tmpfs /home/qa:rw,exec,nosuid,nodev,size=${HOME_TMPFS_MB}m,uid=10001,gid=10001,mode=0700 \\
    --tmpfs /tmp:rw,nosuid,nodev,size=256m,mode=1777 \\
    --cap-drop ALL --security-opt no-new-privileges \\
    --network ${config.network} "$IMAGE" 2>&1 >/dev/null); then
  H=$(holder)
  if [ "$H" != "$TASK" ]; then
    [ -n "$H" ] && fail container-held
    failx container-create-failed "$ERR"
  fi
fi
own
for _ in $(seq 1 50); do qx test -e /tmp/.X11-unix/X99 2>/dev/null && break; sleep 0.2; done
qx test -e /tmp/.X11-unix/X99 || fail display-not-ready
result '{"ok":true,"acquired":true}'
`;
}

/** Receives the payload on stdin into the container. */
export function buildUploadScript(
  config: LinuxQaTargetConfig,
  image: string,
  taskId: string,
): string {
  return taskPrologue(config, image, taskId) + `
own
docker exec -i -u qa "$CID" sh -c 'umask 077; cat > /home/qa/payload'
BYTES=$(qx stat -c %s /home/qa/payload)
result "{\\"ok\\":true,\\"bytes\\":$BYTES}"
`;
}

/** Verifies the payload hash and extracts Floorp into the container. */
export function buildInstallScript(
  config: LinuxQaTargetConfig,
  image: string,
  taskId: string,
  kind: LinuxPayloadKind,
  sha256: string,
): string {
  if (!SHA256.test(sha256)) throw new Error("sha256 must be 64 lowercase hex");
  const flag = kind === "tar.xz" ? "J" : kind === "tar.bz2" ? "j" : "z";
  return taskPrologue(config, image, taskId) + `
own
qx test -f /home/qa/payload || fail payload-missing
GOT=$(qx sha256sum /home/qa/payload | awk '{print $1}')
[ "$GOT" = ${shellQuote(sha256)} ] || fail sha256-mismatch
qx rm -rf /home/qa/app
qx mkdir -p /home/qa/app
qx tar -x${flag}f /home/qa/payload -C /home/qa/app || fail untar-failed
qx rm -f /home/qa/payload
qx test -x /home/qa/app/floorp/floorp || fail executable-missing
VER=$(qx sed -n 's/^Version=//p' /home/qa/app/floorp/application.ini | head -n 1 | tr -cd 'A-Za-z0-9.@+-') || true
result "{\\"ok\\":true,\\"version\\":\\"$VER\\"}"
`;
}

function marionetteSnippet(request: MarionetteRequest): string {
  const { client, request: req } = marionettePayload(request);
  // The request rides in the stdin stream (printf is a builtin), so a large
  // eval script never hits the per-argument limit of docker exec.
  return `{ printf 'import os\\nos.environ["FDH_REQ"] = "%s"\\n' ${
    shellQuote(req)
  }; printf %s ${
    shellQuote(client)
  } | base64 -d; } | docker exec -i -u qa -e FDH_MPORT="$MPORT" "$CID" python3 -
`;
}

/** Launches Floorp on the container's display with a fresh profile. */
export function buildLaunchScript(
  config: LinuxQaTargetConfig,
  image: string,
  taskId: string,
  startUrl = "about:blank",
): string {
  if (!/^(about:[a-z-]+|https?:\/\/[A-Za-z0-9.:/_-]{1,200})$/.test(startUrl)) {
    throw new Error("startUrl must be about:<page> or a plain http(s) URL");
  }
  return taskPrologue(config, image, taskId) + `
own
qx test -x /home/qa/app/floorp/floorp || fail not-installed
qx sh -c 'pgrep -u qa -x floorp >/dev/null' && fail already-running
qx rm -rf /home/qa/profile
qx mkdir -p /home/qa/profile
docker exec -i -u qa "$CID" sh -c 'cat > /home/qa/profile/user.js' <<EOF
user_pref("marionette.port", $MPORT);
user_pref("browser.shell.checkDefaultBrowser", false);
user_pref("browser.startup.page", 0);
user_pref("browser.aboutwelcome.enabled", false);
user_pref("datareporting.policy.dataSubmissionEnabled", false);
user_pref("app.update.disabledForTesting", true);
user_pref("browser.sessionstore.resume_from_crash", false);
EOF
docker exec -d -u qa -e DISPLAY=:99 -e MOZ_REMOTE_ALLOW_SYSTEM_ACCESS=1 "$CID" \\
  /home/qa/app/floorp/floorp -no-remote -profile /home/qa/profile -marionette ${
    shellQuote(startUrl)
  } || fail launch-failed
${marionetteSnippet({ op: "ready", connectTimeout: 90 })}`;
}

/** Runs one Marionette request against the task's running Floorp. */
export function buildMarionetteScript(
  config: LinuxQaTargetConfig,
  image: string,
  taskId: string,
  request: MarionetteRequest,
): string {
  return taskPrologue(config, image, taskId) + `
own
${marionetteSnippet(request)}`;
}

/** Removes the task's container; refuses a container of another task. */
export function buildCleanupScript(
  config: LinuxQaTargetConfig,
  image: string,
  taskId: string,
): string {
  return taskPrologue(config, image, taskId) + `
H=$(holder)
if [ -n "$H" ] && [ "$H" != "$TASK" ]; then fail container-held; fi
CID=""
if [ -n "$H" ]; then
  CID=$(docker container inspect -f '{{.Id}}' "$NAME" 2>/dev/null | tr -cd '0-9a-f') || true
  if [ -n "$CID" ] && [ "$(label_task "$CID")" = "$TASK" ]; then
    docker rm -f "$CID" >/dev/null 2>&1 || true
  fi
fi
GONE=true
if [ -n "$CID" ] && docker container inspect "$CID" >/dev/null 2>&1; then GONE=false; fi
result "{\\"ok\\":true,\\"containerRemoved\\":$GONE}"
`;
}

/** Real transport: the local `ssh` binary, one hop to the Docker host. */
export function createLinuxTransport(
  config: LinuxQaTargetConfig,
  options: Parameters<typeof createRemoteTransport>[1] = {},
): QaTransport {
  return createRemoteTransport(
    (script, mode) => buildHostSshArgs(config, script, mode),
    { timeoutMs: 900_000, ...options },
  );
}

export class LinuxQaError extends QaTargetError {
  constructor(step: string, result: Record<string, unknown>) {
    super("Linux", step, result);
  }
}

/** Typed step wrapper over a transport. */
export class LinuxQaTarget {
  constructor(
    readonly config: LinuxQaTargetConfig,
    readonly image: string,
    private readonly transport: QaTransport,
  ) {
    if (!IMAGE_TAG.test(image)) throw new Error("invalid image tag");
  }

  static async create(
    config: LinuxQaTargetConfig,
    transport: QaTransport,
  ): Promise<LinuxQaTarget> {
    return new LinuxQaTarget(config, await linuxQaImageTag(), transport);
  }

  private async step(
    name: string,
    script: string,
    stdin?: ReadableStream<Uint8Array>,
  ): Promise<Record<string, unknown>> {
    const result = parseGuestResult(await this.transport(script, stdin));
    if (result.ok !== true) throw new LinuxQaError(name, result);
    return result;
  }

  async preflight(): Promise<LinuxQaPreflight> {
    const r = await this.step(
      "preflight",
      buildPreflightScript(this.config, this.image),
    );
    const blockers: string[] = [];
    const freeBytes = typeof r.freeBytes === "number" ? r.freeBytes : NaN;
    const mem = typeof r.availableMemoryMb === "number"
      ? r.availableMemoryMb
      : NaN;
    if (!(freeBytes >= this.config.minFreeBytes)) blockers.push("low-disk");
    if (!(mem >= this.config.minAvailableMemoryMb)) {
      blockers.push("low-memory");
    }
    if (r.imagePresent !== true) blockers.push("image-missing");
    const holder = typeof r.containerHolder === "string"
      ? r.containerHolder
      : null;
    if (holder !== null) blockers.push("container-held");
    return {
      freeBytes,
      availableMemoryMb: mem,
      imagePresent: r.imagePresent === true,
      containerHolder: holder,
      ready: blockers.length === 0,
      blockers,
    };
  }

  async ensureImage(): Promise<{ built: boolean }> {
    const r = await this.step(
      "image",
      buildImageScript(this.config, this.image),
    );
    return { built: r.built === true };
  }

  acquire(taskId: string) {
    return this.step(
      "acquire",
      buildAcquireScript(this.config, this.image, taskId),
    );
  }

  async stage(
    taskId: string,
    kind: LinuxPayloadKind,
    payload: ReadableStream<Uint8Array>,
    sha256: string,
  ): Promise<{ bytes: number; version: string }> {
    const up = await this.step(
      "upload",
      buildUploadScript(this.config, this.image, taskId),
      payload,
    );
    const inst = await this.step(
      "install",
      buildInstallScript(this.config, this.image, taskId, kind, sha256),
    );
    return { bytes: Number(up.bytes), version: String(inst.version ?? "") };
  }

  async launch(
    taskId: string,
    startUrl?: string,
  ): Promise<{ version: string }> {
    const r = await this.step(
      "launch",
      buildLaunchScript(this.config, this.image, taskId, startUrl),
    );
    return { version: String(r.value ?? "") };
  }

  async evaluate(taskId: string, script: string): Promise<unknown> {
    const r = await this.step(
      "eval",
      buildMarionetteScript(this.config, this.image, taskId, {
        op: "eval",
        script,
      }),
    );
    return r.value;
  }

  async screenshot(taskId: string): Promise<string> {
    const r = await this.step(
      "screenshot",
      buildMarionetteScript(this.config, this.image, taskId, {
        op: "screenshot",
      }),
    );
    if (typeof r.png !== "string") {
      throw new LinuxQaError("screenshot", { error: "no-image" });
    }
    return r.png;
  }

  async cleanup(taskId: string): Promise<LinuxQaCleanup> {
    const r = await this.step(
      "cleanup",
      buildCleanupScript(this.config, this.image, taskId),
    );
    const containerRemoved = r.containerRemoved === true;
    return { containerRemoved, clean: containerRemoved };
  }
}
