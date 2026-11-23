import {
  admitM3cPackage,
  highWaterAfterM3cCommitFailure,
  M3cIdentityCatalog,
  M3cJournalError,
  mergeM3cHighWater,
  serializeM3cSignedJournal,
  validateM3cSignedJournal,
} from "../../browser-features/modules/modules/feature-host/M3cSignedJournal.ts";
import type { M3cPackageMetadata } from "../../browser-features/modules/modules/feature-host/M3cSignedJournal.types.ts";
import {
  recordM3cCleanupEvidence,
} from "../../browser-features/modules/modules/feature-host/M3cCleanupEvidence.ts";
import {
  classifyM3cProviderFailure,
} from "../../browser-features/modules/modules/feature-host/M3cProviderTaxonomy.ts";
import {
  M3cSerialQueue,
} from "../../browser-features/modules/modules/feature-host/M3cSerialQueue.ts";

const featureId = "m3c-test";
const firstHash = "1".repeat(64);
const secondHash = "2".repeat(64);
const thirdHash = "3".repeat(64);
const sourceHash = "a".repeat(64);

function assert(condition: unknown, message: string): asserts condition {
  if (!condition) throw new Error(message);
}

function assertThrows(action: () => unknown, code: string): void {
  try {
    action();
  } catch (error) {
    assert(error instanceof M3cJournalError, "expected journal error");
    assert(error.code === code, "unexpected journal error code");
    return;
  }
  throw new Error("expected operation to throw");
}

async function assertRejects(promise: Promise<unknown>): Promise<void> {
  try {
    await promise;
  } catch {
    return;
  }
  throw new Error("expected promise rejection");
}

async function assertRejectsWithMessage(
  promise: Promise<unknown>,
  message: string,
): Promise<void> {
  try {
    await promise;
  } catch (error) {
    assert(error instanceof Error, "expected Error rejection");
    assert(error.message === message, "unexpected rejection category");
    return;
  }
  throw new Error("expected promise rejection");
}

function metadata(
  packageSha256: string,
  releaseSequence: number,
): M3cPackageMetadata {
  return {
    packageSha256,
    sourceSha256: sourceHash,
    version: "same-label",
    releaseSequence,
  };
}

function emptyLifecycle() {
  return {
    schemaVersion: 2,
    featureId,
    desiredEnabled: false,
    selectedVersion: null,
    selectedDigest: null,
    selectedHealth: "unverified",
    selectedHealthBuild: null,
    rollbackVersion: null,
    rollbackDigest: null,
    rollbackHealthBuild: null,
    stateRevision: 0,
    pending: null,
    attempt: null,
    blockedReason: null,
    recentOutcomes: [],
    lastOutcome: null,
  };
}

function signedJournal(
  packages: readonly M3cPackageMetadata[],
  highWater: { releaseSequence: number; packageSha256: string } | null,
  lifecycle: unknown = emptyLifecycle(),
) {
  return validateM3cSignedJournal({
    schemaVersion: 1,
    featureId,
    highWater,
    packages,
    lifecycle,
  }, featureId);
}

Deno.test("M3c high-water admits only monotonic exact retries", () => {
  const first = metadata(firstHash, 1);
  const second = metadata(secondHash, 2);
  assert(admitM3cPackage(null, first).releaseSequence === 1, "first admission");
  const floor = admitM3cPackage(
    { releaseSequence: 1, packageSha256: firstHash },
    second,
  );
  assert(floor.releaseSequence === 2, "higher admission");
  assert(
    admitM3cPackage(floor, second).packageSha256 === secondHash,
    "exact retry",
  );
  assert(
    mergeM3cHighWater(floor, { releaseSequence: 1, packageSha256: firstHash })
      ?.releaseSequence === 2,
    "rollback preserves floor",
  );
  assert(
    highWaterAfterM3cCommitFailure(floor, {
      releaseSequence: 3,
      packageSha256: thirdHash,
    }, false)?.releaseSequence === 2,
    "known pre-replace failure preserves old floor",
  );
  assert(
    highWaterAfterM3cCommitFailure(floor, {
      releaseSequence: 3,
      packageSha256: thirdHash,
    }, true)?.releaseSequence === 3,
    "uncertain replacement conservatively retains candidate floor",
  );
  assertThrows(
    () => admitM3cPackage(floor, metadata(thirdHash, 1)),
    "sequence-too-low",
  );
  assertThrows(
    () => admitM3cPackage(floor, metadata(thirdHash, 2)),
    "sequence-equivocation",
  );
});

