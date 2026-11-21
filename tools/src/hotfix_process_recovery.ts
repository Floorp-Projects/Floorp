import { join, resolve } from "@std/path";
import { MarionetteClient } from "./browser_connector.ts";
import { browserCommand } from "./browser_launcher.ts";
import { BIN_PATH_EXE, PROJECT_ROOT } from "./defines.ts";
import { OS_SERVER_PORT_PREF } from "./port_offset.ts";
import type {
  BrowserDriverResult,
  BrowserLaunch,
  DurableBoundary,
  DurableMarker,
  LinuxProcessIdentity,
  ProcessOwnershipMonitor,
  ProcessRecoveryTrace,
  ProcessSupervisorDependencies,
  RuntimeLockArchitecture,
  ScenarioStep,
  SetupKind,
} from "./hotfix_process_recovery_types.ts";

const RUNS_ROOT = join(PROJECT_ROOT, "_dist", "hotfix-process-test-runs");
const STARTUP_TIMEOUT_MS = 60_000;
const MARKER_TIMEOUT_MS = 90_000;
const PROCESS_EXIT_TIMEOUT_MS = 15_000;
const POLL_INTERVAL_MS = 100;
const FEATURE_HOST_URI =
  "resource://noraneko/modules/feature-host/FeatureHost.sys.mjs";

export function parseLinuxProcStat(pid: number, stat: string): Pick<
  LinuxProcessIdentity,
  "pid" | "parentPid" | "processGroupId" | "sessionId" | "startTimeTicks"
> {
  const close = stat.lastIndexOf(")");
  if (close < 0) throw new Error("malformed /proc stat");
  const fields = stat.slice(close + 2).trim().split(/\s+/);
  if (fields.length <= 19) throw new Error("truncated /proc stat");
  const parentPid = Number(fields[1]);
  const processGroupId = Number(fields[2]);
  const sessionId = Number(fields[3]);
  const startTimeTicks = fields[19] ?? "";
  if (
    !Number.isSafeInteger(pid) || pid < 1 ||
    !Number.isSafeInteger(parentPid) || parentPid < 0 ||
    !Number.isSafeInteger(processGroupId) || processGroupId < 0 ||
    !Number.isSafeInteger(sessionId) || sessionId < 0 ||
    !/^\d+$/.test(startTimeTicks)
  ) throw new Error("invalid /proc stat identity");
  return { pid, parentPid, processGroupId, sessionId, startTimeTicks };
}

export function matchesDurableBoundary(
  boundary: string,
  record: Record<string, unknown>,
): boolean {
  const pending = typeof record.pending === "object" && record.pending !== null
    ? record.pending as Record<string, unknown>
    : null;
  const attempt = typeof record.attempt === "object" && record.attempt !== null
    ? record.attempt as Record<string, unknown>
    : null;
  const attempted = Array.isArray(attempt?.attemptedDigests)
    ? attempt.attemptedDigests
    : [];
  if (boundary === "committed_pending_publish") {
    const outcome = typeof record.lastOutcome === "object" &&
        record.lastOutcome !== null
      ? record.lastOutcome as Record<string, unknown>
      : null;
    const request = typeof outcome?.request === "object" &&
        outcome.request !== null
      ? outcome.request as Record<string, unknown>
      : null;
    return outcome?.status === boundary && request?.op === "update" &&
      request.target === "v2";
  }
  if (!pending) return false;
  if (boundary === "fallback_pending_before_mark") {
    return pending.targetVersion === "v1" && pending.phase === "OLD_STOPPED" &&
      typeof pending.targetDigest === "string" &&
      !attempted.includes(pending.targetDigest);
  }
  if (boundary === "fallback_attempt_marked") {
    return pending.targetVersion === "v1" && pending.phase === "OLD_STOPPED" &&
      typeof pending.targetDigest === "string" &&
      attempted.includes(pending.targetDigest);
  }
  if (boundary === "disable_prepared") {
    return pending.op === "disable" && pending.phase === "PREPARED" &&
      record.desiredEnabled === false;
  }
  if (boundary === "disabled_update_prepared") {
    return pending.op === "update" && pending.phase === "PREPARED" &&
      record.desiredEnabled === false;
  }
  if (boundary === "enable_prepared") {
    return pending.op === "enable" && pending.phase === "PREPARED" &&
      record.desiredEnabled === false;
  }
  return pending.op === "update" && pending.phase === boundary &&
    pending.targetVersion === "v2";
}

export function assertProcessRecoveryPlatform(platform: string): void {
  if (platform !== "linux") {
    throw new Error("test:hotfix-process supports Linux only");
  }
}

export function resolveExpectedRuntimeBuildId(
  sourceLockBytes: Uint8Array,
  installedLockBytes: Uint8Array,
  denoArch = Deno.build.arch,
): string {
  const architecture = runtimeLockArchitectureForDenoArch(denoArch);
  const source: unknown = JSON.parse(new TextDecoder().decode(sourceLockBytes));
  const installed: unknown = JSON.parse(
    new TextDecoder().decode(installedLockBytes),
  );
  if (
    !isRecord(source) || !Array.isArray(source.artifacts) ||
    !isRecord(installed)
  ) {
    throw new Error("runtime lock BuildID metadata is malformed");
  }
  const artifacts = source.artifacts.filter((artifact) =>
    isRecord(artifact) && artifact.platform === "linux" &&
    artifact.architecture === architecture
  );
  if (artifacts.length !== 1) {
    throw new Error(
      `runtime lock must contain one Linux ${architecture} artifact`,
    );
  }
  const expected = (artifacts[0] as Record<string, unknown>).buildId;
  const actual = installed.buildId;
  if (
    typeof expected !== "string" || !/^\d{14}$/.test(expected) ||
    actual !== expected || installed.platform !== "linux" ||
    installed.architecture !== architecture
  ) throw new Error("installed runtime BuildID disagrees with runtime lock");
  return expected;
}

export function runtimeLockArchitectureForDenoArch(
  denoArch: string,
): RuntimeLockArchitecture {
  if (denoArch === "x86_64" || denoArch === "aarch64") return denoArch;
  throw new Error(`unsupported Deno architecture: ${denoArch}`);
}

export function assertOwnedBrowserRoot(
  expected: LinuxProcessIdentity,
  current: LinuxProcessIdentity | null,
  executable: string,
  profilePath: string,
): asserts current is LinuxProcessIdentity {
  if (!current) throw new Error("owned browser root is absent");
  if (
    current.pid !== expected.pid ||
    current.startTimeTicks !== expected.startTimeTicks ||
    current.executable !== executable ||
    current.sessionId !== expected.pid ||
    current.processGroupId !== expected.pid ||
    !hasProfileArgument(current.commandLine, profilePath)
  ) throw new Error("owned browser root identity mismatch");
}

