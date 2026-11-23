// SPDX-License-Identifier: MPL-2.0
import { assert, assertEquals } from "@std/assert";
import type { VerifiedWorkspaceIdentity } from "./fdh_workspace_oidc_types.ts";
import {
  WORKSPACE_SESSION_ERROR_MESSAGES,
  WorkspaceSessionError,
  WorkspaceSessions,
} from "./fdh_workspace_sessions.ts";
import type {
  WorkspaceSessionErrorCode,
  WorkspaceSessionMaterial,
  WorkspaceSessionOptions,
} from "./fdh_workspace_sessions_types.ts";

// Deterministic epoch-millisecond clock; no machine clock and no local TTL.
const NOW_MS = 1_800_000_000_000;
const NOW_SECONDS = NOW_MS / 1000;
const FUTURE_MS = 60_000;
const EMAIL = "dev@floorp.app";
const SUB = "118401337000000000001";
const CLIENT_ID = "701234567890-abc123def456.apps.google.com";
const OTHER_CLIENT_ID = "999999999999-other.apps.google.com";
const ISSUER = "https://accounts.google.com";
const HOSTED_DOMAIN = "floorp.app";
const ACCESS_TOKEN = "opaque-access-token-1";
const REFRESH_TOKEN = "opaque-refresh-token-1";
const UUID_PATTERN =
  /^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/;

let clock = NOW_MS;

function resetClock(value: number = NOW_MS): void {
  clock = value;
}

function identity(
  overrides: Partial<VerifiedWorkspaceIdentity> = {},
): VerifiedWorkspaceIdentity {
  return Object.freeze({
    scope: "workspace-id-token-verification-only",
    liveOAuthVerified: false,
    mcpAuthorizationGranted: false,
    identityKey: SUB,
    email: EMAIL,
    hostedDomain: HOSTED_DOMAIN,
    issuer: ISSUER,
    audience: CLIENT_ID,
    issuedAt: NOW_SECONDS - 60,
    expiresAt: NOW_SECONDS + 300,
    ...overrides,
  });
}

function mutableIdentity() {
  return {
    scope: "workspace-id-token-verification-only" as const,
    liveOAuthVerified: false as const,
    mcpAuthorizationGranted: false as const,
    identityKey: SUB,
    email: EMAIL,
    hostedDomain: HOSTED_DOMAIN,
    issuer: ISSUER,
    audience: [CLIENT_ID, OTHER_CLIENT_ID],
    issuedAt: NOW_SECONDS - 60,
    expiresAt: NOW_SECONDS + 300,
  };
}

function material(
  overrides: Partial<WorkspaceSessionMaterial> = {},
): WorkspaceSessionMaterial {
  return {
    identity: identity(),
    accessToken: ACCESS_TOKEN,
    refreshToken: REFRESH_TOKEN,
    expiresAt: NOW_MS + FUTURE_MS,
    ...overrides,
  };
}

function makeSessions(
  overrides: Partial<WorkspaceSessionOptions> = {},
): WorkspaceSessions {
  return new WorkspaceSessions({
    now: () => clock,
    maxSessions: 10,
    refresh: () => Promise.resolve(material()),
    revoke: () => Promise.resolve(),
    ...overrides,
  });
}

async function attempt(
  operation: () => unknown,
): Promise<WorkspaceSessionErrorCode | "fulfilled"> {
  try {
    await operation();
    return "fulfilled";
  } catch (error) {
    if (error instanceof WorkspaceSessionError) {
      return error.code;
    }
    throw error;
  }
}

async function captureError(
  operation: () => unknown,
): Promise<WorkspaceSessionError> {
  try {
    await operation();
  } catch (error) {
    if (error instanceof WorkspaceSessionError) {
      return error;
    }
    throw error;
  }
  throw new Error("expected WorkspaceSessionError");
}

function deferred(): { promise: Promise<boolean>; release: () => void } {
  let release = () => {};
  const promise = new Promise<boolean>((resolve) => {
    release = () => resolve(true);
  });
  return { promise, release };
}