Deno.test("M3c journal preserves identity distinct from labels and source hashes", () => {
  const first = metadata(firstHash, 1);
  const second = metadata(secondHash, 2);
  const journal = signedJournal(
    [first, second],
    { releaseSequence: 2, packageSha256: secondHash },
  );
  assert(journal.packages.length === 2, "both envelopes remain distinct");
  assert(
    journal.packages[0].version === journal.packages[1].version,
    "label reuse",
  );
  assert(
    journal.packages[0].sourceSha256 === journal.packages[1].sourceSha256,
    "source reuse",
  );
  assert(
    journal.packages[0].packageSha256 !== journal.packages[1].packageSha256,
    "package identity differs",
  );
});

Deno.test("M3c identity catalog is bounded and indexes package identity", () => {
  const catalog = new M3cIdentityCatalog(featureId);
  for (let index = 1; index <= 32; index += 1) {
    const hash = index.toString(16).padStart(64, "0");
    catalog.add(metadata(hash, index));
    assert(catalog.hasVersion(hash), "package hash is not a version token");
    assert(catalog.digestForVersion(hash) === hash, "identity digest changed");
  }
  try {
    catalog.add(metadata("f".repeat(64), 33));
  } catch (error) {
    assert(error instanceof M3cJournalError, "expected catalog error");
    assert(error.code === "catalog-full", "catalog overflow code changed");
    return;
  }
  throw new Error("catalog accepted more than 32 packages");
});

Deno.test("M3c journal rejects unresolved, over-floor and malformed references", () => {
  const selected: Record<string, unknown> = {
    ...emptyLifecycle(),
    selectedVersion: firstHash,
    selectedDigest: firstHash,
  };
  assertThrows(
    () =>
      signedJournal(
        [metadata(firstHash, 1)],
        { releaseSequence: 1, packageSha256: firstHash },
        { ...selected, selectedVersion: thirdHash, selectedDigest: thirdHash },
      ),
    "invalid-journal",
  );
  assertThrows(
    () =>
      signedJournal(
        [metadata(firstHash, 1), metadata(secondHash, 2)],
        { releaseSequence: 1, packageSha256: firstHash },
        {
          ...selected,
          selectedVersion: secondHash,
          selectedDigest: secondHash,
        },
      ),
    "invalid-journal",
  );
  assertThrows(
    () =>
      validateM3cSignedJournal({
        schemaVersion: 1,
        featureId,
        highWater: null,
        packages: [],
        lifecycle: emptyLifecycle(),
        unexpected: true,
      }, featureId),
    "invalid-journal",
  );
});

Deno.test("M3c journal rejects equal-sequence lifecycle equivocation", () => {
  const previous = metadata(firstHash, 1);
  const current = metadata(secondHash, 2);
  const staged = metadata(thirdHash, 2);
  const highWater = { releaseSequence: 2, packageSha256: secondHash };
  const stagedJournal = signedJournal([current, staged], highWater);
  assert(stagedJournal.packages.length === 2, "inert staged metadata retained");

  const historical = signedJournal(
    [previous, current],
    highWater,
    {
      ...emptyLifecycle(),
      selectedVersion: firstHash,
      selectedDigest: firstHash,
    },
  );
  assert(
    historical.lifecycle !== null,
    "lower-sequence historical reference retained",
  );
  assertThrows(
    () =>
      signedJournal(
        [current, staged],
        highWater,
        {
          ...emptyLifecycle(),
          selectedVersion: thirdHash,
          selectedDigest: thirdHash,
        },
      ),
    "invalid-journal",
  );
});