function sameProcessIdentity(
  left: LinuxProcessIdentity,
  right: LinuxProcessIdentity,
): boolean {
  return left.pid === right.pid &&
    left.startTimeTicks === right.startTimeTicks &&
    left.processGroupId === right.processGroupId &&
    left.sessionId === right.sessionId &&
    left.executable === right.executable &&
    canonicalJson(left.commandLine) === canonicalJson(right.commandLine);
}

export function selectOwnedProcessTree(
  root: LinuxProcessIdentity,
  processes: LinuxProcessIdentity[],
  previouslyOwned: LinuxProcessIdentity[] = [root],
): LinuxProcessIdentity[] {
  const byPid = new Map(processes.map((process) => [process.pid, process]));
  const known = new Map(
    previouslyOwned.map((process) => [process.pid, process]),
  );
  if (!known.has(root.pid)) known.set(root.pid, root);
  for (const previous of known.values()) {
    const current = byPid.get(previous.pid);
    if (current && !sameProcessIdentity(previous, current)) {
      throw new Error(
        `owned process identity reused or changed (${previous.pid})`,
      );
    }
  }
  const currentRoot = byPid.get(root.pid);
  if (currentRoot && !sameProcessIdentity(root, currentRoot)) {
    throw new Error("owned root PID identity changed");
  }
  const ownedPids = new Set<number>([root.pid]);
  let changed = true;
  while (changed) {
    changed = false;
    for (const process of processes) {
      if (ownedPids.has(process.parentPid) && !ownedPids.has(process.pid)) {
        ownedPids.add(process.pid);
        changed = true;
      }
    }
  }
  const selected = new Map<number, LinuxProcessIdentity>();
  for (const process of processes) {
    if (
      process.sessionId === root.sessionId ||
      process.processGroupId === root.processGroupId ||
      ownedPids.has(process.pid)
    ) selected.set(process.pid, process);
  }
  for (const previous of known.values()) {
    const current = byPid.get(previous.pid);
    if (current) selected.set(current.pid, current);
  }
  return [...selected.values()].sort((left, right) => right.pid - left.pid);
}

export function assertOwnedMarionetteListener(
  root: LinuxProcessIdentity,
  owners: LinuxProcessIdentity[],
): void {
  if (owners.length !== 1) {
    throw new Error("dedicated Marionette listener ownership is ambiguous");
  }
  const [owner] = owners;
  if (
    owner.pid !== root.pid ||
    owner.startTimeTicks !== root.startTimeTicks ||
    owner.sessionId !== root.sessionId ||
    owner.processGroupId !== root.processGroupId ||
    owner.executable !== root.executable
  ) throw new Error("dedicated Marionette listener has a foreign owner");
}

export async function signalVerifiedOwnedProcessTree(options: {
  root: LinuxProcessIdentity;
  profilePath: string;
  previouslyOwned: LinuxProcessIdentity[];
  dependencies: ProcessSupervisorDependencies;
}): Promise<LinuxProcessIdentity[]> {
  const { root, profilePath, previouslyOwned, dependencies } = options;
  const listed = await dependencies.listProcesses();
  const selected = selectOwnedProcessTree(root, listed, previouslyOwned);
  const currentRoot = listed.find((process) => process.pid === root.pid) ??
    null;
  if (currentRoot) {
    assertOwnedBrowserRoot(root, currentRoot, root.executable, profilePath);
  }
  const verified: LinuxProcessIdentity[] = [];
  for (const expected of selected) {
    const current = await dependencies.readProcess(expected.pid);
    if (!current) continue;
    if (!sameProcessIdentity(expected, current)) {
      throw new Error(`owned process changed before SIGKILL (${expected.pid})`);
    }
    if (expected.pid === root.pid) {
      assertOwnedBrowserRoot(root, current, root.executable, profilePath);
    }
    verified.push(current);
  }
  for (const identity of verified) {
    dependencies.signal(identity.pid, "SIGKILL");
  }
  return verified;
}

export function parseDurableMarker(
  raw: string,
  expectedNonce: string,
  expectedLaunchId: number,
  expectedBoundary: DurableBoundary,
): DurableMarker {
  const value: unknown = JSON.parse(raw);
  if (!isRecord(value)) throw new Error("invalid durable marker");
  const marker = value as Partial<DurableMarker>;
  if (
    marker.schemaVersion !== 1 || marker.runNonce !== expectedNonce ||
    typeof marker.launchId !== "number" ||
    !Number.isSafeInteger(marker.launchId) ||
    marker.launchId < 1 || marker.launchId !== expectedLaunchId ||
    marker.boundary !== expectedBoundary ||
    typeof marker.evaluationCount !== "number" ||
    !Number.isSafeInteger(marker.evaluationCount) ||
    marker.evaluationCount < 0 || !isRecord(marker.snapshot) ||
    typeof marker.snapshot.stateRevision !== "number" ||
    !Number.isSafeInteger(marker.snapshot.stateRevision) ||
    marker.snapshot.stateRevision < 0
  ) throw new Error("durable marker identity or schema mismatch");
  return marker as DurableMarker;
}

function isRecord(value: unknown): value is Record<string, unknown> {
  return typeof value === "object" && value !== null && !Array.isArray(value);
}

export function parseBrowserDriverResult(
  raw: string,
  expected: { runNonce: string; launchId: number; buildId: string },
): BrowserDriverResult {
  const value: unknown = JSON.parse(raw);
  if (!isRecord(value)) throw new Error("invalid browser driver result");
  if (
    value.schemaVersion !== 1 || value.runNonce !== expected.runNonce ||
    value.launchId !== expected.launchId ||
    !Number.isSafeInteger(value.launchId) || value.launchId < 1 ||
    value.buildId !== expected.buildId ||
    typeof value.evaluationCount !== "number" ||
    !Number.isSafeInteger(value.evaluationCount) ||
    value.evaluationCount < 0 ||
    typeof value.evaluationCountBeforeRollback !== "number" ||
    !Number.isSafeInteger(value.evaluationCountBeforeRollback) ||
    value.evaluationCountBeforeRollback < 0 || !isRecord(value.diagnostics) ||
    !isRecord(value.journal) ||
    !(value.outcomeBeforeRollback === null ||
      isRecord(value.outcomeBeforeRollback)) ||
    !(value.rollbackResult === null || isRecord(value.rollbackResult)) ||
    !(value.postRollbackJournal === null || isRecord(value.postRollbackJournal))
  ) throw new Error("browser driver result identity or schema mismatch");
  return value as unknown as BrowserDriverResult;
}

