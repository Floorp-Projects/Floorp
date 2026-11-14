// SPDX-License-Identifier: MPL-2.0
import type { VerifiedWorkspaceIdentity } from "./fdh_workspace_oidc_types.ts";
import type {
  WorkspaceRefreshFlight,
  WorkspaceSessionErrorCode,
  WorkspaceSessionLogoutResult,
  WorkspaceSessionMaterial,
  WorkspaceSessionMaterialSnapshot,
  WorkspaceSessionOptions,
  WorkspaceSessionRecord,
  WorkspaceSessionSnapshot,
} from "./fdh_workspace_sessions_types.ts";

/** The only hosted domain accepted on a workspace identity. */
export const WORKSPACE_SESSION_HOSTED_DOMAIN = "floorp.app";
/** The only email domain accepted on a workspace identity. */
export const WORKSPACE_SESSION_EMAIL_PATTERN = /^[^@\s]+@floorp\.app$/i;
/** Opaque printable identity string, length 1 to 255. */
export const WORKSPACE_SESSION_IDENTITY_PATTERN = /^[\x20-\x7E]{1,255}$/;
/** Opaque session id string, length 1 to 256. */
export const WORKSPACE_SESSION_ID_PATTERN = /^[\x20-\x7E]{1,256}$/;
/** Token strings must not carry a line break of any kind. */
export const WORKSPACE_SESSION_TOKEN_BREAK_PATTERN = /[\r\n]/;
/** Google ID-token issuers accepted verbatim for a workspace identity. */
export const WORKSPACE_SESSION_ISSUERS = Object.freeze(
  [
    "https://accounts.google.com",
    "accounts.google.com",
  ] as const,
);

/** Constant messages. No untrusted value is ever interpolated into them. */
export const WORKSPACE_SESSION_ERROR_MESSAGES = Object.freeze(
  {
    "invalid-options": "Workspace session options rejected",
    "invalid-material": "Workspace session material rejected",
    "invalid-session-id": "Workspace session id rejected",
    "invalid-clock": "Workspace session clock rejected",
    "capacity-exceeded": "Workspace session capacity exceeded",
    "session-not-found": "Workspace session not found",
    "refresh-failed": "Workspace session refresh failed",
  } as const satisfies Record<WorkspaceSessionErrorCode, string>,
);

/** Session failure that carries only a constant code and message. */
export class WorkspaceSessionError extends Error {
  readonly code: WorkspaceSessionErrorCode;

  constructor(code: WorkspaceSessionErrorCode) {
    super(WORKSPACE_SESSION_ERROR_MESSAGES[code]);
    this.name = "WorkspaceSessionError";
    this.code = code;
  }
}

/**
 * Process-local, bounded, private session registry. It owns opaque provider
 * tokens and sanitized snapshots only; it is not a persistence layer, central
 * vault, authorization server, MCP grant, or cross-process registry.
 *
 * The provider expiry from each material controls activity; no local session
 * TTL is invented. Expired records stay in the private map so their refresh
 * material survives, while `get` reports them inactive. Refreshes and logouts
 * are the only lifecycle transitions.
 */
export class WorkspaceSessions {
  readonly #now: () => number;
  readonly #maxSessions: number;
  readonly #refresh: WorkspaceSessionOptions["refresh"];
  readonly #revoke: WorkspaceSessionOptions["revoke"];
  readonly #sessions = new Map<string, WorkspaceSessionRecord>();
  readonly #inflight = new Map<string, WorkspaceRefreshFlight>();

  constructor(options: WorkspaceSessionOptions) {
    const validated = validateOptions(options);
    this.#now = validated.now;
    this.#maxSessions = validated.maxSessions;
    this.#refresh = validated.refresh;
    this.#revoke = validated.revoke;
  }