Deno.test("create returns sanitized opaque snapshots with no provider tokens", () => {
  resetClock();
  const sessions = makeSessions();
  const snapshot = sessions.create(material());
  assertEquals(snapshot.scope, "workspace-oauth-session");
  assertEquals(snapshot.liveOAuthVerified, false);
  assertEquals(snapshot.mcpAuthorizationGranted, false);
  assertEquals(snapshot.version, 1);
  assertEquals(snapshot.expiresAt, NOW_MS + FUTURE_MS);
  assertEquals(snapshot.identity.identityKey, SUB);
  assertEquals(snapshot.identity.email, EMAIL);
  assert(UUID_PATTERN.test(snapshot.id), "opaque CSPRNG session id");
  const second = sessions.create(material());
  assert(snapshot.id !== second.id, "session ids are distinct");
  const json = JSON.stringify(snapshot);
  for (const secret of [ACCESS_TOKEN, REFRESH_TOKEN]) {
    assert(!json.includes(secret), "snapshot JSON carries no provider token");
  }
  assert(!json.includes("accessToken"), "snapshot has no access token field");
  assert(!json.includes("refreshToken"), "snapshot has no refresh token field");
});

Deno.test("material and snapshot mutation cannot change stored private state", () => {
  resetClock();
  const mutableIdentityValue = mutableIdentity();
  const mutableMaterial = {
    identity: mutableIdentityValue,
    accessToken: ACCESS_TOKEN,
    refreshToken: REFRESH_TOKEN,
    expiresAt: NOW_MS + FUTURE_MS,
  };
  const sessions = makeSessions();
  const snapshot = sessions.create(mutableMaterial);
  mutableMaterial.accessToken = "mutated-access-token";
  mutableMaterial.refreshToken = "mutated-refresh-token";
  mutableMaterial.expiresAt = NOW_MS + 999_999;
  mutableIdentityValue.email = "mutated@floorp.app";
  mutableIdentityValue.audience.push("mutated-audience");
  const stored = sessions.get(snapshot.id);
  assertEquals(stored?.identity.email, EMAIL);
  assertEquals(stored?.identity.audience, [CLIENT_ID, OTHER_CLIENT_ID]);
  assertEquals(stored?.expiresAt, NOW_MS + FUTURE_MS);
  const json = JSON.stringify(stored);
  assert(!json.includes("mutated-access-token"));
  assert(!json.includes("mutated-refresh-token"));
  assert(Object.isFrozen(snapshot), "snapshot is frozen");
  assert(Object.isFrozen(snapshot.identity), "snapshot identity is frozen");
  assert(Object.isFrozen(snapshot.identity.audience), "audience is frozen");
  let snapshotMutationCaught = false;
  try {
    (snapshot as unknown as { expiresAt: number }).expiresAt = 0;
  } catch {
    snapshotMutationCaught = true;
  }
  assert(snapshotMutationCaught, "frozen snapshot rejects mutation");
  assertEquals(sessions.get(snapshot.id)?.expiresAt, NOW_MS + FUTURE_MS);
});

Deno.test("expired get is inactive while refresh material survives", async () => {
  resetClock();
  let refreshCalls = 0;
  let seenIdentity: VerifiedWorkspaceIdentity | undefined;
  const sessions = makeSessions({
    refresh: (token, identityArg) => {
      refreshCalls += 1;
      assertEquals(token, REFRESH_TOKEN);
      seenIdentity = identityArg;
      return Promise.resolve(material({ expiresAt: clock + FUTURE_MS }));
    },
  });
  const created = sessions.create(material({ expiresAt: NOW_MS + 1_000 }));
  clock = NOW_MS + 1_000;
  assertEquals(sessions.get(created.id), undefined);
  clock = NOW_MS + 2_000;
  const refreshed = await sessions.refresh(created.id);
  assertEquals(refreshCalls, 1);
  assertEquals(refreshed.version, 2);
  assertEquals(refreshed.expiresAt, NOW_MS + 2_000 + FUTURE_MS);
  assertEquals(seenIdentity?.identityKey, SUB);
  assertEquals(sessions.get(created.id)?.version, 2);
});

Deno.test("concurrent callers share exactly one provider refresh", async () => {
  resetClock();
  const gate = deferred();
  let calls = 0;
  const sessions = makeSessions({
    refresh: async (token, identityArg) => {
      calls += 1;
      assertEquals(token, REFRESH_TOKEN);
      assertEquals(identityArg.identityKey, SUB);
      await gate.promise;
      return material({
        refreshToken: "rotated-refresh-token",
        expiresAt: clock + FUTURE_MS,
      });
    },
  });
  const created = sessions.create(material());
  const first = sessions.refresh(created.id);
  const second = sessions.refresh(created.id);
  gate.release();
  const [firstSnapshot, secondSnapshot] = await Promise.all([first, second]);
  assertEquals(calls, 1);
  assertEquals(firstSnapshot.version, 2);
  assertEquals(secondSnapshot.expiresAt, firstSnapshot.expiresAt);
  assertEquals(sessions.get(created.id)?.version, 2);
});