export async function waitForDurableMarker(options: {
  markerPath: string;
  errorPath: string;
  runNonce: string;
  launchId: number;
  boundary: DurableBoundary;
  timeoutMs: number;
  pollIntervalMs?: number;
}): Promise<DurableMarker> {
  const deadline = Date.now() + options.timeoutMs;
  while (Date.now() < deadline) {
    try {
      return parseDurableMarker(
        await Deno.readTextFile(options.markerPath),
        options.runNonce,
        options.launchId,
        options.boundary,
      );
    } catch (error) {
      if (!(error instanceof Deno.errors.NotFound)) throw error;
    }
    try {
      const rawError = await Deno.readTextFile(options.errorPath);
      throw new Error(`browser driver failed: ${rawError}`);
    } catch (error) {
      if (!(error instanceof Deno.errors.NotFound)) throw error;
    }
    await new Promise((resolve) =>
      setTimeout(resolve, options.pollIntervalMs ?? POLL_INTERVAL_MS)
    );
  }
  throw new Error("timed out waiting for durable marker");
}

function hasProfileArgument(
  commandLine: string[],
  profilePath: string,
): boolean {
  for (let index = 0; index < commandLine.length - 1; index += 1) {
    if (
      (commandLine[index] === "--profile" ||
        commandLine[index] === "-profile") &&
      resolve(commandLine[index + 1]) === profilePath
    ) return true;
  }
  return false;
}

async function readProcessIdentity(
  pid: number,
  failOnPermissionDenied = false,
): Promise<
  LinuxProcessIdentity | null
> {
  try {
    const base = `/proc/${pid}`;
    const stat = await Deno.readTextFile(join(base, "stat"));
    const parsed = parseLinuxProcStat(pid, stat);
    const [executable, commandBytes] = await Promise.all([
      Deno.realPath(join(base, "exe")),
      Deno.readFile(join(base, "cmdline")),
    ]);
    const commandLine = new TextDecoder().decode(commandBytes).split("\0")
      .filter(Boolean);
    return { ...parsed, executable, commandLine };
  } catch (error) {
    if (error instanceof Deno.errors.NotFound) return null;
    if (error instanceof Deno.errors.PermissionDenied) {
      if (failOnPermissionDenied) {
        throw new Error(`cannot verify owned process identity (${pid})`);
      }
      return null;
    }
    throw error;
  }
}

async function listProcesses(): Promise<LinuxProcessIdentity[]> {
  const processes: LinuxProcessIdentity[] = [];
  for await (const entry of Deno.readDir("/proc")) {
    if (!/^\d+$/.test(entry.name)) continue;
    const identity = await readProcessIdentity(Number(entry.name));
    if (identity) processes.push(identity);
  }
  return processes;
}

async function refreshOwnership(ownership: ProcessOwnershipMonitor) {
  if (ownership.failure) throw ownership.failure;
  const selected = selectOwnedProcessTree(
    ownership.root,
    await listProcesses(),
    [...ownership.members.values()],
  );
  for (const process of selected) ownership.members.set(process.pid, process);
}

async function startOwnershipMonitor(
  root: LinuxProcessIdentity,
  executable: string,
  profilePath: string,
): Promise<ProcessOwnershipMonitor> {
  const ownership: ProcessOwnershipMonitor = {
    root,
    executable,
    profilePath,
    members: new Map([[root.pid, root]]),
    failure: null,
    stopped: false,
    task: Promise.resolve(),
  };
  await refreshOwnership(ownership);
  ownership.task = (async () => {
    while (!ownership.stopped) {
      await new Promise((resolve) => setTimeout(resolve, POLL_INTERVAL_MS));
      if (ownership.stopped) break;
      try {
        await refreshOwnership(ownership);
      } catch (error) {
        ownership.failure = error instanceof Error
          ? error
          : new Error(String(error));
        break;
      }
    }
  })();
  return ownership;
}

async function stopOwnershipMonitor(ownership: ProcessOwnershipMonitor) {
  ownership.stopped = true;
  await ownership.task;
  if (ownership.failure) throw ownership.failure;
}

async function waitForBrowserRoot(
  pid: number,
  executable: string,
  profilePath: string,
  child: Deno.ChildProcess,
): Promise<LinuxProcessIdentity> {
  const deadline = Date.now() + STARTUP_TIMEOUT_MS;
  while (Date.now() < deadline) {
    const status = await Promise.race([
      child.status,
      new Promise<null>((resolve) => setTimeout(() => resolve(null), 100)),
    ]);
    if (status) {
      throw new Error(`browser exited before readiness (${status.code})`);
    }
    const current = await readProcessIdentity(pid);
    if (
      current && current.executable === executable &&
      current.sessionId === pid && current.processGroupId === pid &&
      hasProfileArgument(current.commandLine, profilePath)
    ) return current;
  }
  throw new Error("timed out waiting for owned browser root");
}

async function listeningSocketInodes(port: number): Promise<Set<string>> {
  const hexPort = port.toString(16).toUpperCase().padStart(4, "0");
  const inodes = new Set<string>();
  for (const table of ["/proc/net/tcp", "/proc/net/tcp6"]) {
    const content = await Deno.readTextFile(table).catch(() => "");
    for (const line of content.split("\n").slice(1)) {
      const fields = line.trim().split(/\s+/);
      if (
        fields.length > 9 && fields[1]?.endsWith(`:${hexPort}`) &&
        fields[3] === "0A" && fields[9]
      ) inodes.add(fields[9]);
    }
  }
  return inodes;
}

async function socketOwners(inodes: Set<string>): Promise<Set<number>> {
  const owners = new Set<number>();
  for await (const process of Deno.readDir("/proc")) {
    if (!/^\d+$/.test(process.name)) continue;
    const fdDirectory = `/proc/${process.name}/fd`;
    let descriptors: Deno.DirEntry[];
    try {
      descriptors = [];
      for await (const descriptor of Deno.readDir(fdDirectory)) {
        descriptors.push(descriptor);
      }
    } catch {
      continue;
    }
    for (const descriptor of descriptors) {
      try {
        const target = await Deno.readLink(join(fdDirectory, descriptor.name));
        const match = target.match(/^socket:\[(\d+)\]$/);
        if (match && inodes.has(match[1])) owners.add(Number(process.name));
      } catch {
        continue;
      }
    }
  }
  return owners;
}

async function waitForOwnedMarionettePort(
  port: number,
  root: LinuxProcessIdentity,
): Promise<void> {
  const deadline = Date.now() + STARTUP_TIMEOUT_MS;
  while (Date.now() < deadline) {
    const inodes = await listeningSocketInodes(port);
    if (inodes.size > 0) {
      const owners = await socketOwners(inodes);
      if (owners.size === 0) {
        throw new Error(
          "dedicated Marionette listener has no verifiable owner",
        );
      }
      const identities: LinuxProcessIdentity[] = [];
      for (const pid of owners) {
        const owner = await readProcessIdentity(pid);
        if (!owner) {
          throw new Error("dedicated Marionette listener owner is ambiguous");
        }
        identities.push(owner);
      }
      assertOwnedMarionetteListener(root, identities);
      return;
    }
    await new Promise((resolve) => setTimeout(resolve, POLL_INTERVAL_MS));
  }
  throw new Error("timed out waiting for owned Marionette listener");
}