  /** Create one bounded session from validated provider material. */
  create(material: WorkspaceSessionMaterial): WorkspaceSessionSnapshot {
    let copy: WorkspaceSessionMaterialSnapshot;
    try {
      copy = snapshotMaterial(material);
    } catch {
      throw new WorkspaceSessionError("invalid-material");
    }
    const now = readNow(this.#now);
    if (copy.expiresAt <= now) {
      throw new WorkspaceSessionError("invalid-material");
    }
    if (heldSessionCount(this.#sessions, this.#inflight) >= this.#maxSessions) {
      throw new WorkspaceSessionError("capacity-exceeded");
    }
    const id = crypto.randomUUID();
    const generation = 1;
    const snapshot = createSnapshot(
      id,
      copy.identity,
      copy.expiresAt,
      generation,
    );
    const record: WorkspaceSessionRecord = Object.freeze({
      identity: copy.identity,
      accessToken: copy.accessToken,
      refreshToken: copy.refreshToken,
      expiresAt: copy.expiresAt,
      snapshot,
      generation,
    });
    this.#sessions.set(id, record);
    return snapshot;
  }

  /** Active snapshot only; an expired record stays refreshable in private state. */
  get(id: string): WorkspaceSessionSnapshot | undefined {
    const sessionId = assertSessionId(id);
    const record = this.#sessions.get(sessionId);
    if (record === undefined) {
      return undefined;
    }
    const now = readNow(this.#now);
    if (record.expiresAt <= now) {
      return undefined;
    }
    return record.snapshot;
  }

  /**
   * Refresh one session with single-flight provider work. The provider receives
   * the frozen identity captured before the await so it can verify the fresh ID
   * token subject without this registry duplicating token-to-identity state.
   */
  async refresh(id: string): Promise<WorkspaceSessionSnapshot> {
    const sessionId = assertSessionId(id);
    const record = this.#sessions.get(sessionId);
    if (record === undefined) {
      throw new WorkspaceSessionError("session-not-found");
    }
    const existing = this.#inflight.get(sessionId);
    if (existing !== undefined) {
      return await existing.promise;
    }
    const old: WorkspaceSessionMaterialSnapshot = Object.freeze({
      identity: record.identity,
      accessToken: record.accessToken,
      refreshToken: record.refreshToken,
      expiresAt: record.expiresAt,
    });
    const generation = record.generation;
    let settle: (snapshot: WorkspaceSessionSnapshot) => void = () => {};
    let fail: (reason: unknown) => void = () => {};
    const promise = new Promise<WorkspaceSessionSnapshot>((resolve, reject) => {
      settle = resolve;
      fail = reject;
    });
    const flight: WorkspaceRefreshFlight = Object.freeze({
      promise,
      generation,
    });
    this.#inflight.set(sessionId, flight);
    void this.#runRefresh(sessionId, record, old, flight, settle, fail);
    return await promise;
  }

  /**
   * Remove the local record before any provider await, then revoke the old
   * refresh token (or access token when absent). A refresh already in flight
   * makes the provider revocation unconfirmable; a late refresh result is
   * discarded and its returned credential is revoked on a best-effort basis.
   */
  async logout(id: string): Promise<WorkspaceSessionLogoutResult> {
    const sessionId = assertSessionId(id);
    const record = this.#sessions.get(sessionId);
    if (record === undefined) {
      return logoutResult(false);
    }
    const refreshWasInFlight = this.#inflight.get(sessionId) !== undefined;
    const token = record.refreshToken ?? record.accessToken;
    this.#sessions.delete(sessionId);
    let confirmed = false;
    if (this.#revoke !== undefined) {
      try {
        await this.#revoke(token);
        confirmed = !refreshWasInFlight;
      } catch {
        confirmed = false;
      }
    }
    return logoutResult(confirmed);
  }

  async #runRefresh(
    sessionId: string,
    record: WorkspaceSessionRecord,
    old: WorkspaceSessionMaterialSnapshot,
    flight: WorkspaceRefreshFlight,
    settle: (snapshot: WorkspaceSessionSnapshot) => void,
    fail: (reason: unknown) => void,
  ): Promise<void> {
    let fresh: WorkspaceSessionMaterialSnapshot | undefined;
    try {
      if (old.refreshToken === undefined) {
        throw new WorkspaceSessionError("refresh-failed");
      }
      let material: WorkspaceSessionMaterial;
      try {
        material = await this.#refresh(old.refreshToken, old.identity);
      } catch {
        throw new WorkspaceSessionError("refresh-failed");
      }
      try {
        fresh = snapshotMaterial(material);
      } catch {
        throw new WorkspaceSessionError("refresh-failed");
      }
      const now = readNow(this.#now);
      if (fresh.expiresAt <= now) {
        throw new WorkspaceSessionError("refresh-failed");
      }
      assertRefreshIdentityBinding(old.identity, fresh.identity);
      if (
        this.#sessions.get(sessionId) !== record ||
        record.generation !== flight.generation
      ) {
        throw new WorkspaceSessionError("session-not-found");
      }
      const generation = flight.generation + 1;
      const snapshot = createSnapshot(
        sessionId,
        fresh.identity,
        fresh.expiresAt,
        generation,
      );
      const nextRecord: WorkspaceSessionRecord = Object.freeze({
        identity: fresh.identity,
        accessToken: fresh.accessToken,
        refreshToken: fresh.refreshToken ?? old.refreshToken,
        expiresAt: fresh.expiresAt,
        snapshot,
        generation,
      });
      this.#sessions.set(sessionId, nextRecord);
      settle(snapshot);
    } catch (error) {
      if (this.#sessions.get(sessionId) === record) {
        this.#sessions.delete(sessionId);
      }
      if (fresh !== undefined) {
        await this.#bestEffortRevoke(
          fresh.refreshToken ?? fresh.accessToken,
        );
      }
      fail(new WorkspaceSessionError(normalizeErrorCode(error)));
    } finally {
      if (this.#inflight.get(sessionId) === flight) {
        this.#inflight.delete(sessionId);
      }
    }
  }

  async #bestEffortRevoke(token: string): Promise<void> {
    if (this.#revoke === undefined) {
      return;
    }
    try {
      await this.#revoke(token);
    } catch {
      return;
    }
  }
}

function validateOptions(options: unknown): WorkspaceSessionOptions {
  try {
    if (typeof options !== "object" || options === null) {
      throw new WorkspaceSessionError("invalid-options");
    }
    const record = options as unknown as Record<string, unknown>;
    const nowValue = record.now;
    if (typeof nowValue !== "function") {
      throw new WorkspaceSessionError("invalid-options");
    }
    const maxSessions = record.maxSessions;
    if (
      typeof maxSessions !== "number" ||
      !Number.isSafeInteger(maxSessions) ||
      maxSessions <= 0
    ) {
      throw new WorkspaceSessionError("invalid-options");
    }
    const refreshValue = record.refresh;
    if (typeof refreshValue !== "function") {
      throw new WorkspaceSessionError("invalid-options");
    }
    const revokeValue = record.revoke;
    if (revokeValue !== undefined && typeof revokeValue !== "function") {
      throw new WorkspaceSessionError("invalid-options");
    }
    return Object.freeze({
      now: nowValue as unknown as WorkspaceSessionOptions["now"],
      maxSessions,
      refresh: refreshValue as unknown as WorkspaceSessionOptions["refresh"],
      revoke: revokeValue as unknown as WorkspaceSessionOptions["revoke"],
    });
  } catch {
    throw new WorkspaceSessionError("invalid-options");
  }
}

function readNow(clock: () => number): number {
  try {
    const value: unknown = clock();
    if (
      typeof value !== "number" ||
      !Number.isSafeInteger(value) ||
      value < 0
    ) {
      throw new WorkspaceSessionError("invalid-clock");
    }
    return value;
  } catch {
    throw new WorkspaceSessionError("invalid-clock");
  }
}

function assertSessionId(value: unknown): string {
  if (
    typeof value !== "string" ||
    !WORKSPACE_SESSION_ID_PATTERN.test(value)
  ) {
    throw new WorkspaceSessionError("invalid-session-id");
  }
  return value;
}

function readOwnValue(record: object, field: string): unknown {
  try {
    const descriptor = Object.getOwnPropertyDescriptor(record, field);
    if (descriptor === undefined || !("value" in descriptor)) {
      throw new WorkspaceSessionError("invalid-material");
    }
    return descriptor.value;
  } catch {
    throw new WorkspaceSessionError("invalid-material");
  }
}

function readOptionalOwnValue(record: object, field: string): unknown {
  try {
    const descriptor = Object.getOwnPropertyDescriptor(record, field);
    if (descriptor === undefined) {
      return undefined;
    }
    if (!("value" in descriptor)) {
      throw new WorkspaceSessionError("invalid-material");
    }
    return descriptor.value;
  } catch {
    throw new WorkspaceSessionError("invalid-material");
  }
}

function readRequiredString(record: object, field: string): string {
  const value = readOwnValue(record, field);
  if (typeof value !== "string") {
    throw new WorkspaceSessionError("invalid-material");
  }
  return value;
}

function assertToken(value: unknown): string {
  if (
    typeof value !== "string" ||
    value.length === 0 ||
    WORKSPACE_SESSION_TOKEN_BREAK_PATTERN.test(value)
  ) {
    throw new WorkspaceSessionError("invalid-material");
  }
  return value;
}

function safeInteger(value: unknown): number {
  if (typeof value !== "number" || !Number.isSafeInteger(value)) {
    throw new WorkspaceSessionError("invalid-material");
  }
  return value;
}

function copyAudience(raw: unknown): string | readonly string[] {
  try {
    if (typeof raw === "string") {
      if (raw.length === 0) {
        throw new WorkspaceSessionError("invalid-material");
      }
      return raw;
    }
    if (!Array.isArray(raw)) {
      throw new WorkspaceSessionError("invalid-material");
    }
    const lengthDescriptor = Object.getOwnPropertyDescriptor(raw, "length");
    if (lengthDescriptor === undefined || !("value" in lengthDescriptor)) {
      throw new WorkspaceSessionError("invalid-material");
    }
    const length: unknown = lengthDescriptor.value;
    if (
      typeof length !== "number" ||
      !Number.isSafeInteger(length) ||
      length <= 0
    ) {
      throw new WorkspaceSessionError("invalid-material");
    }
    const values: string[] = [];
    for (let index = 0; index < length; index += 1) {
      const descriptor = Object.getOwnPropertyDescriptor(raw, String(index));
      if (descriptor === undefined || !("value" in descriptor)) {
        throw new WorkspaceSessionError("invalid-material");
      }
      const element: unknown = descriptor.value;
      if (typeof element !== "string" || element.length === 0) {
        throw new WorkspaceSessionError("invalid-material");
      }
      values.push(element);
    }
    return Object.freeze(values);
  } catch {
    throw new WorkspaceSessionError("invalid-material");
  }
}

function copyIdentity(source: unknown): VerifiedWorkspaceIdentity {
  if (typeof source !== "object" || source === null) {
    throw new WorkspaceSessionError("invalid-material");
  }
  const scope = readRequiredString(source, "scope");
  if (scope !== "workspace-id-token-verification-only") {
    throw new WorkspaceSessionError("invalid-material");
  }
  if (readOwnValue(source, "liveOAuthVerified") !== false) {
    throw new WorkspaceSessionError("invalid-material");
  }
  if (readOwnValue(source, "mcpAuthorizationGranted") !== false) {
    throw new WorkspaceSessionError("invalid-material");
  }
  const identityKey = readRequiredString(source, "identityKey");
  if (!WORKSPACE_SESSION_IDENTITY_PATTERN.test(identityKey)) {
    throw new WorkspaceSessionError("invalid-material");
  }
  const email = readRequiredString(source, "email");
  if (!WORKSPACE_SESSION_EMAIL_PATTERN.test(email)) {
    throw new WorkspaceSessionError("invalid-material");
  }
  const hostedDomain = readRequiredString(source, "hostedDomain");
  if (hostedDomain !== WORKSPACE_SESSION_HOSTED_DOMAIN) {
    throw new WorkspaceSessionError("invalid-material");
  }
  const issuer = readRequiredString(source, "issuer");
  if (!WORKSPACE_SESSION_ISSUERS.some((allowed) => allowed === issuer)) {
    throw new WorkspaceSessionError("invalid-material");
  }
  const audience = copyAudience(readOwnValue(source, "audience"));
  const issuedAt = safeInteger(readOwnValue(source, "issuedAt"));
  const identityExpiresAt = safeInteger(readOwnValue(source, "expiresAt"));
  if (identityExpiresAt <= issuedAt) {
    throw new WorkspaceSessionError("invalid-material");
  }
  const authorizedPartyRaw = readOptionalOwnValue(source, "authorizedParty");
  let authorizedParty: string | undefined;
  if (authorizedPartyRaw !== undefined) {
    if (
      typeof authorizedPartyRaw !== "string" ||
      authorizedPartyRaw.length === 0
    ) {
      throw new WorkspaceSessionError("invalid-material");
    }
    authorizedParty = authorizedPartyRaw;
  }
  const notBeforeRaw = readOptionalOwnValue(source, "notBefore");
  let notBefore: number | undefined;
  if (notBeforeRaw !== undefined) {
    notBefore = safeInteger(notBeforeRaw);
  }
  return Object.freeze({
    scope: "workspace-id-token-verification-only",
    liveOAuthVerified: false,
    mcpAuthorizationGranted: false,
    identityKey,
    email,
    hostedDomain,
    issuer,
    audience,
    authorizedParty,
    issuedAt,
    expiresAt: identityExpiresAt,
    notBefore,
  });
}

function snapshotMaterial(
  material: unknown,
): WorkspaceSessionMaterialSnapshot {
  if (typeof material !== "object" || material === null) {
    throw new WorkspaceSessionError("invalid-material");
  }
  const identity = copyIdentity(readOwnValue(material, "identity"));
  const accessToken = assertToken(readOwnValue(material, "accessToken"));
  const refreshRaw = readOptionalOwnValue(material, "refreshToken");
  let refreshToken: string | undefined;
  if (refreshRaw !== undefined) {
    refreshToken = assertToken(refreshRaw);
  }
  const expiresAt = safeInteger(readOwnValue(material, "expiresAt"));
  return Object.freeze({
    identity,
    accessToken,
    refreshToken,
    expiresAt,
  });
}

function normalizeErrorCode(error: unknown): WorkspaceSessionErrorCode {
  if (error instanceof WorkspaceSessionError) {
    const code: unknown = error.code;
    if (
      typeof code === "string" &&
      Object.hasOwn(WORKSPACE_SESSION_ERROR_MESSAGES, code)
    ) {
      return code as WorkspaceSessionErrorCode;
    }
  }
  return "refresh-failed";
}

function heldSessionCount(
  sessions: Map<string, WorkspaceSessionRecord>,
  inflight: Map<string, WorkspaceRefreshFlight>,
): number {
  const ids = new Set<string>();
  for (const id of sessions.keys()) {
    ids.add(id);
  }
  for (const id of inflight.keys()) {
    ids.add(id);
  }
  return ids.size;
}

function createSnapshot(
  id: string,
  identity: VerifiedWorkspaceIdentity,
  expiresAt: number,
  version: number,
): WorkspaceSessionSnapshot {
  return Object.freeze({
    id,
    identity,
    expiresAt,
    version,
    scope: "workspace-oauth-session",
    liveOAuthVerified: false,
    mcpAuthorizationGranted: false,
  });
}

function sameAudience(
  old: string | readonly string[],
  fresh: string | readonly string[],
): boolean {
  if (typeof old === "string") {
    return typeof fresh === "string" && old === fresh;
  }
  if (typeof fresh === "string") {
    return false;
  }
  if (old.length !== fresh.length) {
    return false;
  }
  for (let index = 0; index < old.length; index += 1) {
    if (old[index] !== fresh[index]) {
      return false;
    }
  }
  return true;
}

function assertRefreshIdentityBinding(
  old: VerifiedWorkspaceIdentity,
  fresh: VerifiedWorkspaceIdentity,
): void {
  if (fresh.identityKey !== old.identityKey) {
    throw new WorkspaceSessionError("refresh-failed");
  }
  if (fresh.email !== old.email) {
    throw new WorkspaceSessionError("refresh-failed");
  }
  if (fresh.hostedDomain !== old.hostedDomain) {
    throw new WorkspaceSessionError("refresh-failed");
  }
  if (fresh.issuer !== old.issuer) {
    throw new WorkspaceSessionError("refresh-failed");
  }
  if (!sameAudience(old.audience, fresh.audience)) {
    throw new WorkspaceSessionError("refresh-failed");
  }
}

function logoutResult(
  providerRevocationConfirmed: boolean,
): WorkspaceSessionLogoutResult {
  return Object.freeze({ localLogout: true, providerRevocationConfirmed });
}
