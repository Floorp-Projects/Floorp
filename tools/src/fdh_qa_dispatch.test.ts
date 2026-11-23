// SPDX-License-Identifier: MPL-2.0
import { assertEquals, assertRejects } from "@std/assert";
import { executeQaDispatch, planQaDispatch } from "./fdh_qa_dispatch.ts";
import type {
  QaDispatchDependencies,
  QaDispatchRecord,
  QaDispatchRequest,
} from "./fdh_qa_dispatch_types.ts";
const request: QaDispatchRequest = {
  taskId: "qa-owned",
  agent: "lead",
  sourceRunId: 17,
  sourceAttempt: 2,
  sourceHead: "a".repeat(40),
  qaHead: "b".repeat(40),
  mode: "unsigned-validation",
};
function fixture() {
  const events: string[] = [];
  let record: QaDispatchRecord | null = null;
  const deps: QaDispatchDependencies = {
    withLock: async (_key, work) => {
      events.push("lock");
      return await work();
    },
    load: () => Promise.resolve().then(() => record),
    save: (value) => {
      return Promise.resolve().then(() => {
        events.push("save:" + value.phase);
        record = structuredClone(value);
      });
    },
    preflight: () => {
      return Promise.resolve().then(() => {
        events.push("preflight");
        return { workflowId: 99 };
      });
    },
    begin: () => {
      return Promise.resolve().then(() => {
        events.push("intent");
        return { jobId: "intent-owned", epoch: 1, agent: "lead" };
      });
    },
    dispatch: () => {
      return Promise.resolve().then(() => {
        events.push("POST");
        return {
          status: 200,
          body: {
            workflow_run_id: 23,
            run_url:
              "https://api.github.com/repos/Floorp-Projects/Floorp/actions/runs/23",
            html_url:
              "https://github.com/Floorp-Projects/Floorp/actions/runs/23",
          },
        };
      });
    },
    readRun: (id) => {
      return Promise.resolve().then(() => {
        events.push("GET:" + id);
        return {
          id: 23,
          workflow_id: 99,
          run_attempt: 1,
          head_sha: "b".repeat(40),
          event: "workflow_dispatch",
          path: ".github/workflows/validate-final-package-qa.yml",
          repository: { full_name: "Floorp-Projects/Floorp" },
          head_repository: { full_name: "Floorp-Projects/Floorp" },
          status: "queued",
          conclusion: null,
        };
      });
    },
    finish: () => {
      return Promise.resolve().then(() => {
        events.push("confirm");
      });
    },
  };
  return { events, deps, record: () => record };
}
Deno.test("QA plan rejects accessor fields without invoking getters", async () => {
  let reads = 0;
  const input = { ...request };
  Object.defineProperty(input, "taskId", {
    get() {
      reads++;
      return "qa-owned";
    },
  });
  await assertRejects(() => planQaDispatch(input));
  assertEquals(reads, 0);
});