async function isPortOpen(port: number): Promise<boolean> {
  try {
    const connection = await Deno.connect({ hostname: "127.0.0.1", port });
    connection.close();
    return true;
  } catch {
    return false;
  }
}

async function waitForProcessesDead(
  ownership: ProcessOwnershipMonitor,
): Promise<void> {
  const deadline = Date.now() + PROCESS_EXIT_TIMEOUT_MS;
  while (Date.now() < deadline) {
    await refreshOwnership(ownership);
    const stillOwned = selectOwnedProcessTree(
      ownership.root,
      await listProcesses(),
      [...ownership.members.values()],
    );
    if (stillOwned.length === 0) return;
    await signalVerifiedOwnedProcessTree({
      root: ownership.root,
      profilePath: ownership.profilePath,
      previouslyOwned: [...ownership.members.values()],
      dependencies: {
        listProcesses,
        readProcess: (pid) => readProcessIdentity(pid, true),
        signal: (pid, signal) => Deno.kill(pid, signal),
      },
    });
    await new Promise((resolve) => setTimeout(resolve, POLL_INTERVAL_MS));
  }
  throw new Error("owned browser processes did not exit after SIGKILL");
}

async function forceKillOwnedBrowser(
  ownership: ProcessOwnershipMonitor,
  rootStatus: Promise<Deno.CommandStatus>,
): Promise<LinuxProcessIdentity[]> {
  await stopOwnershipMonitor(ownership);
  const captured = await signalVerifiedOwnedProcessTree({
    root: ownership.root,
    profilePath: ownership.profilePath,
    previouslyOwned: [...ownership.members.values()],
    dependencies: {
      listProcesses,
      readProcess: (pid) => readProcessIdentity(pid, true),
      signal: (pid, signal) => Deno.kill(pid, signal),
    },
  });
  const status = await Promise.race([
    rootStatus,
    new Promise<null>((resolve) =>
      setTimeout(() => resolve(null), PROCESS_EXIT_TIMEOUT_MS)
    ),
  ]);
  if (!status) throw new Error("owned setsid root did not report exit");
  await waitForProcessesDead(ownership);
  return captured;
}

async function waitForPortClosed(port: number): Promise<void> {
  const deadline = Date.now() + PROCESS_EXIT_TIMEOUT_MS;
  while (Date.now() < deadline) {
    if (!(await isPortOpen(port))) return;
    await new Promise((resolve) => setTimeout(resolve, POLL_INTERVAL_MS));
  }
  throw new Error("Marionette listener remained open after SIGKILL");
}

function reservePort(): number {
  const listener = Deno.listen({ hostname: "127.0.0.1", port: 0 });
  const port = listener.addr.port;
  listener.close();
  return port;
}

async function writePrivateProfile(
  profilePath: string,
  marionettePort: number,
  osServerPort: number,
) {
  await Deno.mkdir(profilePath, { recursive: true, mode: 0o700 });
  await Deno.chmod(profilePath, 0o700);
  const prefs = [
    'user_pref("nora.startup.mode", "test");',
    'user_pref("nora.dev.allow_http_loader", false);',
    'user_pref("nora.dev.os_server.profile_token_file", false);',
    'user_pref("remote.active-protocols", 0);',
    'user_pref("marionette.enabled", true);',
    `user_pref("marionette.port", ${marionettePort});`,
    `user_pref("${OS_SERVER_PORT_PREF}", ${osServerPort});`,
    'user_pref("browser.sessionstore.resume_from_crash", false);',
    'user_pref("security.allow_parent_unrestricted_js_loads", true);',
    'user_pref("security.disallow_privileged_https_script_loads", false);',
  ].join("\n") + "\n";
  const file = await Deno.open(join(profilePath, "user.js"), {
    createNew: true,
    write: true,
    mode: 0o600,
  });
  try {
    await file.write(new TextEncoder().encode(prefs));
  } finally {
    file.close();
  }
}

async function hash(bytes: Uint8Array): Promise<string> {
  const source = new Uint8Array(bytes.length);
  source.set(bytes);
  const digest = await crypto.subtle.digest("SHA-256", source.buffer);
  return Array.from(
    new Uint8Array(digest),
    (byte) => byte.toString(16).padStart(2, "0"),
  ).join("");
}

async function commandOutput(args: string[]): Promise<string> {
  const output = await new Deno.Command("git", {
    args,
    cwd: PROJECT_ROOT,
    stdout: "piped",
    stderr: "piped",
  }).output();
  if (!output.success) {
    throw new Error(new TextDecoder().decode(output.stderr));
  }
  return new TextDecoder().decode(output.stdout).trim();
}

async function commandOutputBytes(args: string[]): Promise<Uint8Array> {
  const output = await new Deno.Command("git", {
    args,
    cwd: PROJECT_ROOT,
    stdout: "piped",
    stderr: "piped",
  }).output();
  if (!output.success) {
    throw new Error(new TextDecoder().decode(output.stderr));
  }
  return output.stdout;
}

function parseNulDelimitedPaths(output: Uint8Array): string[] {
  if (output.length === 0) return [];
  if (output[output.length - 1] !== 0) {
    throw new Error("git path list is missing its NUL terminator");
  }
  const text = new TextDecoder("utf-8", { fatal: true }).decode(output);
  const paths = text.slice(0, -1).split("\0");
  if (paths.some((path) => !path)) {
    throw new Error("git path list contains an empty path");
  }
  return paths;
}

export async function hashUntrackedSourceFiles(
  rootDirectory: string,
  pathList: Uint8Array,
): Promise<string[]> {
  const root = resolve(rootDirectory);
  const encoder = new TextEncoder();
  const hashes = await Promise.all(
    parseNulDelimitedPaths(pathList).map(async (filePath) => {
      const components = filePath.split("/");
      if (
        filePath.startsWith("/") ||
        components.some((component) =>
          component === "" || component === "." || component === ".."
        )
      ) throw new Error("git path escapes the source root");
      let absolutePath = root;
      let kind: "file" | "symlink" | null = null;
      let contents = new Uint8Array();
      for (let index = 0; index < components.length; index += 1) {
        const component = components[index];
        if (!component) throw new Error("git path contains an empty component");
        absolutePath = join(absolutePath, component);
        const info = await Deno.lstat(absolutePath);
        if (index < components.length - 1) {
          if (!info.isDirectory || info.isSymlink) {
            throw new Error("git path traverses a non-directory source entry");
          }
        } else if (info.isSymlink) {
          kind = "symlink";
          contents = encoder.encode(await Deno.readLink(absolutePath));
        } else if (info.isFile) {
          kind = "file";
          contents = await Deno.readFile(absolutePath);
        } else {
          throw new Error("git path is not a file or symlink");
        }
      }
      const pathToken = Array.from(
        encoder.encode(filePath),
        (byte) => byte.toString(16).padStart(2, "0"),
      ).join("");
      return `${pathToken}:${kind}:${await hash(contents)}`;
    }),
  );
  return hashes.sort();
}

