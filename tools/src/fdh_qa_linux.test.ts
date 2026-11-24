// SPDX-License-Identifier: MPL-2.0

import {
  assert,
  assertEquals,
  assertMatch,
  assertRejects,
  assertStringIncludes,
  assertThrows,
} from "@std/assert";
import { decodeBase64 } from "@std/encoding/base64";
import {
  buildAcquireScript,
  buildCleanupScript,
  buildHostSshArgs,
  buildImageScript,
  buildInstallScript,
  buildLaunchScript,
  buildMarionetteScript,
  buildPreflightScript,
  buildUploadScript,
  LINUX_QA_DOCKERFILE,
  LinuxQaError,
  linuxQaImageTag,
  LinuxQaTarget,
  parseLinuxQaTargetConfig,
} from "./fdh_qa_linux.ts";
import { wrapForStdin } from "./fdh_qa_remote.ts";
import type { LinuxQaTargetConfig } from "./fdh_qa_linux_types.ts";

const RAW = {
  hostSsh: "root@100.86.32.44",
  containerName: "fdh-qa-linux",
  memoryMb: 2560,
  cpus: 2,
  network: "none",
  marionettePort: 2839,
  minFreeBytes: 5_000_000_000,
  minAvailableMemoryMb: 3000,
};
const CONFIG: LinuxQaTargetConfig = parseLinuxQaTargetConfig(RAW);
const IMAGE = "fdh-qa-linux:0123456789ab";
const TASK = "kan-144-linux";
const SHA = "b".repeat(64);

Deno.test("Linux QA config: accepts the Debian target and rejects unsafe values", () => {
  assertEquals(CONFIG.network, "none");
  assert(Object.isFrozen(CONFIG));
  const cases: Array<[Record<string, unknown>, string]> = [
    [{ ...RAW, extra: true }, "unknown key"],
    [{ ...RAW, hostSsh: "root@host;id" }, "hostSsh"],
    [{ ...RAW, containerName: "nod-server" }, "containerName"],
    [{ ...RAW, containerName: "fdh-qa-$(id)" }, "containerName"],
    [{ ...RAW, memoryMb: 128 }, "memoryMb"],
    [{ ...RAW, cpus: 0.1 }, "cpus"],
    [{ ...RAW, cpus: 1.234 }, "cpus"],
    [{ ...RAW, network: "host" }, "network"],
    [{ ...RAW, marionettePort: 22 }, "marionettePort"],
    [{ ...RAW, minAvailableMemoryMb: -1 }, "minAvailableMemoryMb"],
  ];
  for (const [value, message] of cases) {
    assertThrows(() => parseLinuxQaTargetConfig(value), Error, message);
  }
  const missing: Record<string, unknown> = { ...RAW };
  delete missing.network;
  assertThrows(
    () => parseLinuxQaTargetConfig(missing),
    Error,
    "missing network",
  );
});

Deno.test("Linux QA image: tag is derived from the definition; the image runs as an unprivileged user", async () => {
  assertMatch(await linuxQaImageTag(), /^fdh-qa-linux:[0-9a-f]{12}$/);
  assertStringIncludes(LINUX_QA_DOCKERFILE, "USER qa");
  assertStringIncludes(LINUX_QA_DOCKERFILE, "procps");
  assertThrows(
    () => buildPreflightScript(CONFIG, "evil:latest"),
    Error,
    "image tag",
  );
  assertThrows(
    () =>
      new LinuxQaTarget(CONFIG, "fdh-qa-linux:latest", () => Promise.reject()),
    Error,
    "image tag",
  );
});

Deno.test("Linux QA ssh args: one hop; argv mode carries base64 only", () => {
  const stdin = buildHostSshArgs(CONFIG, "echo hi", "stdin");
  assertEquals(stdin.at(-2), "root@100.86.32.44");
  assertEquals(stdin.at(-1), "bash -s");
  const argv = buildHostSshArgs(CONFIG, "echo 'secret text'", "argv");
  const match = /printf %s '([A-Za-z0-9+/=]+)' \| base64 -d/.exec(argv.at(-1)!);
  assert(match);
  assertEquals(
    new TextDecoder().decode(decodeBase64(match[1])),
    "echo 'secret text'",
  );
  assert(!argv.at(-1)!.includes("secret"));
  assertThrows(
    () => buildHostSshArgs(CONFIG, "x".repeat(20_000), "argv"),
    Error,
    "too long",
  );
});