Deno.test("concurrent callers share one provider rejection and invalidate", async () => {
  resetClock();
  const gate = deferred();
  let calls = 0;
  const sessions = makeSessions({
    refresh: async () => {
      calls += 1;
      await gate.promise;
      throw new Error("provider secret " + REFRESH_TOKEN);
    },
  });
  const created = sessions.create(material());
  const first = sessions.refresh(created.id);
  const second = sessions.refresh(created.id);
  gate.release();
  const [firstError, secondError] = await Promise.all([
    captureError(() => first),
    captureError(() => second),
  ]);
  assertEquals(calls, 1);
  assertEquals(firstError.code, "refresh-failed");
  assertEquals(secondError.code, "refresh-failed");
  assertEquals(sessions.get(created.id), undefined);
});

Deno.test("refresh rotates the provider refresh token", async () => {
  resetClock();
  const revoked: string[] = [];
  const sessions = makeSessions({
    refresh: () =>
      Promise.resolve(material({
        refreshToken: "rotated-refresh-token",
        expiresAt: clock + FUTURE_MS,
      })),
    revoke: (token) => {
      revoked.push(token);
      return Promise.resolve();
    },
  });
  const created = sessions.create(material());
  await sessions.refresh(created.id);
  const result = await sessions.logout(created.id);
  assertEquals(result, {
    localLogout: true,
    providerRevocationConfirmed: true,
  });
  assertEquals(revoked, ["rotated-refresh-token"]);
});

Deno.test("refresh without a returned refresh token preserves the old one", async () => {
  resetClock();
  const revoked: string[] = [];
  const sessions = makeSessions({
    refresh: () =>
      Promise.resolve(
        material({ refreshToken: undefined, expiresAt: clock + FUTURE_MS }),
      ),
    revoke: (token) => {
      revoked.push(token);
      return Promise.resolve();
    },
  });
  const created = sessions.create(material());
  await sessions.refresh(created.id);
  await sessions.logout(created.id);
  assertEquals(revoked, [REFRESH_TOKEN]);
});

Deno.test("provider refresh failure invalidates fail-closed and leaks nothing", async () => {
  resetClock();
  const sessions = makeSessions({
    refresh: () =>
      Promise.reject(
        new Error("provider leaked " + REFRESH_TOKEN),
      ),
  });
  const created = sessions.create(material());
  const error = await captureError(() => sessions.refresh(created.id));
  assertEquals(error.code, "refresh-failed");
  assertEquals(
    error.message,
    WORKSPACE_SESSION_ERROR_MESSAGES["refresh-failed"],
  );
  assert(!error.message.includes(REFRESH_TOKEN), "no token in error");
  assertEquals(error.cause, undefined);
  assertEquals(sessions.get(created.id), undefined);
  assertEquals(
    await attempt(() => sessions.refresh(created.id)),
    "session-not-found",
  );
  const second = sessions.create(material());
  const secondError = await captureError(() => sessions.refresh(second.id));
  assert(error !== secondError, "fresh normalized error object");
  assertEquals(secondError.code, "refresh-failed");
});

Deno.test("refresh rejects identity, audience, email, domain, and issuer mismatches", async () => {
  const mismatches: VerifiedWorkspaceIdentity[] = [
    identity({ identityKey: "other-subject" }),
    identity({ audience: [CLIENT_ID, OTHER_CLIENT_ID] }),
    identity({ email: "other@floorp.app" }),
    identity({ email: "dev@example.com" }),
    identity({ hostedDomain: "example.com" }),
    identity({ issuer: "accounts.google.com" }),
  ];
  for (const freshIdentity of mismatches) {
    resetClock();
    const sessions = makeSessions({
      refresh: () =>
        Promise.resolve(material({
          identity: freshIdentity,
          expiresAt: clock + FUTURE_MS,
        })),
    });
    const created = sessions.create(material());
    assertEquals(
      await attempt(() => sessions.refresh(created.id)),
      "refresh-failed",
    );
    assertEquals(sessions.get(created.id), undefined);
  }
});

