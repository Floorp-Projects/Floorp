// SPDX-License-Identifier: MPL-2.0
import { assert, assertEquals, assertRejects } from "@std/assert";
import { createQaDispatchWireTransportForTest } from "./fdh_qa_dispatch_gh.ts";
import type {
  QaDispatchWireExitStatus,
  QaDispatchWireTestChild,
  QaDispatchWireTestFixture,
  QaDispatchWireTestSpawn,
} from "./fdh_qa_dispatch_gh_types.ts";
import type { QaDispatchRequest } from "./fdh_qa_dispatch_types.ts";

const request: QaDispatchRequest = Object.freeze({
  taskId: "qa-owned",
  agent: "lead",
  sourceRunId: 17,
  sourceAttempt: 2,
  sourceHead: "a".repeat(40),
  qaHead: "b".repeat(40),
  mode: "unsigned-validation",
});
const GET_ARGS = Object.freeze([
  "api",
  "--hostname",
  "github.com",
  "--include",
  "--method",
  "GET",
  "-H",
  "Accept: application/vnd.github+json",
  "-H",
  "X-GitHub-Api-Version: 2026-03-10",
  "repos/Floorp-Projects/Floorp/git/ref/heads/main",
]);
const POST_ARGS = Object.freeze([
  "api",
  "--hostname",
  "github.com",
  "--include",
  "--method",
  "POST",
  "-H",
  "Accept: application/vnd.github+json",
  "-H",
  "X-GitHub-Api-Version: 2026-03-10",
  "--input",
  "-",
  "repos/Floorp-Projects/Floorp/actions/workflows/validate-final-package-qa.yml/dispatches",
]);

function stream(
  chunks: readonly Uint8Array[],
  onCancel?: () => void,
): ReadableStream<Uint8Array> {
  return new ReadableStream({
    start(controller) {
      for (const chunk of chunks) controller.enqueue(chunk);
      controller.close();
    },
    cancel() {
      onCancel?.();
    },
  });
}
function bytes(value: string): Uint8Array {
  return new TextEncoder().encode(value);
}
function pendingStream(onCancel?: () => void): ReadableStream<Uint8Array> {
  return new ReadableStream({
    cancel() {
      onCancel?.();
    },
  });
}
function response(status: number, body: string): Uint8Array {
  return bytes(
    "HTTP/2 " + status + "\r\ncontent-type: application/json\r\n\r\n" + body,
  );
}
function child(
  stdout: ReadableStream<Uint8Array>,
  stderr: ReadableStream<Uint8Array>,
  status: Promise<QaDispatchWireExitStatus>,
  onKill?: () => void,
  stdin?: WritableStream<Uint8Array>,
): QaDispatchWireTestChild {
  return {
    stdout,
    stderr,
    status,
    stdin,
    kill() {
      onKill?.();
    },
  };
}
function deps(
  make: (
    args: readonly string[],
    stdinBody: string | null,
  ) => QaDispatchWireTestChild,
  timer?: (callback: () => void, milliseconds: number) => unknown,
): QaDispatchWireTestFixture {
  const spawns: QaDispatchWireTestSpawn[] = [];
  return {
    spawns,
    deps: {
      spawn(args, stdinBody) {
        spawns.push([[...args], stdinBody]);
        return make(args, stdinBody);
      },
      now: () => performance.now(),
      setTimer: timer ??
        ((callback, milliseconds) => setTimeout(callback, milliseconds)),
      clearTimer: (handle) => clearTimeout(handle as number),
    },
  };
}

Deno.test("main ref uses only the fixed GET endpoint and headers", async () => {
  const f = deps(() =>
    child(
      stream([response(200, '{"ref":"refs/heads/main"}')]),
      stream([]),
      Promise.resolve({ success: true, code: 0 }),
    )
  );
  const result = await createQaDispatchWireTransportForTest(f.deps).mainRef();
  assertEquals(result.status, 200);
  assertEquals(result.body, { ref: "refs/heads/main" });
  assertEquals(f.spawns.length, 1);
  assertEquals(f.spawns[0][0], GET_ARGS);
  assertEquals(f.spawns[0][1], null);
});

Deno.test("dispatch derives the four fixed inputs and POSTs JSON stdin", async () => {
  let stdinText = "";
  const stdin = new WritableStream<Uint8Array>({
    write(chunk) {
      stdinText += new TextDecoder().decode(chunk);
    },
  });
  const f = deps(() =>
    child(
      stream([response(
        200,
        JSON.stringify({
          workflow_run_id: 23,
          run_url:
            "https://api.github.com/repos/Floorp-Projects/Floorp/actions/runs/23",
          html_url: "https://github.com/Floorp-Projects/Floorp/actions/runs/23",
        }),
      )]),
      stream([]),
      Promise.resolve({ success: true, code: 0 }),
      undefined,
      stdin,
    )
  );
  const result = await createQaDispatchWireTransportForTest(f.deps).dispatch(
    request,
  );
  assertEquals(result.status, 200);
  assertEquals(f.spawns.length, 1);
  assertEquals(f.spawns[0][0], POST_ARGS);
  assertEquals(
    f.spawns[0][1],
    JSON.stringify({
      ref: "main",
      inputs: {
        package_run_id: "17",
        floorp_head_sha: "a".repeat(40),
        validation_only: true,
        pre_publication: false,
      },
    }),
  );
  assertEquals(stdinText, f.spawns[0][1]);
});

