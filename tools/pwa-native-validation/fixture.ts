// SPDX-License-Identifier: MPL-2.0
// Isolated compiler/signing fixture; never calls installation or Launch Services.
import { MacOSSupport } from "../../browser-features/modules/modules/pwa/supports/MacOS.sys.mts";
import { isAbsolute, join, relative, resolve, sep } from "node:path";

const COMPILE = "/usr/bin/osacompile";
const SIGN = "/usr/bin/codesign";
const SCRIPT = `on run
set p to POSIX path of (path to me)
do shell script quoted form of (p & "Contents/Resources/launcher.sh")
end run`;
const ID = "one.ablaze.floorp.pwa.fixture";
const MAX_OUTPUT = 32768;

export class ExposedSupport extends MacOSSupport {
  compile(path: string): Promise<void> { return super.compileApplet(path); }
  sign(path: string): Promise<void> { return super.signApplet(path, ID); }
  verify(path: string): Promise<"valid" | "invalid"> { return super.verifyApplet(path); }
}

type Request = { command: string; arguments: string[]; stderr: string };
type ProcessResult = { code: number; output: string };
type Spawn = (command: string, args: string[]) => Promise<ProcessResult>;

export async function privateRoot(path: string): Promise<string> {
  if (!path || !isAbsolute(path)) throw new Error("root must be absolute");
  const info = await Deno.lstat(path);
  if (!info.isDirectory || info.isSymlink) throw new Error("root must be a real directory");
  const canonical = await Deno.realPath(path);
  if (resolve(path) !== canonical) throw new Error("root must be canonical, no symlink ancestors");
  if (Deno.build.os !== "windows" && (info.mode === null || (info.mode & 0o077) !== 0)) {
    throw new Error("root must be private (0700)");
  }
  return canonical;
}

export async function fixturePath(root: string, path: string, mustExist: boolean): Promise<void> {
  if (!isAbsolute(path) || resolve(path) !== path) throw new Error("fixture path must be absolute and normalized");
  const rel = relative(root, path);
  if (!rel || rel === ".." || rel.startsWith(".." + sep) || isAbsolute(rel)) throw new Error("fixture path escapes root");
  // Only one direct child, no attacker-controlled path components.
  if (rel.includes(sep) || rel !== "fixture.app") throw new Error("unexpected fixture path");
  if (mustExist) {
    const info = await Deno.lstat(path);
    if (!info.isDirectory || info.isSymlink || await Deno.realPath(path) !== path) throw new Error("unsafe fixture directory");
  } else {
    try { await Deno.lstat(path); throw new Error("fixture already exists"); }
    catch (error) { if (!(error instanceof Deno.errors.NotFound)) throw error; }
  }
}

export function adapter(root: string, spawn: Spawn) {
  return { Subprocess: { call: async (req: Request) => {
    if (req.stderr !== "stdout" || !Array.isArray(req.arguments)) throw new Error("invalid subprocess request");
    const args = req.arguments;
    let path: string;
    if (req.command === COMPILE && args.length === 4 && args[0] === "-o" && args[2] === "-e" && args[3] === SCRIPT) {
      path = args[1]; await fixturePath(root, path, false);
    } else if (req.command === SIGN && args.length === 8 && args[0] === "--force" && args[1] === "--deep" && args[2] === "--sign" && args[3] === "-" && args[4] === "--identifier" && args[5] === ID && args[6] === "--timestamp=none") {
      path = args[7]; await fixturePath(root, path, true);
    } else if (req.command === SIGN && args.length === 4 && args[0] === "--verify" && args[1] === "--deep" && args[2] === "--strict") {
      path = args[3]; await fixturePath(root, path, true);
    } else throw new Error("command or argv not allowlisted");
    const result = await spawn(req.command, args);
    let consumed = false;
    return { stdout: { readString: async () => { if (consumed) return ""; consumed = true; return result.output; } }, wait: async () => ({ exitCode: result.code }) };
  } } };
}

export async function bounded(command: string, args: string[], timeout = 30000, settled?: (pid: number, status: Deno.CommandStatus) => void): Promise<ProcessResult> {
  const env = { HOME: Deno.env.get("HOME") ?? "", TMPDIR: Deno.env.get("TMPDIR") ?? "", TEMP: Deno.env.get("TEMP") ?? "", TMP: Deno.env.get("TMP") ?? "", DENO_DIR: Deno.env.get("DENO_DIR") ?? "", PATH: "/usr/bin:/bin" };
  const child = new Deno.Command(command, { args, stdin: "null", stdout: "piped", stderr: "piped", clearEnv: true, env }).spawn();
  let timedOut = false;
  let overflow = false;
  const timer = setTimeout(() => { timedOut = true; try { child.kill("SIGKILL"); } catch { /* already exited */ } }, timeout);
  const read = async (stream: ReadableStream<Uint8Array>) => {
    const reader = stream.getReader(); const pieces: Uint8Array[] = []; let length = 0;
    try { for (;;) { const { value, done } = await reader.read(); if (done) break; length += value.length; if (length > MAX_OUTPUT) { overflow = true; try { child.kill("SIGKILL"); } catch { /* exited */ } break; } pieces.push(value); } }
    finally { reader.releaseLock(); }
    const buffer = new Uint8Array(pieces.reduce((n, p) => n + p.length, 0)); let offset = 0; for (const p of pieces) { buffer.set(p, offset); offset += p.length; }
    return new TextDecoder().decode(buffer);
  };
  try {
    const [out, err, status] = await Promise.allSettled([read(child.stdout), read(child.stderr), child.status]);
    if (status.status === "fulfilled") settled?.(child.pid, status.value);
    if (timedOut) throw new Error("command timed out");
    if (overflow) throw new Error("command output exceeded bound");
    if (out.status === "rejected" || err.status === "rejected" || status.status === "rejected") throw new Error("child output or status failed");
    return { code: status.value.code, output: out.value + err.value };
  } finally { clearTimeout(timer); }
}

export async function installAdapter(root: string, spawn: Spawn): Promise<() => void> {
  await privateRoot(root);
  const target = globalThis as unknown as { ChromeUtils?: unknown };
  const previous = target.ChromeUtils;
  target.ChromeUtils = { importESModule: (uri: string) => {
    if (uri !== "resource://gre/modules/Subprocess.sys.mjs") throw new Error("unexpected module URI");
    return adapter(root, spawn);
  } };
  return () => { if (previous === undefined) delete target.ChromeUtils; else target.ChromeUtils = previous; };
}

export { COMPILE, SIGN, ID, SCRIPT, join };
