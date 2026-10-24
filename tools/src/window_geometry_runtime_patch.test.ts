// SPDX-License-Identifier: MPL-2.0
import { assert, assertEquals, assertRejects } from "@std/assert";
import { runInNewContext } from "node:vm";
import type {
  WindowGeometryCloseHandler,
  WindowGeometryContext,
  WindowGeometryEvent,
  WindowGeometryEventTarget,
  WindowGeometryListener,
  WindowGeometryNativeWindow,
  WindowGeometryRequest,
  WindowGeometrySnapshot,
  WindowGeometryUpdater,
} from "./window_geometry_test_types.ts";

class EventTargetFixture implements WindowGeometryEventTarget {
  listeners = new Map<string, Set<WindowGeometryListener>>();
  addEventListener(type: string, listener: WindowGeometryListener): void {
    if (!this.listeners.has(type)) this.listeners.set(type, new Set());
    this.listeners.get(type)!.add(listener);
  }
  removeEventListener(type: string, listener: WindowGeometryListener): void {
    this.listeners.get(type)?.delete(listener);
  }
  emit(event: WindowGeometryEvent): void {
    for (const listener of this.listeners.get(event.type) ?? []) {
      listener(event);
    }
  }
  get count(): number {
    return [...this.listeners.values()].reduce((sum, set) => sum + set.size, 0);
  }
}

class WindowFixture extends EventTargetFixture
  implements WindowGeometryNativeWindow {
  screenX = 10;
  screenY = 20;
  outerWidth = 1000;
  outerHeight = 800;
  closed = false;
  windowRoot = new EventTargetFixture();
  snapshot(): WindowGeometrySnapshot {
    return {
      left: this.screenX,
      top: this.screenY,
      width: this.outerWidth,
      height: this.outerHeight,
    };
  }
  resize(width: number, height: number, trusted = true): void {
    this.outerWidth = width;
    this.outerHeight = height;
    this.emit({ type: "resize", isTrusted: trusted, target: this });
  }
  move(left: number, top: number, trusted = true): void {
    this.screenX = left;
    this.screenY = top;
    this.windowRoot.emit({
      type: "MozUpdateWindowPos",
      isTrusted: trusted,
      target: this,
    });
  }
}

class ContextFixture implements WindowGeometryContext {
  active = true;
  unloaded = false;
  accessible = true;
  handlers = new Set<WindowGeometryCloseHandler>();
  canAccessWindow(): boolean {
    return this.accessible;
  }
  callOnClose(handler: WindowGeometryCloseHandler): void {
    this.handlers.add(handler);
  }
  forgetOnClose(handler: WindowGeometryCloseHandler): void {
    this.handlers.delete(handler);
  }
  close(): void {
    this.unloaded = true;
    for (const handler of [...this.handlers]) handler.close();
  }
}

const request = (
  values: Partial<WindowGeometrySnapshot>,
): WindowGeometryRequest => ({
  left: null,
  top: null,
  width: null,
  height: null,
  ...values,
});
async function flush(): Promise<void> {
  for (let index = 0; index < 12; index++) await Promise.resolve();
}

