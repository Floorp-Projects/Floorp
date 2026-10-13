// SPDX-License-Identifier: MPL-2.0

import { CASES, verifyBrowserResult } from "./cases.ts";
import { parseRequest, validateProduct } from "./validate.ts";
import { SinglePassMarionette } from "./marionette.ts";
import type { BrowserResult, PilotRequest } from "./types.ts";
import {
  assert as observedAssert,
  assertEquals as observedEquals,
  beginAssertions,
  endAssertions,
  firstCaseException,
  recordCaseException,
} from "./observed_harness.ts";

function assert(
  condition: unknown,
  message = "Host contract assertion failed",
): asserts condition {
  if (!condition) throw new Error(message);
}

async function rejects(
  operation: () => unknown | Promise<unknown>,
): Promise<void> {
  let rejected = false;
  try {
    await operation();
  } catch {
    rejected = true;
  }
  assert(rejected, "Required rejection was not observed");
}

const nonce = "12345678-1234-1234-1234-123456789abc";

Deno.test("generated observer calls original assertions and retains recovered failure", async () => {
  const events = beginAssertions();
  try {
    observedAssert(true, "first positive");
    await rejects(() => observedEquals(1, 2, "original failure"));
    observedAssert(true, "later positive");
    assert(
      events.length === 3 && events[0].passed && !events[1].passed &&
        events[2].passed,
    );
  } finally {
    endAssertions();
  }
});

Deno.test("precleanup exception observation preserves the first original error", () => {
  beginAssertions();
  try {
    recordCaseException(new Error("original body"));
    recordCaseException(new Error("later cleanup"));
    assert(firstCaseException() === "Error: original body");
  } finally {
    endAssertions();
  }
});
const identity = {
  appBuildID: "20261013142916",
  platformBuildID: "20261013142916",
  version: "157.0.1",
};
function passing(): BrowserResult {
  return {
    schema: "qa3-final-workspace-browser-v1",
    nonce,
    bootstrap: {
      ...identity,
      coreResource: "chrome://noraneko/content/core.js",
      resolvedCoreURL:
        "jar:file:///input/browser/browser/omni.ja!/chrome/noraneko/content/core.js",
      startupMode: "production",
      httpLoaderAllowed: false,
      workspaceAPI: true,
    },
    cases: CASES.map((test) => ({
      id: test.id,
      name: test.name,
      status: "PASS",
      assertions: Array.from(
        { length: test.minimumAssertions },
        () => ({ passed: true, message: "observed fixture assertion" }),
      ),
      originalError: null,
      error: null,
    })),
  };
}

function request(): PilotRequest {
  return {
    schema: "qa3-final-workspace-request-v1",
    controllerHead: "a".repeat(40),
    executionRun: 1,
    executionAttempt: 1,
    floorpHead: "b".repeat(40),
    bundleRun: 2,
    bundleAttempt: 1,
    bundleArtifact: {
      id: 3,
      name: "fixture-final-bundle",
      digest: "sha256:" + "c".repeat(64),
    },
    runtimeHead: "d".repeat(40),
    runtimeRun: 4,
    runtimeAttempt: 1,
    target: "linux-x86_64",
  };
}

Deno.test("host contract accepts only the exact six required source identities", () => {
  assert(
    CASES.length === 6 && new Set(CASES.map((test) => test.id)).size === 6,
  );
  verifyBrowserResult(passing(), nonce, identity);
});

const mutations: [string, (result: BrowserResult) => void][] = [
  ["missing case", (r) => {
    r.cases.pop();
  }],
  ["duplicate case", (r) => {
    r.cases[1] = structuredClone(r.cases[0]);
  }],
  ["foreign case", (r) => {
    r.cases[0].id = "other";
  }],
  ["changed order", (r) => {
    r.cases.reverse();
  }],
  ["unexecuted case", (r) => {
    r.cases[0].status = "NOT_RUN";
  }],
  ["hidden recovered first assertion", (r) => {
    r.cases[0].assertions[0].passed = false;
  }],
  ["hidden recovered first exception", (r) => {
    r.cases[0].originalError = "first failure";
  }],
  ["cleanup exception", (r) => {
    r.cases[0].error = "cleanup failure";
  }],
  ["empty assertions", (r) => {
    r.cases[0].assertions = [];
  }],
  ["below required assertion count", (r) => {
    r.cases[0].assertions.pop();
  }],
  ["foreign attempt nonce", (r) => {
    r.nonce = "different";
  }],
  ["development bootstrap", (r) => {
    r.bootstrap.startupMode = "dev";
  }],
  ["HTTP feature loader", (r) => {
    r.bootstrap.httpLoaderAllowed = true;
  }],
  ["HTTP core resource", (r) => {
    r.bootstrap.resolvedCoreURL = "http://localhost:5181/core.js";
  }],
  ["foreign package core", (r) => {
    r.bootstrap.resolvedCoreURL = "file:///other/core.js";
  }],
  ["missing workspace API", (r) => {
    r.bootstrap.workspaceAPI = false;
  }],
  ["different compiled application", (r) => {
    r.bootstrap.appBuildID = "20251013142916";
  }],
  ["different native Runtime", (r) => {
    r.bootstrap.platformBuildID = "20251013142916";
  }],
  ["different Firefox version", (r) => {
    r.bootstrap.version = "158.0.0";
  }],
  ["undeclared retry metadata", (r) => {
    Object.assign(r.cases[0], { retries: 1 });
  }],
  ["undeclared skip metadata", (r) => {
    Object.assign(r.cases[0], { skips: 1 });
  }],
];
for (const [name, mutate] of mutations) {
  Deno.test(`host contract rejects ${name}`, async () => {
    const result = passing();
    mutate(result);
    await rejects(() => verifyBrowserResult(result, nonce, identity));
  });
}