Deno.test("refresh accepts an exact deep-equal audience array", async () => {
  resetClock();
  const sessions = makeSessions({
    refresh: () =>
      Promise.resolve(material({
        identity: identity({
          audience: [CLIENT_ID, OTHER_CLIENT_ID],
        }),
        expiresAt: clock + FUTURE_MS,
      })),
  });
  const created = sessions.create(
    material({
      identity: identity({
        audience: [CLIENT_ID, OTHER_CLIENT_ID],
      }),
    }),
  );
  const refreshed = await sessions.refresh(created.id);
  assertEquals(refreshed.version, 2);
  assertEquals(refreshed.identity.audience, [CLIENT_ID, OTHER_CLIENT_ID]);
  assert(Object.isFrozen(refreshed.identity.audience));
});

Deno.test("refresh material expiry must be strictly future", async () => {
  resetClock();
  const sessions = makeSessions({
    refresh: () => Promise.resolve(material({ expiresAt: clock })),
  });
  const created = sessions.create(material());
  assertEquals(
    await attempt(() => sessions.refresh(created.id)),
    "refresh-failed",
  );
  assertEquals(sessions.get(created.id), undefined);
});

Deno.test("logout during an in-flight refresh prevents revival and revokes late credential", async () => {
  resetClock();
  const gate = deferred();
  const providerTokens: string[] = [];
  const revoked: string[] = [];
  const sessions = makeSessions({
    refresh: async (token) => {
      providerTokens.push(token);
      await gate.promise;
      return material({
        accessToken: "late-access-token",
        refreshToken: "late-refresh-token",
        expiresAt: clock + FUTURE_MS,
      });
    },
    revoke: (token) => {
      revoked.push(token);
      return Promise.resolve();
    },
  });
  const created = sessions.create(material());
  const pending = sessions.refresh(created.id);
  assertEquals(providerTokens, [REFRESH_TOKEN]);
  const logoutResult = await sessions.logout(created.id);
  assertEquals(logoutResult, {
    localLogout: true,
    providerRevocationConfirmed: false,
  });
  assertEquals(revoked, [REFRESH_TOKEN]);
  assertEquals(sessions.get(created.id), undefined);
  gate.release();
  assertEquals(await attempt(() => pending), "session-not-found");
  assertEquals(revoked, [REFRESH_TOKEN, "late-refresh-token"]);
  assertEquals(sessions.get(created.id), undefined);
});

Deno.test("logout revokes refresh token when present and access token when absent", async () => {
  resetClock();
  const revoked: string[] = [];
  const sessions = makeSessions({
    revoke: (token) => {
      revoked.push(token);
      return Promise.resolve();
    },
  });
  const withRefresh = sessions.create(material());
  const withoutRefresh = sessions.create(
    material({ refreshToken: undefined }),
  );
  const first = await sessions.logout(withRefresh.id);
  const second = await sessions.logout(withoutRefresh.id);
  assertEquals(first.providerRevocationConfirmed, true);
  assertEquals(second.providerRevocationConfirmed, true);
  assertEquals(revoked, [REFRESH_TOKEN, ACCESS_TOKEN]);
});

Deno.test("logout provider failure keeps constant local logout and double logout is safe", async () => {
  resetClock();
  const sessions = makeSessions({
    revoke: () =>
      Promise.reject(
        new Error("revoke leaked " + REFRESH_TOKEN),
      ),
  });
  const created = sessions.create(material());
  const first = await sessions.logout(created.id);
  assertEquals(first, {
    localLogout: true,
    providerRevocationConfirmed: false,
  });
  assertEquals(sessions.get(created.id), undefined);
  const second = await sessions.logout(created.id);
  assertEquals(second, {
    localLogout: true,
    providerRevocationConfirmed: false,
  });
  assert(first !== second, "logout results are fresh constant objects");
});

Deno.test("capacity is strict and bounded without evicting expired material", async () => {
  resetClock();
  const sessions = makeSessions({ maxSessions: 2 });
  const first = sessions.create(material({ expiresAt: NOW_MS + 1_000 }));
  const second = sessions.create(material());
  clock = NOW_MS + 1_000;
  assertEquals(sessions.get(first.id), undefined);
  assertEquals(
    await attempt(() => sessions.create(material())),
    "capacity-exceeded",
  );
  await sessions.logout(first.id);
  const third = sessions.create(material());
  assert(third.id !== second.id);
});