async function buildTrace(
  runNonce: string,
  port: number,
  osServerPort: number,
): Promise<
  ProcessRecoveryTrace
> {
  const [
    sourceCommit,
    sourceStatus,
    diff,
    untracked,
    runtimeLock,
    installedLock,
  ] = await Promise.all([
    commandOutput(["rev-parse", "HEAD"]),
    commandOutput(["status", "--porcelain=v1"]),
    commandOutput(["diff", "--binary", "HEAD"]),
    commandOutputBytes([
      "ls-files",
      "--others",
      "--exclude-standard",
      "-z",
    ]),
    Deno.readFile(join(PROJECT_ROOT, "floorp-runtime.lock.json")),
    Deno.readFile(
      join(PROJECT_ROOT, "_dist", "bin", ".floorp-runtime-lock.json"),
    )
      .catch(() => new Uint8Array()),
  ]);
  const untrackedHashes = await hashUntrackedSourceFiles(
    PROJECT_ROOT,
    untracked,
  );
  const runtimeExpectedBuildId = resolveExpectedRuntimeBuildId(
    runtimeLock,
    installedLock,
  );
  return {
    schemaVersion: 1,
    runNonce,
    sourceCommit,
    sourceStatus,
    sourcePatchSha256: await hash(
      new TextEncoder().encode(`${diff}\n${untrackedHashes.join("\n")}`),
    ),
    runtimeLockSha256: await hash(runtimeLock),
    runtimeInstalledLockSha256: installedLock.length
      ? await hash(installedLock)
      : null,
    runtimeExpectedBuildId,
    runtimeBuildId: null,
    marionettePort: port,
    osServerPort,
    assertions: [],
    scenarios: [],
    failure: null,
  };
}

async function recordTrace(path: string, trace: ProcessRecoveryTrace) {
  await Deno.writeTextFile(path, `${JSON.stringify(trace, null, 2)}\n`);
}

function driverScript(): string {
  return `
const config = arguments[0];
const matches = ${matchesDurableBoundary.toString()};
const run = async () => {
  const hostModule = ChromeUtils.importESModule(${
    JSON.stringify(FEATURE_HOST_URI)
  });
  const createHost = hostModule.createM0FeatureHostForTests;
  await IOUtils.makeDirectory(PathUtils.parent(config.eventPath), { createAncestors: true, ignoreExisting: true, permissions: 0o700 });
  const observer = async (snapshot) => {
    await IOUtils.makeDirectory(PathUtils.parent(config.eventPath), { createAncestors: true, ignoreExisting: true, permissions: 0o700 });
    const evaluationCount = hostModule.m0PayloadEvaluationsForTests();
    const event = { schemaVersion: 1, runNonce: config.runNonce, launchId: config.launchId, evaluationCount, snapshot };
    await IOUtils.writeJSON(config.eventPath.replace(".json", "-" + snapshot.stateRevision + ".json"), event, { flush: true });
    if (config.boundary && matches(config.boundary, snapshot)) {
      const marker = { schemaVersion: 1, runNonce: config.runNonce, launchId: config.launchId, boundary: config.boundary, evaluationCount, snapshot };
      await IOUtils.writeJSON(config.markerPath, marker, { flush: true });
      await new Promise(() => {});
    }
  };
  try {
    const session = await createHost(true, config.root, config.reopen, undefined, { afterDurableWriteForTests: observer, excludeWindowsForTests: true });
    const host = session.host;
    if (!config.reopen) {
      await host.install("process-install", 0, "v1");
      if (config.setup === "enabled_update" || config.setup === "disable") {
        await host.enable("process-enable", host.diagnostics.stateRevision);
      }
      if (config.setup === "enabled_update") await host.update("process-update", host.diagnostics.stateRevision, "v2");
      if (config.setup === "disabled_update") await host.update("process-update", host.diagnostics.stateRevision, "v2");
      if (config.setup === "disable") await host.disable("process-disable", host.diagnostics.stateRevision);
      if (config.setup === "enable") await host.enable("process-interrupted-enable", host.diagnostics.stateRevision);
    }
    const journal = await IOUtils.readJSON(PathUtils.join(config.root, "state.json"));
    const outcomeBeforeRollback = journal.lastOutcome;
    const evaluationCountBeforeRollback = hostModule.m0PayloadEvaluationsForTests();
    let rollbackResult = null;
    let postRollbackJournal = null;
    if (config.checkRollback) {
      try {
        rollbackResult = await host.rollback("process-rollback-check", host.diagnostics.stateRevision);
      } catch (error) {
        rollbackResult = { error: String(error) };
      }
      postRollbackJournal = await IOUtils.readJSON(PathUtils.join(config.root, "state.json"));
    }
    await IOUtils.writeJSON(config.resultPath, {
      schemaVersion: 1,
      runNonce: config.runNonce,
      launchId: config.launchId,
      buildId: Services.appinfo.appBuildID,
      evaluationCount: hostModule.m0PayloadEvaluationsForTests(),
      evaluationCountBeforeRollback,
      diagnostics: host.diagnostics,
      journal,
      outcomeBeforeRollback,
      rollbackResult,
      postRollbackJournal,
    }, { flush: true });
  } catch (error) {
    await IOUtils.writeJSON(config.errorPath, { message: String(error), runNonce: config.runNonce, launchId: config.launchId }, { flush: true });
  }
};
globalThis.__m0ProcessRecovery = run();
globalThis.__m0ProcessRecovery.catch(() => {});
return "started";
`;
}

async function executeDriver(
  client: MarionetteClient,
  config: Record<string, unknown>,
): Promise<void> {
  await client.setContext("chrome");
  const result: unknown = await client.executeScript(driverScript(), [config]);
  if (result !== "started") {
    throw new Error(`unexpected Marionette driver response: ${String(result)}`);
  }
}

async function drainStream(
  stream: ReadableStream<Uint8Array>,
  filePath: string,
) {
  const file = await Deno.open(filePath, {
    create: true,
    append: true,
    write: true,
    mode: 0o600,
  });
  try {
    for await (const chunk of stream) await file.write(chunk);
  } finally {
    file.close();
  }
}