Deno.test("host request preserves separate product/controller/run/attempt identities", () => {
  const r = request();
  const accepted = parseRequest(r);
  assert(
    accepted.floorpHead !== accepted.controllerHead &&
      accepted.bundleRun !== accepted.executionRun,
  );
});
for (const key of ["executionAttempt", "bundleAttempt", "runtimeAttempt"]) {
  Deno.test(`host request rejects later ${key}`, async () => {
    await rejects(() => parseRequest({ ...request(), [key]: 2 }));
  });
}
for (
  const extra of [
    { target: "linux-aarch64" },
    { runtimeHead: "abbreviated" },
    { executionRun: 0 },
    { bundleArtifact: { id: 0, name: "x", digest: "missing" } },
    { recoveredFirstFailure: true },
  ]
) {
  Deno.test(`host request rejects invalid provenance ${JSON.stringify(extra)}`, async () => {
    await rejects(() => parseRequest({ ...request(), ...extra }));
  });
}

Deno.test("bundle memory bound is checked before parsing large release members", async () => {
  const temp = await Deno.makeTempDir();
  try {
    const file = await Deno.open(`${temp}/oversized`, {
      createNew: true,
      write: true,
    });
    await file.truncate(1024 ** 3 + 1);
    file.close();
    await rejects(() => validateProduct(temp, request()));
  } finally {
    await Deno.remove(temp, { recursive: true });
  }
});

async function writePacket(
  connection: Deno.TcpConn,
  value: unknown,
): Promise<void> {
  const bytes = new TextEncoder().encode(JSON.stringify(value));
  const prefix = new TextEncoder().encode(`${bytes.length}:`);
  const packet = new Uint8Array(prefix.length + bytes.length);
  packet.set(prefix);
  packet.set(bytes, prefix.length);
  for (let offset = 0; offset < packet.length;) {
    offset += await connection.write(packet.subarray(offset, offset + 7));
  }
}

async function readPacket(connection: Deno.TcpConn): Promise<unknown> {
  const one = new Uint8Array(1);
  let prefix = "";
  while (true) {
    assert(await connection.read(one) !== null);
    if (one[0] === 58) break;
    prefix += String.fromCharCode(one[0]);
  }
  const bytes = new Uint8Array(Number(prefix));
  let count = 0;
  while (count < bytes.length) {
    const read = await connection.read(bytes.subarray(count));
    assert(read !== null);
    count += read;
  }
  return JSON.parse(new TextDecoder().decode(bytes));
}

async function withPeer(
  peer: (connection: Deno.TcpConn) => Promise<void>,
  test: (port: number) => Promise<void>,
): Promise<void> {
  const listener = Deno.listen({ hostname: "127.0.0.1", port: 0 });
  const serving = (async () => {
    const connection = await listener.accept();
    try {
      await peer(connection);
    } finally {
      connection.close();
    }
  })();
  try {
    await test((listener.addr as Deno.NetAddr).port);
    await serving;
  } finally {
    listener.close();
  }
}

async function session(connection: Deno.TcpConn): Promise<void> {
  await writePacket(connection, { marionetteProtocol: 3 });
  const sent = await readPacket(connection) as unknown[];
  assert(sent[2] === "WebDriver:NewSession");
  await writePacket(connection, [1, sent[1], null, {
    sessionId: "host-fixture-session",
  }]);
}

Deno.test("single-pass transport handles UTF-8 and partial packets with original transcript", async () => {
  await withPeer(async (connection) => {
    await session(connection);
    const sent = await readPacket(connection) as unknown[];
    assert((sent[3] as Record<string, unknown>).text === "日本語");
    await writePacket(connection, [1, sent[1], null, { value: "日本語" }]);
  }, async (port) => {
    const transcript: unknown[] = [];
    const client = await SinglePassMarionette.connect(
      port,
      (event) => transcript.push(event),
    );
    try {
      const response = await client.command("HostFixture", {
        text: "日本語",
      }) as { value: string };
      assert(response.value === "日本語");
      assert(transcript.length === 5);
    } finally {
      client.close();
    }
  });
});

Deno.test("single-pass transport rejects wrong message identity", async () => {
  await withPeer(async (connection) => {
    await session(connection);
    await readPacket(connection);
    await writePacket(connection, [1, 999, null, { value: "fake success" }]);
  }, async (port) => {
    const client = await SinglePassMarionette.connect(port);
    try {
      await rejects(() => client.command("HostFixture", {}));
    } finally {
      client.close();
    }
  });
});

Deno.test("single-pass handshake rejection does not retry a session", async () => {
  await withPeer(async (connection) => {
    await writePacket(connection, { marionetteProtocol: 2 });
    assert(await connection.read(new Uint8Array(1)) === null);
  }, async (port) => {
    await rejects(() => SinglePassMarionette.connect(port));
  });
});

Deno.test("single-pass timeout closes the original socket without leaked reads", async () => {
  await withPeer(async (connection) => {
    await session(connection);
    await readPacket(connection);
    assert(await connection.read(new Uint8Array(1)) === null);
  }, async (port) => {
    const client = await SinglePassMarionette.connect(port);
    try {
      await rejects(() => client.command("HostFixture", {}, 20));
    } finally {
      client.close();
    }
  });
});

Deno.test("single-pass packet bound rejects oversized peer frames", async () => {
  await withPeer(async (connection) => {
    await connection.write(new TextEncoder().encode("9999999:"));
    assert(await connection.read(new Uint8Array(1)) === null);
  }, async (port) => {
    await rejects(() => SinglePassMarionette.connect(port));
  });
});