Deno.test("constructor rejects non-positive capacity and missing or invalid seams", async () => {
  resetClock();
  const invalidOptions: unknown[] = [
    null,
    {
      now: () => clock,
      maxSessions: 0,
      refresh: () => Promise.resolve(material()),
    },
    {
      now: () => clock,
      maxSessions: -1,
      refresh: () => Promise.resolve(material()),
    },
    {
      now: () => clock,
      maxSessions: 1.5,
      refresh: () => Promise.resolve(material()),
    },
    {
      now: () => clock,
      maxSessions: NaN,
      refresh: () => Promise.resolve(material()),
    },
    { maxSessions: 1, refresh: () => Promise.resolve(material()) },
    { now: () => clock, maxSessions: 1 },
    { now: () => clock, maxSessions: 1, refresh: "not-function" },
    {
      now: () => clock,
      maxSessions: 1,
      refresh: () => Promise.resolve(material()),
      revoke: "not-function",
    },
  ];
  for (const options of invalidOptions) {
    assertEquals(
      await attempt(() =>
        new WorkspaceSessions(options as unknown as WorkspaceSessionOptions)
      ),
      "invalid-options",
    );
  }
});

Deno.test("create rejects malformed expiry, tokens, and non-future material", async () => {
  resetClock();
  const sessions = makeSessions();
  const invalidMaterials: unknown[] = [
    null,
    material({ expiresAt: NaN }),
    material({ expiresAt: Infinity }),
    material({ expiresAt: 1.5 }),
    material({ expiresAt: NOW_MS }),
    material({ expiresAt: NOW_MS - 1 }),
    material({ accessToken: "" }),
    material({ accessToken: "line\nbreak" }),
    material({ accessToken: "carriage\rreturn" }),
    material({ accessToken: 42 as unknown as string }),
    material({ refreshToken: "" }),
    material({ refreshToken: "line\nbreak" }),
    material({ refreshToken: 42 as unknown as string }),
    material({ identity: null as unknown as VerifiedWorkspaceIdentity }),
  ];
  for (const invalid of invalidMaterials) {
    assertEquals(
      await attempt(() =>
        sessions.create(invalid as unknown as WorkspaceSessionMaterial)
      ),
      "invalid-material",
    );
  }
});

Deno.test("create rejects identities outside the workspace rules", async () => {
  resetClock();
  const sessions = makeSessions();
  const invalidIdentities: VerifiedWorkspaceIdentity[] = [
    identity({ email: "dev@example.com" }),
    identity({ hostedDomain: "example.com" }),
    identity({ issuer: "https://accounts.example.invalid" }),
    identity({ identityKey: "line\nbreak" }),
    identity({ audience: [] }),
  ];
  for (const invalidIdentity of invalidIdentities) {
    assertEquals(
      await attempt(() =>
        sessions.create(material({ identity: invalidIdentity }))
      ),
      "invalid-material",
    );
  }
});

Deno.test("clock failures are constant and fail refresh closed", async () => {
  resetClock();
  let clockMode = "good";
  let clockValue = NOW_MS;
  const sessions = makeSessions({
    now: () => {
      if (clockMode === "throw") {
        throw new Error("clock leaked " + REFRESH_TOKEN);
      }
      if (clockMode === "nan") {
        return NaN;
      }
      if (clockMode === "fraction") {
        return 1.5;
      }
      return clockValue;
    },
  });
  const created = sessions.create(material());
  clockMode = "throw";
  const getError = await captureError(() => sessions.get(created.id));
  assertEquals(getError.code, "invalid-clock");
  assertEquals(
    getError.message,
    WORKSPACE_SESSION_ERROR_MESSAGES["invalid-clock"],
  );
  assert(!getError.message.includes(REFRESH_TOKEN), "no clock detail");
  assertEquals(getError.cause, undefined);
  const refreshError = await captureError(() => sessions.refresh(created.id));
  assertEquals(refreshError.code, "invalid-clock");
  clockMode = "good";
  assertEquals(sessions.get(created.id), undefined);
  clockMode = "nan";
  assertEquals(
    await attempt(() => sessions.create(material())),
    "invalid-clock",
  );
  clockMode = "fraction";
  assertEquals(
    await attempt(() => sessions.create(material())),
    "invalid-clock",
  );
  clockValue = NOW_MS;
});