async function launchBrowser(
  profilePath: string,
  port: number,
  launchId: number,
  runDirectory: string,
  expectedBuildId: string,
): Promise<BrowserLaunch> {
  const executable = await Deno.realPath(BIN_PATH_EXE);
  const args = browserCommand({
    binaryPath: BIN_PATH_EXE,
    marionette: true,
    noRemote: true,
    profilePath,
  }).slice(1);
  args.push("--headless");
  const child = new Deno.Command("setsid", {
    args: [BIN_PATH_EXE, ...args],
    cwd: PROJECT_ROOT,
    env: {
      FLOORP_SKIP_PATCHER: "1",
      MOZ_REMOTE_ALLOW_SYSTEM_ACCESS: "1",
    },
    stdout: "piped",
    stderr: "piped",
  }).spawn();
  const stdout = drainStream(
    child.stdout!,
    join(runDirectory, `browser-${launchId}.stdout.log`),
  );
  const stderr = drainStream(
    child.stderr!,
    join(runDirectory, `browser-${launchId}.stderr.log`),
  );
  let root: LinuxProcessIdentity | null = null;
  let ownership: ProcessOwnershipMonitor | null = null;
  let client: MarionetteClient | null = null;
  try {
    root = await waitForBrowserRoot(
      child.pid,
      executable,
      profilePath,
      child,
    );
    ownership = await startOwnershipMonitor(root, executable, profilePath);
    await waitForOwnedMarionettePort(port, root);
    client = await MarionetteClient.connect(port);
    await client.setContext("chrome");
    const buildId: unknown = await client.executeScript(
      "return Services.appinfo.appBuildID",
    );
    if (buildId !== expectedBuildId) {
      throw new Error("running Floorp BuildID disagrees with runtime lock");
    }
    return { child, root, ownership, client, port, stdout, stderr, buildId };
  } catch (error) {
    if (ownership) {
      await forceKillOwnedBrowser(
        ownership,
        child.status,
      );
      await waitForPortClosed(port);
    }
    if (client) await client.close();
    throw error;
  }
}

async function killBrowser(launch: BrowserLaunch) {
  const processes = await forceKillOwnedBrowser(
    launch.ownership,
    launch.child.status,
  );
  const status = await launch.child.status;
  await waitForPortClosed(launch.port);
  await Promise.all([launch.stdout, launch.stderr]);
  try {
    await launch.client.close();
  } catch {
    // The browser process was forcibly terminated before the session closed.
  }
  return { processes, exit: status, portClosed: true };
}

function canonicalJson(value: unknown): string {
  if (Array.isArray(value)) {
    return `[${value.map((item) => canonicalJson(item)).join(",")}]`;
  }
  if (isRecord(value)) {
    return `{${
      Object.keys(value).sort().map((key) =>
        `${JSON.stringify(key)}:${canonicalJson(value[key])}`
      ).join(",")
    }}`;
  }
  const encoded = JSON.stringify(value);
  if (encoded === undefined) throw new Error("journal contains non-JSON data");
  return encoded;
}

async function captureDiskJournal(
  root: string,
  runDirectory: string,
  launchId: number,
  marker: DurableMarker | null,
) {
  const bytes = await Deno.readFile(join(root, "state.json"));
  const snapshot: unknown = JSON.parse(new TextDecoder().decode(bytes));
  if (!isRecord(snapshot)) throw new Error("disk journal is not an object");
  const path = join(runDirectory, `journal-after-kill-${launchId}.json`);
  await Deno.writeFile(path, bytes, { createNew: true, mode: 0o600 });
  return {
    path,
    byteLength: bytes.byteLength,
    sha256: await hash(bytes),
    snapshot,
    canonicalMatch: marker
      ? canonicalJson(snapshot) === canonicalJson(marker.snapshot)
      : null,
  };
}

function scenarioConfig(
  runNonce: string,
  launchId: number,
  root: string,
  runDirectory: string,
  setup: SetupKind,
  step: ScenarioStep,
): Record<string, unknown> {
  const directory = join(runDirectory, `events-${launchId}`);
  return {
    runNonce,
    launchId,
    root,
    setup,
    reopen: step.reopen,
    boundary: step.boundary,
    checkRollback: step.checkRollback ?? false,
    eventPath: join(directory, "event.json"),
    markerPath: join(directory, "boundary.json"),
    resultPath: join(directory, "result.json"),
    errorPath: join(directory, "error.json"),
  };
}

async function launchStep(options: {
  runNonce: string;
  launchId: number;
  root: string;
  runDirectory: string;
  profilePath: string;
  port: number;
  expectedBuildId: string;
  setup: SetupKind;
  step: ScenarioStep;
}): Promise<Record<string, unknown>> {
  const launch = await launchBrowser(
    options.profilePath,
    options.port,
    options.launchId,
    options.runDirectory,
    options.expectedBuildId,
  );
  const config = scenarioConfig(
    options.runNonce,
    options.launchId,
    options.root,
    options.runDirectory,
    options.setup,
    options.step,
  );
  let killAttempted = false;
  try {
    await executeDriver(launch.client, config);
    const errorPath = String(config.errorPath);
    if (options.step.boundary) {
      const marker = await waitForDurableMarker({
        markerPath: String(config.markerPath),
        errorPath,
        runNonce: options.runNonce,
        launchId: options.launchId,
        boundary: options.step.boundary,
        timeoutMs: MARKER_TIMEOUT_MS,
      });
      killAttempted = true;
      const killed = await killBrowser(launch);
      const diskJournal = await captureDiskJournal(
        options.root,
        options.runDirectory,
        options.launchId,
        marker,
      );
      return {
        marker,
        process: { ...killed, diskJournal },
        root: launch.root,
        buildId: launch.buildId,
      };
    }
    const result = parseBrowserDriverResult(
      await waitForDriverResult(
        String(config.resultPath),
        errorPath,
        MARKER_TIMEOUT_MS,
      ),
      {
        runNonce: options.runNonce,
        launchId: options.launchId,
        buildId: launch.buildId,
      },
    );
    killAttempted = true;
    const killed = await killBrowser(launch);
    const diskJournal = await captureDiskJournal(
      options.root,
      options.runDirectory,
      options.launchId,
      null,
    );
    return {
      result,
      process: { ...killed, diskJournal },
      root: launch.root,
      buildId: launch.buildId,
    };
  } finally {
    if (!killAttempted) await killBrowser(launch);
  }
}

async function waitForDriverResult(
  resultPath: string,
  errorPath: string,
  timeoutMs: number,
): Promise<string> {
  const deadline = Date.now() + timeoutMs;
  while (Date.now() < deadline) {
    try {
      return await Deno.readTextFile(resultPath);
    } catch (error) {
      if (!(error instanceof Deno.errors.NotFound)) throw error;
    }
    try {
      const error = await Deno.readTextFile(errorPath);
      throw new Error(`browser driver failed: ${error}`);
    } catch (error) {
      if (!(error instanceof Deno.errors.NotFound)) throw error;
    }
    await new Promise((resolve) => setTimeout(resolve, POLL_INTERVAL_MS));
  }
  throw new Error("timed out waiting for browser recovery result");
}

