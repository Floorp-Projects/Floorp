/// <reference lib="deno.ns" />
// SPDX-License-Identifier: MPL-2.0
import { bindDocumentLifetime } from "../../bridge/startup/src/document-lifetime.ts";
function assert(value: unknown) {
  if (!value) throw Error("Assertion failed");
}
function setup(
  readyState = "loading",
  subscribeThrows = false,
  unsubscribeThrows = false,
) {
  const win = new EventTarget();
  const doc = Object.assign(new EventTarget(), {
    readyState,
    defaultView: win,
  });
  let subscriptions = 0,
    removals = 0,
    initializations = 0,
    disconnects = 0,
    errors = 0;
  const dispose = bindDocumentLifetime(doc, () => {
    initializations++;
  }, () => {
    if (subscribeThrows) throw Error("registration");
    subscriptions++;
    return () => {
      removals++;
      if (unsubscribeThrows) throw Error("removal");
    };
  }, () => {
    disconnects++;
  }, () => {
    errors++;
  });
  return {
    doc,
    win,
    dispose,
    counts: () => ({
      subscriptions,
      removals,
      initializations,
      disconnects,
      errors,
    }),
  };
}
Deno.test("unload removes strong subscription and disconnects mutation observer once", () => {
  const h = setup("complete");
  assert(h.counts().initializations === 1);
  h.win.dispatchEvent(new Event("unload"));
  h.dispose();
  assert(
    h.counts().subscriptions === 1 && h.counts().removals === 1 &&
      h.counts().disconnects === 1,
  );
});
Deno.test("unload before DOMContentLoaded cancels pending initialization", () => {
  const h = setup();
  h.win.dispatchEvent(new Event("unload"));
  h.doc.dispatchEvent(new Event("DOMContentLoaded"));
  assert(
    h.counts().initializations === 0 && h.counts().removals === 1 &&
      h.counts().disconnects === 1,
  );
});
Deno.test("DOMContentLoaded initializes once and cleanup works", () => {
  const h = setup();
  h.doc.dispatchEvent(new Event("DOMContentLoaded"));
  h.doc.dispatchEvent(new Event("DOMContentLoaded"));
  assert(h.counts().initializations === 1);
  h.win.dispatchEvent(new Event("unload"));
  assert(h.counts().removals === 1 && h.counts().disconnects === 1);
});
Deno.test("failed observer registration still cleans up DOM observer", () => {
  const h = setup("complete", true);
  h.dispose();
  assert(
    h.counts().errors === 1 && h.counts().removals === 0 &&
      h.counts().disconnects === 1,
  );
});
Deno.test("failed removal is reported and cannot prevent mutation disconnect", () => {
  const h = setup("complete", false, true);
  h.dispose();
  assert(h.counts().errors === 1 && h.counts().disconnects === 1);
});
Deno.test("document without a window is not initialized or globally subscribed", () => {
  const doc = Object.assign(new EventTarget(), {
    readyState: "complete",
    defaultView: null,
  });
  let calls = 0;
  bindDocumentLifetime(
    doc,
    () => calls++,
    () => {
      calls++;
      return () => {};
    },
    () => {},
    () => {},
  );
  assert(calls === 0);
});

Deno.test("i18n subscription removes the exact callback and resets binding state", () => {
  const win = new EventTarget();
  const doc = Object.assign(new EventTarget(), {
    readyState: "complete",
    defaultView: win,
  });
  const observers = new Set<() => void>();
  let bound = false, translated = 0;
  const callback = () => {
    translated++;
  };
  bindDocumentLifetime(doc, () => {}, () => {
    observers.add(callback);
    bound = true;
    return () => {
      assert(observers.delete(callback));
    };
  }, () => {
    bound = false;
  }, () => {
    throw Error("unexpected failure");
  });
  for (const observer of observers) observer();
  assert(translated === 1 && bound);
  win.dispatchEvent(new Event("unload"));
  for (const observer of observers) observer();
  assert(translated === 1 && !bound && observers.size === 0);
});

Deno.test("i18next cleanup removes only this document's two event callbacks", () => {
  const win = new EventTarget();
  const doc = Object.assign(new EventTarget(), {
    readyState: "complete",
    defaultView: win,
  });
  const listeners = new Map<string, Set<() => void>>([[
    "initialized",
    new Set(),
  ], ["languageChanged", new Set()]]);
  let ownUpdates = 0, otherUpdates = 0;
  const ours = () => ownUpdates++;
  const other = () => otherUpdates++;
  listeners.get("languageChanged")!.add(other);
  bindDocumentLifetime(
    doc,
    () => {},
    () => {
      listeners.get("initialized")!.add(ours);
      listeners.get("languageChanged")!.add(ours);
      return () => {
        listeners.get("initialized")!.delete(ours);
        listeners.get("languageChanged")!.delete(ours);
      };
    },
    () => {},
    () => {},
  );
  for (const callback of listeners.get("languageChanged")!) callback();
  assert(ownUpdates === 1 && otherUpdates === 1);
  win.dispatchEvent(new Event("unload"));
  for (const callback of listeners.get("languageChanged")!) callback();
  assert(
    ownUpdates === 1 && otherUpdates === 2 &&
      listeners.get("initialized")!.size === 0,
  );
});

Deno.test("locale subscription preserves immediate callback and live updates until unload", () => {
  const win = new EventTarget();
  const doc = Object.assign(new EventTarget(), {
    readyState: "complete",
    defaultView: win,
  });
  const listeners = new Set<() => void>();
  let updates = 0;
  const callback = () => updates++;
  bindDocumentLifetime(
    doc,
    () => {},
    () => {
      listeners.add(callback);
      callback();
      return () => {
        assert(listeners.delete(callback));
      };
    },
    () => {},
    () => {},
  );
  assert(updates === 1);
  for (const listener of listeners) listener();
  assert(updates === 2);
  win.dispatchEvent(new Event("unload"));
  for (const listener of listeners) listener();
  assert(updates === 2 && listeners.size === 0);
});