Deno.test("clock need not be monotonic but logout never resurrects", async () => {
  resetClock();
  const sessions = makeSessions();
  const created = sessions.create(material({ expiresAt: NOW_MS + 1_000 }));
  clock = NOW_MS + 5_000;
  assertEquals(sessions.get(created.id), undefined);
  clock = NOW_MS;
  assertEquals(sessions.get(created.id)?.version, 1);
  await sessions.logout(created.id);
  clock = NOW_MS - 10_000;
  assertEquals(sessions.get(created.id), undefined);
});

Deno.test("refresh callback receives the frozen original identity", async () => {
  resetClock();
  let seen: VerifiedWorkspaceIdentity | undefined;
  const sessions = makeSessions({
    refresh: (token, identityArg) => {
      assertEquals(token, REFRESH_TOKEN);
      seen = identityArg;
      return Promise.resolve(material({ expiresAt: clock + FUTURE_MS }));
    },
  });
  const created = sessions.create(material());
  await sessions.refresh(created.id);
  assert(seen !== undefined);
  assertEquals(seen.identityKey, SUB);
  assertEquals(seen.email, EMAIL);
  assert(Object.isFrozen(seen), "provider identity argument is frozen");
  let mutationCaught = false;
  try {
    (seen as unknown as { email: string }).email = "mutated@floorp.app";
  } catch {
    mutationCaught = true;
  }
  assert(mutationCaught, "provider cannot mutate the identity snapshot");
  assertEquals(seen.email, EMAIL);
});

Deno.test("one-argument refresh callbacks remain assignable", async () => {
  resetClock();
  const sessions = new WorkspaceSessions({
    now: () => clock,
    maxSessions: 1,
    refresh: (token: string) => {
      assertEquals(token, REFRESH_TOKEN);
      return Promise.resolve(material({ expiresAt: clock + FUTURE_MS }));
    },
  });
  const created = sessions.create(material());
  const refreshed = await sessions.refresh(created.id);
  assertEquals(refreshed.version, 2);
});

Deno.test("provider material mutation after refresh cannot change the snapshot", async () => {
  resetClock();
  const mutableIdentityValue = mutableIdentity();
  const returnedMaterial = {
    identity: mutableIdentityValue,
    accessToken: "fresh-access-token",
    refreshToken: "fresh-refresh-token",
    expiresAt: NOW_MS + FUTURE_MS,
  };
  const sessions = makeSessions({
    refresh: () => Promise.resolve(returnedMaterial),
  });
  const created = sessions.create(
    material({
      identity: identity({
        audience: [CLIENT_ID, OTHER_CLIENT_ID],
      }),
    }),
  );
  await sessions.refresh(created.id);
  mutableIdentityValue.email = "mutated@floorp.app";
  returnedMaterial.accessToken = "mutated-access-token";
  returnedMaterial.refreshToken = "mutated-refresh-token";
  returnedMaterial.expiresAt = NOW_MS + 999_999;
  const stored = sessions.get(created.id);
  assertEquals(stored?.identity.email, EMAIL);
  assertEquals(stored?.expiresAt, NOW_MS + FUTURE_MS);
  const json = JSON.stringify(stored);
  assert(!json.includes("mutated-access-token"));
  assert(!json.includes("mutated-refresh-token"));
});

Deno.test("missing refresh credential fails closed without provider refresh I/O", async () => {
  resetClock();
  let providerCalls = 0;
  const sessions = makeSessions({
    refresh: () => {
      providerCalls += 1;
      return Promise.resolve(material({ expiresAt: clock + FUTURE_MS }));
    },
  });
  const created = sessions.create(material({ refreshToken: undefined }));
  const error = await captureError(() => sessions.refresh(created.id));
  assertEquals(error.code, "refresh-failed");
  assertEquals(
    error.message,
    WORKSPACE_SESSION_ERROR_MESSAGES["refresh-failed"],
  );
  assertEquals(providerCalls, 0);
  assertEquals(sessions.get(created.id), undefined);
  assertEquals(
    await attempt(() => sessions.refresh(created.id)),
    "session-not-found",
  );
});