Deno.test("M3c journal serialization is bounded", () => {
  const lifecycle = { ...emptyLifecycle(), extra: "x".repeat(256 * 1024) };
  const journal = signedJournal([], null, lifecycle);
  assertThrows(
    () => serializeM3cSignedJournal(journal),
    "journal-too-large",
  );
});

Deno.test("M3c provider taxonomy blocks infrastructure without candidate fallback", () => {
  for (
    const code of [
      "cache-missing",
      "cache-corrupt",
      "invalid-signature",
      "feature-mismatch",
      "host-incompatible",
      "runtime-incompatible",
      "capability-denied",
      "source-digest-mismatch",
      "size-limit",
    ]
  ) {
    assert(
      classifyM3cProviderFailure(code).kind === "candidate-unavailable",
      code + " must be a candidate failure",
    );
  }
  const infrastructure = [
    ["cache-storage", "cache-io"],
    ["cache-unsafe-layout", "unsafe-layout"],
    ["cache-closed", "cache-contract"],
    ["invalid-policy", "cache-contract"],
    ["unsupported-crypto", "unsupported-crypto"],
    ["surprise", "unexpected"],
  ] as const;
  for (const [code, reason] of infrastructure) {
    const result = classifyM3cProviderFailure(code);
    assert(
      result.kind === "infrastructure" && result.reason === reason,
      code + " must block as " + reason,
    );
  }
});

Deno.test("M3c cleanup evidence is bounded and monotonic", () => {
  const evidence: string[] = [];
  recordM3cCleanupEvidence(evidence, "runtime-stop-failed");
  recordM3cCleanupEvidence(evidence, "runtime-stop-failed");
  assert(evidence.length === 1, "duplicate cleanup evidence");
  for (let index = 0; index < 32; index += 1) {
    recordM3cCleanupEvidence(evidence, "failure-" + index);
  }
  assert(evidence.slice().length === 16, "cleanup evidence is bounded");
  assert(evidence.includes("runtime-stop-failed"), "cleanup failure is sticky");
});

Deno.test("M3c FIFO reserves before nested preparation and preserves order", async () => {
  const queue = new M3cSerialQueue(16);
  const order: string[] = [];
  let nested: Promise<string> | null = null;
  const outer = queue.schedule(() => {
    nested = queue.schedule(() => () =>
      Promise.resolve().then(() => {
        order.push("nested");
        return "nested";
      })
    );
    return () =>
      Promise.resolve().then(() => {
        order.push("outer");
        return "outer";
      });
  });
  await outer;
  assert(nested !== null, "nested call was not scheduled");
  await nested;
  assert(order.join(",") === "outer,nested", "FIFO order changed");
});

Deno.test("M3c FIFO accepts 16 operations and rejects the 17th before preparation", async () => {
  const queue = new M3cSerialQueue();
  let release!: () => void;
  const gate = new Promise<void>((resolve) => release = resolve);
  let started!: () => void;
  const firstStarted = new Promise<void>((resolve) => started = resolve);
  let preparations = 0;
  const pending: Promise<number>[] = [];
  for (let index = 0; index < 16; index += 1) {
    pending.push(queue.schedule(() => {
      preparations += 1;
      return async () => {
        if (index === 0) started();
        if (index === 0) await gate;
        return index;
      };
    }));
  }
  await firstStarted;
  assert(preparations === 16, "accepted preparations were not run");
  const seventeenth = queue.schedule(() => {
    preparations += 1;
    return () => Promise.resolve(16);
  });
  await assertRejectsWithMessage(seventeenth, "busy");
  assert(preparations === 16, "seventeenth preparation was inspected");
  release();
  const values = await Promise.all(pending);
  assert(values.length === 16, "accepted operations were lost");
  await queue.close(() => Promise.resolve());
  assert(queue.pendingCount === 0, "accepted operation slots did not drain");
});

