// SPDX-License-Identifier: MPL-2.0

import {
  createIsolatedBrowserLaunch,
  startIsolatedBrowser,
} from "../src/browser_launcher.ts";
import type { IsolatedBrowserProcessControl } from "../src/browser_launcher.ts";
import { SinglePassMarionette } from "./marionette.ts";
import { verifyBrowserResult } from "./cases.ts";
import type { BrowserResult, PreparedProductSubject } from "./types.ts";

const INPUT = "/input";
const MAX_LOG = 2 * 1024 * 1024;

function cgroup(): string {
  const lines = Deno.readTextFileSync("/proc/self/cgroup").trim().split("\n");
  if (
    lines.length !== 1 ||
    !lines[0].startsWith("0::/system.slice/qa3-workspace-")
  ) {
    throw new Error("A dedicated systemd cgroup v2 worker is required");
  }
  return lines[0].slice(3);
}

function generation(pid: number): string {
  const text = Deno.readTextFileSync(`/proc/${pid}/stat`);
  return text.slice(text.lastIndexOf(")") + 2).split(" ")[19];
}

function processIds(group: string): number[] {
  const ids: number[] = [];
  const visit = (directory: string) => {
    ids.push(
      ...Deno.readTextFileSync(`${directory}/cgroup.procs`).trim().split(/\s+/)
        .filter(Boolean).map(Number),
    );
    for (const entry of Deno.readDirSync(directory)) {
      if (entry.isDirectory) visit(`${directory}/${entry.name}`);
    }
  };
  visit(`/sys/fs/cgroup${group}`);
  return [...new Set(ids)].filter((pid) => pid !== Deno.pid);
}

function portListening(port: number): boolean {
  return ["/proc/net/tcp", "/proc/net/tcp6"].some((file) =>
    Deno.readTextFileSync(file).split("\n").slice(1).some((line) => {
      const fields = line.trim().split(/\s+/);
      return fields[3] === "0A" &&
        Number.parseInt(fields[1]?.split(":")[1], 16) === port;
    })
  );
}

async function wait(
  predicate: () => boolean,
  milliseconds: number,
): Promise<void> {
  const deadline = Date.now() + milliseconds;
  while (!predicate()) {
    if (Date.now() >= deadline) {
      throw new Error("Bounded readiness or owned cleanup deadline expired");
    }
    await new Promise((resolve) => setTimeout(resolve, 50));
  }
}

async function captureLog(
  stream: ReadableStream<Uint8Array>,
  path: string,
): Promise<void> {
  const file = await Deno.open(path, {
    createNew: true,
    write: true,
    mode: 0o600,
  });
  let total = 0;
  try {
    for await (const data of stream) {
      total += data.length;
      if (total > MAX_LOG) {
        throw new Error(
          "Original browser diagnostic stream exceeded its bound",
        );
      }
      let offset = 0;
      while (offset < data.length) {
        offset += await file.write(data.subarray(offset));
      }
    }
  } finally {
    file.close();
  }
}