Deno.test("valid renamed identity rejection revokes the fresh rotated credential", async () => {
  resetClock();
  const revoked: string[] = [];
  const sessions = makeSessions({
    refresh: () =>
      Promise.resolve(
        material({
          identity: identity({ email: "renamed@floorp.app" }),
          refreshToken: "fresh-rotated-token",
          expiresAt: clock + FUTURE_MS,
        }),
      ),
    revoke: (token) => {
      revoked.push(token);
      return Promise.resolve();
    },
  });
  const created = sessions.create(material());
  assertEquals(
    await attempt(() => sessions.refresh(created.id)),
    "refresh-failed",
  );
  assertEquals(sessions.get(created.id), undefined);
  assertEquals(revoked, ["fresh-rotated-token", REFRESH_TOKEN]);
});

Deno.test("valid expired fresh material rejection revokes the fresh credential", async () => {
  resetClock();
  const revoked: string[] = [];
  const sessions = makeSessions({
    refresh: () =>
      Promise.resolve(
        material({
          refreshToken: "fresh-expired-token",
          expiresAt: clock,
        }),
      ),
    revoke: (token) => {
      revoked.push(token);
      return Promise.resolve();
    },
  });
  const created = sessions.create(material());
  assertEquals(
    await attempt(() => sessions.refresh(created.id)),
    "refresh-failed",
  );
  assertEquals(sessions.get(created.id), undefined);
  assertEquals(revoked, ["fresh-expired-token", REFRESH_TOKEN]);
});

Deno.test("post-response clock failure revokes the fresh credential", async () => {
  resetClock();
  let clockMode = "good";
  const revoked: string[] = [];
  const sessions = makeSessions({
    now: () => {
      if (clockMode === "bad") {
        throw new Error("clock trap");
      }
      return clock;
    },
    refresh: () =>
      Promise.resolve(
        material({
          refreshToken: "fresh-clock-token",
          expiresAt: clock + FUTURE_MS,
        }),
      ),
    revoke: (token) => {
      revoked.push(token);
      return Promise.resolve();
    },
  });
  const created = sessions.create(material());
  clockMode = "bad";
  const error = await captureError(() => sessions.refresh(created.id));
  assertEquals(error.code, "invalid-clock");
  assertEquals(sessions.get(created.id), undefined);
  assertEquals(revoked, ["fresh-clock-token", REFRESH_TOKEN]);
});

Deno.test("held logout refresh consumes capacity until the flight settles", async () => {
  resetClock();
  const gate = deferred();
  const sessions = makeSessions({
    maxSessions: 1,
    refresh: async () => {
      await gate.promise;
      return material({ expiresAt: clock + FUTURE_MS });
    },
  });
  const first = sessions.create(material());
  const pending = sessions.refresh(first.id);
  const logout = await sessions.logout(first.id);
  assertEquals(logout.providerRevocationConfirmed, false);
  assertEquals(
    await attempt(() => sessions.create(material())),
    "capacity-exceeded",
  );
  gate.release();
  assertEquals(await attempt(() => pending), "session-not-found");
  const second = sessions.create(material());
  assert(second.id !== first.id);
});

Deno.test("negative clock readings are rejected", async () => {
  resetClock();
  let clockValue = -1;
  const sessions = makeSessions({ now: () => clockValue });
  assertEquals(
    await attempt(() => sessions.create(material())),
    "invalid-clock",
  );
  clockValue = NOW_MS;
  const created = sessions.create(material());
  clockValue = -100;
  assertEquals(
    await attempt(() => sessions.get(created.id)),
    "invalid-clock",
  );
});

Deno.test("exotic audience traps normalize to constant material errors", async () => {
  resetClock();
  const exoticAudience = [CLIENT_ID];
  Object.defineProperty(exoticAudience, 0, {
    get() {
      throw new Error("audience secret " + REFRESH_TOKEN);
    },
    enumerable: true,
    configurable: true,
  });
  const sessions = makeSessions();
  const error = await captureError(() =>
    sessions.create(
      material({ identity: identity({ audience: exoticAudience }) }),
    )
  );
  assertEquals(error.code, "invalid-material");
  assertEquals(
    error.message,
    WORKSPACE_SESSION_ERROR_MESSAGES["invalid-material"],
  );
  assert(!error.message.includes(REFRESH_TOKEN));
});

Deno.test("forged session error codes normalize to a constant refresh failure", async () => {
  resetClock();
  const forged = new WorkspaceSessionError("refresh-failed");
  (forged as unknown as { code: string }).code = "forged-secret-code";
  const sessions = makeSessions({
    refresh: () => Promise.reject(forged),
  });
  const created = sessions.create(material());
  const error = await captureError(() => sessions.refresh(created.id));
  assertEquals(error.code, "refresh-failed");
  assertEquals(
    error.message,
    WORKSPACE_SESSION_ERROR_MESSAGES["refresh-failed"],
  );
  assertEquals(sessions.get(created.id), undefined);
});