async function harness() {
  const patch = await Deno.readTextFile(
    ".github/patches/floorp-runtime/common/chrome-extension-windows.patch",
  );
  const additions = patch.split("\n").filter((line) =>
    line.startsWith("+") && !line.startsWith("+++")
  ).map((line) => line.slice(1)).join("\n");
  const start = additions.indexOf("const chromeWindowGeometryUpdates =");
  const end = additions.indexOf("    onBoundsChanged(", start);
  assert(start >= 0 && end > start);
  const timers = new Map<number, { at: number; callback: () => void }>();
  const closeListeners = new Set<
    (window: WindowGeometryNativeWindow) => void
  >();
  let sequence = 0;
  let now = 0;
  const scope: { update?: WindowGeometryUpdater; [key: string]: unknown } = {
    ExtensionError: Error,
    // Stock position normalization depends on screen APIs. This dependency
    // preserves its omitted-coordinate behavior; the helper owns its timing.
    sanitizePositionParams(
      options: WindowGeometryRequest,
      window: WindowGeometryNativeWindow,
    ) {
      if (options.left === null && options.top === null) return;
      if (options.left === null) options.left = window.screenX;
      if (options.top === null) options.top = window.screenY;
    },
    ChromeUtils: {
      importESModule: () => ({
        setTimeout(callback: () => void, delay: number) {
          const id = ++sequence;
          timers.set(id, { at: now + delay, callback });
          return id;
        },
        clearTimeout(id: number | undefined) {
          if (id !== undefined) timers.delete(id);
        },
      }),
    },
    windowTracker: {
      addCloseListener: (
        listener: (window: WindowGeometryNativeWindow) => void,
      ) => closeListeners.add(listener),
      removeCloseListener: (
        listener: (window: WindowGeometryNativeWindow) => void,
      ) => closeListeners.delete(listener),
    },
  };
  runInNewContext(
    additions.slice(start, end) +
      "\nthis.update = updateChromeWindowGeometry;",
    scope,
  );
  assert(scope.update);
  return {
    update: scope.update,
    timers,
    closeListeners,
    async advance(milliseconds: number) {
      now += milliseconds;
      for (const [id, timer] of [...timers]) {
        if (timer.at <= now) {
          timers.delete(id);
          timer.callback();
        }
      }
      await flush();
    },
    close(window: WindowFixture) {
      window.closed = true;
      for (const listener of [...closeListeners]) listener(window);
    },
    cleaned(window: WindowFixture, context: ContextFixture) {
      assertEquals(window.count + window.windowRoot.count, 0);
      assertEquals(context.handlers.size, 0);
      assertEquals(closeListeners.size, 0);
      assertEquals(timers.size, 0);
    },
  };
}

Deno.test("Chrome window update waits for trusted native resize and reads actual new geometry", async () => {
  const h = await harness();
  const window = new WindowFixture();
  const context = new ContextFixture();
  let finished = false;
  const pending = h.update(context, window, request({ width: 900 }), () => {});
  pending.then(() => finished = true);
  await flush();
  assertEquals(finished, false);
  window.resize(900, 800, false);
  await flush();
  assertEquals(
    finished,
    false,
    "An untrusted event does not acknowledge a widget request",
  );
  window.emit({ type: "resize", isTrusted: true, target: new WindowFixture() });
  await flush();
  assertEquals(
    finished,
    false,
    "Another window's event cannot acknowledge this request",
  );
  window.resize(900, 800);
  assertEquals((await pending).completion, "acknowledged");
  assertEquals(window.snapshot().width, 900);
  h.cleaned(window, context);
});

Deno.test("Chrome window update distinguishes synchronous and no-op completion without a timer wait", async () => {
  const h = await harness();
  const window = new WindowFixture();
  const context = new ContextFixture();
  const changed = await h.update(
    context,
    window,
    request({ width: 900 }),
    () => {
      window.outerWidth = 900;
    },
  );
  assertEquals(changed.completion, "synchronous");
  const same = await h.update(
    context,
    window,
    request({ width: 900 }),
    () => {},
  );
  assertEquals(same.completion, "unchanged");
  h.cleaned(window, context);
});

Deno.test("Clamped native geometry is returned at the bound, with observed groups distinct from exact acknowledgement", async () => {
  const h = await harness();
  const window = new WindowFixture();
  const context = new ContextFixture();
  let finished = false;
  const pending = h.update(
    context,
    window,
    request({ left: -100, width: 1 }),
    () => {},
  );
  pending.then(() => finished = true);
  await flush();
  window.move(0, 20);
  await flush();
  assertEquals(
    finished,
    false,
    "Move acknowledgement does not complete pending resize",
  );
  window.resize(400, 800);
  await flush();
  assertEquals(
    finished,
    false,
    "A nonmatching resize may still be intermediate",
  );
  await h.advance(2000);
  const completion = await pending;
  assertEquals(completion.completion, "timeout");
  assertEquals(completion.nativeEvents.move, true);
  assertEquals(completion.nativeEvents.resize, true);
  assertEquals(window.snapshot(), {
    left: 0,
    top: 20,
    width: 400,
    height: 800,
  });
  h.cleaned(window, context);
});

Deno.test("A prior or intermediate trusted resize cannot resolve a newer request before its target", async () => {
  const h = await harness();
  const window = new WindowFixture();
  const context = new ContextFixture();
  let finished = false;
  const pending = h.update(context, window, request({ width: 1100 }), () => {});
  pending.then(() => finished = true);
  await flush();
  window.resize(1020, 800);
  await flush();
  assertEquals(finished, false);
  window.resize(1100, 800);
  assertEquals((await pending).completion, "acknowledged");
  h.cleaned(window, context);
});