function addAssertion(
  trace: ProcessRecoveryTrace,
  id: string,
  passed: boolean,
  detail: string,
) {
  trace.assertions.push({ id, passed, detail });
  if (!passed) throw new Error(`${id}: ${detail}`);
}

async function runScenario(
  trace: ProcessRecoveryTrace,
  options: {
    name: string;
    setup: SetupKind;
    steps: ScenarioStep[];
    root: string;
    runNonce: string;
    runDirectory: string;
    profilePath: string;
    port: number;
    expectedBuildId: string;
    allocateLaunchId: () => number;
  },
) {
  const launches: Record<string, unknown>[] = [];
  const scenario = {
    name: options.name,
    root: options.root,
    profilePath: options.profilePath,
    launches,
  };
  trace.scenarios.push(scenario);
  for (let index = 0; index < options.steps.length; index += 1) {
    const step = options.steps[index];
    const data = await launchStep({
      ...options,
      launchId: options.allocateLaunchId(),
      step,
    });
    data.m0Root = options.root;
    data.profilePath = options.profilePath;
    const previous = launches[launches.length - 1];
    if (previous) {
      addAssertion(
        trace,
        `${options.name}-same-profile-and-root-${index}`,
        previous.profilePath === data.profilePath &&
          previous.m0Root === data.m0Root,
        `profile=${String(data.profilePath)} root=${String(data.m0Root)}`,
      );
    }
    launches.push(data);
    if (trace.runtimeBuildId === null) {
      trace.runtimeBuildId = String(data.buildId);
    }
    if (step.boundary) {
      const marker = data.marker as DurableMarker;
      addAssertion(
        trace,
        `${options.name}-durable-${step.boundary}-${marker.launchId}`,
        marker.boundary === step.boundary &&
          marker.snapshot.stateRevision !== undefined &&
          (step.boundary !== "disabled_update_prepared" ||
            marker.evaluationCount === 0),
        `revision=${
          String(marker.snapshot.stateRevision)
        } evaluationCount=${marker.evaluationCount}`,
      );
      const diskJournal = (data.process as Record<string, unknown>)
        .diskJournal as Record<string, unknown>;
      addAssertion(
        trace,
        `${options.name}-external-disk-matches-${marker.launchId}`,
        diskJournal.canonicalMatch === true,
        `bytes=${String(diskJournal.byteLength)} sha256=${
          String(diskJournal.sha256)
        }`,
      );
    }
    const process = data.process as Record<string, unknown>;
    const exit = process.exit as Record<string, unknown>;
    addAssertion(
      trace,
      `${options.name}-sigkill-${String(data.buildId)}-${index}`,
      exit.signal === "SIGKILL" && process.portClosed === true,
      `signal=${String(exit.signal)} portClosed=${String(process.portClosed)}`,
    );
    await recordTrace(join(options.runDirectory, "trace.json"), trace);
  }
  const final = launches[launches.length - 1]?.result as
    | Record<string, unknown>
    | undefined;
  if (!final) throw new Error(`${options.name}: final recovery result missing`);
  return { scenario, final };
}