Deno.test("private registry state is non-enumerable and exposes no token getters", () => {
  resetClock();
  const sessions = makeSessions();
  sessions.create(material());
  assertEquals(JSON.stringify(sessions), "{}");
  assertEquals(Object.keys(sessions), []);
  const ownNames = Object.getOwnPropertyNames(sessions);
  assert(
    !ownNames.some((name) => /token|refresh|access|session/i.test(name)),
    "private fields are not enumerable token state",
  );
});

Deno.test("runtime source keeps process-local offline boundaries and no token getters", async () => {
  const source = await Deno.readTextFile(
    new URL("./fdh_workspace_sessions.ts", import.meta.url),
  );
  const forbidden = [
    "fetch(",
    "Deno.",
    "console.",
    "localStorage",
    "cause",
    "JSON.stringify",
    "setTimeout",
    "refresh_token",
  ];
  for (const token of forbidden) {
    assert(
      !source.includes(token),
      "forbidden token in runtime source: " + token,
    );
  }
  assert(
    !/\bget\s+(accessToken|refreshToken)\b/.test(source),
    "runtime exposes no provider token getters",
  );
});

Deno.test("types module declares types only", async () => {
  const source = await Deno.readTextFile(
    new URL("./fdh_workspace_sessions_types.ts", import.meta.url),
  );
  assert(source.includes("import type"));
  const runtimeExport = /\bexport\s+(const|let|var|function|class|enum)\b/;
  assert(!runtimeExport.test(source), "runtime export in types module");
});
Deno.test("rejected provider refresh invalidates locally before old-grant revocation settles", async () => {
  resetClock();
  const entered = deferred();
  const cleanup = deferred();
  const revoked: string[] = [];
  const sessions = makeSessions({
    maxSessions: 1,
    refresh: () => Promise.reject(new Error("transient provider failure")),
    revoke: async (token) => {
      revoked.push(token);
      entered.release();
      await cleanup.promise;
    },
  });
  const created = sessions.create(material());
  const pending = attempt(() => sessions.refresh(created.id));
  await entered.promise;
  try {
    assertEquals(sessions.get(created.id), undefined);
    assertEquals(
      (await sessions.logout(created.id)).providerRevocationConfirmed,
      false,
    );
    assertEquals(revoked, [REFRESH_TOKEN]);
    assertEquals(
      await attempt(() => sessions.create(material())),
      "capacity-exceeded",
    );
  } finally {
    cleanup.release();
    await pending;
  }
  assertEquals(await pending, "refresh-failed");
  assert(sessions.create(material()).id !== created.id);
});
Deno.test("failed admission revokes an identical old and fresh credential only once", async () => {
  resetClock();
  const revoked: string[] = [];
  const sessions = makeSessions({
    refresh: () =>
      Promise.resolve(
        material({ identity: identity({ email: "renamed@floorp.app" }) }),
      ),
    revoke: (token) => {
      revoked.push(token);
      return Promise.resolve();
    },
  });
  const created = sessions.create(material());
  assertEquals(
    await attempt(() => sessions.refresh(created.id)),
    "refresh-failed",
  );
  assertEquals(revoked, [REFRESH_TOKEN]);
});
Deno.test("failed old-grant revocation never restores a session or exposes provider errors", async () => {
  resetClock();
  const revoked: string[] = [];
  const sessions = makeSessions({
    refresh: () => Promise.reject(new Error("network " + ACCESS_TOKEN)),
    revoke: (token) => {
      revoked.push(token);
      return Promise.reject(new Error("revoke " + REFRESH_TOKEN));
    },
  });
  const created = sessions.create(material());
  const error = await captureError(() => sessions.refresh(created.id));
  assertEquals(revoked, [REFRESH_TOKEN]);
  assertEquals(sessions.get(created.id), undefined);
  assertEquals(error.code, "refresh-failed");
  assertEquals(
    error.message,
    WORKSPACE_SESSION_ERROR_MESSAGES["refresh-failed"],
  );
  assertEquals(error.cause, undefined);
  assert(!String(error).includes(REFRESH_TOKEN));
});