Deno.test("An ignored native window request resolves at the bound with unchanged actual geometry", async () => {
  const h = await harness();
  const window = new WindowFixture();
  const context = new ContextFixture();
  const original = window.snapshot();
  let finished = false;
  const pending = h.update(context, window, request({ width: 900 }), () => {});
  pending.then(() => finished = true);
  await flush();
  await h.advance(1999);
  assertEquals(finished, false);
  await h.advance(1);
  assertEquals((await pending).completion, "timeout");
  assertEquals(window.snapshot(), original);
  h.cleaned(window, context);
});

Deno.test("Window updates serialize per target while other windows can acknowledge independently", async () => {
  const h = await harness();
  const first = new WindowFixture();
  const second = new WindowFixture();
  const context = new ContextFixture();
  const calls: string[] = [];
  const one = h.update(
    context,
    first,
    request({ width: 900 }),
    () => calls.push("one"),
  );
  const two = h.update(
    context,
    first,
    request({ width: 800 }),
    () => calls.push("two"),
  );
  const other = h.update(context, second, request({ width: 700 }), () => {
    calls.push("other");
    second.outerWidth = 700;
  });
  await other;
  assertEquals(calls, ["one", "other"]);
  first.resize(900, 800);
  await one;
  await flush();
  assertEquals(calls, ["one", "other", "two"]);
  first.resize(800, 800);
  assertEquals((await two).completion, "acknowledged");
  h.cleaned(first, context);
  assertEquals(second.count + second.windowRoot.count, 0);
});

Deno.test("Cancelling a queued context neither mutates its window later nor releases the active FIFO slot", async () => {
  const h = await harness();
  const window = new WindowFixture();
  const activeContext = new ContextFixture();
  const cancelledContext = new ContextFixture();
  const calls: string[] = [];
  const first = h.update(
    activeContext,
    window,
    request({ width: 900 }),
    () => calls.push("first"),
  );
  const cancelled = h.update(
    cancelledContext,
    window,
    request({ width: 800 }),
    () => calls.push("cancelled"),
  );
  const rejection = assertRejects(() => cancelled, Error, "context closed");
  await flush();
  cancelledContext.close();
  await rejection;
  const last = h.update(
    activeContext,
    window,
    request({ width: 700 }),
    () => calls.push("last"),
  );
  await flush();
  assertEquals(calls, ["first"]);
  window.resize(900, 800);
  await first;
  await flush();
  assertEquals(calls, ["first", "last"]);
  window.resize(700, 800);
  await last;
  assertEquals(cancelledContext.handlers.size, 0);
  h.cleaned(window, activeContext);
});

Deno.test("A queued one-axis move derives its omitted coordinate after the preceding native move", async () => {
  const h = await harness();
  const window = new WindowFixture();
  const context = new ContextFixture();
  const one = request({ left: 100 });
  const two = request({ top: 200 });
  const applied: WindowGeometryRequest[] = [];
  const first = h.update(context, window, one, () => applied.push({ ...one }));
  const second = h.update(context, window, two, () => applied.push({ ...two }));
  await flush();
  assertEquals(applied.length, 1);
  window.move(100, 20);
  await first;
  await flush();
  assertEquals(applied[1], request({ left: 100, top: 200 }));
  window.move(100, 200);
  await second;
  h.cleaned(window, context);
});

Deno.test("Active context or target-window closure cancels native acknowledgement and cleans listeners", async () => {
  for (const kind of ["context", "window"] as const) {
    const h = await harness();
    const window = new WindowFixture();
    const context = new ContextFixture();
    const pending = h.update(
      context,
      window,
      request({ width: 900 }),
      () => {},
    );
    const rejection = assertRejects(() => pending, Error, "closed");
    await flush();
    if (kind === "context") context.close();
    else h.close(window);
    await rejection;
    h.cleaned(window, context);
  }
});

Deno.test("Window access revocation and native request errors cannot leave a queued mutation or listener", async () => {
  const h = await harness();
  const window = new WindowFixture();
  const context = new ContextFixture();
  const pending = h.update(context, window, request({ width: 900 }), () => {});
  const rejection = assertRejects(() => pending, Error, "no longer available");
  await flush();
  context.accessible = false;
  window.resize(900, 800);
  await rejection;
  h.cleaned(window, context);
  context.accessible = true;
  await assertRejects(
    () =>
      h.update(context, window, request({ width: 800 }), () => {
        throw new Error("native request failed");
      }),
    Error,
    "native request failed",
  );
  h.cleaned(window, context);
});