Deno.test("Linux QA scripts: only the named, labelled container is touched", () => {
  const acquire = buildAcquireScript(CONFIG, IMAGE, TASK);
  assertStringIncludes(acquire, "NAME='fdh-qa-linux'");
  assertStringIncludes(acquire, "--label fdh.qa=1");
  assertStringIncludes(acquire, '--label "fdh.qa.task=$TASK"');
  assertStringIncludes(acquire, "--memory 2560m --memory-swap 2560m");
  assertStringIncludes(acquire, "--cpus 2");
  assertStringIncludes(
    acquire,
    "--cap-drop ALL --security-opt no-new-privileges",
  );
  assertStringIncludes(acquire, "--network none");
  assert(!acquire.includes("--privileged"));
  assert(!/-p \d|--publish/.test(acquire), "Marionette must not be published");
  // A same-named container we did not create is reported, never removed.
  assertStringIncludes(acquire, "printf %s -foreign");
  const cleanup = buildCleanupScript(CONFIG, IMAGE, TASK);
  assertStringIncludes(
    cleanup,
    'if [ -n "$H" ] && [ "$H" != "$TASK" ]; then fail container-held; fi',
  );
  assertStringIncludes(cleanup, 'docker rm -f "$NAME"');
  for (const script of [cleanup, acquire]) {
    assert(!/docker (rm|stop|kill)[^\n]*\$\(docker ps/.test(script));
    assert(!script.includes("prune"));
  }
});

Deno.test("Linux QA scripts: work inside the container runs as qa, after an ownership check", () => {
  const scripts = [
    buildUploadScript(CONFIG, IMAGE, TASK),
    buildInstallScript(CONFIG, IMAGE, TASK, "tar.xz", SHA),
    buildLaunchScript(CONFIG, IMAGE, TASK),
    buildMarionetteScript(CONFIG, IMAGE, TASK, { op: "screenshot" }),
  ];
  for (const script of scripts) {
    assertStringIncludes(script, "\nown\n");
    for (const line of script.split("\n")) {
      if (/docker exec /.test(line)) assertStringIncludes(line, "-u qa");
    }
  }
  const install = buildInstallScript(CONFIG, IMAGE, TASK, "tar.xz", SHA);
  assert(
    install.indexOf("fail sha256-mismatch") < install.indexOf("tar -xJf"),
    "hash before extract",
  );
  assertStringIncludes(
    buildInstallScript(CONFIG, IMAGE, TASK, "tar.bz2", SHA),
    "tar -xjf",
  );
  assertThrows(
    () => buildInstallScript(CONFIG, IMAGE, TASK, "tar.xz", "XYZ"),
    Error,
    "sha256",
  );
  assertThrows(
    () => buildLaunchScript(CONFIG, IMAGE, TASK, "file:///etc/passwd"),
    Error,
    "startUrl",
  );
  assertThrows(() => buildAcquireScript(CONFIG, IMAGE, "Bad Task"), Error);
});

Deno.test("Linux QA scripts: image build writes only a temp context and is skipped when present", () => {
  const script = buildImageScript(CONFIG, IMAGE);
  assertStringIncludes(
    script,
    'if docker image inspect "$IMAGE" >/dev/null 2>&1; then result \'{"ok":true,"built":false}\'; exit 0; fi',
  );
  assertStringIncludes(script, "mktemp -d /tmp/fdh-qa-image.XXXXXX");
  assertStringIncludes(script, "trap 'rm -rf \"$CTX\"' EXIT");
  assertStringIncludes(script, '--label fdh.qa.image=1 "$CTX"');
});

function fake(replies: Array<string | [string, number]>) {
  const calls: Array<{ script: string; stdin: boolean }> = [];
  const transport = (script: string, stdin?: ReadableStream<Uint8Array>) => {
    calls.push({ script, stdin: stdin !== undefined });
    const reply = replies.shift() ?? "";
    const [stdout, code] = typeof reply === "string" ? [reply, 0] : reply;
    return Promise.resolve({ code, stdout, stderr: "" });
  };
  return { calls, transport };
}

Deno.test("Linux QA target: preflight blockers fail closed", async () => {
  const { transport } = fake([
    'FDH_RESULT {"ok":true,"freeBytes":10,"availableMemoryMb":100,"imagePresent":false,"containerHolder":"-foreign"}',
    'FDH_RESULT {"ok":true,"imagePresent":true,"containerHolder":null}',
  ]);
  const target = new LinuxQaTarget(CONFIG, IMAGE, transport);
  const first = await target.preflight();
  assertEquals(first.blockers, [
    "low-disk",
    "low-memory",
    "image-missing",
    "container-held",
  ]);
  assertEquals(first.containerHolder, "-foreign");
  // Missing numbers are blockers, not passes.
  assertEquals((await target.preflight()).blockers, ["low-disk", "low-memory"]);
});

Deno.test("Linux QA target: stage streams only the upload; errors carry the step", async () => {
  const { calls, transport } = fake([
    'FDH_RESULT {"ok":true,"bytes":7}',
    'FDH_RESULT {"ok":true,"version":"12.21.1@159.0.1"}',
  ]);
  const target = new LinuxQaTarget(CONFIG, IMAGE, transport);
  assertEquals(
    await target.stage(TASK, "tar.xz", new Blob(["x"]).stream(), SHA),
    { bytes: 7, version: "12.21.1@159.0.1" },
  );
  assertEquals(calls.map((c) => c.stdin), [true, false]);
  const failing = fake([
    ['FDH_RESULT {"ok":false,"error":"container-held"}', 3],
  ]);
  const error = await assertRejects(
    () => new LinuxQaTarget(CONFIG, IMAGE, failing.transport).acquire(TASK),
    LinuxQaError,
    "Linux QA acquire failed: container-held",
  );
  assertEquals(error.step, "acquire");
});

Deno.test("Linux QA target: cleanup is clean only when the container is gone", async () => {
  const { transport } = fake([
    'FDH_RESULT {"ok":true,"containerRemoved":true}',
    'FDH_RESULT {"ok":true,"containerRemoved":false}',
  ]);
  const target = new LinuxQaTarget(CONFIG, IMAGE, transport);
  assertEquals((await target.cleanup(TASK)).clean, true);
  assertEquals((await target.cleanup(TASK)).clean, false);
});

Deno.test({
  name:
    "Linux QA scripts: every generated script parses with bash -n (needs --allow-run)",
  ignore: Deno.permissions.querySync({ name: "run" }).state !== "granted",
  async fn() {
    const scripts = [
      buildPreflightScript(CONFIG, IMAGE),
      buildImageScript(CONFIG, IMAGE),
      buildAcquireScript(CONFIG, IMAGE, TASK),
      buildUploadScript(CONFIG, IMAGE, TASK),
      buildInstallScript(CONFIG, IMAGE, TASK, "tar.xz", SHA),
      buildLaunchScript(CONFIG, IMAGE, TASK),
      buildMarionetteScript(CONFIG, IMAGE, TASK, { op: "eval", script: "1" }),
      buildCleanupScript(CONFIG, IMAGE, TASK),
    ];
    for (const script of scripts) {
      const child = new Deno.Command("bash", {
        args: ["-n"],
        stdin: "piped",
        stdout: "null",
        stderr: "piped",
      }).spawn();
      const writer = child.stdin.getWriter();
      await writer.write(new TextEncoder().encode(wrapForStdin(script)));
      await writer.close();
      const out = await child.output();
      assertEquals(out.code, 0, new TextDecoder().decode(out.stderr));
    }
  },
});