Deno.test("invalid dispatch request rejects before spawning", async () => {
  const f = deps(() =>
    child(
      stream([]),
      stream([]),
      Promise.resolve({ success: true, code: 0 }),
    )
  );
  const wire = createQaDispatchWireTransportForTest(f.deps);
  await assertRejects(() =>
    wire.dispatch(
      { ...request, sourceRunId: 0 } as unknown as QaDispatchRequest,
    )
  );
  assertEquals(f.spawns.length, 0);
});

Deno.test("204 is an honest status with no fabricated body", async () => {
  const f = deps(() =>
    child(
      stream([response(204, "")]),
      stream([]),
      Promise.resolve({ success: true, code: 0 }),
    )
  );
  const result = await createQaDispatchWireTransportForTest(f.deps).dispatch(
    request,
  );
  assertEquals(result.status, 204);
  assertEquals(result.body, null);
});

Deno.test("denied and malformed responses stay sanitized", async () => {
  const denied = deps(() =>
    child(
      stream([response(403, '{"message":"Resource not accessible"}')]),
      stream([]),
      Promise.resolve({ success: false, code: 1 }),
    )
  );
  const deniedResult = await createQaDispatchWireTransportForTest(denied.deps)
    .dispatch(request);
  assertEquals(deniedResult.status, 403);
  const malformed = deps(() =>
    child(
      stream([bytes("not-an-http-response")]),
      stream([]),
      Promise.resolve({ success: true, code: 0 }),
    )
  );
  const malformedResult = await createQaDispatchWireTransportForTest(
    malformed.deps,
  )
    .dispatch(request);
  assertEquals(malformedResult.status, 0);
  assertEquals(malformedResult.body, null);
});

Deno.test("stderr secrets are never copied into the public result", async () => {
  const f = deps(() =>
    child(
      stream([response(200, '{"workflow_run_id":23}')]),
      stream([bytes("token=secret-value")]),
      Promise.resolve({ success: true, code: 0 }),
    )
  );
  const result = await createQaDispatchWireTransportForTest(f.deps).dispatch(
    request,
  );
  assertEquals(result.status, 200);
  assert(!JSON.stringify(result).includes("secret-value"));
});

Deno.test("timeout kills the owned child and cancels readers", async () => {
  let killed = false, canceled = false;
  let resolveStatus: (value: QaDispatchWireExitStatus) => void = () => {};
  const status = new Promise<QaDispatchWireExitStatus>((resolve) => {
    resolveStatus = resolve;
  });
  const f = deps(
    () =>
      child(
        pendingStream(() => {
          canceled = true;
        }),
        pendingStream(() => {
          canceled = true;
        }),
        status,
        () => {
          killed = true;
          resolveStatus({ success: false, code: -1 });
        },
      ),
    (callback) => {
      callback();
      return 1;
    },
  );
  const result = await createQaDispatchWireTransportForTest(f.deps).dispatch(
    request,
  );
  assertEquals(result.status, 0);
  assertEquals(killed, true);
  assertEquals(canceled, true);
});

Deno.test("stdout cap kills the owned child without an unbounded buffer", async () => {
  let killed = false,
    resolveStatus: (value: QaDispatchWireExitStatus) => void = () => {};
  const status = new Promise<QaDispatchWireExitStatus>((resolve) => {
    resolveStatus = resolve;
  });
  const huge = new Uint8Array(129 * 1024);
  const f = deps(() =>
    child(
      stream([huge]),
      stream([]),
      status,
      () => {
        killed = true;
        resolveStatus({ success: false, code: -1 });
      },
    )
  );
  const result = await createQaDispatchWireTransportForTest(f.deps).dispatch(
    request,
  );
  assertEquals(result.status, 0);
  assertEquals(killed, true);
});

Deno.test("one spawn per call; no automatic retry", async () => {
  const f = deps(() =>
    child(
      stream([response(200, "{}")]),
      stream([]),
      Promise.resolve({ success: true, code: 0 }),
    )
  );
  const wire = createQaDispatchWireTransportForTest(f.deps);
  await wire.mainRef();
  await wire.dispatch(request);
  assertEquals(f.spawns.length, 2);
});