Deno.test("QA dispatch default plan has zero dependency effects", async () => {
  const f = fixture();
  const result = await executeQaDispatch(request, f.deps);
  assertEquals(result.status, "planned");
  assertEquals(f.events, []);
  assertEquals(result.publicationAuthorized, false);
  assertEquals(result.qaPassed, false);
});
Deno.test("QA dispatch rejects malformed and capability-override input before I/O", async () => {
  for (
    const invalid of [
      { ...request, sourceRunId: 0 },
      { ...request, sourceHead: "A".repeat(40) },
      { ...request, taskId: "../escape" },
      { ...request, repository: "other/repo" },
      { ...request, mode: "sign-anything" },
    ]
  ) {
    const f = fixture();
    assertEquals(
      (await executeQaDispatch(invalid, f.deps, "execute")).status,
      "invalid",
    );
    assertEquals(f.events, []);
  }
});
Deno.test("QA dispatch plan snapshots input before asynchronous hashing", async () => {
  const mutable = { ...request };
  const pending = planQaDispatch(mutable);
  mutable.sourceRunId = 91;
  const plan = await pending;
  assertEquals(plan.inputs.package_run_id, "17");
  assertEquals(plan.ref, "main");
  assertEquals(plan.inputs.validation_only, true);
});
Deno.test("QA dispatch journals before POST and saves exact returned ID before GET", async () => {
  const f = fixture();
  const result = await executeQaDispatch(request, f.deps, "execute");
  assertEquals(result.status, "assigned");
  assertEquals(result.runId, 23);
  assertEquals(result.runAttempt, 1);
  assertEquals(f.events, [
    "lock",
    "preflight",
    "intent",
    "save:intent",
    "POST",
    "save:dispatched",
    "GET:23",
    "save:dispatched",
    "confirm",
    "save:assigned",
  ]);
  assertEquals(result.qaPassed, false);
});
Deno.test("QA dispatch duplicate uses exact stored run without a second POST", async () => {
  const f = fixture();
  await executeQaDispatch(request, f.deps, "execute");
  f.events.length = 0;
  assertEquals(
    (await executeQaDispatch(request, f.deps, "execute")).status,
    "assigned",
  );
  assertEquals(f.events.includes("POST"), false);
  assertEquals(f.events.includes("GET:23"), true);
});
Deno.test("QA dispatch legacy 204 is unknown and never blindly redispatched", async () => {
  const f = fixture();
  f.deps.dispatch = () => {
    return Promise.resolve().then(() => {
      f.events.push("POST");
      return { status: 204, body: null };
    });
  };
  assertEquals(
    (await executeQaDispatch(request, f.deps, "execute")).status,
    "unknown",
  );
  f.events.length = 0;
  assertEquals(
    (await executeQaDispatch(request, f.deps, "execute")).status,
    "pending",
  );
  assertEquals(f.events.includes("POST"), false);
  assertEquals(f.events.some((e) => e.startsWith("GET:")), false);
});
Deno.test("QA dispatch lost response stays pending without candidate-run guessing", async () => {
  const f = fixture();
  f.deps.dispatch = () => {
    return Promise.resolve().then(() => {
      f.events.push("POST");
      throw new Error("transport lost");
    });
  };
  assertEquals(
    (await executeQaDispatch(request, f.deps, "execute")).status,
    "unknown",
  );
  f.events.length = 0;
  await executeQaDispatch(request, f.deps, "execute");
  assertEquals(f.events.includes("POST"), false);
});
Deno.test("QA dispatch foreign returned URL cannot authorize readback", async () => {
  const f = fixture();
  f.deps.dispatch = () =>
    Promise.resolve().then(() => ({
      status: 200,
      body: {
        workflow_run_id: 23,
        run_url: "https://evil.example/runs/23",
        html_url: "https://github.com/Floorp-Projects/Floorp/actions/runs/23",
      },
    }));
  assertEquals(
    (await executeQaDispatch(request, f.deps, "execute")).status,
    "unknown",
  );
  assertEquals(f.events.includes("GET:23"), false);
});
Deno.test("QA dispatch readback mismatch never confirms assignment", async () => {
  const f = fixture();
  const original = f.deps.readRun;
  f.deps.readRun = async (id) => ({
    ...await original(id) as Record<string, unknown>,
    head_sha: "c".repeat(40),
  });
  assertEquals(
    (await executeQaDispatch(request, f.deps, "execute")).status,
    "mismatch",
  );
  assertEquals(f.events.includes("confirm"), false);
});
Deno.test("QA dispatch readback outage preserves exact ID for reconciliation", async () => {
  const f = fixture();
  const original = f.deps.readRun;
  f.deps.readRun = () => {
    return Promise.resolve().then(() => {
      throw new Error("private transport detail");
    });
  };
  assertEquals(
    (await executeQaDispatch(request, f.deps, "execute")).status,
    "pending",
  );
  assertEquals(f.record()?.runId, 23);
  f.deps.readRun = original;
  f.events.length = 0;
  assertEquals(
    (await executeQaDispatch(request, f.deps, "reconcile")).status,
    "assigned",
  );
  assertEquals(f.events.includes("POST"), false);
});
Deno.test("QA reconcile without existing intent has no network or mutation", async () => {
  const f = fixture();
  assertEquals(
    (await executeQaDispatch(request, f.deps, "reconcile")).status,
    "pending",
  );
  assertEquals(f.events, ["lock"]);
});
Deno.test("QA dispatch preflight refusal creates no intent or POST", async () => {
  const f = fixture();
  f.deps.preflight = () => Promise.resolve().then(() => (null));
  assertEquals(
    (await executeQaDispatch(request, f.deps, "execute")).status,
    "mismatch",
  );
  assertEquals(f.events.includes("intent"), false);
  assertEquals(f.events.includes("POST"), false);
});
Deno.test("QA failed intent confirmation keeps dispatched attempt and retries only confirmation", async () => {
  const f = fixture();
  let confirms = 0;
  f.deps.finish = () => {
    confirms++;
    return confirms === 1
      ? Promise.reject(new Error("fenced"))
      : Promise.resolve();
  };
  assertEquals(
    (await executeQaDispatch(request, f.deps, "execute")).status,
    "pending",
  );
  assertEquals(f.record()?.phase, "dispatched");
  assertEquals(f.record()?.runAttempt, 1);
  assertEquals(
    (await executeQaDispatch(request, f.deps, "execute")).status,
    "assigned",
  );
  assertEquals(confirms, 2);
  assertEquals(f.events.filter((event) => event === "POST").length, 1);
});
Deno.test("QA another agent may observe but never impersonate pending holder", async () => {
  const f = fixture();
  let confirms = 0;
  f.deps.finish = () => {
    confirms++;
    return Promise.reject(new Error("fenced"));
  };
  await executeQaDispatch(request, f.deps, "execute");
  assertEquals(
    (await executeQaDispatch({ ...request, agent: "peer" }, f.deps, "execute"))
      .status,
    "pending",
  );
  assertEquals(confirms, 1);
  assertEquals(f.record()?.intent.agent, "lead");
  assertEquals(f.events.filter((event) => event === "POST").length, 1);
});
Deno.test("QA current run rerun cannot silently replace the durably observed attempt", async () => {
  const f = fixture();
  let confirms = 0;
  f.deps.finish = () => {
    confirms++;
    return Promise.reject(new Error("fenced"));
  };
  await executeQaDispatch(request, f.deps, "execute");
  f.deps.readRun = () =>
    Promise.resolve({
      id: 23,
      workflow_id: 99,
      run_attempt: 2,
      head_sha: request.qaHead,
      event: "workflow_dispatch",
      path: ".github/workflows/validate-final-package-qa.yml",
      repository: { full_name: "Floorp-Projects/Floorp" },
      head_repository: { full_name: "Floorp-Projects/Floorp" },
      status: "queued",
      conclusion: null,
    });
  assertEquals(
    (await executeQaDispatch(request, f.deps, "reconcile")).status,
    "mismatch",
  );
  assertEquals(f.record()?.runAttempt, 1);
  assertEquals(confirms, 1);
  assertEquals(f.events.filter((event) => event === "POST").length, 1);
});
Deno.test("QA inconsistent durable phase/run identity is rejected before readback or confirmation", async () => {
  const plan = await planQaDispatch(request);
  const base: QaDispatchRecord = {
    schemaVersion: 1,
    key: plan.key,
    request,
    workflowId: 99,
    intent: { jobId: "intent-owned", epoch: 1, agent: "lead" },
    phase: "intent",
    runId: null,
    runAttempt: null,
  };
  for (
    const invalid of [
      { phase: "intent", runId: 23, runAttempt: 1 },
      { phase: "unknown", runId: 23, runAttempt: 1 },
      { phase: "dispatched", runId: null, runAttempt: null },
      { phase: "mismatch", runId: null, runAttempt: null },
    ] as const
  ) {
    const f = fixture();
    f.deps.load = () => Promise.resolve({ ...base, ...invalid });
    assertEquals(
      (await executeQaDispatch(request, f.deps, "execute")).status,
      "unknown",
    );
    assertEquals(
      f.events.some((event) =>
        event === "POST" || event.startsWith("GET:") || event === "confirm"
      ),
      false,
    );
  }
});
Deno.test("QA accepted exact run ID survives a local save error in the returned outcome", async () => {
  const f = fixture(), save = f.deps.save;
  f.deps.save = (record) =>
    record.phase === "dispatched"
      ? Promise.reject(new Error("SENSITIVE-LOCAL-IO"))
      : save(record);
  const result = await executeQaDispatch(request, f.deps, "execute");
  assertEquals(result.status, "unknown");
  assertEquals(result.runId, 23);
  assertEquals(f.record()?.phase, "intent");
  assertEquals(f.record()?.runId, null);
  assertEquals(
    (await executeQaDispatch(request, f.deps, "execute")).status,
    "pending",
  );
  assertEquals(f.events.filter((event) => event === "POST").length, 1);
  assertEquals(
    f.events.some((event) => event === "GET:23" || event === "confirm"),
    false,
  );
});