Deno.test(
  "M3c FIFO retains failed preparations in accepted slots until predecessors drain",
  async () => {
    const queue = new M3cSerialQueue();
    let release!: () => void;
    const gate = new Promise<void>((resolve) => release = resolve);
    let started!: () => void;
    const firstStarted = new Promise<void>((resolve) => started = resolve);
    const predecessor = queue.schedule(() => async () => {
      started();
      await gate;
      return "head";
    });
    await firstStarted;

    let preparations = 0;
    const expected = Array.from(
      { length: 15 },
      (_, index) => new Error("saturated preparation " + index),
    );
    const failed = expected.map((error) =>
      queue.schedule(() => {
        preparations += 1;
        throw error;
      })
    );
    assert(preparations === 15, "accepted preparations were not inspected");
    assert(
      queue.pendingCount === 16,
      "failed preparations did not retain slots",
    );

    let overflowPreparationInspected = false;
    const overflow = queue.schedule(() => {
      overflowPreparationInspected = true;
      return () => Promise.resolve("overflow");
    });
    await assertRejectsWithMessage(overflow, "busy");
    assert(
      !overflowPreparationInspected,
      "17th preparation was inspected before rejection",
    );
    assert(
      queue.pendingCount === 16,
      "rejected 17th call changed accepted operation count",
    );

    release();
    const settled = await Promise.allSettled([predecessor, ...failed]);
    assert(settled[0]?.status === "fulfilled", "gated predecessor failed");
    for (let index = 0; index < expected.length; index += 1) {
      const result = settled[index + 1];
      assert(result?.status === "rejected", "failed preparation fulfilled");
      assert(
        result.reason === expected[index],
        "queued preparation rejection changed",
      );
    }
    assert(
      Number(queue.pendingCount) === 0,
      "failed preparation slots did not drain",
    );
    await queue.close(() => Promise.resolve());
  },
);

Deno.test("M3c FIFO runs failed preparation and preserves its rejection", async () => {
  const queue = new M3cSerialQueue();
  const expected = new Error("preparation failure");
  let preparations = 0;
  const failed = queue.schedule(() => {
    preparations += 1;
    throw expected;
  });
  let rejected = false;
  try {
    await failed;
  } catch (error) {
    assert(error === expected, "preparation error was changed");
    assert(
      error instanceof Error && error.message === "preparation failure",
      "unexpected preparation rejection category",
    );
    rejected = true;
  }
  assert(rejected, "throwing preparation fulfilled");
  assert(preparations === 1, "throwing preparation did not execute");
  await queue.close(() => Promise.resolve());
  assert(queue.pendingCount === 0, "failed preparation slot did not drain");
});

Deno.test("M3c FIFO nested close drains accepted preparation work", async () => {
  const queue = new M3cSerialQueue();
  let release!: () => void;
  const gate = new Promise<void>((resolve) => release = resolve);
  let closeCalls = 0;
  let closePromise: Promise<void> | null = null;
  const active = queue.schedule(() => {
    closePromise = queue.close(() => {
      closeCalls += 1;
      return Promise.resolve();
    });
    return async () => {
      await gate;
      return "done";
    };
  });
  assert(closePromise !== null, "nested close did not start");
  const denied = queue.schedule(() => {
    throw new Error("closed queue inspected preparation");
  });
  await assertRejectsWithMessage(denied, "facade-closed");
  assert(closeCalls === 0, "close ran before accepted work completed");
  release();
  await active;
  await closePromise;
  assert(Number(closeCalls) === 1, "nested close did not run exactly once");
});

Deno.test("M3c FIFO close stops admission synchronously and shares drain", async () => {
  const queue = new M3cSerialQueue(2);
  let release!: () => void;
  const gate = new Promise<void>((resolve) => release = resolve);
  let closed = 0;
  const active = queue.schedule(() => async () => await gate);
  const closeOne = queue.close(() =>
    Promise.resolve().then(() => {
      closed += 1;
    })
  );
  const closeTwo = queue.close(() =>
    Promise.resolve().then(() => {
      closed += 1;
    })
  );
  assert(closeOne === closeTwo, "close promise was not shared");
  const late = queue.schedule(() => {
    throw new Error("must not inspect after close");
  });
  await assertRejects(late);
  release();
  await Promise.all([active, closeOne]);
  assert(closed === 1, "close action did not run exactly once");
});