export async function runWorker(state: string): Promise<void> {
  if (Deno.build.os !== "linux" || Deno.build.arch !== "x86_64") {
    throw new Error("Pilot requires native Linux x64");
  }
  const group = cgroup();
  if (processIds(group).length) {
    throw new Error("Worker cgroup contains pre-existing processes");
  }
  const subject = JSON.parse(
    await Deno.readTextFile(`${INPUT}/subject.json`),
  ) as PreparedProductSubject;
  const binary = `${INPUT}/browser/${subject.binaryRelativePath}`;
  if (
    subject.binaryRelativePath.includes("..") ||
    subject.binaryRelativePath.startsWith("/")
  ) throw new Error("Product binary path is not owned");
  const listener = Deno.listen({ hostname: "127.0.0.1", port: 0 });
  const port = (listener.addr as Deno.NetAddr).port;
  listener.close();
  const launch = createIsolatedBrowserLaunch({
    binaryPath: binary,
    port,
  });
  const nonce = crypto.randomUUID();
  const requests: unknown[] = [];
  const transcript: unknown[] = [];
  let transcriptBytes = 0;
  const logs: Promise<void>[] = [];
  const rootGenerations = new Map<number, string>();
  let cleanupPhase = false;
  let earlyExit = false;
  let diagnosticFailure: unknown;
  let browserResult: unknown = null;
  let originalError: unknown;
  let cleanupError: unknown;
  let client: SinglePassMarionette | undefined;

  const terminate = async () => {
    cleanupPhase = true;
    for (const signal of ["SIGTERM", "SIGKILL"] as const) {
      for (const pid of processIds(group)) {
        try {
          Deno.kill(pid, signal);
        } catch (error) {
          if (!(error instanceof Deno.errors.NotFound)) throw error;
        }
      }
      try {
        await wait(
          () => processIds(group).length === 0,
          signal === "SIGTERM" ? 5000 : 10000,
        );
        break;
      } catch (error) {
        if (signal === "SIGKILL") throw error;
      }
    }
    if (processIds(group).length || portListening(port)) {
      throw new Error("Owned process tree or port survived cleanup");
    }
  };
  const control: IsolatedBrowserProcessControl = {
    async capture(child, view, platform) {
      if (
        platform !== "linux" || view.profilePath !== launch.profilePath ||
        Deno.readTextFileSync(`/proc/${child.pid}/cgroup`).trim() !==
          `0::${group}` ||
        Deno.readLinkSync(`/proc/${child.pid}/exe`) !== binary
      ) throw new Error("Native process ownership mismatch");
      rootGenerations.set(child.pid, generation(child.pid));
      await wait(
        () =>
          portListening(port) || earlyExit || diagnosticFailure !== undefined,
        45000,
      );
      if (earlyExit || diagnosticFailure !== undefined) {
        throw new Error(
          "Browser exited or diagnostics failed before readiness",
        );
      }
      if (generation(child.pid) !== rootGenerations.get(child.pid)) {
        throw new Error("Browser root generation changed");
      }
      return { platform, rootPid: child.pid };
    },
    async stop() {
      await terminate();
    },
    async abort() {
      await terminate();
    },
  };
  const output: Record<string, unknown> = {
    schema: "qa3-final-workspace-worker-v1",
    subject,
    nonce,
    cgroup: group,
    sourceBinding: JSON.parse(
      await Deno.readTextFile(`${INPUT}/support-binding.json`),
    ),
    profile: launch.profilePath,
    marionettePort: port,
    status: "FAIL",
    publicationAuthorized: false,
    nativeProductQualification: "UNVERIFIED",
  };
  let running: Awaited<ReturnType<typeof startIsolatedBrowser>> | undefined;
  const finishCleanup = async () => {
    if (running) await running.stop();
    else await terminate();
    await Promise.all(logs);
    if (diagnosticFailure !== undefined) throw diagnosticFailure;
    try {
      await Deno.lstat(launch.profilePath);
      throw new Error("Owned profile remains");
    } catch (error) {
      if (!(error instanceof Deno.errors.NotFound)) throw error;
    }
    output.ownedCleanupComplete = true;
  };
  try {
    running = await startIsolatedBrowser(launch, {
      environment: {
        HOME: state,
        TMPDIR: state,
        PATH: "/usr/bin:/bin",
        LANG: "C.UTF-8",
      },
      processControl: control,
      spawn(command, options) {
        const child = new Deno.Command(command[0], {
          args: [...command.slice(1), "--headless"],
          clearEnv: true,
          env: options.env,
          stdin: "null",
          stdout: "piped",
          stderr: "piped",
        }).spawn();
        output.originalCommand = [...command, "--headless"];
        const watch = (p: Promise<void>) => {
          logs.push(p);
          void p.catch((error) => {
            diagnosticFailure ??= error;
          });
        };
        watch(captureLog(child.stdout, `${state}/browser.stdout`));
        watch(captureLog(child.stderr, `${state}/browser.stderr`));
        void child.status.then(() => {
          if (!cleanupPhase) earlyExit = true;
        });
        return child;
      },
    });
    output.rootPid = running.pid;
    output.rootGeneration = rootGenerations.get(running.pid);
    client = await SinglePassMarionette.connect(port, (event) => {
      transcriptBytes += new TextEncoder().encode(event.json).length;
      if (transcriptBytes > 4 * 1024 * 1024 || transcript.length > 1024) {
        throw new Error("Original transcript exceeded its bound");
      }
      transcript.push(event);
    });
    const send = async (
      command: string,
      params: Record<string, unknown>,
      ms = 30000,
    ) => {
      const response = await client!.command(command, params, ms);
      requests.push({ sequence: requests.length, command, response });
      return response;
    };
    await send("Marionette:SetContext", { value: "chrome" });
    const deadline = Date.now() + 45000;
    while (true) {
      const readiness = await send("WebDriver:ExecuteScript", {
        script:
          'const w=Services.wm.getMostRecentWindow("navigator:browser"); return Boolean(w?.gBrowserInit?.delayedStartupFinished && w.workspacesFuncs);',
        args: [],
      }) as { value?: unknown };
      if (readiness.value === true) break;
      if (Date.now() >= deadline) {
        throw new Error("Production window did not become ready");
      }
      await new Promise((resolve) => setTimeout(resolve, 200));
    }
    await send("WebDriver:ExecuteScript", {
      script:
        'const w=Services.wm.getMostRecentWindow("navigator:browser"); Services.scriptloader.loadSubScript("file:///input/browser-payload.js",w); return true;',
      args: [],
    });
    await send("WebDriver:SetTimeouts", { script: 900000 });
    const response = await send("WebDriver:ExecuteAsyncScript", {
      script:
        'const done=arguments[arguments.length-1],w=Services.wm.getMostRecentWindow("navigator:browser"); w.__QA3_WORKSPACE_PILOT__.run(arguments[0]).then(done,error=>done({originalBootstrapError:String(error)}));',
      args: [nonce],
      scriptTimeout: 900000,
    }, 900000) as { value?: unknown };
    browserResult = response.value;
    verifyBrowserResult(browserResult as BrowserResult, nonce, {
      appBuildID: subject.expectedAppBuildID,
      platformBuildID: subject.expectedPlatformBuildID,
      version: subject.expectedFirefoxVersion,
    });
    if (earlyExit || diagnosticFailure !== undefined) {
      throw new Error(
        "Browser exited or diagnostic capture failed before verified completion",
      );
    }
  } catch (error) {
    originalError = error;
  } finally {
    client?.close();
    try {
      await finishCleanup();
    } catch (error) {
      cleanupError = error;
    }
    output.browserResult = browserResult;
    output.requests = requests;
    output.transcript = transcript;
    output.originalError = originalError === undefined
      ? null
      : String(originalError).slice(0, 2048);
    output.cleanupError = cleanupError === undefined
      ? null
      : String(cleanupError).slice(0, 2048);
    if (originalError === undefined && cleanupError === undefined) {
      output.status = "PASS_WORKSPACE_BEHAVIOR_ONLY";
    }
    await Deno.writeTextFile(
      `${state}/worker-result.json`,
      JSON.stringify(output, null, 2) + "\n",
      { createNew: true, mode: 0o600 },
    );
  }
  if (output.status !== "PASS_WORKSPACE_BEHAVIOR_ONLY") {
    throw new Error(
      "Original required execution or cleanup failed; see immutable worker evidence",
    );
  }
}

if (import.meta.main) {
  if (Deno.args.length !== 1) {
    throw new Error("Worker requires one owned state directory");
  }
  await runWorker(Deno.args[0]);
}