async function runProcessRecoveryMatrix(): Promise<void> {
  assertProcessRecoveryPlatform(Deno.build.os);
  const runNonce = crypto.randomUUID();
  const runDirectory = join(RUNS_ROOT, `run-${runNonce}`);
  const profilePath = join(runDirectory, "profile");
  const port = await reservePort();
  let osServerPort = await reservePort();
  while (osServerPort === port) osServerPort = await reservePort();
  await Deno.mkdir(runDirectory, { recursive: true, mode: 0o700 });
  await Deno.chmod(runDirectory, 0o700);
  await writePrivateProfile(profilePath, port, osServerPort);
  const trace = await buildTrace(runNonce, port, osServerPort);
  const tracePath = join(runDirectory, "trace.json");
  await recordTrace(tracePath, trace);
  let succeeded = false;
  let scenarioCounter = 0;
  let launchCounter = 0;
  const rootFor = () =>
    join(profilePath, "floorp-hotfix-m0-tests", `m0-${crypto.randomUUID()}`);
  const stepOptions = (root: string, setup: SetupKind, steps: ScenarioStep[]) =>
    runScenario(trace, {
      name: `scenario-${++scenarioCounter}`,
      setup,
      steps,
      root,
      runNonce,
      runDirectory,
      profilePath,
      port,
      expectedBuildId: trace.runtimeExpectedBuildId,
      allocateLaunchId: () => ++launchCounter,
    });
  try {
    const phaseScenarios: Array<[DurableBoundary, SetupKind]> = [
      ["PREPARED", "enabled_update"],
      ["OLD_STOPPED", "enabled_update"],
      ["CANDIDATE_READY", "enabled_update"],
      ["disabled_update_prepared", "disabled_update"],
      ["disable_prepared", "disable"],
      ["enable_prepared", "enable"],
    ];
    for (const [boundary, setup] of phaseScenarios) {
      const root = rootFor();
      const { final, scenario } = await stepOptions(root, setup, [
        { boundary, reopen: false },
        { boundary: null, reopen: true },
      ]);
      const journal = final.journal as Record<string, unknown>;
      const expectedEnabled = boundary === "PREPARED" ||
          boundary === "OLD_STOPPED" || boundary === "CANDIDATE_READY"
        ? true
        : false;
      addAssertion(
        trace,
        `${boundary}-recovery-intent`,
        journal.desiredEnabled === expectedEnabled,
        `desiredEnabled=${String(journal.desiredEnabled)}`,
      );
      if (setup === "enabled_update") {
        addAssertion(
          trace,
          `${boundary}-restores-v1`,
          journal.selectedVersion === "v1" &&
            (journal.lastOutcome as Record<string, unknown> | null)?.status ===
              "failed_restored",
          `selected=${String(journal.selectedVersion)} outcome=${
            String(
              (journal.lastOutcome as Record<string, unknown> | null)?.status,
            )
          }`,
        );
      }
      if (setup === "disabled_update") {
        addAssertion(
          trace,
          `${boundary}-disabled-update-keeps-v1`,
          journal.selectedVersion === "v1" && final.evaluationCount === 0,
          `selected=${String(journal.selectedVersion)} evaluations=${
            String(final.evaluationCount)
          }`,
        );
      }
      const interruptedMarker = scenario.launches[0]?.marker as
        | DurableMarker
        | undefined;
      if (interruptedMarker) {
        const interrupted = interruptedMarker.snapshot;
        const pending = interrupted.pending as Record<string, unknown>;
        const outcome = journal.lastOutcome as Record<string, unknown> | null;
        addAssertion(
          trace,
          `${boundary}-recovery-preserves-transaction-descriptor`,
          outcome?.txId === pending.txId &&
            canonicalJson(outcome?.request) === canonicalJson(pending.request),
          `txId=${String(outcome?.txId)} request preserved=${
            canonicalJson(outcome?.request) === canonicalJson(pending.request)
          }`,
        );
      }
      if (
        boundary === "disabled_update_prepared" ||
        boundary === "disable_prepared" || boundary === "enable_prepared"
      ) {
        addAssertion(
          trace,
          `${boundary}-no-restart-evaluation`,
          final.evaluationCount === 0,
          `evaluationCount=${String(final.evaluationCount)}`,
        );
      }
    }

    const fallbackRoot = rootFor();
    const fallback = await stepOptions(fallbackRoot, "enabled_update", [
      { boundary: "committed_pending_publish", reopen: false },
      { boundary: "fallback_pending_before_mark", reopen: true },
      { boundary: "fallback_pending_before_mark", reopen: true },
      { boundary: null, reopen: true, checkRollback: true },
    ]);
    const finalJournal = fallback.final.journal as Record<string, unknown>;
    const lastOutcome = fallback.final.outcomeBeforeRollback as
      | Record<string, unknown>
      | null;
    const successfulMarkers = fallback.scenario.launches.slice(1, 3).map(
      (launch) => (launch.marker as DurableMarker).snapshot,
    );
    addAssertion(
      trace,
      "fallback-recovery-original-transaction",
      lastOutcome?.txId === "process-update" &&
        (lastOutcome.request as Record<string, unknown> | undefined)?.target ===
          "v2",
      `txId=${String(lastOutcome?.txId)} request.target=${
        String(
          (lastOutcome?.request as Record<string, unknown> | undefined)?.target,
        )
      }`,
    );
    addAssertion(
      trace,
      "fallback-recovery-evaluates-once",
      fallback.final.evaluationCountBeforeRollback === 1,
      `evaluationCount=${String(fallback.final.evaluationCountBeforeRollback)}`,
    );
    addAssertion(
      trace,
      "unpublished-candidate-not-rollback-target",
      finalJournal.rollbackVersion !== "v2" &&
        finalJournal.selectedVersion === "v1" &&
        (fallback.final.postRollbackJournal as Record<string, unknown>)
            .selectedVersion !== "v2",
      `selected=${String(finalJournal.selectedVersion)} rollback=${
        String(finalJournal.rollbackVersion)
      }`,
    );
    const originalCommitted =
      (fallback.scenario.launches[0].marker as DurableMarker).snapshot;
    addAssertion(
      trace,
      "committed-write-origin-tx",
      (originalCommitted.lastOutcome as Record<string, unknown>).txId ===
        "process-update",
      "committed_pending_publish came from the emitted update operation",
    );
    addAssertion(
      trace,
      "fallback-pre-mark-interrupted-twice",
      successfulMarkers.every((record, index) => {
        const row = record.attempt as Record<string, unknown>;
        const pending = record.pending as Record<string, unknown>;
        const digests = row.attemptedDigests as string[];
        const marker = fallback.scenario.launches[index + 1]
          .marker as DurableMarker;
        const previous = fallback.scenario.launches[index]
          .marker as DurableMarker;
        const previousAttempt = previous.snapshot.attempt as Record<
          string,
          unknown
        >;
        return pending.phase === "OLD_STOPPED" &&
          marker.evaluationCount === 0 &&
          !digests.includes(String(pending.targetDigest)) &&
          pending.txId === "process-update" &&
          row.originTxId === "process-update" &&
          row.recoveryEpoch === previousAttempt.recoveryEpoch &&
          canonicalJson(row.request) ===
            canonicalJson(previousAttempt.request) &&
          canonicalJson(digests) ===
            canonicalJson(previousAttempt.attemptedDigests) &&
          (pending.request as Record<string, unknown>).op === "update" &&
          (pending.request as Record<string, unknown>).target === "v2" &&
          (row.request as Record<string, unknown>).target === "v2";
      }),
      "both interrupted records leave the fallback unattempted",
    );
    addAssertion(
      trace,
      "successful-fallback-outcome-failed-restored",
      (finalJournal.lastOutcome as Record<string, unknown> | null)?.status ===
          "failed_restored" && lastOutcome?.status === "failed_restored",
      `journal=${
        String(
          (finalJournal.lastOutcome as Record<string, unknown> | null)?.status,
        )
      } outcome=${String(lastOutcome?.status)}`,
    );

    const exhaustedRoot = rootFor();
    const exhausted = await stepOptions(exhaustedRoot, "enabled_update", [
      { boundary: "committed_pending_publish", reopen: false },
      { boundary: "fallback_pending_before_mark", reopen: true },
      { boundary: "fallback_pending_before_mark", reopen: true },
      { boundary: "fallback_attempt_marked", reopen: true },
      { boundary: null, reopen: true },
    ]);
    const marked = (exhausted.scenario.launches[3].marker as DurableMarker)
      .snapshot;
    const markedPending = marked.pending as Record<string, unknown>;
    const markedAttempt = marked.attempt as Record<string, unknown>;
    const markedDigests = markedAttempt.attemptedDigests as string[];
    addAssertion(
      trace,
      "fallback-marked-within-budget-before-evaluation",
      markedDigests.length === 2 && new Set(markedDigests).size === 2 &&
        markedDigests.includes(String(markedPending.targetDigest)) &&
        (exhausted.scenario.launches[3].marker as DurableMarker)
            .evaluationCount === 0,
      `attempted=${JSON.stringify(markedDigests)} evalCount=${
        (exhausted.scenario.launches[3].marker as DurableMarker).evaluationCount
      }`,
    );
    const exhaustedJournal = exhausted.final.journal as Record<string, unknown>;
    addAssertion(
      trace,
      "attempted-fallback-not-retried-after-restart",
      exhausted.final.evaluationCount === 0 &&
        exhaustedJournal.blockedReason !== null &&
        (exhaustedJournal.lastOutcome as Record<string, unknown> | null)
            ?.status ===
          "failed_quarantined",
      `evaluationCount=${String(exhausted.final.evaluationCount)} blocked=${
        String(exhaustedJournal.blockedReason)
      }`,
    );
    succeeded = true;
  } catch (error) {
    trace.failure = error instanceof Error ? error.message : String(error);
    throw error;
  } finally {
    await recordTrace(tracePath, trace);
    if (succeeded) {
      await Deno.remove(profilePath, { recursive: true }).catch(() => {});
    }
  }
}

if (import.meta.main) {
  runProcessRecoveryMatrix().then(() => {
    console.log(JSON.stringify({ result: "passed" }));
  }).catch((error: unknown) => {
    console.error(JSON.stringify({
      result: "failed",
      error: error instanceof Error ? error.message : String(error),
    }));
    Deno.exitCode = 1;
  });
}
