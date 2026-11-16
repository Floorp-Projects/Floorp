/// <reference lib="deno.ns" />
// SPDX-License-Identifier: MPL-2.0
import {
  captureWebDocument,
  LatestWebObservation,
} from "../../browser-features/chrome/common/pwa/browserObservation.ts";
function assert(v: unknown) {
  if (!v) throw new Error("Assertion failed");
}
function browser(scheme = "https") {
  return {
    isConnected: true,
    currentURI: { scheme, spec: `${scheme}://example.test/` },
    browsingContext: { currentWindowGlobal: { isClosed: false } },
  };
}
function deferred() {
  let resolve!: () => void;
  const promise = new Promise<void>((r) => resolve = r);
  return { resolve, promise };
}
Deno.test("non-web, missing globals and disconnected browsers are skipped", async () => {
  for (
    const b of [
      browser("about"),
      browser("chrome"),
      browser("data"),
      { ...browser(), isConnected: false },
      { ...browser(), browsingContext: null },
      null,
    ]
  ) {
    let calls = 0;
    await new LatestWebObservation().run(b, () => b, () => {
      calls++;
      return Promise.resolve();
    }, () => {
      calls++;
    });
    assert(calls === 0);
    assert(captureWebDocument(b) === null);
  }
});
Deno.test("ordinary HTTPS and loopback document observation completes", async () => {
  for (const b of [browser(), browser("http")]) {
    let updates = 0;
    await new LatestWebObservation().run(b, () => b, async (current) => {
      await Promise.resolve();
      if (current()) updates++;
    }, () => {
      throw Error("unexpected");
    });
    assert(updates === 1);
  }
});
Deno.test("live manifest rejection is reported without an unhandled rejection", async () => {
  const b = browser();
  const failure = new Error("real manifest failure");
  let seen;
  await new LatestWebObservation().run(b, () => b, () => {
    return Promise.reject(failure);
  }, (e) => {
    seen = e;
  });
  assert(seen === failure);
});
Deno.test("close during await cancels rather than reporting", async () => {
  const b = browser(), wait = deferred();
  let reports = 0;
  const task = new LatestWebObservation().run(b, () => b, async () => {
    await wait.promise;
    throw Error("destroyed actor");
  }, () => {
    reports++;
  });
  b.isConnected = false;
  wait.resolve();
  await task;
  assert(reports === 0);
});
Deno.test("navigation to same URL with a new WindowGlobal invalidates result", () => {
  const b = browser(), capture = captureWebDocument(b)!;
  b.browsingContext.currentWindowGlobal = { isClosed: false };
  assert(!capture.current());
});
Deno.test("selection or navigation change prevents a stale UI update", async () => {
  for (const change of ["select", "navigate", "closed"]) {
    const b = browser(), wait = deferred();
    let selected = b, updates = 0;
    const task = new LatestWebObservation().run(
      b,
      () => selected,
      async (current) => {
        await wait.promise;
        if (current()) updates++;
      },
      () => {},
    );
    if (change === "select") selected = browser();
    if (change === "navigate") b.currentURI.spec += "new";
    if (change === "closed") {
      b.browsingContext.currentWindowGlobal.isClosed = true;
    }
    wait.resolve();
    await task;
    assert(updates === 0);
  }
});
Deno.test("newer observation including non-web cancels old work", async () => {
  const b = browser(), wait = deferred(), runner = new LatestWebObservation();
  let updates = 0;
  const old = runner.run(b, () => b, async (current) => {
    await wait.promise;
    if (current()) updates++;
  }, () => {});
  await runner.run(browser("about"), () => b, async () => {}, () => {});
  wait.resolve();
  await old;
  assert(updates === 0);
});
Deno.test("selected-browser teardown getter is safe", async () => {
  const b = browser();
  await new LatestWebObservation().run(b, () => {
    throw Error("window closed");
  }, () => {
    return Promise.reject(Error("must not run"));
  }, () => {
    throw Error("must not report");
  });
});
