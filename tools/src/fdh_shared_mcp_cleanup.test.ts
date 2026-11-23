// SPDX-License-Identifier: MPL-2.0
import { assertEquals, assertRejects } from "@std/assert";
import { cleanupProtocolContainer } from "../integration/fdh_shared_mcp_spike.ts";
import type {
  ProtocolCleanupDependencies,
  ProtocolContainerOwnership,
} from "../integration/fdh_shared_mcp_spike_types.ts";
const identity: ProtocolContainerOwnership = {
  containerID: "a".repeat(64),
  image: "pinned-image",
  name: "owned-name",
  nonce: "owned-nonce",
};
function operations(
  events: string[],
  overrides: Partial<ProtocolCleanupDependencies> = {},
): ProtocolCleanupDependencies {
  return {
    recover: () => {
      events.push("lookup");
      return Promise.resolve(identity);
    },
    assertOwned: () => {
      events.push("verify-id-name-image-nonce");
      return Promise.resolve();
    },
    stop: (id) => {
      events.push("stop:" + id);
      return Promise.resolve();
    },
    confirmAbsent: (id) => {
      events.push("absence:" + id);
      return Promise.resolve(true);
    },
    ...overrides,
  };
}
Deno.test("protocol cleanup verifies exact owner then stops and independently reads absence", async () => {
  const events: string[] = [];
  await cleanupProtocolContainer(identity, operations(events));
  assertEquals(events, [
    "verify-id-name-image-nonce",
    "stop:" + identity.containerID,
    "absence:" + identity.containerID,
  ]);
});
Deno.test("protocol cleanup uncertain launch is recovered and verified before stop", async () => {
  const events: string[] = [];
  await cleanupProtocolContainer(undefined, operations(events));
  assertEquals(events, [
    "lookup",
    "verify-id-name-image-nonce",
    "stop:" + identity.containerID,
    "absence:" + identity.containerID,
  ]);
});
Deno.test("protocol cleanup successful name absence does not stop anything", async () => {
  const events: string[] = [];
  await cleanupProtocolContainer(
    undefined,
    operations(events, { recover: () => Promise.resolve(null) }),
  );
  assertEquals(events, []);
});
Deno.test("protocol cleanup Docker query failure is not absence", async () => {
  const events: string[] = [];
  await assertRejects(() =>
    cleanupProtocolContainer(
      undefined,
      operations(events, {
        recover: () => Promise.reject(Error("query-unavailable")),
      }),
    )
  );
  assertEquals(events, []);
});
Deno.test("protocol cleanup nonce mismatch never stops by name", async () => {
  const events: string[] = [];
  await assertRejects(() =>
    cleanupProtocolContainer(
      identity,
      operations(events, {
        assertOwned: () => Promise.reject(Error("owner-mismatch")),
      }),
    )
  );
  assertEquals(events, []);
});
Deno.test("protocol cleanup uncertain stop is not success", async () => {
  const events: string[] = [];
  await assertRejects(() =>
    cleanupProtocolContainer(
      identity,
      operations(events, {
        stop: () => Promise.reject(Error("stop-unconfirmed")),
      }),
    )
  );
  assertEquals(events, ["verify-id-name-image-nonce"]);
});
Deno.test("protocol cleanup requires post-stop absence not stop acknowledgement", async () => {
  const events: string[] = [];
  await assertRejects(() =>
    cleanupProtocolContainer(
      identity,
      operations(events, { confirmAbsent: () => Promise.resolve(false) }),
    )
  );
  assertEquals(events, [
    "verify-id-name-image-nonce",
    "stop:" + identity.containerID,
  ]);
});
